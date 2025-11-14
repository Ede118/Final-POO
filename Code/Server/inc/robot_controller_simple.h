#ifndef ROBOT_CONTROLLER_SIMPLE_H
#define ROBOT_CONTROLLER_SIMPLE_H

#include "comunicacion_controlador_simple.h"
#include "estado_robot.h"
#include "aprendizaje.h"
#include <iostream>
#include <sstream>
#include <algorithm> 

class RobotControllerSimple {
private:
    ComunicacionControladorSimple& comm;
    EstadoRobot& estado;
    Aprendizaje* aprendizaje = nullptr;
    void procesarRespuestaArduino(const std::string& respuesta);
    void registrarAprendizaje(const std::string& cmd);

public:
    RobotControllerSimple(ComunicacionControladorSimple& c, EstadoRobot& e)
        : comm(c), estado(e) {
        std::cout << "🤖 RobotControllerSimple inicializado" << std::endl;
    }

    void setAprendizaje(Aprendizaje* a) { aprendizaje = a; }

    void mover(float x, float y, float z, float f, bool abs);
    void setAbs(bool abs);
    void setMotores(bool on);
    void setGarra(bool on);
    void emergencia();
    void resetEmergencia();
    void ejecutarArchivo(const std::string& ruta);
    void ejecutarComando(const std::string& cmd);


    
    
};

#endif