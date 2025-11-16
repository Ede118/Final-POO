#pragma once
#ifndef ADMINISTRADOR_SISTEMA_H
#define ADMINISTRADOR_SISTEMA_H

#include <mutex>
#include <iostream>
#include <string>

class AdministradorSistema {
    mutable std::mutex mtx;
    bool remotoHabilitado = true;
    std::string storagePath;

public:
    AdministradorSistema();
    explicit AdministradorSistema(std::string storageFile);
    void setRemoto(bool on);
    bool getRemoto() const;

private:
    void persistStateLocked() const;
    void loadStateFromDisk();
};

#endif
