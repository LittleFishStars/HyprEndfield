#pragma once
#include <QObject>
#include <QColor>
#include <QImage>
#include <QUrl>
#include <QVariantMap>
#include <QQuickImageProvider>
#include <functional>
#include <optional>

// AppShortcutStore/Canvas rules, with .desktop references and GIO launching on Linux.
class AppShortcutsController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap state READ state NOTIFY changed)
    Q_PROPERTY(QVariantList presets READ presets NOTIFY changed)
public:
    using TestLauncher = std::function<QString(const QString &)>;
    explicit AppShortcutsController(QObject *parent = nullptr, QString directory = {}, TestLauncher launcher = {});
    QVariantMap state() const;
    QVariantList presets() const;
    QImage artwork(const QString &id, const QString &preset, QColor accent, QSize size) const;
    Q_INVOKABLE bool add(const QUrl &url);
    Q_INVOKABLE bool rename(const QString &id, const QString &name);
    Q_INVOKABLE bool setIcon(const QString &id, const QString &preset);
    Q_INVOKABLE bool remove(const QString &id);
    Q_INVOKABLE void paste();
    Q_INVOKABLE void refresh();
    Q_INVOKABLE bool requestOpen(const QString &id);
    void launchPending();
signals:
    void changed();
    void launchRequested();
    void launchFailed();
    void opened(const QString &name);
    void added(const QString &id);
private:
    struct Entry { QString id, name, originalName, path, executable, iconPreset; qint64 created = 0; };
    struct App { QString name, executable; QImage icon; QColor color = QColor("#2A2A2A"); };
    std::optional<App> inspect(const QString &path) const;
    void load();
    bool commit(const QList<Entry> &next);
    bool fail(const QString &message);
    static QString normalizeName(const QString &value);
    QString m_directory, m_path, m_error, m_pending;
    QList<Entry> m_entries;
    QHash<QString, App> m_apps;
    QByteArray m_saved;
    bool m_readable = true, m_launching = false;
    TestLauncher m_testLauncher;
};

class ShortcutIconProvider final : public QQuickImageProvider {
public:
    explicit ShortcutIconProvider(AppShortcutsController *controller) : QQuickImageProvider(QQuickImageProvider::Image), m_controller(controller) {}
    QImage requestImage(const QString &id, QSize *size, const QSize &requested) override {
        const auto parts = id.split('/');
        QColor accent("#" + parts.value(2)); if (!accent.isValid()) accent = QColor("#FFFA00");
        const auto image = m_controller->artwork(parts.value(0), parts.value(1, "original"), accent, requested);
        if (size) *size = image.size();
        return image;
    }
private:
    AppShortcutsController *m_controller;
};
