#include "app/AppServer.h"
#include "app/Logger.h"
#include "app/RobotController.h"
#include "app/StorageService.h"
#include "app/PasswordHasher.h"
#include "app/TokenService.h"

namespace app {

AppServer::AppServer(StorageService& storage,
                     Logger& logger,
                     PasswordHasher& hasher,
                     TokenService& tokens,
                     RobotController& robot)
    : storage_(storage),
      logger_(logger),
      robot_(robot),
      auth_(storage, logger, hasher, tokens) {}

} // namespace app

