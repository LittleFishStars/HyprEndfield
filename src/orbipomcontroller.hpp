#pragma once
#include <QElapsedTimer>
#include <QJSEngine>
#include <QImage>
#include <QQuickPaintedItem>
#include <QTimer>
#include <QVariantMap>
#include <memory>

// OrbiPomSession / OrbiPomRuntime: the unmodified offline JS owns every rule.
class OrbiPomController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap state READ state NOTIFY changed)
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activityChanged)
    Q_PROPERTY(bool held READ held WRITE setHeld NOTIFY activityChanged)
    Q_PROPERTY(bool lowPower READ lowPower WRITE setLowPower NOTIFY activityChanged)
public:
    explicit OrbiPomController(QObject *parent = nullptr);
    ~OrbiPomController() override;
    QVariantMap state() const;
    bool active() const { return m_active; }
    bool held() const { return m_held; }
    bool lowPower() const { return m_lowPower; }
    void setActive(bool value);
    void setHeld(bool value);
    void setLowPower(bool value);
    Q_INVOKABLE bool start(int seed = -1);
    Q_INVOKABLE void togglePause();
    Q_INVOKABLE void move(qreal x, qreal y);
    Q_INVOKABLE void release(qreal x, qreal y);
    Q_INVOKABLE void drop();
    Q_INVOKABLE void skill(const QString &name);
    Q_INVOKABLE void cancelSkill();
    void advance(qreal seconds);
signals:
    void changed();
    void activityChanged();
    void actionFinished(const QString &action, bool success, const QString &message);
private:
    friend class OrbiPomTests;
    QJSValue call(const QString &method, const QJSValueList &arguments = {});
    bool ensureRuntime();
    void refresh();
    void reconcile();
    void saveBest();
    std::unique_ptr<QJSEngine> m_engine;
    QJSValue m_bridge;
    QTimer m_clock;
    QElapsedTimer m_elapsed;
    QVariantMap m_state;
    QString m_error;
    int m_best = 0;
    bool m_active = false, m_held = false, m_lowPower = false, m_manualPause = false;
};

// Original sprite placement / collision rings from OrbiPomPiece.
class OrbiPomBoard : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(OrbiPomController *controller READ controller WRITE setController NOTIFY appearanceChanged)
    Q_PROPERTY(QColor accent MEMBER m_accent NOTIFY appearanceChanged)
    Q_PROPERTY(QColor dangerInk MEMBER m_dangerInk NOTIFY appearanceChanged)
public:
    explicit OrbiPomBoard(QQuickItem *parent = nullptr);
    OrbiPomController *controller() const { return m_controller; }
    void setController(OrbiPomController *controller);
    void paint(QPainter *painter) override;
signals:
    void appearanceChanged();
private:
    OrbiPomController *m_controller = nullptr;
    QColor m_accent = QColor("#FFFA00"), m_dangerInk = QColor("#E5383B");
    QImage m_images[11];
};
