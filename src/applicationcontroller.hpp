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

public:
    explicit ApplicationController(QObject *parent = nullptr);

    [[nodiscard]] bool hudVisible() const;
    void setWindow(QQuickWindow *window);
    bool listen(const QString &serverName);
    void executeCommand(const QString &command);

public slots:
    void showHud();
    void hideHud();
    void toggleHud();
    void quitApplication();

signals:
    void hudVisibleChanged();

private slots:
    void acceptConnection();

private:
    void setHudVisible(bool visible);

    QPointer<QQuickWindow> m_window;
    QLocalServer *m_server = nullptr;
    bool m_hudVisible = false;
};
