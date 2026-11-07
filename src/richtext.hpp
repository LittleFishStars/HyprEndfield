#pragma once
#include <QQuickTextDocument>
#include <QTextCursor>
#include <QTextCharFormat>
#include <QTextDocument>
#include <QVariantMap>
#include <algorithm>

// Shared Qt equivalent of EndfieldHUD NotesRichText / HUDStandardRichText.
inline QVariantMap richTextStyle(QObject *object, int position) {
    auto *wrapper = qobject_cast<QQuickTextDocument *>(object);
    if (!wrapper || !wrapper->textDocument()) return {};
    QTextCursor cursor(wrapper->textDocument());
    cursor.setPosition(std::clamp(position, 0, wrapper->textDocument()->characterCount() - 1));
    const auto f = cursor.charFormat();
    return {{"family", f.fontFamilies().toStringList().value(0, "HarmonyOS Sans")},
        {"size", f.fontPointSize() > 0 ? f.fontPointSize() : 12},
        {"color", f.foreground().style() == Qt::NoBrush ? "#EDEDED" : f.foreground().color().name().toUpper()},
        {"bold", f.fontWeight() >= QFont::Bold}, {"italic", f.fontItalic()},
        {"underline", f.fontUnderline()}, {"strikeout", f.fontStrikeOut()}};
}
inline bool formatRichText(QObject *object, int start, int end, const QString &key, const QVariant &value) {
    auto *wrapper = qobject_cast<QQuickTextDocument *>(object);
    if (!wrapper || !wrapper->textDocument()) return false;
    auto *document = wrapper->textDocument(); QTextCursor cursor(document);
    const int upper = std::max(0, document->characterCount() - 1);
    start = std::clamp(start, 0, upper); end = std::clamp(end, 0, upper);
    if (start == end) cursor.select(QTextCursor::Document);
    else { cursor.setPosition(std::min(start, end)); cursor.setPosition(std::max(start, end), QTextCursor::KeepAnchor); }
    QTextCharFormat format;
    if (key == "family") format.setFontFamilies({value.toString()});
    else if (key == "size") format.setFontPointSize(std::clamp(value.toDouble(), 6., 144.));
    else if (key == "color" && QColor(value.toString()).isValid()) format.setForeground(QColor(value.toString()));
    else if (key == "bold") format.setFontWeight(value.toBool() ? QFont::Bold : QFont::Normal);
    else if (key == "italic") format.setFontItalic(value.toBool());
    else if (key == "underline") format.setFontUnderline(value.toBool());
    else if (key == "strikeout") format.setFontStrikeOut(value.toBool());
    else return false;
    cursor.mergeCharFormat(format); return true;
}
