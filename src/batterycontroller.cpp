#include "batterycontroller.hpp"
#include <QDir>
#include <QFile>
#include <QLocale>
#include <algorithm>
#include <cmath>
#include <optional>

namespace
{
QString read(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? QString::fromLatin1(file.read(256)).trimmed() : QString();
}
std::optional<double> number(const QString& path)
{
    bool ok = false;
    const double value = read(path).toDouble(&ok);
    return ok && std::isfinite(value) && value >= 0 ? std::optional(value) : std::nullopt;
}
}
BatteryController::BatteryController(QObject* parent, QString root)
    : QObject(parent)
    , m_root(std::move(root))
{
    m_timer.setInterval(15000);
    connect(&m_timer, &QTimer::timeout, this, &BatteryController::refresh);
    refresh();
}
void BatteryController::setPresented(bool presented)
{
    if (m_presented == presented)
        return;
    m_presented = presented;
    emit presentedChanged();
    if (presented)
    {
        refresh();
        m_timer.start();
    }
    else
        m_timer.stop();
}
void BatteryController::refresh()
{
    QVariantMap next { { "available", false }, { "current", "—" }, { "full", "—" }, { "percentage", -1 },
        { "progress", 0.0 }, { "estimated", false }, { "unit", "mAh" }, { "status", "未检测到电池" } };
    for (const auto& name : QDir(m_root).entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name))
    {
        const auto path = m_root + '/' + name + '/';
        if (read(path + "type") != "Battery" || read(path + "scope") == "Device"
            || read(path + "present") == "0")
            continue;
        next["available"] = true;
        const auto status = read(path + "status");
        next["status"] = status == "Charging" ? "正在充电"
            : status == "Full"                ? "已充满"
            : status == "Discharging"         ? "使用电池"
            : status == "Not charging"        ? "未充电"
                                              : "状态未知";
        auto current = number(path + "charge_now"), full = number(path + "charge_full");
        double divisor = 1000;
        if (!current || !full || *full <= 0)
        {
            current = number(path + "energy_now");
            full = number(path + "energy_full");
            const auto voltage = number(path + "voltage_min_design");
            if (voltage && *voltage > 0)
            {
                // µWh / nominal µV gives Ah. Mark the mAh conversion as estimated.
                divisor = *voltage / 1000;
                next["estimated"] = true;
            }
            else
            {
                divisor = 1000000;
                next["unit"] = "Wh";
            }
        }
        const bool valid = current && full && *full > 0;
        const QLocale locale(QLocale::English, QLocale::UnitedStates);
        if (valid)
        {
            const bool energy = next["unit"] == "Wh";
            next["current"] = energy ? locale.toString(*current / divisor, 'f', 1)
                                     : locale.toString(qRound64(*current / divisor));
            next["full"] = energy ? locale.toString(*full / divisor, 'f', 1)
                                  : locale.toString(qRound64(*full / divisor));
        }
        else
            next["estimated"] = false;
        const auto reported = number(path + "capacity");
        if (reported || valid)
        {
            const double percent
                = std::clamp(reported.value_or(valid ? 100 * *current / *full : 0), 0.0, 100.0);
            next["percentage"] = qRound(percent);
            next["progress"] = percent / 100;
        }
        break;
    }
    if (next != m_state)
    {
        m_state = next;
        emit stateChanged();
    }
}
