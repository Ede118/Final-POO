#include <iostream>
#include <string>
#include <sstream>
#include <regex>
#include <cctype>
#include <algorithm>
#include <iomanip>
#include <ctime>
#include <fstream>
#include <filesystem>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstdlib>
#include <thread>
#include <chrono>
#include <unordered_map>
#include <functional>
#include <netinet/in.h>
#include <unistd.h>
#include <atomic>
#include <cmath>

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
const std::chrono::seconds kClosingGrace(5);
const std::chrono::milliseconds kRequestDedupWindow(350);

struct CommandContext {
    Server& server;
    bool& running;
    bool& closing;
    int* listenFd;
    std::atomic<bool>* closingServed;
    Login* login;
    RobotControllerSimple* robot;
    EstadoRobot* estado;
    Aprendizaje* aprendizaje;
    AdministradorSistema* admin;
};

using CommandFn = std::function<std::string(const std::string&, CommandContext&)>;

std::string trimCopy(const std::string& s) {
    auto isSpace = [](unsigned char c){ return std::isspace(c); };
    auto begin = std::find_if_not(s.begin(), s.end(), isSpace);
    auto end = std::find_if_not(s.rbegin(), s.rend(), isSpace).base();
    if (begin >= end) return "";
    return std::string(begin, end);
}

std::string toLowerCopy(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::tolower(c); });
    return s;
}

bool parseOnOffToken(const std::string& token, bool& value) {
    auto t = toLowerCopy(token);
    if (t == "on" || t == "encender" || t == "enable" || t == "activar") {
        value = true;
        return true;
    }
    if (t == "off" || t == "apagar" || t == "disable" || t == "desactivar") {
        value = false;
        return true;
    }
    return false;
}

bool snapshotsEqual(const EstadoRobot::Snapshot& a, const EstadoRobot::Snapshot& b) {
    auto approx = [](float lhs, float rhs) {
        return std::fabs(lhs - rhs) < 1e-3f;
    };
    return approx(a.x, b.x) && approx(a.y, b.y) && approx(a.z, b.z)
        && a.motores == b.motores && a.garra == b.garra
        && a.modoAbs == b.modoAbs && a.emergencia == b.emergencia;
}

std::string runRpc(CommandContext& ctx, const std::string& method, const json& payload = json::object()) {
    if (!ctx.login || !ctx.robot || !ctx.estado || !ctx.aprendizaje || !ctx.admin) {
        return "RPC no disponible en este contexto";
    }
    std::ostringstream xml;
    xml << "<?xml version=\"1.0\"?><methodCall><methodName>" << method << "</methodName><params>"
        << "<param><value><string>" << payload.dump() << "</string></value></param>"
        << "</params></methodCall>";
    return ctx.server.procesarRPC(xml.str(), *ctx.login, *ctx.robot, *ctx.estado, *ctx.aprendizaje, *ctx.admin);
}

