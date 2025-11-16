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

#include "login.h"
#include "robot_controller_simple.h"
#include "comunicacion_controlador_simple.h"
#include "estado_robot.h"
#include "aprendizaje.h"
#include "administrador_sistema.h"
#include "json.hpp"
#include "server.h"

using json = nlohmann::json;
namespace fs = std::filesystem;

std::string toString(ServerState s) {
    switch (s) {
        case ServerState::STABLE: return "STABLE";
        case ServerState::BUSY:   return "BUSY";
        case ServerState::ERROR:  return "ERROR";
    }
    return "UNKNOWN";
}

void Server::press_enter(bool flag) {
    if (flag){
        std::string dummy;
        std::getline(std::cin, dummy);  // Bloquea hasta ENTER
        std::system("clear");
    } else{
        std::string dummy;
        std::getline(std::cin, dummy);  // Bloquea hasta ENTER
    }
}

void Server::pause_sec(int s) {
    std::this_thread::sleep_for(std::chrono::seconds(s));
}


bool Server::parseCleanFlag(int argc, char* argv[]) {
    bool clean = false;
    const std::string key = "--cleanterminal=";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg.rfind(key, 0) == 0) { 
            std::string value = arg.substr(key.size());
            if (value == "y" || value == "yes" || value == "1") {
                clean = true;
            }
        }
    }
    return clean;
}



// [Las funciones auxiliares igual que antes...]
bool Server::endsWith(const std::string& str, const std::string& suffix) {
    if (suffix.size() > str.size()) return false;
    return std::equal(suffix.rbegin(), suffix.rend(), str.rbegin());
}

// Función para extraer y parsear JSON
bool Server::extractJsonParam(const std::string& body, json& j) {
    std::regex pat("<param><value><string>([^<]*)</string></value></param>");
    std::sregex_iterator it(body.begin(), body.end(), pat), end;
    
    if (it != end) {
        try {
            std::string jsonStr = (*it)[1];
            std::cout << "📋 JSON recibido: " << jsonStr << std::endl;
            j = json::parse(jsonStr);
            return true;
        } catch (const std::exception& e) {
            std::cerr << "❌ Error parseando JSON: " << e.what() << std::endl;
            std::cerr << "📋 JSON problemático: " << (*it)[1] << std::endl;
        }
    }
    return false;
}
std::string Server::readFile(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file) return "";
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

std::string Server::getMimeType(const std::string& path) {
    if (endsWith(path, ".html")) return "text/html";
    if (endsWith(path, ".css")) return "text/css";
    if (endsWith(path, ".js")) return "application/javascript";
    if (endsWith(path, ".png")) return "image/png";
    if (endsWith(path, ".jpg") || endsWith(path, ".jpeg")) return "image/jpeg";
    if (endsWith(path, ".gif")) return "image/gif";
    if (endsWith(path, ".ico")) return "image/x-icon";
    if (endsWith(path, ".mp3")) return "audio/mpeg";
    return "text/plain";
}

std::string Server::serveStaticFile(const std::string& requestPath) {
    std::cout << "📁 SOLICITUD DE ARCHIVO: " << requestPath << std::endl;

    // Buscamos primero en la carpeta del cliente (relativa a Code/build): ../HTML
    std::string clientBase = "HTML";
    std::string serverBase = "."; // carpeta actual del servidor (Code/build)

    std::string relpath;
    if (requestPath == "/" || requestPath == "/index.html") relpath = "/signin.html";
    else relpath = requestPath;

    fs::path clientPath = fs::path(clientBase) / relpath.substr(1);
    fs::path serverPath = fs::path(serverBase) / relpath.substr(1);

    std::string filepath;
    std::string servedFrom;

    if (!relpath.empty() && relpath[0] == '/') {
        // try client dir first
        if (fs::exists(clientPath) && fs::is_regular_file(clientPath)) {
            filepath = clientPath.string();
            servedFrom = clientBase;
        } else if (fs::exists(serverPath) && fs::is_regular_file(serverPath)) {
            filepath = serverPath.string();
            servedFrom = serverBase;
        }
    }

    if (filepath.empty()) {
        return "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\nContent-Length: 13\r\nAccess-Control-Allow-Origin: *\r\n\r\n404 Not Found";
    }

    std::cout << "📁 Sirviendo archivo: " << filepath << " (desde: " << servedFrom << ")" << std::endl;

    std::string content = readFile(filepath);
    std::string mimeType = getMimeType(filepath);

    std::ostringstream response;
    response << "HTTP/1.1 200 OK\r\n"
             << "Content-Type: " << mimeType << "\r\n"
             << "Content-Length: " << content.size() << "\r\n"
             << "Access-Control-Allow-Origin: *\r\n"
             << "\r\n"
             << content;

    return response.str();
}

