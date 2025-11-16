#include "server.h"
#include "logger.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace {
std::string trim(const std::string& s) {
    auto begin = s.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return "";
    }
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(begin, end - begin + 1);
}

std::string xmlEscape(const std::string& input) {
    std::string out;
    out.reserve(input.size());
    for (char c : input) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            case '\'': out += "&apos;"; break;
            default: out += c; break;
        }
    }
    return out;
}

std::string buildStructResponse(const std::vector<std::pair<std::string, std::string>>& members) {
    std::ostringstream out;
    out << "<?xml version=\"1.0\"?>"
        << "<methodResponse><params><param><value><struct>";
    for (const auto& m : members) {
        out << "<member><name>" << xmlEscape(m.first) << "</name><value><string>"
            << xmlEscape(m.second) << "</string></value></member>";
    }
    out << "</struct></value></param></params></methodResponse>";
    return out.str();
}

std::string buildFault(const std::string& message) {
    std::ostringstream out;
    out << "<?xml version=\"1.0\"?>"
        << "<methodResponse><fault><value><struct>"
        << "<member><name>status</name><value><string>error</string></value></member>"
        << "<member><name>message</name><value><string>" << xmlEscape(message)
        << "</string></value></member>"
        << "</struct></value></fault></methodResponse>";
    return out.str();
}

std::string extractMethodName(const std::string& body) {
    const std::string open = "<methodName>";
    const std::string close = "</methodName>";
    auto begin = body.find(open);
    if (begin == std::string::npos) return {};
    begin += open.size();
    auto end = body.find(close, begin);
    if (end == std::string::npos) return {};
    return trim(body.substr(begin, end - begin));
}

int privilegeLevel(const std::string& privilege) {
    if (privilege == "admin") return 2;
    if (privilege == "user") return 1;
    return 0;
}

std::string formatFloat(float value) {
    std::ostringstream os;
    os << std::fixed << std::setprecision(3) << value;
    return os.str();
}
}

std::string toString(ServerState s) {
    switch (s) {
        case ServerState::STABLE: return "STABLE";
        case ServerState::BUSY: return "BUSY";
        case ServerState::ERROR: return "ERROR";
    }
    return "UNKNOWN";
}

bool Server::parseCleanFlag(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--clean" || arg == "--clear" || arg == "-c") {
            return true;
        }
    }
    return false;
}

bool Server::endsWith(const std::string& str, const std::string& suffix) {
    if (suffix.size() > str.size()) return false;
    return std::equal(suffix.rbegin(), suffix.rend(), str.rbegin());
}

std::vector<std::string> Server::extractMultipleParams(const std::string& body, int count) {
    std::vector<std::string> params;
    size_t pos = 0;
    while (true) {
        auto start = body.find("<string>", pos);
        if (start == std::string::npos) break;
        start += 8;
        auto end = body.find("</string>", start);
        if (end == std::string::npos) break;
        params.emplace_back(body.substr(start, end - start));
        pos = end + 9;
        if (count > 0 && static_cast<int>(params.size()) >= count) break;
    }
    return params;
}

bool Server::extractParams(const std::string& body, std::string& u, std::string& p) {
    auto params = extractMultipleParams(body, 2);
    if (params.size() < 2) return false;
    u = params[0];
    p = params[1];
    return true;
}

bool Server::extractJsonParam(const std::string& body, json& j) {
    auto params = extractMultipleParams(body, -1);
    for (auto& param : params) {
        auto candidate = trim(param);
        if (candidate.empty()) continue;
        if (candidate.front() != '{' && candidate.front() != '[') {
            continue;
        }
        try {
            j = json::parse(candidate);
            return true;
        } catch (const std::exception&) {
            return false;
        }
    }
    return false;
}

std::string Server::readFile(const std::string& filepath) {
    std::ifstream in(filepath, std::ios::binary);
    if (!in) return {};
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

std::string Server::getMimeType(const std::string& path) {
    auto ext = fs::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c){ return std::tolower(c); });
    if (ext == ".html" || ext == ".htm") return "text/html; charset=utf-8";
    if (ext == ".css") return "text/css; charset=utf-8";
    if (ext == ".js") return "application/javascript; charset=utf-8";
    if (ext == ".json") return "application/json";
    if (ext == ".png") return "image/png";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".gif") return "image/gif";
    if (ext == ".svg") return "image/svg+xml";
    if (ext == ".ico") return "image/x-icon";
    if (ext == ".txt" || ext == ".log") return "text/plain; charset=utf-8";
    if (ext == ".csv") return "text/csv; charset=utf-8";
    if (ext == ".mp4") return "video/mp4";
    if (ext == ".webm") return "video/webm";
    return "application/octet-stream";
}

