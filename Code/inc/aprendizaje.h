
#ifndef APRENDIZAJE_H
#define APRENDIZAJE_H

#include <fstream>
#include <string>
#include <mutex>
#include <iostream>
#include <filesystem>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

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
            // Al detener, generar un CSV con los comandos guardados y colocarlo en la carpeta 'aprendizajes'
            try {
                namespace fs = std::filesystem;
                fs::path dir = "aprendizajes";
                fs::create_directories(dir);

                auto now = std::chrono::system_clock::now();
                std::time_t t = std::chrono::system_clock::to_time_t(now);
                std::tm tm;
                localtime_r(&t, &tm);
                std::ostringstream ss;
                ss << std::put_time(&tm, "%Y%m%d_%H%M%S");

                fs::path csvpath = dir / ("aprendizaje_" + ss.str() + ".csv");

                std::ifstream in(rutaArchivo);
                std::ofstream out(csvpath, std::ios::out | std::ios::trunc);
                if (in && out) {
                    out << "gcode\n";
                    std::string line;
                    while (std::getline(in, line)) {
                        // Escapar comillas dobles para CSV
                        std::string esc = line;
                        size_t pos = 0;
                        while ((pos = esc.find('"', pos)) != std::string::npos) { esc.insert(pos, "\""); pos += 2; }
                        out << '"' << esc << '"' << '\n';
                    }
                    std::cout << "📁 Archivo CSV guardado: " << csvpath.string() << "\n";
                    // Además, copiar el .gcode original a 'aprendizajes' y a 'jobs' con el mismo timestamp
                    try {
                        fs::path gcodeDst = dir / ("aprendizaje_" + ss.str() + ".gcode");
                        // copiar archivo origen (rutaArchivo) -> aprendizajes/aprendizaje_<ts>.gcode
                        fs::copy_file(rutaArchivo, gcodeDst, fs::copy_options::overwrite_existing);
                        std::cout << "📁 Archivo GCODE guardado: " << gcodeDst.string() << "\n";

                        // Asegurar carpeta jobs y copiar allí también
                        fs::create_directories("jobs");
                        fs::path jobsDst = fs::path("jobs") / gcodeDst.filename();
                        fs::copy_file(rutaArchivo, jobsDst, fs::copy_options::overwrite_existing);
                        std::cout << "📁 Copia GCODE en jobs: " << jobsDst.string() << "\n";
                    } catch (const std::exception& e2) {
                        std::cerr << "❌ Excepción al copiar GCODE: " << e2.what() << "\n";
                    }
                } else {
                    std::cerr << "❌ No se pudo leer el archivo de aprendizaje o crear CSV: " << rutaArchivo << "\n";
                }
            } catch (const std::exception& e) {
                std::cerr << "❌ Excepción al guardar CSV: " << e.what() << "\n";
            }
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
