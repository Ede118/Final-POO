#pragma once
#ifndef COMUNICACION_CONTROLADOR_SIMPLE_H
#define COMUNICACION_CONTROLADOR_SIMPLE_H

#include <string>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <chrono>
#include <thread>
#include <algorithm> 
class ComunicacionControladorSimple {
private:
    int fd = -1;
    std::string puerto;
    speed_t baudrate;

public:
    ComunicacionControladorSimple(const std::string& device = "/dev/ttyUSB0", 
                                 speed_t baud = B19200);
    ~ComunicacionControladorSimple();

    bool isOpen() const { return fd >= 0; }
    std::string enviarComando(const std::string& comando, int timeout_ms = 2000);

private:
    void openPort();
    std::string recibirRespuesta(int timeout_ms); 
};

#endif
