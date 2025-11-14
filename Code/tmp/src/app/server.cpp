#include "XmlRpc.h"
#include "app/AppServer.h"
#include "app/Logger.h"
#include "app/PasswordHasher.h"
#include "app/RPCAuthLogin.h"
#include "app/RobotController.h"
#include "app/StorageService.h"
#include "app/TokenService.h"
#include <chrono>
#include <filesystem>
#include <iostream>
#include <cstdlib>

int main(int argc, char** argv) {
  using namespace app;

  int port = (argc > 1) ? std::atoi(argv[1]) : 8080;

  std::filesystem::create_directories("Code/tmp");

  StorageService storage("Code/db/users.sqlite3");
  if (!storage.open()) {
    std::cerr << "No se pudo abrir la base de datos en " << storage.path() << "\n";
    return 1;
  }
  if (!storage.initializeSchema()) {
    std::cerr << "No se pudo inicializar el esquema de la base de datos.\n";
    return 1;
  }

  Logger logger("Code/tmp/auth.log");
  RobotController robot;
  PasswordHasher& hasher = defaultPasswordHasher();
  TokenService& tokenService = defaultTokenService();

  XmlRpc::setVerbosity(1);
  XmlRpc::XmlRpcServer server;
  AppServer app(storage, logger, hasher, tokenService, robot);

  RpcAuthLogin m_login(&server, app);

  server.enableIntrospection(true);

  if (!server.bindAndListen(port)) {
    std::cerr << "No se pudo bindear al puerto " << port << "\n";
    return 1;
  }
  std::cout << "Servidor RPC escuchando en puerto " << port << "\n";

  while (true) {
    server.work(0.1); // 100 ms
    storage.purgeExpiredSessions();
  }
  return 0;
}
