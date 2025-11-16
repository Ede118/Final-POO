#pragma once
#include <iostream>
#include <string>
#include <sstream>
#include <regex>
#include <fstream>
#include <filesystem>
#include <netinet/in.h>
#include <unistd.h>
#include <cstdlib>
#include <thread>
#include <chrono>
#include <unordered_map>
#include <functional>

#include "login.h"
#include "robot_controller_simple.h"
#include "comunicacion_controlador_simple.h"
#include "estado_robot.h"
#include "aprendizaje.h"
#include "administrador_sistema.h"
#include "json.hpp"

using json = nlohmann::json;

enum class ServerState {
    STABLE,
    BUSY,
    ERROR
};

std::string toString(ServerState s);

class Server {
public:
    Server() : state(ServerState::STABLE) {}

    std::string ping() {
        return "pong";
    }

    std::string setBusy() {
        state = ServerState::BUSY;
        return "Server set to BUSY";
    }

    std::string setStable() {
        state = ServerState::STABLE;
        return "Server set to STABLE";
    }

    std::string fail() {
        state = ServerState::ERROR;
        return "Server forced to ERROR";
    }

    ServerState getState() const {
        return state;
    }

    bool parseCleanFlag(int argc, char* argv[]);
    bool endsWith(const std::string& str, const std::string& suffix);
    bool extractJsonParam(const std::string& body, json& j);
    std::string readFile(const std::string& filepath);
    std::string getMimeType(const std::string& path);
    std::string serveStaticFile(const std::string& requestPath);
    bool extractParams(const std::string& body,std::string& u,std::string& p);
    std::vector<std::string> extractMultipleParams(const std::string& body, int count);
    
    std::string procesarRPC(const std::string& body, Login& login, RobotControllerSimple& robot,
                        EstadoRobot& estado, Aprendizaje& aprendizaje, AdministradorSistema& admin,
                        bool quiet = false);
    
    void press_enter(bool flag);
    void pause_sec(int s);
    void parseHttpRequest(const std::string& request, std::string& method, std::string& path);

private:
    ServerState state;
};
