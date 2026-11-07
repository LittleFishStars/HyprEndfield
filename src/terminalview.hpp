#pragma once

#include "terminalsession.hpp"
#include <QQuickPaintedItem>
#include <QPointer>
#include <QFont>

// The terminal is rendered inside the same projected plane as the other pages.
// A high-resolution texture keeps the cell grid legible under HUD perspective.
class TerminalView : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(TerminalSession *session READ session WRITE setSession NOTIFY sessionChanged)
    Q_PROPERTY(qreal renderScale READ renderScale WRITE setRenderScale NOTIFY renderScaleChanged)
public:
    explicit TerminalView(QQuickItem *parent = nullptr);
    TerminalSession *session() const { return m_session; }
    void setSession(TerminalSession *session);
    qreal renderScale() const { return m_renderScale; }
    void setRenderScale(qreal scale);
    void paint(QPainter *painter) override;
    QVariant inputMethodQuery(Qt::InputMethodQuery query) const override;
    Q_INVOKABLE void activate();
    Q_INVOKABLE void deactivate();
    Q_INVOKABLE void copySelection();
    Q_INVOKABLE void pasteClipboard();
protected:
    bool event(QEvent *event) override;
    void geometryChange(const QRectF &geometry, const QRectF &old) override;
    void keyPressEvent(QKeyEvent *event) override;
    void inputMethodEvent(QInputMethodEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
signals:
    void sessionChanged();
    void renderScaleChanged();
private:
    struct Cell { QString text; QColor foreground, background; VTermScreenCellAttrs attributes{}; int width = 1; };
    void resizeTerminal();
    void refresh();
    void scroll(int rows);
    QPoint cellAt(QPointF position) const;
    int selectionPosition(QPointF position) const;
    bool reportMouse(QMouseEvent *event, bool pressed, bool release = false);
    QRectF cursorRect() const;
    QPointer<TerminalSession> m_session;
    QVector<Cell> m_cells;
    QFont m_font;
    qreal m_cellWidth = 8, m_cellHeight = 18, m_ascent = 13, m_renderScale = 2;
    int m_rows = 2, m_columns = 2, m_scrollOffset = 0, m_topRow = 0;
    int m_selectionStart = -1, m_selectionEnd = -1;
    qint64 m_scrollSerial = 0;
    bool m_selecting = false, m_cursorVisible = false;
    int m_cursorShape = VTERM_PROP_CURSORSHAPE_BLOCK;
    VTermPos m_cursor{};
    QString m_preedit;
};
