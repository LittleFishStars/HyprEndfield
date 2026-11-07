#pragma once
#include <QObject>
#include <QTimer>
#include <QVariantMap>

class BatteryController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap state READ state NOTIFY stateChanged)
    Q_PROPERTY(bool presented READ presented WRITE setPresented NOTIFY presentedChanged)
public:
    explicit BatteryController(
        QObject* parent = nullptr, QString root = QStringLiteral("/sys/class/power_supply"));
    QVariantMap state() const { return m_state; }
    bool presented() const { return m_presented; }
    void setPresented(bool presented);
    Q_INVOKABLE void refresh();
signals:
    void stateChanged();
    void presentedChanged();

private:
    QString m_root;
    QVariantMap m_state;
    QTimer m_timer;
    bool m_presented = false;
};
