#include "comunicacion_controlador_simple.h"
#include <sys/select.h>

ComunicacionControladorSimple::ComunicacionControladorSimple(const std::string& device, speed_t baud)
    : puerto(device), baudrate(baud) {
    openPort();
}

ComunicacionControladorSimple::~ComunicacionControladorSimple() {
    if (fd >= 0) {
        close(fd);
        std::cout << "🔌 Puerto serial cerrado" << std::endl;
    }
}

void ComunicacionControladorSimple::openPort() {
    fd = open(puerto.c_str(), O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0) {
        std::cerr << "❌ No se pudo abrir " << puerto << ": " << strerror(errno) << std::endl;
        std::cout << "📝 Modo simulación activado" << std::endl;
        return;
    }

    // Configuración del puerto
    struct termios tty;
    if (tcgetattr(fd, &tty) != 0) {
        std::cerr << "❌ Error tcgetattr: " << strerror(errno) << std::endl;
        close(fd);
        fd = -1;
        return;
    }

    cfsetospeed(&tty, baudrate);
    cfsetispeed(&tty, baudrate);
    
    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
    tty.c_iflag &= ~IGNBRK;
    tty.c_lflag = 0;
    tty.c_oflag = 0;
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 5;
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~(PARENB | PARODD);
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CRTSCTS;

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        std::cerr << "❌ Error tcsetattr" << std::endl;
        close(fd);
        fd = -1;
        return;
    }

     std::cout << "✅ Puerto abierto: " << puerto << " (" << baudrate << " baud)" << std::endl;
    std::cout << "⏳ Esperando inicialización del Arduino..." << std::endl;
    // Esperar inicialización
    std::this_thread::sleep_for(std::chrono::seconds(3));
    char buffer[256];
        int cleared = 0;
        while (true) {
            int n = read(fd, buffer, sizeof(buffer) - 1);
            if (n <= 0) break;
            cleared += n;
        }
    if (cleared > 0) {
            std::cout << "🧹 Limpiados " << cleared << " bytes del buffer" << std::endl;
        }
        
        tcflush(fd, TCIOFLUSH);
        std::cout << "🚀 Arduino listo para comandos" << std::endl;
    }

std::string ComunicacionControladorSimple::enviarComando(const std::string& comando, int timeout_ms) {
    if (fd < 0) {
        std::cout << "➡️ SIMULACIÓN: " << comando << std::endl;
        return "SIM:OK";
    }

    // LIMPIAR BUFFER DE ENTRADA ANTES (IMPORTANTE)
    tcflush(fd, TCIFLUSH);

    std::string cmd = comando;
    while (!cmd.empty() && (cmd.back() == '\n' || cmd.back() == '\r')) {
        cmd.pop_back();
    }
    cmd += "\r\n";

    std::cout << "📤 ENVIANDO: '" << comando << "'" << std::endl;
    
    ssize_t w = write(fd, cmd.c_str(), cmd.length());
    if (w < 0) {
        std::cerr << "❌ Error escribiendo: " << strerror(errno) << std::endl;
        return "ERROR:WRITE";
    }
    tcdrain(fd);

    // AUMENTAR TIMEOUT para comandos que toman tiempo
    int actual_timeout = timeout_ms;
    if (comando.find("G28") != std::string::npos ||  // Home puede tomar tiempo
        comando.find("G1") != std::string::npos) {   // Movimientos también
        actual_timeout = 10000; // 10 segundos
    }
    
    return recibirRespuesta(actual_timeout);
}

std::string ComunicacionControladorSimple::recibirRespuesta(int timeout_ms) {
    if (fd < 0) return "SIM:OK";
    
    std::string respuesta;
    char buffer[256];
    auto start_time = std::chrono::steady_clock::now();
    bool response_complete = false;
    bool received_anything = false;
    
    std::cout << "📥 Esperando respuesta..." << std::endl;

    while (true) {
        // Verificar timeout
        auto current_time = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            current_time - start_time).count();
        
        if (elapsed > timeout_ms) {
            std::cout << "⏰ Timeout después de " << elapsed << "ms" << std::endl;
            break;
        }

        // Leer datos disponibles (NO-BLOCKING)
        ssize_t n = read(fd, buffer, sizeof(buffer) - 1);
        if (n > 0) {
            buffer[n] = '\0';
            respuesta += buffer;
            received_anything = true;
            
            std::cout << "📥 Recibido: " << buffer;
            
            // Verificar si tenemos "ok" o "error" (como en el test)
            std::string resp_lower = respuesta;
            std::transform(resp_lower.begin(), resp_lower.end(), resp_lower.begin(), ::tolower);
            
            if (resp_lower.find("ok") != std::string::npos || 
                resp_lower.find("error") != std::string::npos) {
                
                std::cout << "✅ Respuesta completa detectada" << std::endl;
                response_complete = true;
                
                // ESPERAR 500ms EXTRA para datos pendientes (COMO EL TEST)
                auto extra_start = std::chrono::steady_clock::now();
                while (std::chrono::steady_clock::now() - extra_start < std::chrono::milliseconds(500)) {
                    n = read(fd, buffer, sizeof(buffer) - 1);
                    if (n > 0) {
                        buffer[n] = '\0';
                        respuesta += buffer;
                        std::cout << "📥 Extra: " << buffer;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
                break;
            }
        } else if (n == 0) {
            // No hay datos disponibles
            if (response_complete) break;
            
            // Si ya recibimos algo pero no tenemos OK/ERROR, esperar un poco más
            if (received_anything) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        } else {
            // Error de lectura
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                std::cerr << "❌ Error leyendo: " << strerror(errno) << std::endl;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    // Limpiar respuesta (mantener múltiples líneas como en el test)
    if (!respuesta.empty()) {
        // Solo eliminar \r, mantener \n para formato legible
        respuesta.erase(std::remove(respuesta.begin(), respuesta.end(), '\r'), respuesta.end());
        
        // Eliminar espacios extra al final
        while (!respuesta.empty() && 
               (respuesta.back() == ' ' || respuesta.back() == '\n' || respuesta.back() == '\t')) {
            respuesta.pop_back();
        }
    }

    std::cout << "📥 RESPUESTA FINAL (" << respuesta.length() << " chars):" << std::endl;
    std::cout << respuesta << std::endl;
    
    return respuesta.empty() ? "TIMEOUT" : respuesta;
}