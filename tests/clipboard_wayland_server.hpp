#pragma once

#include "wlr-data-control-unstable-v1-server-protocol.h"

#include <QByteArray>
#include <QTemporaryDir>

#include <wayland-server.h>

#include <atomic>
#include <cstring>
#include <fcntl.h>
#include <mutex>
#include <optional>
#include <sys/socket.h>
#include <sys/un.h>
#include <thread>
#include <unistd.h>

// A private compositor fixture. It never connects to the desktop session and
// exposes only the clipboard protocol needed to exercise real fd transfers.
class ClipboardWaylandServer
{
public:
    ClipboardWaylandServer()
    {
        if (!directory.isValid()) return;
        path = (directory.path() + "/wayland-test").toUtf8();
        const int socket = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
        if (socket < 0) return;
        sockaddr_un address{};
        address.sun_family = AF_UNIX;
        std::strncpy(address.sun_path, path.constData(), sizeof(address.sun_path) - 1);
        if (::bind(socket, reinterpret_cast<sockaddr *>(&address), sizeof(address)) < 0
            || ::listen(socket, 16) < 0) { ::close(socket); return; }
        display = wl_display_create();
        if (!display || wl_display_add_socket_fd(display, socket) < 0) { ::close(socket); return; }
        socketDescriptor = socket;
        wl_global_create(display, &wl_seat_interface, 1, this, bindSeat);
        wl_global_create(display, &zwlr_data_control_manager_v1_interface, 2, this, bindManager);
        thread = std::thread([this] {
            while (!stopping) {
                wl_event_loop_dispatch(wl_display_get_event_loop(display), 5);
                std::optional<QByteArray> next;
                { std::lock_guard lock(mutex); next.swap(pendingText); }
                if (next && device) sendOffer(*next);
                wl_display_flush_clients(display);
            }
            wl_display_destroy_clients(display);
        });
    }

    ~ClipboardWaylandServer()
    {
        stopping = true;
        if (thread.joinable()) thread.join();
        if (display) wl_display_destroy(display);
        if (socketDescriptor >= 0) ::close(socketDescriptor);
        if (copyReadDescriptor >= 0) ::close(copyReadDescriptor);
        const int copyWrite = copyWriteDescriptor.exchange(-1);
        if (copyWrite >= 0) ::close(copyWrite);
    }

    bool valid() const { return thread.joinable(); }
    QByteArray socketPath() const { return path; }
    int nextCopyDescriptor()
    {
        int descriptors[2];
        if (::pipe2(descriptors, O_CLOEXEC | O_NONBLOCK) < 0) return -1;
        copyReadDescriptor = descriptors[0];
        copyWriteDescriptor = descriptors[1];
        return copyReadDescriptor;
    }
    void offerText(const QByteArray &text)
    {
        std::lock_guard lock(mutex);
        pendingText = text;
    }

private:
    static void destroyResource(wl_client *, wl_resource *resource) { wl_resource_destroy(resource); }
    static void bindSeat(wl_client *client, void *, uint32_t version, uint32_t id)
    {
        auto *resource = wl_resource_create(client, &wl_seat_interface, version, id);
        wl_resource_set_implementation(resource, nullptr, nullptr, nullptr);
        wl_seat_send_capabilities(resource, 0);
    }
    static void bindManager(wl_client *client, void *data, uint32_t version, uint32_t id)
    {
        auto *resource = wl_resource_create(client, &zwlr_data_control_manager_v1_interface, version, id);
        wl_resource_set_implementation(resource, &managerImplementation, data, nullptr);
    }
    static void createSource(wl_client *client, wl_resource *manager, uint32_t id)
    {
        auto *resource = wl_resource_create(client, &zwlr_data_control_source_v1_interface, 1, id);
        wl_resource_set_implementation(resource, &sourceImplementation, wl_resource_get_user_data(manager), nullptr);
    }
    static void offerMime(wl_client *, wl_resource *, const char *) {}
    static void getDevice(wl_client *client, wl_resource *manager, uint32_t id, wl_resource *)
    {
        auto *self = static_cast<ClipboardWaylandServer *>(wl_resource_get_user_data(manager));
        self->device = wl_resource_create(client, &zwlr_data_control_device_v1_interface,
                                         wl_resource_get_version(manager), id);
        wl_resource_set_implementation(self->device, &deviceImplementation, self, [](wl_resource *resource) {
            static_cast<ClipboardWaylandServer *>(wl_resource_get_user_data(resource))->device = nullptr;
        });
        // Startup must ignore the existing selection without requesting bytes.
        self->sendOffer(QByteArrayLiteral("initial selection must remain unread"));
    }
    static void setSelection(wl_client *, wl_resource *device, wl_resource *source)
    {
        auto *self = static_cast<ClipboardWaylandServer *>(wl_resource_get_user_data(device));
        if (!source) return;
        self->sendOffer(QByteArrayLiteral("self-generated selection"));
        const int copyWrite = self->copyWriteDescriptor.exchange(-1);
        if (copyWrite >= 0) {
            zwlr_data_control_source_v1_send_send(source, "text/plain;charset=utf-8", copyWrite);
            ::close(copyWrite);
        }
    }
    static void setPrimary(wl_client *, wl_resource *, wl_resource *) {}
    static void receive(wl_client *, wl_resource *offer, const char *, int descriptor)
    {
        const auto *bytes = static_cast<QByteArray *>(wl_resource_get_user_data(offer));
        ::write(descriptor, bytes->constData(), static_cast<size_t>(bytes->size()));
        ::close(descriptor);
    }
    void sendOffer(const QByteArray &bytes)
    {
        auto *resource = wl_resource_create(wl_resource_get_client(device), &zwlr_data_control_offer_v1_interface, 1, 0);
        wl_resource_set_implementation(resource, &offerImplementation, new QByteArray(bytes), [](wl_resource *offer) {
            delete static_cast<QByteArray *>(wl_resource_get_user_data(offer));
        });
        zwlr_data_control_device_v1_send_data_offer(device, resource);
        zwlr_data_control_offer_v1_send_offer(resource, "text/plain;charset=utf-8");
        zwlr_data_control_device_v1_send_selection(device, resource);
    }

    QTemporaryDir directory;
    QByteArray path;
    wl_display *display = nullptr;
    wl_resource *device = nullptr;
    int socketDescriptor = -1;
    int copyReadDescriptor = -1;
    std::atomic<int> copyWriteDescriptor = -1;
    std::thread thread;
    std::atomic<bool> stopping = false;
    std::mutex mutex;
    std::optional<QByteArray> pendingText;
    inline static const struct zwlr_data_control_manager_v1_interface managerImplementation{
        createSource, getDevice, destroyResource};
    inline static const struct zwlr_data_control_source_v1_interface sourceImplementation{offerMime, destroyResource};
    inline static const struct zwlr_data_control_device_v1_interface deviceImplementation{
        setSelection, destroyResource, setPrimary};
    inline static const struct zwlr_data_control_offer_v1_interface offerImplementation{receive, destroyResource};
};
