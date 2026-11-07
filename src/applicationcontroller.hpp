#pragma once

#include <QObject>
#include <QPointer>
#include <QString>

class QLocalServer;
class QQuickWindow;

class ApplicationController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool hudVisible READ hudVisible NOTIFY hudVisibleChanged)
    Q_PROPERTY(bool hudOpen READ hudOpen NOTIFY hudOpenChanged)
    Q_PROPERTY(QString localUserName READ localUserName CONSTANT)

public:
    explicit ApplicationController(QObject *parent = nullptr);
    ~ApplicationController() override;

    [[nodiscard]] bool hudVisible() const;
    [[nodiscard]] bool hudOpen() const;
    [[nodiscard]] QString localUserName() const { return qEnvironmentVariable("USER"); }
    void setWindow(QQuickWindow *window);
    bool listen(const QString &serverName);
    void executeCommand(const QString &command);

public slots:
    void showHud();
    void hideHud();
    void toggleHud();
    void completeHide();
    void quitApplication();

signals:
    void hudVisibleChanged();
    void hudOpenChanged();

private slots:
    void acceptConnection();

private:
    void activateHyprlandSubmap();
    void resetHyprlandSubmap();
    void setHudVisible(bool visible);
    void setHudOpen(bool open);

    QPointer<QQuickWindow> m_window;
    QLocalServer *m_server = nullptr;
    bool m_hudVisible = false;
    bool m_hudOpen = false;
    bool m_hyprlandSubmapActive = false;
};
