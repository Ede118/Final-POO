#include "../inc/logger.h"
#include <fstream>
#include <filesystem>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <sstream>

Logger logger; // definición de la instancia global

namespace fs = std::filesystem;
static std::mutex log_mtx;

static std::string nowIso() {
    using namespace std::chrono;
    auto now = system_clock::now();
    auto t = system_clock::to_time_t(now);
    std::tm tm;
    localtime_r(&t, &tm);
    std::ostringstream ss;
    ss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S");
    return ss.str();
}

Logger::Logger(const std::string& path) : filepath(path) {
    ensureLogFile();
}

Logger::~Logger() {}

void Logger::ensureLogFile() {
    std::lock_guard<std::mutex> l(log_mtx);
    fs::path p = filepath;
    if (!p.has_parent_path()) {
        // ensure logs/ exists
        fs::create_directories("logs");
        p = fs::path("logs") / p;
        filepath = p.string();
    } else {
        fs::create_directories(p.parent_path());
    }

    bool writeHeader = !fs::exists(filepath) || fs::file_size(filepath) == 0;
    if (writeHeader) {
        std::ofstream f(filepath, std::ios::app);
        if (f) {
            // Cabecera: timestamp,category,detail,user,node,response_code,module,message
            f << "timestamp,category,detail,user,node,response_code,module,message\n";
        }
    }
}

void Logger::logRequest(const std::string& detail, const std::string& user,
                        const std::string& node, int response_code) {
    std::lock_guard<std::mutex> l(log_mtx);
    std::ofstream out(filepath, std::ios::app);
    if (!out) return;
    std::string ts = nowIso();
    // escape double quotes by doubling them
    auto esc = [](const std::string& s){
        std::string out; out.reserve(s.size()+4);
        for (char c: s) {
            if (c == '"') { out += '"'; out += '"'; }
            else out += c;
        }
        return out;
    };
    std::string d = esc(detail);
    std::string u = esc(user);
    std::string n = esc(node);

    out << '"' << ts << '"' << ','
        << '"' << "request" << '"' << ','
        << '"' << d << '"' << ','
        << '"' << u << '"' << ','
        << '"' << n << '"' << ','
        << response_code << ','
        << ',' // module empty
        << ',' << '\n';
}

void Logger::logEvent(const std::string& module, const std::string& message) {
    std::lock_guard<std::mutex> l(log_mtx);
    std::ofstream f(filepath, std::ios::app);
    if (!f) return;
    std::string ts = nowIso();
    auto esc = [](const std::string& s){
        std::string out; out.reserve(s.size()+4);
        for (char c: s) {
            if (c=='"') out += '"', out += '"';
            else out += c;
        }
        return out;
    };
    std::string m = esc(module);
    std::string msg = esc(message);
    f << '"' << ts << '"' << ','
      << '"' << "event" << '"' << ','
      << ',' // detail empty
      << ',' // user empty
      << ',' // node empty
      << ',' // response_code empty
      << '"' << m << '"' << ','
      << '"' << msg << '"' << '\n';
}
