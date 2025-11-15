#pragma once
#ifndef ESTADO_ROBOT_H
#define ESTADO_ROBOT_H

#include <mutex>

class EstadoRobot {
    mutable std::mutex mtx;
    float x=0, y=0, z=0;
    bool motores=false;
    bool garra=false;
    bool modoAbs=true;
    bool emergencia=false;

public:
    void setPos(float nx,float ny,float nz){
        std::lock_guard<std::mutex> l(mtx);
        x=nx; y=ny; z=nz;
    }
    void setMotores(bool on){ std::lock_guard<std::mutex> l(mtx); motores=on; }
    void setGarra(bool on){ std::lock_guard<std::mutex> l(mtx); garra=on; }
    void setModo(bool abs){ std::lock_guard<std::mutex> l(mtx); modoAbs=abs; }
    void setEmergencia(bool e){ std::lock_guard<std::mutex> l(mtx); emergencia=e; }

    struct Snapshot {
        float x,y,z;
        bool motores,garra,modoAbs,emergencia;
    };
    Snapshot leer() const {
        std::lock_guard<std::mutex> l(mtx);
        return {x,y,z,motores,garra,modoAbs,emergencia};
    }
};

#endif
