#ifndef ADMINISTRADOR_SISTEMA_H
#define ADMINISTRADOR_SISTEMA_H

#include <mutex>
#include <iostream>

class AdministradorSistema {
    mutable std::mutex mtx;  // Add mutable here
    bool remotoHabilitado = true;

public:
    void setRemoto(bool on);
    bool getRemoto() const;
};

#endif