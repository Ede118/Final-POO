#pragma once
#ifndef COMUNICACION_CONTROLADOR_H
#define COMUNICACION_CONTROLADOR_H

#include <string>
#include <fstream>
#include <mutex>
#include <iostream>

class ComunicacionControlador {
    std::string puerto;
    std::ofstream serial;
    std::mutex mtx;

public:
    explicit ComunicacionControlador(const std::string& p = "/dev/ttyUSB0")
        : puerto(p) {
        serial.open(puerto, std::ios::out);
        if (serial.is_open())
            std::cout << "✅ Puerto abierto: " << puerto << "\n";
        else
            std::cerr << "⚠️ No se pudo abrir puerto " << puerto << "\n";
    }

    ~ComunicacionControlador() {
        if (serial.is_open()) serial.close();
    }

    bool enviar(const std::string& cmd) {
        std::lock_guard<std::mutex> lock(mtx);
        if (!serial.is_open()) {
            std::cout << "➡️ Simulación: " << cmd << std::endl;
            return true;
        }
        serial << cmd << "\n";
        serial.flush();
        std::cout << "➡️ CMD → Arduino: " << cmd << "\n";
        return true;
    }
};

#endif
