#pragma once

#ifdef BUILD_GUI

#include <QString>
#include <vector>

namespace ak {
namespace gui {
namespace widgets {

struct ServiceOption {
    QString displayName;
    QString serviceCode;
};

const std::vector<ServiceOption>& serviceOptions();
QString serviceCodeForDisplay(const QString& displayName);
QString displayForServiceCode(const QString& serviceCode);

} // namespace widgets
} // namespace gui
} // namespace ak

#endif // BUILD_GUI

