// Qt counterparts of the original store's UUID and wall-clock helpers.
#include "core/data/data_store.hpp"
#include <QUuid>
#include <QDateTime>
namespace ehud::data {
std::string makeUUID() { return QUuid::createUuid().toString(QUuid::WithoutBraces).toUpper().toStdString(); }
bool validUUID(std::string_view value) noexcept {
    if (value.size() != 36) return false;
    for (unsigned i = 0; i < value.size(); ++i) {
        if (i == 8 || i == 13 || i == 18 || i == 23) { if (value[i] != '-') return false; }
        else if (!((value[i] >= '0' && value[i] <= '9') || (value[i] >= 'a' && value[i] <= 'f') || (value[i] >= 'A' && value[i] <= 'F'))) return false;
    }
    return true;
}
double foundationNow() { return QDateTime::currentMSecsSinceEpoch() / 1000. - 978307200.; }
}
