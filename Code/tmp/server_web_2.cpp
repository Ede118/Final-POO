// server_web.cpp - Servidor HTTP/XML-RPC minimal con CORS y preflight
// Compila en Linux (g++) y Windows (MinGW). Maneja: OPTIONS (CORS), POST XML-RPC (ServerTest, Eco, sumar).

#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <thread>
#include <algorithm>
#include <cctype>
#include <cstring>

#ifdef _WIN32
  #define NOMINMAX
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #pragma comment(lib, "Ws2_32.lib")
  using socklen_t = int;
  static int close_socket(SOCKET s){ return ::closesocket(s); }
  static const int INVALID_SOCK = INVALID_SOCKET;
#else
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <unistd.h>
  #include <fcntl.h>
  #define SOCKET int
  static int close_socket(SOCKET s){ return ::close(s); }
  static const int INVALID_SOCK = -1;
#endif

// --------------------- Utils: trim y lowercase ---------------------
static std::string to_lower(std::string s){
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::tolower(c); });
    return s;
}

static std::string trim(std::string s){
    auto not_space = [](int ch){ return !std::isspace(ch); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
    s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
    return s;
}

// --------------------- Descubrir IP LAN (truco UDP) ---------------------
static std::string ip_lan(){
    SOCKET sock = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == INVALID_SOCK) return "127.0.0.1";

    sockaddr_in dst{};
    dst.sin_family = AF_INET;
    dst.sin_port = htons(80);
    inet_pton(AF_INET, "1.1.1.1", &dst.sin_addr);

    if (::connect(sock, reinterpret_cast<sockaddr*>(&dst), sizeof(dst)) < 0) {
        close_socket(sock);
        return "127.0.0.1";
    }
    sockaddr_in name{};
    socklen_t namelen = sizeof(name);
    if (::getsockname(sock, reinterpret_cast<sockaddr*>(&name), &namelen) < 0) {
        close_socket(sock);
        return "127.0.0.1";
    }
    close_socket(sock);

    char buf[INET_ADDRSTRLEN]{};
    if (!::inet_ntop(AF_INET, &name.sin_addr, buf, sizeof(buf))) {
        return "127.0.0.1";
    }
    return std::string(buf);
}

// --------------------- HTTP helpers ---------------------
struct HttpRequest {
    std::string method;
    std::string path;
    std::string version;
    std::vector<std::pair<std::string,std::string>> headers;
    std::string body;
};

static bool starts_with(std::string_view s, std::string_view pref){
    return s.size() >= pref.size() && std::equal(pref.begin(), pref.end(), s.begin());
}

static std::string get_header(const HttpRequest& r, std::string key){
    key = to_lower(key);
    for (auto& kv : r.headers){
        if (to_lower(kv.first) == key) return kv.second;
    }
    return "";
}

static HttpRequest parse_http_request(const std::string& raw){
    HttpRequest req;
    auto pos = raw.find("\r\n");
    if (pos == std::string::npos) return req;
    std::string line = raw.substr(0, pos);
    // Request line
    {
        auto p1 = line.find(' ');
        auto p2 = (p1==std::string::npos) ? std::string::npos : line.find(' ', p1+1);
        if (p1!=std::string::npos && p2!=std::string::npos){
            req.method  = line.substr(0, p1);
            req.path    = line.substr(p1+1, p2-p1-1);
            req.version = line.substr(p2+1);
        }
    }
    // Headers
    size_t cur = pos + 2;
    while (true){
        auto eol = raw.find("\r\n", cur);
        if (eol == std::string::npos) break;
        if (eol == cur){ // empty line
            cur += 2;
            break;
        }
        std::string hline = raw.substr(cur, eol - cur);
        auto colon = hline.find(':');
        if (colon != std::string::npos){
            std::string k = trim(hline.substr(0, colon));
            std::string v = trim(hline.substr(colon+1));
            req.headers.emplace_back(k, v);
        }
        cur = eol + 2;
    }
    // Body
    if (cur < raw.size()){
        req.body = raw.substr(cur);
    }
    return req;
}

