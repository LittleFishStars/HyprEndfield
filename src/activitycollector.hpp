#pragma once

#include <QMap>
#include <QStringList>
#include <QVariantMap>
#include <optional>

namespace Activity {
struct Counters { quint64 first = 0, second = 0; };
struct Process {
    int pid = 0, parent = 0, threads = 0;
    quint64 start = 0, ticks = 0, memory = 0;
    QString name, executable;
    // Engine counters are shared by duplicated DRM client descriptors.
    QMap<QString, quint64> gpuEngines;
    QMap<QString, double> gpuCapacities;
    std::optional<double> gpuPercent;
};
struct App { QString id, name, icon; QList<int> pids; };
struct Reading {
    double seconds = 0;
    std::optional<Counters> cpu;
    std::optional<quint64> memoryUsed, memoryTotal;
    std::optional<double> gpu;
    QMap<QString, Counters> network, disks;
    QMap<int, Process> processes;
    QMap<QString, App> apps;
    bool applications = false;
    long ticksPerSecond = 100;
};
struct Paths {
    QString proc = "/proc", sys = "/sys";
    QStringList desktopDirs;
    bool helpers = true;
};

std::optional<Counters> parseCpu(const QByteArray &text);
std::optional<Process> parseProcess(const QByteArray &text, long pageSize);
void parseDrm(const QByteArray &text, Process &process);
QMap<int, double> parseNvidiaProcesses(const QByteArray &text);
QVariantMap derive(const Reading &now, const Reading *previous);
QVariantList deriveApps(const Reading &now, const Reading *previous);
QVariantList sortedApps(QVariantList values, const QString &key, bool descending);

// Used on one worker, never on the scene/render thread. It reads metadata and
// counters only, not command arguments, file contents, or process environments.
class Collector {
public:
    explicit Collector(Paths paths = {});
    Reading sample(bool applications);
private:
    struct Desktop { QString id, name, icon, binary, windowClass; };
    void loadDesktopEntries();
    void collectApplications(Reading &reading);
    Paths m_paths;
    QList<Desktop> m_desktops;
    bool m_catalogLoaded = false;
};
}
