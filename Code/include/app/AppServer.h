#pragma once

#include "app/AuthService.h"

namespace app {

class StorageService;
class Logger;
class RobotController;
class PasswordHasher;
class TokenService;

class AppServer {
public:
  AppServer(StorageService& storage,
            Logger& logger,
            PasswordHasher& hasher,
            TokenService& tokens,
            RobotController& robot);

  AuthService& auth() { return auth_; }
  StorageService& storage() { return storage_; }
  Logger& logger() { return logger_; }
  RobotController& robot() { return robot_; }

private:
  StorageService& storage_;
  Logger& logger_;
  RobotController& robot_;
  AuthService auth_;
};

} // namespace app
