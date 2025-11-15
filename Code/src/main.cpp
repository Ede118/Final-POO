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

using json = nlohmann::json;
namespace fs = std::filesystem;

void press_enter(bool flag) {
    if (flag){
        std::string dummy;
        std::getline(std::cin, dummy);  // Bloquea hasta ENTER
        std::system("clear");
    } else{
        std::string dummy;
        std::getline(std::cin, dummy);  // Bloquea hasta ENTER
    }
}

void pause_sec(int s) {
    std::this_thread::sleep_for(std::chrono::seconds(s));
}


bool parseCleanFlag(int argc, char* argv[]) {
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

    // Buscamos primero en la carpeta del cliente (relativa a Code/Server): ../Client/HTML
    std::string clientBase = "../Client/HTML";
    std::string serverBase = "."; // carpeta actual del servidor (Code/Server)

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

int main(int argc, char* argv[]) {
    bool cleanTerminal = parseCleanFlag(argc, argv);

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
        pause_sec(5);
        return 1;
    }
    
    listen(server_fd, 5);
    std::cout << "🚀 Servidor escuchando en puerto 8080" << std::endl;
    press_enter(cleanTerminal);



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

                            // Si la ruta es /upload -> guardar CSV, convertir a .gcode y ejecutar
                            if (path.rfind("/upload", 0) == 0) {
                                try {
                                    // Extraer nombre de archivo desde query ?name=...
                                    std::string filename = "uploaded.csv";
                                    auto qpos = path.find('?');
                                    if (qpos != std::string::npos) {
                                        std::string query = path.substr(qpos + 1);
                                        // buscar name=
                                        auto npos = query.find("name=");
                                        if (npos != std::string::npos) {
                                            filename = query.substr(npos + 5);
                                            // decode simple %20 etc.
                                            auto urlDecode = [](std::string s){
                                                std::string out; out.reserve(s.size());
                                                for (size_t i=0;i<s.size();++i) {
                                                    if (s[i]=='%' && i+2<s.size()) {
                                                        std::string hex = s.substr(i+1,2);
                                                        char c = (char) strtol(hex.c_str(), nullptr, 16);
                                                        out.push_back(c); i+=2;
                                                    } else if (s[i]=='+') out.push_back(' ');
                                                    else out.push_back(s[i]);
                                                }
                                                return out;
                                            };
                                            filename = urlDecode(filename);
                                        }
                                    }

                                    // Sanear filename: quitar directorios
                                    size_t lastSlash = filename.find_last_of("/\\");
                                    if (lastSlash != std::string::npos) filename = filename.substr(lastSlash+1);

                                    namespace fs = std::filesystem;
                                    fs::create_directories("uploads");
                                    fs::create_directories("jobs");

                                    fs::path csvpath = fs::path("uploads") / filename;
                                    // Guardar CSV crudo
                                    std::ofstream fout(csvpath, std::ios::out | std::ios::binary);
                                    fout << body;
                                    fout.close();

                                    // Convertir CSV a GCODE (.gcode)
                                    std::ifstream fin(csvpath);
                                    std::string base = filename;
                                    // quitar extension .csv
                                    auto posdot = base.find_last_of('.');
                                    if (posdot != std::string::npos) base = base.substr(0,posdot);
                                    fs::path gcodepath = fs::path("jobs") / (base + ".gcode");
                                    std::ofstream gout(gcodepath, std::ios::out | std::ios::trunc);
                                    if (fin && gout) {
                                        std::string line;
                                        bool first = true;
                                        while (std::getline(fin, line)) {
                                            if (first) { first = false; continue; } // saltar header
                                            // trim
                                            auto l = line;
                                            while (!l.empty() && (l.back()=='\r' || l.back()=='\n')) l.pop_back();
                                            if (l.size()>=2 && l.front()=='"' && l.back()=='"') {
                                                l = l.substr(1, l.size()-2);
                                                // des-escape ""
                                                std::string tmp; tmp.reserve(l.size());
                                                for (size_t i=0;i<l.size();++i) {
                                                    if (l[i]=='"' && i+1<l.size() && l[i+1]=='"') { tmp.push_back('"'); ++i; }
                                                    else tmp.push_back(l[i]);
                                                }
                                                l = tmp;
                                            }
                                            if (!l.empty()) gout << l << "\n";
                                        }
                                        gout.close();
                                        fin.close();

                                        // No ejecutar automáticamente: guardar el GCODE y devolver ruta.
                                        std::ostringstream out;
                                        std::string ok = std::string("Archivo subido: ") + gcodepath.string();
                                        out << "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: " << ok.size() << "\r\nAccess-Control-Allow-Origin: *\r\n\r\n" << ok;
                                        respuestaHttp = out.str();
                                    } else {
                                        std::string err = "Error al convertir CSV a GCODE";
                                        std::ostringstream out; out << "HTTP/1.1 500 Internal Server Error\r\nContent-Type: text/plain\r\nContent-Length: " << err.size() << "\r\nAccess-Control-Allow-Origin: *\r\n\r\n" << err;
                                        respuestaHttp = out.str();
                                    }
                                } catch (const std::exception& e) {
                                    std::string err = std::string("Exception: ") + e.what();
                                    std::ostringstream out; out << "HTTP/1.1 500 Internal Server Error\r\nContent-Type: text/plain\r\nContent-Length: " << err.size() << "\r\nAccess-Control-Allow-Origin: *\r\n\r\n" << err;
                                    respuestaHttp = out.str();
                                }
                            } else {
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