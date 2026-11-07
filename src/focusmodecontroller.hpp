#pragma once

#include <QObject>
#include <QSettings>
#include <QTimer>
#include <QVariantMap>

#include <functional>
#include <memory>
#include <optional>

class FocusModeController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap state READ state NOTIFY stateChanged)
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)

public:
    using Clock = std::function<double()>;

    explicit FocusModeController(QObject *parent = nullptr,
                                 Clock clock = {},
                                 QString settingsPath = {});
    ~FocusModeController() override;

    [[nodiscard]] QVariantMap state() const;
    [[nodiscard]] bool active() const { return m_active; }
    void setActive(bool value);

    Q_INVOKABLE bool chooseCountdown(int seconds);
    Q_INVOKABLE bool chooseCustomDuration(const QString &text);
    Q_INVOKABLE void chooseStopwatch();
    Q_INVOKABLE void start();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void resume();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void reset();
    Q_INVOKABLE void refresh();
    Q_INVOKABLE QString durationEditText() const;

public slots:
    void shutdown();

signals:
    void stateChanged();
    void activeChanged();
    void completed();

private:
    enum class Kind { Countdown, Stopwatch };
    enum class Phase { Idle, Running, Paused, Stopped, Completed };

    struct Snapshot {
        double elapsed = 0;
        double remaining = 0;
        int displayedSeconds = 0;
    };

    [[nodiscard]] double now() const;
    [[nodiscard]] Snapshot snapshotAt(double time) const;
    [[nodiscard]] double trackedWorkSecondsAt(double time) const;
    [[nodiscard]] QString timeTextAt(double time) const;
    void refreshAt(double time);
    void checkpoint(double time);
    void changed(double time);
    void publish(double time, bool force);
    void reconcileTimers(double time);
    void persistTrackedWorkSeconds(bool synchronize = false);
    static std::optional<int> parseDuration(const QString &text);
    static QString formatTime(qint64 seconds);
    static QString kindName(Kind value);
    static QString phaseName(Phase value);
    static double continuousTime();

    Clock m_clock;
    std::unique_ptr<QSettings> m_settings;
    QTimer m_displayTimer;
    QTimer m_completionTimer;
    Kind m_kind = Kind::Countdown;
    Phase m_phase = Phase::Idle;
    double m_duration = 30 * 60;
    double m_accumulated = 0;
    std::optional<double> m_startedAt;
    double m_trackedTotal = 0;
    double m_trackedSessionElapsed = 0;
    std::optional<int> m_publishedSecond;
    bool m_active = false;
    bool m_shutdown = false;
};
