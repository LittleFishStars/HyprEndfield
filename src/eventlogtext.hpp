#pragma once
#include "endfield/modules/event_log.hpp"
#include "endfield/core/localization.hpp"
namespace endfield::modules {
enum class EventLogFilter : std::uint8_t { all, timer, shelf, devices, other };
EventLogFilter eventLogFilterOf(EventKind) noexcept;
bool eventLogFilterIncludes(EventLogFilter, EventKind) noexcept;
std::string eventLogFilterTitle(EventLogFilter, core::Language);
std::string eventLogLine(const SystemEvent&, core::Language);
}