std::unordered_map<std::string, CommandFn> buildCommandTable() {
    std::unordered_map<std::string, CommandFn> cmds;

    cmds["ping"] = [](const std::string&, CommandContext& ctx) {
        return ctx.server.ping();
    };

    cmds["start"] = [](const std::string&, CommandContext& ctx) {
        return "Server state: " + toString(ctx.server.getState());
    };

    cmds["busy"] = [](const std::string&, CommandContext& ctx) {
        return ctx.server.setBusy();
    };

    cmds["stable"] = [](const std::string&, CommandContext& ctx) {
        return ctx.server.setStable();
    };

    cmds["fail"] = [](const std::string&, CommandContext& ctx) {
        return ctx.server.fail();
    };

    cmds["status"] = [](const std::string&, CommandContext& ctx) {
        return "Server state: " + toString(ctx.server.getState());
    };

    cmds["rpc"] = [](const std::string& args, CommandContext& ctx) {
        if (!ctx.login || !ctx.robot || !ctx.estado || !ctx.aprendizaje || !ctx.admin) {
            return std::string("RPC not available in current context");
        }
        std::istringstream iss(args);
        std::string method;
        iss >> method;
        if (method.empty()) {
            return std::string("Usage: rpc <method> [jsonPayload]");
        }
        std::string rest;
        std::getline(iss, rest);
        rest = trimCopy(rest);
        std::ostringstream xml;
        xml << "<?xml version=\"1.0\"?><methodCall><methodName>" << method << "</methodName><params>";
        if (!rest.empty()) {
            xml << "<param><value><string>" << rest << "</string></value></param>";
        } else {
            xml << "<param><value><string>{}</string></value></param>";
        }
        xml << "</params></methodCall>";
        return ctx.server.procesarRPC(xml.str(), *ctx.login, *ctx.robot, *ctx.estado, *ctx.aprendizaje, *ctx.admin);
    };
    
    cmds["pkill"] = cmds["exit"] = [](const std::string&, CommandContext& ctx) {
        ctx.running = false;
        ctx.closing = true;
        if (ctx.closingServed) {
            ctx.closingServed->store(false);
        }
        auto listenPtr = ctx.listenFd;
        auto servedPtr = ctx.closingServed;
        std::thread([listenPtr, servedPtr]{
            const auto maxWait = std::chrono::seconds(45);
            auto start = std::chrono::steady_clock::now();
            bool served = false;
            while (std::chrono::steady_clock::now() - start < maxWait) {
                if (servedPtr && servedPtr->load()) {
                    served = true;
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            if (served) {
                std::this_thread::sleep_for(kClosingGrace);
            }
            if (listenPtr && *listenPtr >= 0) {
                shutdown(*listenPtr, SHUT_RDWR);
            }
        }).detach();
        return "Shutting down server...";
    };

    

    cmds["help"] = cmds["--help"] = cmds["--h"] = [](const std::string&, CommandContext&) {
        std::ostringstream out;
        out << "Comandos disponibles:\n"
            << "  ping | busy | stable | fail | status\n"
            << "  motors on|off        -> Enciende o apaga motores (método 'motors')\n"
            << "  gripper on|off       -> Activa o desactiva la garra (método 'gripper')\n"
            << "  setAbs | setRel      -> Cambia modo absoluto/relativo\n"
            << "  emergencyStop        -> Parada de emergencia inmediata\n"
            << "  resetEmergency       -> Limpia la emergencia\n"
            << "  enableRemote | disableRemote -> Control remoto ON/OFF\n"
            << "  exportLog [dir]      -> Copia HTML/static_server.log a un archivo con timestamp\n"
            << "  rpc <metodo> [json]  -> Envía una llamada RPC manual\n"
            << "  pkill / exit         -> Cierra el servidor mostrando server_terminated.html\n";
        return out.str();
    };

    cmds["motors"] = [](const std::string& args, CommandContext& ctx) {
        std::istringstream iss(args);
        std::string tok; iss >> tok;
        if (tok.empty()) return std::string("Uso: motors on|off");
        bool on;
        if (!parseOnOffToken(tok, on)) return std::string("Uso: motors on|off");
        json payload; payload["on"] = on;
        return runRpc(ctx, "motors", payload);
    };

    cmds["gripper"] = cmds["grip"] = [](const std::string& args, CommandContext& ctx) {
        std::istringstream iss(args);
        std::string tok; iss >> tok;
        if (tok.empty()) return std::string("Uso: gripper on|off");
        bool on;
        if (!parseOnOffToken(tok, on)) return std::string("Uso: gripper on|off");
        json payload; payload["on"] = on;
        return runRpc(ctx, "gripper", payload);
    };

    cmds["setAbs"] = cmds["setabs"] = [](const std::string&, CommandContext& ctx) {
        return runRpc(ctx, "setAbs");
    };

    cmds["setRel"] = cmds["setrel"] = [](const std::string&, CommandContext& ctx) {
        return runRpc(ctx, "setRel");
    };

    cmds["emergencyStop"] = cmds["estop"] = [](const std::string&, CommandContext& ctx) {
        json payload;
        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        std::ostringstream ts;
        ts << std::put_time(std::localtime(&t), "%FT%T");
        payload["timestamp"] = ts.str();
        payload["source"] = "cli";
        return runRpc(ctx, "emergencyStop", payload);
    };

    cmds["resetEmergency"] = cmds["resetestop"] = [](const std::string&, CommandContext& ctx) {
        return runRpc(ctx, "resetEmergency");
    };

    cmds["enableRemote"] = [](const std::string&, CommandContext& ctx) {
        return runRpc(ctx, "enableRemote");
    };

    cmds["disableRemote"] = [](const std::string&, CommandContext& ctx) {
        return runRpc(ctx, "disableRemote");
    };

    cmds["exportLog"] = cmds["exportlog"] = [](const std::string& args, CommandContext&) {
        namespace fs = std::filesystem;
        fs::path source = fs::path("HTML") / "static_server.log";
        if (!fs::exists(source)) {
            return std::string("No existe el archivo de log en ") + source.string();
        }
        fs::path targetDir = trimCopy(args);
        if (targetDir.empty()) targetDir = "exported_logs";
        std::error_code ec;
        fs::create_directories(targetDir, ec);
        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        std::ostringstream filename;
        filename << "log_" << std::put_time(std::localtime(&t), "%Y%m%d_%H%M%S") << ".log";
        fs::path dest = fs::path(targetDir) / filename.str();
        fs::copy_file(source, dest, fs::copy_options::overwrite_existing, ec);
        if (ec) {
            return std::string("No se pudo exportar log: ") + ec.message();
        }
        return std::string("Log exportado en ") + dest.string();
    };



    return cmds;
}

std::string dispatchCommand(const std::string& line,
                            CommandContext& ctx,
                            const std::unordered_map<std::string, CommandFn>& commands) {
    std::istringstream iss(line);
    std::string cmd;
    iss >> cmd;
    std::string args;
    std::getline(iss, args);
    auto pos = args.find_first_not_of(" \t");
    if (pos == std::string::npos) args.clear();
    else args.erase(0, pos);

    if (cmd.empty()) return "";
    auto it = commands.find(cmd);
    if (it == commands.end()) {
        return "Unknown command: " + cmd;
    }
    return it->second(args, ctx);
}



int main(int argc, char* argv[]) {
    Server ServerB;
    bool running = true;
    bool closing = false;
    int listenFdStorage = -1;
    std::atomic<bool> closingServed{false};
    bool cleanTerminal = ServerB.parseCleanFlag(argc, argv);
    CommandContext ctx{ServerB, running, closing, &listenFdStorage, &closingServed,
                       nullptr, nullptr, nullptr, nullptr, nullptr};
    auto commands = buildCommandTable();
    std::thread replThread([&]{
        bool firstCommandOutput = true;
        std::string line;
        while (running && std::getline(std::cin, line)) {
                if (line.empty()) continue;
                try {
                    if (cleanTerminal && !firstCommandOutput) {
                        std::system("clear");
                    }
                    firstCommandOutput = false;
                    auto resp = dispatchCommand(line, ctx, commands);
                    if (!resp.empty()) {
                        std::cout << resp << std::endl;
                    }
                    if (!running) break; // por si cmd cambia running
                } catch (const std::exception& e) {
                    std::cerr << "REPL error: " << e.what() << std::endl;
                }
            }
        });

    std::cout << "🤖 INICIANDO SERVIDOR ROBOT RRR (DEBUG)" << std::endl;

    Login login;
    if(!login.isConnected()) return 1;

    EstadoRobot estado;
    ComunicacionControladorSimple comm("/dev/ttyUSB0", B19200);
    Aprendizaje aprendizaje;
    AdministradorSistema admin;
    RobotControllerSimple robot(comm, estado);
    robot.setAprendizaje(&aprendizaje);

    ctx.login = &login;
    ctx.robot = &robot;
    ctx.estado = &estado;
    ctx.aprendizaje = &aprendizaje;
    ctx.admin = &admin;

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    listenFdStorage = server_fd;
    sockaddr_in address{}; 
    address.sin_family = AF_INET; 
    address.sin_addr.s_addr = INADDR_ANY; 
    address.sin_port = htons(8080);
    
    // Permitir reuso del puerto
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    if (bind(server_fd, (sockaddr*)&address, sizeof(address)) < 0) {
        std::cerr << "❌ ERROR bind: " << strerror(errno) << std::endl;
        ServerB.pause_sec(5);
        return 1;
    }
    
    listen(server_fd, 5);
    std::cout << "🚀 Servidor escuchando en puerto 8080" << std::endl;
    std::cout << "🔗 Mandar [start] para empezar el servidor." << std::endl;
    ServerB.press_enter(cleanTerminal);

    bool firstFeedback = true;
    struct RequestCache {
        std::string signature;
        std::string response;
        EstadoRobot::Snapshot estado{};
        std::chrono::steady_clock::time_point timestamp{};
    };
    RequestCache lastRequestCache{};
    bool hasCachedRequest = false;

    while(ctx.running || closing) {
        int client_fd = accept(server_fd, nullptr, nullptr);
        if (client_fd < 0) {
            if (closing) break;
            else continue;
        }
        char buffer[8192] = {0};
        ssize_t n = read(client_fd, buffer, sizeof(buffer) - 1);
        bool suppressLogging = false;
        bool duplicateRequest = false;
        bool snapshotCaptured = false;
        std::string requestSignature;
        std::chrono::steady_clock::time_point requestTimestamp;
        EstadoRobot::Snapshot snapshotBefore{};
        std::string respuestaHttp;
        
        if(n > 0) {
            std::string req(buffer, n);
            std::string method, path;
            ServerB.parseHttpRequest(req, method, path);
            requestTimestamp = std::chrono::steady_clock::now();
            snapshotBefore = estado.leer();
            snapshotCaptured = true;
            std::size_t bodyHash = std::hash<std::string>{}(req);
            requestSignature = method + "|" + path + "|" + std::to_string(bodyHash);
            bool isHealthCheck = (path.rfind("/health.txt", 0) == 0);
            bool isStatusPoll = false;
            if (!isHealthCheck && method == "POST") {
                if (req.find("<methodName>getEstado</methodName>") != std::string::npos) {
                    isStatusPoll = true;
                }
            }
            suppressLogging = isHealthCheck || isStatusPoll;
            if (hasCachedRequest) {
                const bool withinWindow = (requestTimestamp - lastRequestCache.timestamp) <= kRequestDedupWindow;
                if (withinWindow &&
                    requestSignature == lastRequestCache.signature &&
                    snapshotsEqual(snapshotBefore, lastRequestCache.estado)) {
                    duplicateRequest = true;
                    suppressLogging = true;
                    respuestaHttp = lastRequestCache.response;
                }
            }
            if (!suppressLogging) {
                if (cleanTerminal && !firstFeedback) {
                    std::system("clear");
                }
                firstFeedback = false;
                std::cout << "🌐 SOLICITUD: " << method << " " << path << std::endl;
            }
            
            if (!duplicateRequest) {
                if (closing) {
                    std::string closingHtml = ServerB.readFile("HTML/server_terminated.html");
                    if (closingHtml.empty()) {
                        closingHtml =
                            "<!DOCTYPE html><html><head><meta charset=\"utf-8\"><title>Server Closed</title>"
                            "<style>body{font-family:system-ui;background:#0f172a;color:#e2e8f0;display:flex;"
                            "align-items:center;justify-content:center;height:100vh;margin:0;text-align:center}"
                            "</style></head><body><div><h1>[SERVER TERMINATED]</h1>"
                            "<p>El servicio se cerró manualmente.</p></div></body></html>";
                    }
                    std::ostringstream out;
                    out << "HTTP/1.1 503 Service Unavailable\r\n"
                        << "Content-Type: text/html; charset=utf-8\r\n"
                        << "Content-Length: " << closingHtml.size() << "\r\n"
                        << "Connection: close\r\n"
                        << "Cache-Control: no-store\r\n"
                        << "Access-Control-Allow-Origin: *\r\n"
                        << "X-Server-Closing: yes\r\n\r\n"
                        << closingHtml;
                    respuestaHttp = out.str();
                } else if (method == "GET") {
                    if (isHealthCheck) {
                    const std::string body = "ok";
                    std::ostringstream out;
                    out << "HTTP/1.1 200 OK\r\n"
                        << "Content-Type: text/plain; charset=utf-8\r\n"
                        << "Cache-Control: no-store\r\n"
                        << "Content-Length: " << body.size() << "\r\n"
                        << "Access-Control-Allow-Origin: *\r\n\r\n"
                        << body;
                    respuestaHttp = out.str();
                    } else {
                        respuestaHttp = ServerB.serveStaticFile(path);
                    }
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
                                }                                 
                                catch (const std::exception& e) {
                                    std::string err = std::string("Exception: ") + e.what();
                                    std::ostringstream out; out << "HTTP/1.1 500 Internal Server Error\r\nContent-Type: text/plain\r\nContent-Length: " << err.size() << "\r\nAccess-Control-Allow-Origin: *\r\n\r\n" << err;
                                    respuestaHttp = out.str();
                                    ServerB.press_enter(cleanTerminal);
                                }
                            } else {
                                const bool quietRpc = isStatusPoll;
                                std::string resp = ServerB.procesarRPC(body, login, robot, estado, aprendizaje, admin, quietRpc);
                        
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
            }
            
            const bool wasClosingResponse = closing;
            if (!respuestaHttp.empty()) {
                write(client_fd, respuestaHttp.c_str(), respuestaHttp.size());
                if (wasClosingResponse) {
                    closingServed.store(true);
                }
                if (!suppressLogging) {
                    std::cout << "✅ RESPUESTA ENVIADA (" << respuestaHttp.size() << " bytes)" << std::endl;
                }
                if (snapshotCaptured) {
                    lastRequestCache.signature = requestSignature;
                    lastRequestCache.response = respuestaHttp;
                    lastRequestCache.estado = snapshotBefore;
                    lastRequestCache.timestamp = requestTimestamp;
                    hasCachedRequest = true;
                }
            } else {
                std::string error = "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\nContent-Length: 13\r\nAccess-Control-Allow-Origin: *\r\n\r\n404 Not Found";
                write(client_fd, error.c_str(), error.size());
                if (!suppressLogging) {
                    std::cout << "❌ ENVIADO 404" << std::endl;
                }
            }
        }
        close(client_fd);
        if (!suppressLogging) {
            std::cout << "----------------------------------------" << std::endl;
        }
    }
    close(server_fd);
    if (replThread.joinable()) {
        replThread.join();
    }
    return 0;

    return 0;
}
