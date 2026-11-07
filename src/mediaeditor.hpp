#pragma once
#include "endfield/modules/media_assembly_model.hpp"
#include <QQuickPaintedItem>
#include <QFutureWatcher>
#include <QImage>
#include <QMediaPlayer>
#include <QVideoSink>
#include <QAudioOutput>
#include <QTimer>
#include <atomic>

class MediaEditor : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(QVariantMap state READ state NOTIFY changed)
    Q_PROPERTY(QVariantList filters READ filters NOTIFY catalogChanged)
    Q_PROPERTY(QVariantList stickers READ stickers NOTIFY catalogChanged)
    Q_PROPERTY(bool presented READ presented WRITE setPresented NOTIFY presentedChanged)
    Q_PROPERTY(bool cropping READ cropping WRITE setCropping NOTIFY changed)
public:
    explicit MediaEditor(QQuickItem *parent = nullptr);
    ~MediaEditor() override;
    void paint(QPainter *) override;
    QVariantMap state() const;
    QVariantList filters() const;
    QVariantList stickers() const;
    bool presented() const { return m_presented; }
    void setPresented(bool presented);
    bool cropping() const { return m_cropping; }
    void setCropping(bool cropping);
    Q_INVOKABLE void openSource(const QUrl &url);
    Q_INVOKABLE void closeSource();
    Q_INVOKABLE void setAdjustment(const QString &key, double value);
    Q_INVOKABLE void setFilter(const QString &id);
    Q_INVOKABLE void addSticker(const QString &id);
    Q_INVOKABLE void reset(const QString &tool = {});
    Q_INVOKABLE void zoomBy(double factor);
    Q_INVOKABLE void resetZoom();
    Q_INVOKABLE void togglePlayback();
    Q_INVOKABLE void seek(double seconds);
    Q_INVOKABLE void exportTo(const QUrl &url, bool overwrite = false);
    Q_INVOKABLE void cancelExport();
    Q_INVOKABLE void retranslate() { emit catalogChanged(); emit changed(); }
signals:
    void changed();
    void catalogChanged();
    void presentedChanged();
protected:
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void wheelEvent(QWheelEvent *) override;
private:
    void requestPreview();
    void renderPreview();
    endfield::core::Rect imageRect() const;
    void editChanged();
    bool editable() const;
    endfield::modules::MediaAssemblyAdjustments m_adjustments;
    endfield::modules::MediaAssemblyViewport m_viewport;
    QVariantMap m_metadata;
    QString m_error, m_message, m_pendingOverwrite;
    QImage m_original, m_preview;
    QFutureWatcher<QVariantMap> m_open;
    QFutureWatcher<QImage> m_render;
    QFutureWatcher<QString> m_export;
    std::atomic_bool m_cancel{false};
    std::atomic<double> m_progress{0};
    QTimer m_renderTimer, m_progressTimer;
    QMediaPlayer m_player;
    QVideoSink m_sink;
    QAudioOutput m_audio;
    qint64 m_sourceBytes = 0, m_sourceModified = 0;
    bool m_presented = false, m_cropping = false, m_previewPending = false;
    int m_revision = 0, m_renderRevision = 0, m_cropCorner = -1;
    int m_selected = -1;
    std::optional<endfield::modules::MediaAssemblyHandle> m_handle;
    QPointF m_dragStart, m_previous;
    endfield::modules::MediaAssemblySticker m_dragSticker;
    endfield::core::Rect m_dragCrop;
};