static std::string http_response(int code, std::string_view status, std::string_view content_type, std::string_view body, bool cors=true){
    std::string resp;
    resp += "HTTP/1.1 " + std::to_string(code) + " " + std::string(status) + "\r\n";
    if (cors){
        resp += "Access-Control-Allow-Origin: *\r\n";
        resp += "Access-Control-Allow-Methods: POST, OPTIONS\r\n";
        resp += "Access-Control-Allow-Headers: Content-Type, Accept\r\n";
    }
    resp += "Content-Type: " + std::string(content_type) + "\r\n";
    resp += "Content-Length: " + std::to_string(body.size()) + "\r\n";
    resp += "Connection: close\r\n\r\n";
    resp += std::string(body);
    return resp;
}

static std::string http_no_content(){
    std::string resp;
    resp += "HTTP/1.1 204 No Content\r\n";
    resp += "Access-Control-Allow-Origin: *\r\n";
    resp += "Access-Control-Allow-Methods: POST, OPTIONS\r\n";
    resp += "Access-Control-Allow-Headers: Content-Type, Accept\r\n";
    resp += "Content-Length: 0\r\n";
    resp += "Connection: close\r\n\r\n";
    return resp;
}

// --------------------- XML-RPC helpers (parsing naive) ---------------------
static std::string xmlrpc_ok_string(const std::string& s){
    std::string esc = s;
    // naive escape
    auto repl = [&](const char* from, const char* to){
        size_t pos=0;
        while ((pos = esc.find(from, pos)) != std::string::npos){
            esc.replace(pos, std::strlen(from), to);
            pos += std::strlen(to);
        }
    };
    repl("&", "&amp;");
    repl("<", "&lt;");
    repl(">", "&gt;");
    return std::string("<?xml version=\"1.0\"?>\n"
    "<methodResponse><params><param><value><string>") + esc +
    "</string></value></param></params></methodResponse>\n";
}

static std::string xmlrpc_ok_int(int v){
    return "<?xml version=\"1.0\"?>\n"
           "<methodResponse><params><param><value><int>" + std::to_string(v) +
           "</int></value></param></params></methodResponse>\n";
}

static std::string xmlrpc_fault(int code, const std::string& msg){
    return "<?xml version=\"1.0\"?>\n"
           "<methodResponse><fault><value><struct>"
           "<member><name>faultCode</name><value><int>" + std::to_string(code) + "</int></value></member>"
           "<member><name>faultString</name><value><string>" + msg + "</string></value></member>"
           "</struct></value></fault></methodResponse>\n";
}

static std::string between(std::string_view s, std::string_view a, std::string_view b){
    auto i = s.find(a);
    if (i == std::string_view::npos) return "";
    i += a.size();
    auto j = s.find(b, i);
    if (j == std::string_view::npos) return "";
    return std::string(s.substr(i, j - i));
}

// --------------------- Handler XML-RPC ---------------------
static std::string handle_xmlrpc(const std::string& xml){
    std::string method = between(xml, "<methodName>", "</methodName>");
    if (method.empty()){
        return xmlrpc_fault(400, "methodName missing");
    }
    if (method == "ServerTest"){
        return xmlrpc_ok_string("OK");
    }
    if (method == "Eco"){
        // primer <string> del body
        std::string echoed = between(xml, "<string>", "</string>");
        return xmlrpc_ok_string(echoed);
    }
    if (method == "sumar"){
        // tomar dos <int> o <i4>
        int a = 0, b = 0;
        std::string a_str = between(xml, "<int>", "</int>");
        if (a_str.empty()) a_str = between(xml, "<i4>", "</i4>");
        if (a_str.empty()) return xmlrpc_fault(400, "missing first int");
        size_t p2 = xml.find("</int>", xml.find(a_str));
        size_t start2 = std::string::npos;
        // buscar segunda aparición
        size_t next_pos = xml.find("<int>", p2 == std::string::npos ? 0 : p2 + 5);
        bool used_i4 = false;
        if (next_pos == std::string::npos){
            next_pos = xml.find("<i4>", p2 == std::string::npos ? 0 : p2 + 5);
            used_i4 = true;
        }
        if (next_pos == std::string::npos) return xmlrpc_fault(400, "missing second int");
        std::string b_str = between(xml, used_i4 ? "<i4>" : "<int>", used_i4 ? "</i4>" : "</int>");
        try{
            a = std::stoi(a_str);
            b = std::stoi(b_str);
        }catch(...){
            return xmlrpc_fault(400, "parse error");
        }
        return xmlrpc_ok_int(a + b);
    }
    return xmlrpc_fault(404, "unknown method: " + method);
}

