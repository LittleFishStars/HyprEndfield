#pragma once
#include <QQuickPaintedItem>
#include <QFutureWatcher>
#include <QVariantList>

// EndfieldHUD ProjectionWorkspaceView.refreshBackground: a separate retained
// dot layer below media and ink, rebuilt by Qt when the Wayland surface changes.
class ProjectionDots : public QQuickPaintedItem {
    Q_OBJECT
public:
    explicit ProjectionDots(QQuickItem *parent = nullptr) : QQuickPaintedItem(parent) {}
    void paint(QPainter *painter) override;
};

// EndfieldHUD ProjectionModel + NotesDrawing; session content, no note files.
class ProjectionCanvas : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(QColor penColor MEMBER m_color NOTIFY penChanged)
    Q_PROPERTY(qreal penWidth MEMBER m_width NOTIFY penChanged)
    Q_PROPERTY(bool erasing MEMBER m_erasing NOTIFY penChanged)
    Q_PROPERTY(int strokeCount READ strokeCount NOTIFY contentChanged)
    Q_PROPERTY(QVariantList media READ media NOTIFY mediaChanged)
    Q_PROPERTY(QString message READ message NOTIFY messageChanged)
public:
    explicit ProjectionCanvas(QQuickItem *parent = nullptr);
    ~ProjectionCanvas() override;
    void paint(QPainter *painter) override;
    int strokeCount() const { return m_strokes.size(); }
    QVariantList media() const { return m_media; }
    QString message() const { return m_message; }
    Q_INVOKABLE void clearContent();
    Q_INVOKABLE void importMedia(const QUrl &url);
    Q_INVOKABLE void removeMedia(const QString &id);
    Q_INVOKABLE void placeMedia(const QString &id, qreal x, qreal y, qreal width, qreal height);
    Q_INVOKABLE void beginStroke(QPointF point);
    Q_INVOKABLE void moveStroke(QPointF point);
    Q_INVOKABLE void endStroke();
signals:
    void eventOccurred(const QString &action);
    void penChanged();
    void contentChanged();
    void mediaChanged();
    void messageChanged();
protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
private:
    struct Stroke { QList<QPointF> points; QColor color; qreal width; };
    void sample(QPointF point);
    void erase(QPointF point);
    QList<Stroke> m_strokes;
    Stroke m_pending;
    QColor m_color = QColor("#FFFA00");
    qreal m_width = 12;
    bool m_erasing = false, m_drawing = false;
    QVariantList m_media;
    QFutureWatcher<QVariantMap> m_import;
    QString m_message;
};