std::string Server::serveStaticFile(const std::string& requestPath) {
    std::string path = requestPath;
    if (path.empty() || path[0] != '/') {
        path = "/" + path;
    }
    auto query = path.find('?');
    if (query != std::string::npos) {
        path = path.substr(0, query);
    }
    if (path == "/" || path == "/index.html") {
        path = "/signin.html";
    }

    fs::path base("HTML");
    fs::path requested = base / path.substr(1);
    std::error_code ec;
    fs::path canonicalBase = fs::weakly_canonical(base, ec);
    if (ec) canonicalBase = fs::absolute(base);
    fs::path canonicalRequest = fs::weakly_canonical(requested, ec);
    if (ec) canonicalRequest = fs::absolute(requested);

    auto baseStr = canonicalBase.string();
    auto reqStr = canonicalRequest.string();
    if (reqStr.compare(0, baseStr.size(), baseStr) != 0) {
        const std::string body = "403 Forbidden";
        std::ostringstream out;
        out << "HTTP/1.1 403 Forbidden\r\n"
            << "Content-Type: text/plain; charset=utf-8\r\n"
            << "Access-Control-Allow-Origin: *\r\n"
            << "Content-Length: " << body.size() << "\r\n\r\n"
            << body;
        return out.str();
    }

    if (!fs::exists(canonicalRequest) || fs::is_directory(canonicalRequest)) {
        const std::string body = "404 Not Found";
        std::ostringstream out;
        out << "HTTP/1.1 404 Not Found\r\n"
            << "Content-Type: text/plain; charset=utf-8\r\n"
            << "Access-Control-Allow-Origin: *\r\n"
            << "Content-Length: " << body.size() << "\r\n\r\n"
            << body;
        return out.str();
    }

    auto data = readFile(canonicalRequest.string());
    std::ostringstream out;
    out << "HTTP/1.1 200 OK\r\n"
        << "Content-Type: " << getMimeType(canonicalRequest.string()) << "\r\n"
        << "Cache-Control: no-store\r\n"
        << "Access-Control-Allow-Origin: *\r\n"
        << "Content-Length: " << data.size() << "\r\n\r\n"
        << data;
    return out.str();
}

void Server::press_enter(bool cleanTerminal) {
    if (!cleanTerminal) {
        std::cout << "Presione ENTER para continuar (CTRL+D para mantener consola limpia)..." << std::endl;
    }
}

void Server::pause_sec(int s) {
    if (s <= 0) return;
    std::this_thread::sleep_for(std::chrono::seconds(s));
}

void Server::parseHttpRequest(const std::string& request, std::string& method, std::string& path) {
    std::istringstream iss(request);
    iss >> method >> path;
    if (path.empty()) path = "/";
}

