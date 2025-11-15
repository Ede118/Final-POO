#include <iostream>
#include <string>
#include <sstream>
#include <regex>
#include <fstream>
#include <filesystem>
#include <netinet/in.h>
#include <unistd.h>
#include "login.h"
#include "robot_controller_simple.h"
#include "comunicacion_controlador_simple.h"
#include "estado_robot.h"
#include "aprendizaje.h"
#include "administrador_sistema.h"
#include "json.hpp"

using json = nlohmann::json;
namespace fs = std::filesystem;

// [Las funciones auxiliares igual que antes...]
bool endsWith(const std::string& str, const std::string& suffix) {
    if (suffix.size() > str.size()) return false;
    return std::equal(suffix.rbegin(), suffix.rend(), str.rbegin());
}

// Función para extraer y parsear JSON
bool extractJsonParam(const std::string& body, json& j) {
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

std::string readFile(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file) return "";
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

std::string getMimeType(const std::string& path) {
    if (endsWith(path, ".html")) return "text/html";
    if (endsWith(path, ".css")) return "text/css";
    if (endsWith(path, ".js")) return "application/javascript";
    if (endsWith(path, ".png")) return "image/png";
    if (endsWith(path, ".jpg") || endsWith(path, ".jpeg")) return "image/jpeg";
    if (endsWith(path, ".gif")) return "image/gif";
    if (endsWith(path, ".ico")) return "image/x-icon";
    return "text/plain";
}

std::string serveStaticFile(const std::string& requestPath) {
    std::cout << "📁 SOLICITUD DE ARCHIVO: " << requestPath << std::endl;
    
    std::string filepath;
    if (requestPath == "/" || requestPath == "/index.html") {
        filepath = "HTML/signin.html";
    } else if (requestPath == "/signin.html") {
        filepath = "HTML/signin.html";
    } else if (requestPath == "/user_panel.html") {
        filepath = "HTML/user_panel.html";
    } else if (requestPath == "/server_panel.html") {
        filepath = "HTML/admin_panel.html";
    } else if (requestPath == "/viewer_panel.html") {
        filepath = "HTML/viewer_panel.html";
    } else {
        filepath = requestPath.substr(1);
    }
    
    if (!fs::exists(filepath)) {
        return "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\nContent-Length: 13\r\nAccess-Control-Allow-Origin: *\r\n\r\n404 Not Found";
    }
    
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

bool extractParams(const std::string& body,std::string& u,std::string& p){
    std::regex pat("<param><value><string>([^<]*)</string></value></param>");
    std::sregex_iterator it(body.begin(),body.end(),pat),end;
    int i=0;
    for(;it!=end && i<2;++it,++i){ if(i==0)u=(*it)[1]; else p=(*it)[1]; }
    return i==2;
}

std::vector<std::string> extractMultipleParams(const std::string& body, int count) {
    std::vector<std::string> params;
    std::regex pat("<param><value><string>([^<]*)</string></value></param>");
    std::sregex_iterator it(body.begin(), body.end(), pat), end;
    for(int i = 0; it != end && i < count; ++it, ++i) {
        params.push_back((*it)[1]);
    }
    return params;
}

std::string procesarRPC(const std::string& body, Login& login, RobotControllerSimple& robot,
                        EstadoRobot& estado, Aprendizaje& aprendizaje, AdministradorSistema& admin) {
    size_t s = body.find("<methodName>"), e = body.find("</methodName>");
    std::string metodo = (s != std::string::npos && e != std::string::npos) ? 
                         body.substr(s + 12, e - (s + 12)) : "desconocido";
    std::ostringstream xml;

    std::cout << "=== INICIO PROCESAR RPC ===" << std::endl;
    std::cout << "🔍 MÉTODO: " << metodo << std::endl;
    std::cout << "📋 BODY: " << body << std::endl;

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
        std::string u, p;
        if(extractParams(body, u, p)) {
            bool absoluto = (p == "true" || p == "1");
            robot.setAbs(absoluto);
            xml << "<?xml version=\"1.0\"?><methodResponse><params><param><value><struct>"
                << "<member><name>status</name><value><string>success</string></value></member>"
                << "<member><name>message</name><value><string>Modo cambiado a " << (absoluto?"absoluto":"relativo") << "</string></value></member>"
                << "</struct></value></param></params></methodResponse>";
        }
    }
    else if(metodo == "getEstado") {
        auto s = estado.leer();
        xml << "<?xml version=\"1.0\"?><methodResponse><params><param><value><struct>"
            << "<member><name>x</name><value><double>" << s.x << "</double></value></member>"
            << "<member><name>y</name><value><double>" << s.y << "</double></value></member>"
            << "<member><name>z</name><value><double>" << s.z << "</double></value></member>"
            << "<member><name>modo</name><value><string>" << (s.modoAbs?"ABS":"REL") << "</string></value></member>"
            << "<member><name>motores</name><value><string>" << (s.motores?"ON":"OFF") << "</string></value></member>"
            << "<member><name>garra</name><value><string>" << (s.garra?"ON":"OFF") << "</string></value></member>"
            << "<member><name>emergencia</name><value><string>" << (s.emergencia?"SI":"NO") << "</string></value></member>"
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
    else {
        xml << "<?xml version=\"1.0\"?><methodResponse><params><param><value><struct>"
            << "<member><name>status</name><value><string>error</string></value></member>"
            << "<member><name>message</name><value><string>Método desconocido</string></value></member>"
            << "</struct></value></param></params></methodResponse>";
    }

    std::string respuesta = xml.str();
    std::cout << "📤 RESPUESTA XML: " << respuesta << std::endl;
    std::cout << "=== FIN PROCESAR RPC ===" << std::endl;
    
    return respuesta;
}

void parseHttpRequest(const std::string& request, std::string& method, std::string& path) {
    std::istringstream iss(request);
    iss >> method >> path;
}

// ----------------------------------------------------------------------------------------------------------------- //
// -------------------------------------------------- MAIN -------------------------------------------------------   //
// ----------------------------------------------------------------------------------------------------------------- //

int main() {
    std::cout << "🤖 INICIANDO SERVIDOR ROBOT RRR (DEBUG)" << std::endl;

    Login login;
    if(!login.isConnected()) return 1;

    EstadoRobot estado;
    ComunicacionControladorSimple comm("/dev/ttyUSB0", B19200);
    Aprendizaje aprendizaje;
    AdministradorSistema admin;
    RobotControllerSimple robot(comm, estado);
    robot.setAprendizaje(&aprendizaje);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in address{}; 
    address.sin_family = AF_INET; 
    address.sin_addr.s_addr = INADDR_ANY; 
    address.sin_port = htons(8080);
    
    // Permitir reuso del puerto
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    if (bind(server_fd, (sockaddr*)&address, sizeof(address)) < 0) {
        std::cerr << "❌ ERROR bind: " << strerror(errno) << std::endl;
        return 1;
    }
    
    listen(server_fd, 5);
    std::cout << "🚀 Servidor escuchando en puerto 8080" << std::endl;

    while(true) {
        int client_fd = accept(server_fd, nullptr, nullptr);
        char buffer[8192] = {0};
        ssize_t n = read(client_fd, buffer, sizeof(buffer) - 1);
        
        if(n > 0) {
            std::string req(buffer);
            std::string method, path;
            parseHttpRequest(req, method, path);
            
            std::cout << "🌐 SOLICITUD: " << method << " " << path << std::endl;

            std::string respuestaHttp;
            
            if (method == "GET") {
                respuestaHttp = serveStaticFile(path);
            } 
            else if (method == "OPTIONS") {
                respuestaHttp = "HTTP/1.1 200 OK\r\nAccess-Control-Allow-Origin: *\r\nAccess-Control-Allow-Methods: POST, GET, OPTIONS\r\nAccess-Control-Allow-Headers: Content-Type\r\nContent-Length: 0\r\n\r\n";
            }
            else if (method == "POST") {
                size_t body_pos = req.find("\r\n\r\n");
                if (body_pos != std::string::npos) {
                    std::string body = req.substr(body_pos + 4);
                    std::string resp = procesarRPC(body, login, robot, estado, aprendizaje, admin);
                    
                    std::ostringstream out;
                    out << "HTTP/1.1 200 OK\r\n"
                        << "Access-Control-Allow-Origin: *\r\n"
                        << "Access-Control-Allow-Methods: POST, OPTIONS\r\n"
                        << "Access-Control-Allow-Headers: Content-Type\r\n"
                        << "Content-Type: text/xml\r\n"
                        << "Content-Length: " << resp.size() << "\r\n"
                        << "\r\n"
                        << resp;
                    respuestaHttp = out.str();
                }
            }
            
            if (!respuestaHttp.empty()) {
                write(client_fd, respuestaHttp.c_str(), respuestaHttp.size());
                std::cout << "✅ RESPUESTA ENVIADA (" << respuestaHttp.size() << " bytes)" << std::endl;
            } else {
                std::string error = "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\nContent-Length: 13\r\nAccess-Control-Allow-Origin: *\r\n\r\n404 Not Found";
                write(client_fd, error.c_str(), error.size());
                std::cout << "❌ ENVIADO 404" << std::endl;
            }
        }
        close(client_fd);
        std::cout << "----------------------------------------" << std::endl;
    }
    return 0;
}