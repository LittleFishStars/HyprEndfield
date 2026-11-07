#pragma once
#include <string>
#include <string_view>
#include <span>
#include <QCoreApplication>
namespace endfield::core {
enum class Language { system, english, simplifiedChinese, traditionalChinese, japanese, korean };
inline bool isCJK(Language language) noexcept { return language != Language::system && language != Language::english; }
inline std::string localized(std::string_view english, std::string_view chinese, Language) {
    const std::string source(chinese.empty() ? english : chinese);
    return QCoreApplication::translate("HyprEndfield", source.c_str()).toStdString();
}
inline std::string localized(std::string_view english, std::string_view chinese, Language language,
                             std::span<const std::string_view> arguments) {
    QString text = QString::fromStdString(localized(english, chinese, language));
    for (std::size_t i = 0; i < arguments.size(); ++i)
        text.replace(QStringLiteral("{%1}").arg(i), QString::fromUtf8(arguments[i].data(), arguments[i].size()));
    return text.toStdString();
}
}
