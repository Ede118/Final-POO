#include "app/RobotController.h"

namespace app {

void RobotController::setMode(const std::string& mode) {
  mode_ = mode;
}

std::string RobotController::currentMode() const {
  return mode_;
}

} // namespace app
