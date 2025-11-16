#include "administrador_sistema.h"

#include <fstream>
#include <filesystem>
#include <utility>

namespace {
constexpr const char* kDefaultRemoteStateFile = "db/remote_state.dat";
}

AdministradorSistema::AdministradorSistema()
    : AdministradorSistema(kDefaultRemoteStateFile) {}

AdministradorSistema::AdministradorSistema(std::string storageFile)
    : storagePath(std::move(storageFile)) {
    if (storagePath.empty()) {
        storagePath = kDefaultRemoteStateFile;
    }
    loadStateFromDisk();
}

void AdministradorSistema::setRemoto(bool on) {
    std::lock_guard<std::mutex> lock(mtx);
    if (remotoHabilitado == on) {
        std::cout << (on ? "🌐 Modo remoto ya estaba habilitado\n"
                         : "🔒 Modo remoto ya estaba deshabilitado\n");
        return;
    }
    remotoHabilitado = on;
    persistStateLocked();
    std::cout << (on ? "🌐 Modo remoto habilitado\n" : "🔒 Modo remoto deshabilitado\n");
}

bool AdministradorSistema::getRemoto() const {
    std::lock_guard<std::mutex> lock(mtx);
    return remotoHabilitado;
}

void AdministradorSistema::persistStateLocked() const {
    namespace fs = std::filesystem;
    fs::path filePath(storagePath);
    if (!filePath.empty() && filePath.has_parent_path()) {
        std::error_code ec;
        fs::create_directories(filePath.parent_path(), ec);
    }
    std::ofstream out(storagePath, std::ios::trunc);
    if (out) {
        out << (remotoHabilitado ? 1 : 0);
    }
}

void AdministradorSistema::loadStateFromDisk() {
    std::lock_guard<std::mutex> lock(mtx);
    std::ifstream in(storagePath);
    if (!in) {
        remotoHabilitado = true;
        return;
    }
    int stored = 1;
    in >> stored;
    remotoHabilitado = (stored != 0);
}
