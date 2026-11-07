#pragma once

#include <QObject>
#include <QPointer>
#include <QString>

class QLocalServer;
class QQuickWindow;
class SettingsController;

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
    void setSettings(SettingsController *settings);
    bool listen(const QString &serverName);
    void executeCommand(const QString &command);

public slots:
    void showHud();
    void hideHud();
    void toggleHud();
    void completeHide();
    void quitApplication();
    void beginFileDrag();
    void endFileDrag(bool delivered);

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
    void finishFileDrag();
    void selectScreen();
    void checkFocus();

    QPointer<QQuickWindow> m_window;
    QLocalServer *m_server = nullptr;
    SettingsController *m_settings = nullptr;
    bool m_receivedFocus = false;
    bool m_hudVisible = false;
    bool m_hudOpen = false;
    bool m_hyprlandSubmapActive = false;
    bool m_fileDragActive = false;
    bool m_fileDragFinished = false;
    bool m_fileDragDelivered = false;
    bool m_fileDragRetracted = false;
};