std::string Server::procesarRPC(const std::string& body, Login& login, RobotControllerSimple& robot,
                        EstadoRobot& estado, Aprendizaje& aprendizaje, AdministradorSistema& admin,
                        bool quiet) {
    (void)quiet;
    const std::string method = extractMethodName(body);
    if (method.empty()) {
        return buildFault("methodName ausente");
    }

    if (method == "ping") {
        return buildStructResponse({{"status", "ok"}, {"message", "pong"}});
    }

    if (method == "login") {
        std::string user, pass;
        if (!extractParams(body, user, pass)) {
            json payload;
            if (extractJsonParam(body, payload)) {
                user = payload.value("username", std::string());
                pass = payload.value("password", std::string());
            }
        }
        if (user.empty() || pass.empty()) {
            return buildFault("Credenciales incompletas");
        }
        auto auth = login.authenticate(user, pass);
        if (!auth.success) {
            return buildStructResponse({{"status", "error"}, {"message", auth.message}});
        }
        logger.logEvent("auth", "Login de " + user + " como " + auth.privilege);
        return buildStructResponse({
            {"status", "success"},
            {"message", auth.message},
            {"token", auth.token},
            {"privilege", auth.privilege},
            {"user", user}
        });
    }

    json payload = json::object();
    extractJsonParam(body, payload);

    struct RpcSession {
        std::string username;
        std::string privilege;
        bool authenticated;
    };

    auto resolveSession = [&](int minLevel, RpcSession& session, std::string& error) {
        std::string token;
        if (payload.contains("token") && payload["token"].is_string()) {
            token = payload["token"].get<std::string>();
        }
        if (token.empty()) {
            session = {"local", "admin", false};
            return true;
        }
        auto username = login.usernameForToken(token);
        if (username.empty()) {
            error = "Token inválido";
            return false;
        }
        auto privilege = login.privilegeForToken(token);
        if (privilege.empty()) privilege = "viewer";
        if (privilegeLevel(privilege) < minLevel) {
            error = "Privilegios insuficientes";
            return false;
        }
        session = {username, privilege, true};
        return true;
    };

    auto ok = [&](const std::string& message){
        return buildStructResponse({{"status", "ok"}, {"message", message}});
    };

    if (method == "getEstado") {
        RpcSession session;
        std::string err;
        if (!resolveSession(0, session, err)) {
            return buildFault(err);
        }
        auto snapshot = estado.leer();
        bool remoto = admin.getRemoto();
        return buildStructResponse({
            {"status", "ok"},
            {"x", formatFloat(snapshot.x)},
            {"y", formatFloat(snapshot.y)},
            {"z", formatFloat(snapshot.z)},
            {"motores", snapshot.motores ? "ON" : "OFF"},
            {"garra", snapshot.garra ? "ON" : "OFF"},
            {"modo", snapshot.modoAbs ? "ABS" : "REL"},
            {"emergencia", snapshot.emergencia ? "SI" : "NO"},
            {"remoto", remoto ? "ON" : "OFF"}
        });
    }

    auto requireUser = [&](int level, RpcSession& session) -> std::optional<std::string> {
        std::string err;
        if (!resolveSession(level, session, err)) {
            return err;
        }
        return std::nullopt;
    };

    RpcSession session;

    if (method == "move") {
        if (auto err = requireUser(1, session)) return buildFault(*err);
        double x = payload.value("x", 0.0);
        double y = payload.value("y", 0.0);
        double z = payload.value("z", 0.0);
        double f = payload.value("f", 1200.0);
        bool abs = payload.value("abs", true);
        robot.mover(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z), static_cast<float>(f), abs);
        logger.logEvent("rpc", session.username + " move x:" + std::to_string(x) + " y:" + std::to_string(y));
        return ok("Movimiento enviado");
    }
    if (method == "motors") {
        if (auto err = requireUser(1, session)) return buildFault(*err);
        bool on = payload.value("on", false);
        robot.setMotores(on);
        return ok(on ? "Motores encendidos" : "Motores apagados");
    }
    if (method == "gripper") {
        if (auto err = requireUser(1, session)) return buildFault(*err);
        bool on = payload.value("on", false);
        robot.setGarra(on);
        return ok(on ? "Garra activada" : "Garra desactivada");
    }
    if (method == "setAbs") {
        if (auto err = requireUser(1, session)) return buildFault(*err);
        robot.setAbs(true);
        return ok("Modo absoluto");
    }
    if (method == "setRel") {
        if (auto err = requireUser(1, session)) return buildFault(*err);
        robot.setAbs(false);
        return ok("Modo relativo");
    }
    if (method == "home") {
        if (auto err = requireUser(1, session)) return buildFault(*err);
        robot.ejecutarComando("G28");
        return ok("Home ejecutado");
    }
    if (method == "sendGcode") {
        if (auto err = requireUser(1, session)) return buildFault(*err);
        auto line = payload.value("line", std::string());
        if (line.empty()) return buildFault("Linea vacía");
        robot.ejecutarComando(line);
        return ok("Comando enviado");
    }
    if (method == "runFile") {
        if (auto err = requireUser(1, session)) return buildFault(*err);
        auto path = payload.value("path", std::string());
        if (path.empty()) return buildFault("Ruta vacía");
        robot.ejecutarArchivo(path);
        return ok("Archivo en ejecución");
    }
    if (method == "startLearning") {
        if (auto err = requireUser(1, session)) return buildFault(*err);
        auto file = payload.value("file", std::string());
        aprendizaje.iniciar(file);
        return ok("Aprendizaje iniciado");
    }
    if (method == "stopLearning") {
        if (auto err = requireUser(1, session)) return buildFault(*err);
        aprendizaje.detener();
        return ok("Aprendizaje detenido");
    }
    if (method == "emergencyStop") {
        if (auto err = requireUser(1, session)) return buildFault(*err);
        robot.emergencia();
        estado.setEmergencia(true);
        return ok("Emergencia activada");
    }
    if (method == "resetEmergency") {
        if (auto err = requireUser(1, session)) return buildFault(*err);
        robot.resetEmergencia();
        estado.setEmergencia(false);
        return ok("Emergencia reseteada");
    }
    if (method == "enableRemote") {
        if (auto err = requireUser(2, session)) return buildFault(*err);
        admin.setRemoto(true);
        return ok("Control remoto habilitado");
    }
    if (method == "disableRemote") {
        if (auto err = requireUser(2, session)) return buildFault(*err);
        admin.setRemoto(false);
        return ok("Control remoto deshabilitado");
    }

    return buildFault("Método desconocido: " + method);
}
