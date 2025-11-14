#ifndef APRENDIZAJE_H
#define APRENDIZAJE_H

#include <fstream>
#include <string>
#include <mutex>
#include <iostream>

class Aprendizaje {
    std::ofstream log;
    std::mutex mtx;
    bool activo = false;
    std::string rutaArchivo = "aprendizaje.gcode";

public:
    void iniciar(const std::string& ruta = "aprendizaje.gcode") {
        std::lock_guard<std::mutex> lock(mtx);
        rutaArchivo = ruta;
        log.open(rutaArchivo, std::ios::out | std::ios::trunc);
        activo = log.is_open();
        std::cout << (activo ? "📘 Aprendizaje iniciado -> " : "❌ No se pudo abrir ") << rutaArchivo << "\n";
    }

    void detener() {
        std::lock_guard<std::mutex> lock(mtx);
        if (activo) {
            log.close();
            activo = false;
            std::cout << "📕 Aprendizaje detenido.\n";
        }
    }

    void registrar(const std::string& cmd) {
        std::lock_guard<std::mutex> lock(mtx);
        if (activo && log.is_open()) {
            log << cmd << "\n";
        }
    }

    bool estaActivo() const { return activo; }
};

#endif
