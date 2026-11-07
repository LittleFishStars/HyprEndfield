#pragma once

#include <QColor>
#include <QDateTime>
#include <QObject>
#include <QPointF>
#include <QUrl>
#include <QVariantMap>

class NotesController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap state READ state NOTIFY stateChanged)

public:
    explicit NotesController(QObject *parent = nullptr, QString directory = {});

    QVariantMap state() const;

    Q_INVOKABLE bool setCentreTab(const QString &tab);
    Q_INVOKABLE bool setCentreFolded(bool folded);
    Q_INVOKABLE bool setCentreText(const QString &text);
    Q_INVOKABLE bool setCentreStyle(const QString &key, const QVariant &value);
    Q_INVOKABLE bool addCentreTask(const QString &text);
    Q_INVOKABLE bool updateCentreTask(const QString &id, const QString &text);
    Q_INVOKABLE bool toggleCentreTask(const QString &id);
    Q_INVOKABLE bool toggleCentreFlag(const QString &id);
    Q_INVOKABLE bool moveCentreTask(const QString &id, int offset);
    Q_INVOKABLE bool removeCentreTask(const QString &id);

    Q_INVOKABLE QString createNote(const QString &kind, double x, double y);
    Q_INVOKABLE QString importImage(const QUrl &url, double x, double y);
    Q_INVOKABLE bool updateNoteGeometry(const QString &id, double x, double y,
                                        double width, double height);
    Q_INVOKABLE bool updateNoteText(const QString &id, const QString &text);
    Q_INVOKABLE bool toggleNotePin(const QString &id);
    Q_INVOKABLE bool removeNote(const QString &id);
    Q_INVOKABLE bool addNoteTask(const QString &id, const QString &text);
    Q_INVOKABLE bool updateNoteTask(const QString &id, const QString &taskId,
                                    const QString &text);
    Q_INVOKABLE bool toggleNoteTask(const QString &id, const QString &taskId);
    Q_INVOKABLE bool moveNoteTask(const QString &id, const QString &taskId, int offset);
    Q_INVOKABLE bool removeNoteTask(const QString &id, const QString &taskId);
    Q_INVOKABLE bool appendStroke(const QString &id, const QVariantList &points,
                                  double width, const QString &color);
    Q_INVOKABLE bool eraseDrawing(const QString &id, double x, double y, double radius);
    Q_INVOKABLE bool eraseDrawingPath(const QString &id, const QVariantList &points, double radius);
    Q_INVOKABLE bool clearDrawing(const QString &id);

    static bool validColor(const QString &color);

    struct Task {
        QString id;
        QString text;
        bool checked = false;
        bool flagged = false;
    };
    struct Style {
        QString family = QStringLiteral("HarmonyOS Sans");
        double size = 14;
        QString color = QStringLiteral("#EEEEEE");
        bool bold = false;
        bool italic = false;
        bool underline = false;
        bool strikeout = false;
    };
    struct Centre {
        QString tab = QStringLiteral("todo");
        bool folded = false;
        QString text;
        Style style;
        QList<Task> tasks;
    };
    struct Stroke {
        QList<QPointF> points;
        double width = 8;
        QString color = QStringLiteral("#FAD40A");
    };
    struct Note {
        QString id;
        QString kind;
        QString text;
        QList<Task> tasks;
        QString imageName;
        double x = 24;
        double y = 50;
        double width = 210;
        double height = 140;
        int z = 0;
        bool pinned = false;
        QDateTime created;
        QList<Stroke> strokes;
    };

signals:
    void stateChanged();
    void actionFinished(const QString &action, bool success, const QString &message);

private:

    void load();
    bool persist(const Centre &centre, const QList<Note> &notes);
    void fail(const QString &message, const QString &action = {});
    int noteIndex(const QList<Note> &notes, const QString &id) const;
    int nextZ(const QList<Note> &notes) const;
    static void constrain(Note &note);
    static QVariantMap taskState(const Task &task);

    QString m_directory;
    QString m_imagesDirectory;
    QString m_path;
    QByteArray m_persisted;
    Centre m_centre;
    QList<Note> m_notes;
    QString m_error;
    bool m_available = true;
};