bool Server::extractParams(const std::string& body,std::string& u,std::string& p){
    std::regex pat("<param><value><string>([^<]*)</string></value></param>");
    std::sregex_iterator it(body.begin(),body.end(),pat),end;
    int i=0;
    for(;it!=end && i<2;++it,++i){ if(i==0)u=(*it)[1]; else p=(*it)[1]; }
    return i==2;
}

std::vector<std::string> Server::extractMultipleParams(const std::string& body, int count) {
    std::vector<std::string> params;
    std::regex pat("<param><value><string>([^<]*)</string></value></param>");
    std::sregex_iterator it(body.begin(), body.end(), pat), end;
    for(int i = 0; it != end && i < count; ++it, ++i) {
        params.push_back((*it)[1]);
    }
    return params;
}

std::string Server::procesarRPC(const std::string& body, Login& login, RobotControllerSimple& robot,
                        EstadoRobot& estado, Aprendizaje& aprendizaje, AdministradorSistema& admin,
                        bool quiet) {
    size_t s = body.find("<methodName>"), e = body.find("</methodName>");
    std::string metodo = (s != std::string::npos && e != std::string::npos) ? 
                         body.substr(s + 12, e - (s + 12)) : "desconocido";
    std::ostringstream xml;

    if (!quiet) {
        std::cout << "=== INICIO PROCESAR RPC ===" << std::endl;
        std::cout << "🔍 MÉTODO: " << metodo << std::endl;
        std::cout << "📋 BODY: " << body << std::endl;
    }

    if(metodo == "login"){ 
        std::string u, p;
        if(extractParams(body, u, p)) {
            auto res = login.authenticate(u, p);
            xml << "<?xml version=\"1.0\"?><methodResponse><params><param><value><struct>"
                << "<member><name>status</name><value><string>" << (res.success?"success":"fail") << "</string></value></member>"
                << "<member><name>privilege</name><value><string>" << res.privilege << "</string></value></member>"
                << "<member><name>token</name><value><string>" << res.token << "</string></value></member>"
                << "<member><name>message</name><value><string>" << res.message << "</string></value></member>"
                << "</struct></value></param></params></methodResponse>";
        }
    }
    else if(metodo == "gripper") {
        json j;
        if(extractJsonParam(body, j)) {
            bool activar = j.value("on", false);
            std::cout << "🦾 GARRA parseado - activar: " << activar << std::endl;
            robot.setGarra(activar);
            xml << "<?xml version=\"1.0\"?><methodResponse><params><param><value><struct>"
                << "<member><name>status</name><value><string>success</string></value></member>"
                << "<member><name>message</name><value><string>Garra " << (activar?"activada":"desactivada") << "</string></value></member>"
                << "</struct></value></param></params></methodResponse>";
        }
    }
    else if(metodo == "motors") {
        json j;
        if(extractJsonParam(body, j)) {
            bool activar = j.value("on", false);
            std::cout << "⚙️ MOTORES parseado - activar: " << activar << std::endl;
            robot.setMotores(activar);
            xml << "<?xml version=\"1.0\"?><methodResponse><params><param><value><struct>"
                << "<member><name>status</name><value><string>success</string></value></member>"
                << "<member><name>message</name><value><string>Motores " << (activar?"encendidos":"apagados") << "</string></value></member>"
                << "</struct></value></param></params></methodResponse>";
        }
    }
    else if(metodo == "move") {
        json j;
        if(extractJsonParam(body, j)) {
            try {
                float x = j.value("x", 0.0f);
                float y = j.value("y", 0.0f);
                float z = j.value("z", 0.0f);
                float f = j.value("f", 1200.0f);
                
                std::cout << "🎯 MOVER parseado - X:" << x << " Y:" << y << " Z:" << z << " F:" << f << std::endl;
                
                auto estadoActual = estado.leer();
                robot.mover(x, y, z, f, estadoActual.modoAbs);
                
                xml << "<?xml version=\"1.0\"?><methodResponse><params><param><value><struct>"
                    << "<member><name>status</name><value><string>success</string></value></member>"
                    << "<member><name>message</name><value><string>Movimiento enviado: X=" << x << " Y=" << y << " Z=" << z << " F=" << f << "</string></value></member>"
                    << "</struct></value></param></params></methodResponse>";
            } catch (const std::exception& e) {
                xml << "<?xml version=\"1.0\"?><methodResponse><params><param><value><struct>"
                    << "<member><name>status</name><value><string>error</string></value></member>"
                    << "<member><name>message</name><value><string>Error en parámetros: " << e.what() << "</string></value></member>"
                    << "</struct></value></param></params></methodResponse>";
            }
        } else {
            xml << "<?xml version=\"1.0\"?><methodResponse><params><param><value><struct>"
                << "<member><name>status</name><value><string>error</string></value></member>"
                << "<member><name>message</name><value><string>No se pudo parsear JSON</string></value></member>"
                << "</struct></value></param></params></methodResponse>";
        }
    }
    else if (metodo == "runFile") {
        json j;
        if (extractJsonParam(body, j)) {
            std::string path = j.value("path", "");
            // Sanear path: no permitir traversal
            if (path.find("..") != std::string::npos) {
                xml << "<?xml version=\"1.0\"?><methodResponse><params><param><value><struct>"
                    << "<member><name>status</name><value><string>error</string></value></member>"
                    << "<member><name>message</name><value><string>Ruta no permitida</string></value></member>"
                    << "</struct></value></param></params></methodResponse>";
            } else {
                if (!fs::exists(path)) {
                    xml << "<?xml version=\"1.0\"?><methodResponse><params><param><value><struct>"
                        << "<member><name>status</name><value><string>error</string></value></member>"
                        << "<member><name>message</name><value><string>Archivo no encontrado</string></value></member>"
                        << "</struct></value></param></params></methodResponse>";
                } else {
                    robot.ejecutarArchivo(path);
                    xml << "<?xml version=\"1.0\"?><methodResponse><params><param><value><struct>"
                        << "<member><name>status</name><value><string>success</string></value></member>"
                        << "<member><name>message</name><value><string>Ejecución iniciada: " << path << "</string></value></member>"
                        << "</struct></value></param></params></methodResponse>";
                }
            }
        }
    }
    else if (metodo == "sendGcode") {
        json j;
        if (extractJsonParam(body, j)) {
            std::string line = j.value("line", "");
            if (!line.empty()) {
                std::cout << "➡️ EJECUTANDO LÍNEA GCODE: " << line << std::endl;
                robot.ejecutarComando(line);
                xml << "<?xml version=\"1.0\"?><methodResponse><params><param><value><struct>"
                    << "<member><name>status</name><value><string>success</string></value></member>"
                    << "<member><name>message</name><value><string>Comando enviado: " << line << "</string></value></member>"
                    << "</struct></value></param></params></methodResponse>";
            } else {
                xml << "<?xml version=\"1.0\"?><methodResponse><params><param><value><struct>"
                    << "<member><name>status</name><value><string>error</string></value></member>"
                    << "<member><name>message</name><value><string>Línea vacía</string></value></member>"
                    << "</struct></value></param></params></methodResponse>";
            }
        }
    }
    else if(metodo == "emergencyStop") {
        robot.emergencia();
        xml << "<?xml version=\"1.0\"?><methodResponse><params><param><value><struct>"
            << "<member><name>status</name><value><string>success</string></value></member>"
            << "<member><name>message</name><value><string>Emergencia activada</string></value></member>"
            << "</struct></value></param></params></methodResponse>";
    }
    else if(metodo == "resetEmergency") {
        robot.resetEmergencia();
        xml << "<?xml version=\"1.0\"?><methodResponse><params><param><value><struct>"
            << "<member><name>status</name><value><string>success</string></value></member>"
            << "<member><name>message</name><value><string>Emergencia reseteada</string></value></member>"
            << "</struct></value></param></params></methodResponse>";
    }
    else if(metodo == "setAbs") {
        robot.setAbs(true);
        xml << "<?xml version=\"1.0\"?><methodResponse><params><param><value><struct>"
            << "<member><name>status</name><value><string>success</string></value></member>"
            << "<member><name>message</name><value><string>Modo cambiado a absoluto</string></value></member>"
            << "</struct></value></param></params></methodResponse>";
    }
    else if(metodo == "setRel") {
        robot.setAbs(false);
        xml << "<?xml version=\"1.0\"?><methodResponse><params><param><value><struct>"
            << "<member><name>status</name><value><string>success</string></value></member>"
            << "<member><name>message</name><value><string>Modo cambiado a relativo</string></value></member>"
            << "</struct></value></param></params></methodResponse>";
    }
    else if(metodo == "getEstado") {
        auto s = estado.leer();
        bool remoto = admin.getRemoto();
        xml << "<?xml version=\"1.0\"?><methodResponse><params><param><value><struct>"
            << "<member><name>x</name><value><double>" << s.x << "</double></value></member>"
            << "<member><name>y</name><value><double>" << s.y << "</double></value></member>"
            << "<member><name>z</name><value><double>" << s.z << "</double></value></member>"
            << "<member><name>modo</name><value><string>" << (s.modoAbs?"ABS":"REL") << "</string></value></member>"
            << "<member><name>motores</name><value><string>" << (s.motores?"ON":"OFF") << "</string></value></member>"
            << "<member><name>garra</name><value><string>" << (s.garra?"ON":"OFF") << "</string></value></member>"
            << "<member><name>emergencia</name><value><string>" << (s.emergencia?"SI":"NO") << "</string></value></member>"
            << "<member><name>remoto</name><value><string>" << (remoto?"ON":"OFF") << "</string></value></member>"
            << "</struct></value></param></params></methodResponse>";
    }
    else if(metodo == "home") {
    // Comando de home típico en G-code
    robot.ejecutarComando("G28");
    xml << "<?xml version=\"1.0\"?><methodResponse><params><param><value><struct>"
        << "<member><name>status</name><value><string>success</string></value></member>"
        << "<member><name>message</name><value><string>Home ejecutado</string></value></member>"
        << "</struct></value></param></params></methodResponse>";
}
    else if (metodo == "startLearning") {
        // Iniciar modo aprendizaje: abrir archivo para registrar comandos
        aprendizaje.iniciar();
        xml << "<?xml version=\"1.0\"?><methodResponse><params><param><value><struct>"
            << "<member><name>status</name><value><string>success</string></value></member>"
            << "<member><name>message</name><value><string>Aprendizaje iniciado</string></value></member>"
            << "</struct></value></param></params></methodResponse>";
    }
    else if (metodo == "stopLearning") {
        // Detener modo aprendizaje: cerrar archivo y generar CSV
        aprendizaje.detener();
        xml << "<?xml version=\"1.0\"?><methodResponse><params><param><value><struct>"
            << "<member><name>status</name><value><string>success</string></value></member>"
            << "<member><name>message</name><value><string>Aprendizaje detenido y guardado</string></value></member>"
            << "</struct></value></param></params></methodResponse>";
    }
    else if (metodo == "enableRemote") {
        admin.setRemoto(true);
        xml << "<?xml version=\"1.0\"?><methodResponse><params><param><value><struct>"
            << "<member><name>status</name><value><string>success</string></value></member>"
            << "<member><name>message</name><value><string>Control remoto habilitado</string></value></member>"
            << "</struct></value></param></params></methodResponse>";
    }
    else if (metodo == "disableRemote") {
        admin.setRemoto(false);
        xml << "<?xml version=\"1.0\"?><methodResponse><params><param><value><struct>"
            << "<member><name>status</name><value><string>success</string></value></member>"
            << "<member><name>message</name><value><string>Control remoto deshabilitado</string></value></member>"
            << "</struct></value></param></params></methodResponse>";
    }
    else {
        xml << "<?xml version=\"1.0\"?><methodResponse><params><param><value><struct>"
            << "<member><name>status</name><value><string>error</string></value></member>"
            << "<member><name>message</name><value><string>Método desconocido</string></value></member>"
            << "</struct></value></param></params></methodResponse>";
    }

    std::string respuesta = xml.str();
    if (!quiet) {
        std::cout << "📤 RESPUESTA XML: " << respuesta << std::endl;
        std::cout << "=== FIN PROCESAR RPC ===" << std::endl;
    }
    
    return respuesta;
}

void Server::parseHttpRequest(const std::string& request, std::string& method, std::string& path) {
    std::istringstream iss(request);
    iss >> method >> path;
}