// --------------------- Servidor ---------------------
static void serve_client(SOCKET cfd){
    // Leer todo el request (cabeceras + body según Content-Length)
    std::string req_buf;
    char tmp[4096];
    int header_end = -1;
    int content_length = 0;

    while (true){
#ifdef _WIN32
        int n = ::recv(cfd, tmp, sizeof(tmp), 0);
#else
        int n = ::recv(cfd, tmp, sizeof(tmp), 0);
#endif
        if (n <= 0) break;
        req_buf.append(tmp, tmp + n);
        if (header_end < 0){
            auto pos = req_buf.find("\r\n\r\n");
            if (pos != std::string::npos){
                header_end = (int)pos + 4;
                // buscar Content-Length
                auto req = parse_http_request(req_buf.substr(0, header_end));
                std::string cl = get_header(req, "content-length");
                if (!cl.empty()) content_length = std::max(0, std::stoi(cl));
            }
        }
        if (header_end >= 0){
            int body_have = (int)req_buf.size() - header_end;
            if (body_have >= content_length) break;
        }
        if ((int)req_buf.size() > 1024*1024) break; // evitar DOS
    }

    HttpRequest req = parse_http_request(req_buf);
    std::string method = req.method;

    if (method == "OPTIONS"){
        std::string resp = http_no_content();
        send(cfd, resp.c_str(), (int)resp.size(), 0);
        close_socket(cfd);
        return;
    }

    if (method != "POST"){
        const char* body = "Only POST allowed";
        std::string resp = http_response(405, "Method Not Allowed", "text/plain; charset=utf-8", body);
        send(cfd, resp.c_str(), (int)resp.size(), 0);
        close_socket(cfd);
        return;
    }

    // Procesar XML-RPC
    if (req.body.empty()){
        std::string fault = xmlrpc_fault(411, "Content-Length required or empty body");
        std::string resp = http_response(400, "Bad Request", "text/xml; charset=utf-8", fault);
        send(cfd, resp.c_str(), (int)resp.size(), 0);
        close_socket(cfd);
        return;
    }

    std::string xml = req.body;
    std::string result = handle_xmlrpc(xml);
    std::string resp = http_response(200, "OK", "text/xml; charset=utf-8", result);
    send(cfd, resp.c_str(), (int)resp.size(), 0);
    close_socket(cfd);
}

int main(){
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0){
        std::cerr << "WSAStartup failed\n";
        return 1;
    }
#endif

    SOCKET sfd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (sfd == INVALID_SOCK){
        std::cerr << "No pude crear socket\n";
        return 1;
    }

    int yes = 1;
#ifdef _WIN32
    setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof(yes));
#else
    setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
#endif

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY); // 0.0.0.0
    addr.sin_port = htons(8080);

    if (::bind(sfd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0){
        std::cerr << "bind() falló\n";
        return 1;
    }
    if (::listen(sfd, 16) < 0){
        std::cerr << "listen() falló\n";
        return 1;
    }

    std::cout << "Servidor XML-RPC escuchando en 0.0.0.0:8080\n";
    std::cout << "Accesible desde: http://" << ip_lan() << ":8080\n";

    while (true){
        sockaddr_in cli{};
        socklen_t clen = sizeof(cli);
#ifdef _WIN32
        SOCKET cfd = ::accept(sfd, reinterpret_cast<sockaddr*>(&cli), &clen);
#else
        SOCKET cfd = ::accept(sfd, reinterpret_cast<sockaddr*>(&cli), &clen);
#endif
        if (cfd == INVALID_SOCK){
            continue;
        }
        std::thread(serve_client, cfd).detach();
    }

#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
