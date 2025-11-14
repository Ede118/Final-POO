#pragma once

#include <string>

namespace app {

class RobotController {
public:
  RobotController() = default;

  void setMode(const std::string& mode);
  std::string currentMode() const;

private:
  std::string mode_{"idle"};
};

} // namespace app
