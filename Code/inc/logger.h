#ifndef LOGGER_H
#define LOGGER_H

#include <string>

class Logger {
public:
    explicit Logger(const std::string& path = "logs/server_log.csv");
    ~Logger();

    // Registra una petición HTTP
    // detail: texto con método y ruta
    // user: usuario que realizó la petición (o "-" si desconocido)
    // node: IP/host origen
    // response_code: código HTTP de respuesta
    void logRequest(const std::string& detail, const std::string& user,
                    const std::string& node, int response_code);

    // Registra un evento general
    // module: módulo que genera el evento (ej. "server", "rpc", "auth")
    // message: detalle del evento
    void logEvent(const std::string& module, const std::string& message);

private:
    std::string filepath;
    void ensureLogFile();
};

// Instancia global usable desde los distintos módulos
extern Logger logger;

#endif // LOGGER_H
