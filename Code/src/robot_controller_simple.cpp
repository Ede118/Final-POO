#include "robot_controller_simple.h"

void RobotControllerSimple::mover(float x, float y, float z, float f, bool abs) {
    std::cout << "🎯 MOVER - X:" << x << " Y:" << y << " Z:" << z 
              << " F:" << f << " ABS:" << abs << std::endl;
    
    // Convertir a absoluto si está en modo relativo
    if (!abs) {
        auto s = estado.leer();
        x += s.x; 
        y += s.y; 
        z += s.z;
        std::cout << "🔄 Convertido a absoluto - X:" << x << " Y:" << y << " Z:" << z << std::endl;
    }
    
    // Actualizar estado
    estado.setPos(x, y, z);
    
    // Generar comando G-code
    std::ostringstream cmd;
    cmd << "G1 X" << x << " Y" << y << " Z" << z << " F" << f;
    
    ejecutarComando(cmd.str());
    registrarAprendizaje(cmd.str());
}

void RobotControllerSimple::setAbs(bool abs) {
    std::cout << "🎛️ MODO: " << (abs ? "ABSOLUTO" : "RELATIVO") << std::endl;
    estado.setModo(abs);
    ejecutarComando(abs ? "G90" : "G91");
    registrarAprendizaje(abs ? "G90" : "G91");
}

void RobotControllerSimple::setMotores(bool on) {
    std::cout << "⚙️ MOTORES: " << (on ? "ENCENDER" : "APAGAR") << std::endl;
    estado.setMotores(on);
    ejecutarComando(on ? "M17" : "M18");
    registrarAprendizaje(on ? "M17" : "M18");
}

void RobotControllerSimple::setGarra(bool on) {
    std::cout << "🦾 GARRA: " << (on ? "ACTIVAR" : "DESACTIVAR") << std::endl;
    estado.setGarra(on);
    ejecutarComando(on ? "M3" : "M5");
    registrarAprendizaje(on ? "M3" : "M5");
}

void RobotControllerSimple::emergencia() {
    std::cout << "🛑 EMERGENCIA ACTIVADA" << std::endl;
    estado.setEmergencia(true);
    ejecutarComando("M112");
    registrarAprendizaje("M112");
}

void RobotControllerSimple::resetEmergencia() {
    std::cout << "🔄 RESET EMERGENCIA" << std::endl;
    estado.setEmergencia(false);
    // No enviamos comando, solo reset estado interno
}

void RobotControllerSimple::ejecutarArchivo(const std::string& ruta) {
    std::cout << "📁 EJECUTANDO ARCHIVO: " << ruta << std::endl;
    // Implementación simple - puedes expandir esto
    std::ifstream file(ruta);
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty()) {
            ejecutarComando(line);
            registrarAprendizaje(line);
        }
    }
}
void RobotControllerSimple::procesarRespuestaArduino(const std::string& respuesta) {
    // Convertir a minúsculas para comparación case-insensitive
    std::string respLower = respuesta;
    std::transform(respLower.begin(), respLower.end(), respLower.begin(), ::tolower);
    
    if (respLower.find("ok") != std::string::npos) {
        std::cout << "🟢 Arduino reporta: OK" << std::endl;
    } else if (respLower.find("error") != std::string::npos) {
        std::cerr << "🔴 Arduino reporta: ERROR" << std::endl;
        // estado.setError(true); // Si tienes este método en EstadoRobot
    } else if (respLower.find("alarm") != std::string::npos) {
        std::cerr << "🚨 ALARMA del Arduino" << std::endl;
        estado.setEmergencia(true);
    } else if (respLower.find("sim:ok") != std::string::npos) {
        std::cout << "🔵 Simulación: Comando aceptado" << std::endl;
    } else if (respuesta.empty()) {
        std::cout << "⚠️  Arduino no respondió (timeout)" << std::endl;
    }
}

void RobotControllerSimple::ejecutarComando(const std::string& cmd) {
    std::string respuesta = comm.enviarComando(cmd);
    
    std::cout << "✅ Comando '" << cmd << "' | Respuesta: '" << respuesta << "'" << std::endl;
    
    // Procesar respuesta del Arduino
    procesarRespuestaArduino(respuesta);
    
    // Solo si quieres verificar errores específicos
    if (respuesta.find("ERROR") != std::string::npos || respuesta.empty()) {
        std::cerr << "❌ El Arduino reportó un error" << std::endl;
    }
}

void RobotControllerSimple::registrarAprendizaje(const std::string& cmd) {
    if (aprendizaje && aprendizaje->estaActivo()) {
        aprendizaje->registrar(cmd);
    }
}