#pragma once

#include <QColor>
#include <QElapsedTimer>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QQmlPropertyMap>
#include <QSettings>
#include <QTimer>
#include <QTranslator>
#include <optional>

class QQuickWindow;

// Persist only HUD preferences. Module documents remain owned by their controllers.
class SettingsController final : public QQmlPropertyMap
{
    Q_OBJECT
    Q_PROPERTY(QVariantList displays READ displays NOTIFY displaysChanged)
    Q_PROPERTY(QVariantList icons READ icons CONSTANT)
    Q_PROPERTY(QString notice READ notice NOTIFY noticeChanged)
    Q_PROPERTY(int layoutRemaining READ layoutRemaining NOTIFY layoutChanged)
    Q_PROPERTY(bool light READ light NOTIFY appearanceChanged)
    Q_PROPERTY(QColor accent READ accent NOTIFY appearanceChanged)
    Q_PROPERTY(QUrl centerLogo READ centerLogo NOTIFY appearanceChanged)
    Q_PROPERTY(QVariantMap updateState READ updateState NOTIFY updateStateChanged)
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(QString buildId READ buildId CONSTANT)
    Q_PROPERTY(bool batteryAlertVisible READ batteryAlertVisible NOTIFY batteryAlertChanged)
    Q_PROPERTY(bool editingBatteryPosition READ editingBatteryPosition NOTIFY batteryAlertChanged)
public:
    explicit SettingsController(QObject *parent = nullptr, QString preferences = {}, QString autostart = {}, QString data = {});
    QVariantList displays() const;
    QVariantList icons() const { return m_icons; }
    QString notice() const { return m_notice; }
    int layoutRemaining() const;
    bool light() const;
    QColor accent() const;
    QUrl centerLogo() const;
    QVariantMap updateState() const { return m_update; }
    QString version() const;
    QString buildId() const;
    bool batteryAlertVisible() const { return m_alertVisible || m_editBattery; }
    bool editingBatteryPosition() const { return m_editBattery; }
    void setBatteryWindow(QQuickWindow *window);
    void updateBattery(const QVariantMap &state);
    Q_INVOKABLE void editBatteryPosition();
    Q_INVOKABLE void placeBattery(double x, double y);
    Q_INVOKABLE void cancelBatteryPosition();
    Q_INVOKABLE bool set(const QString &key, const QVariant &value);
    Q_INVOKABLE void previewLayout(const QString &key, double value);
    Q_INVOKABLE void confirmLayout();
    Q_INVOKABLE void revertLayout();
    Q_INVOKABLE void restoreDefaults();
    Q_INVOKABLE void resetModules();
    Q_INVOKABLE void moveModule(const QString &key, int destination);
    Q_INVOKABLE void showModule(const QString &key, bool visible);
    Q_INVOKABLE void importLogo(const QUrl &url);
    Q_INVOKABLE void checkUpdates();
    Q_INVOKABLE void openLink(const QString &key);
    Q_INVOKABLE void clearNotice();
    Q_INVOKABLE QColor tone(const QColor &color) const;
    Q_INVOKABLE QColor color(const QColor &color) const;
    Q_INVOKABLE bool beginBootLog();
    Q_INVOKABLE void setNotice(const QString &message);
    static QStringList defaultModules();
signals:
    void preferenceChanged(const QString &key, const QVariant &value);
    void displaysChanged();
    void noticeChanged();
    void appearanceChanged();
    void languageChanged();
    void layoutChanged();
    void updateStateChanged();
    void batteryAlertChanged();
protected:
    QVariant updateValue(const QString &key, const QVariant &input) override;
private:
    QString storageKey(const QString &key) const;
    QVariant normalized(const QString &key, const QVariant &input) const;
    bool saveAutostart(bool enabled);
    void changed(const QString &key, const QVariant &value);
    void applyLanguage();
    void positionBattery();
    QSettings m_store;
    QString m_autostart, m_data, m_notice;
    QVariantMap m_defaults, m_layout;
    QVariantList m_icons;
    QTimer m_layoutTimer;
    QElapsedTimer m_layoutClock;
    QTranslator *m_translator = nullptr;
    QPointer<QQuickWindow> m_batteryWindow;
    QTimer m_alertTimer;
    bool m_alertVisible = false, m_editBattery = false, m_hasBattery = false;
    std::optional<bool> m_plugged;
    QNetworkAccessManager m_network;
    QVariantMap m_update{{"phase", "idle"}, {"detail", ""}, {"latest", ""}};
};
