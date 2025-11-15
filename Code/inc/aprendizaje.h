#ifndef APRENDIZAJE_H
#define APRENDIZAJE_H

#include <fstream>
#include <string>
#include <mutex>
#include <iostream>
#include <filesystem>

class Aprendizaje {
    std::ofstream log;
    std::mutex mtx;
    bool activo = false;
    std::string rutaArchivo = "aprendizaje.gcode";

public:
    void iniciar(const std::string& ruta = "aprendizaje.gcode") {
        std::lock_guard<std::mutex> lock(mtx);
        // si no se proporcionó ruta específica, generar nombre con timestamp
        if (ruta.empty() || ruta == "aprendizaje.gcode") {
            // generar timestamp YYYYMMDD_HHMMSS usando chrono
            using namespace std::chrono;
            auto now = system_clock::now();
            std::time_t t = system_clock::to_time_t(now);
            std::tm tm;
            localtime_r(&t, &tm);
            char buf[64];
            std::strftime(buf, sizeof(buf), "aprendizaje_%Y%m%d_%H%M%S.gcode", &tm);
            rutaArchivo = std::string("aprendizaje gcode/") + std::string(buf);
        } else {
            rutaArchivo = ruta;
            if (rutaArchivo.find('/') == std::string::npos && rutaArchivo.find('\\') == std::string::npos) {
                rutaArchivo = std::string("aprendizaje gcode/") + rutaArchivo;
            }
        }
        // Asegurar carpeta 'aprendizaje gcode'
        try {
            std::filesystem::create_directories("aprendizaje gcode");
        } catch(...) {}
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
