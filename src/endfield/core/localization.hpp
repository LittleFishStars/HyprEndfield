#pragma once
#include <string>
#include <string_view>
#include <QCoreApplication>
namespace endfield::core {
enum class Language { system, english, simplifiedChinese, traditionalChinese, japanese, korean };
inline std::string localized(std::string_view english, std::string_view chinese, Language) {
    const std::string source(chinese.empty() ? english : chinese);
    return QCoreApplication::translate("HyprEndfield", source.c_str()).toStdString();
}
}
