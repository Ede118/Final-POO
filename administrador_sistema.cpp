#include "administrador_sistema.h"

void AdministradorSistema::setRemoto(bool on) {
    std::lock_guard<std::mutex> lock(mtx);
    remotoHabilitado = on;
    std::cout << (on ? "🌐 Modo remoto habilitado\n" : "🔒 Modo remoto deshabilitado\n");
}

bool AdministradorSistema::getRemoto() const {
    std::lock_guard<std::mutex> lock(mtx);
    return remotoHabilitado;
}