#include "login.h"
#include "logger.h"
#include <sqlite3.h>
#include <iostream>
#include <string>
// storage for tokens
struct TokenInfo {
    std::string username;
    std::string privilege;
};
static std::unordered_map<std::string, TokenInfo> tokens;
static std::mutex token_mtx;

Login::Login() {
    std::cout << "🔌 Conectando a base de datos SQLite..." << std::endl;
    if (sqlite3_open("users.sqlite3", &db) != SQLITE_OK) {
        std::cerr << "❌ Error abriendo base SQLite: " << sqlite3_errmsg(db) << std::endl;
        db = nullptr;
    } else {
        std::cout << "✅ Conectado a SQLite correctamente" << std::endl;
        
        // Crear tabla si no existe
        const char* create_sql =
            "CREATE TABLE IF NOT EXISTS users ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "username TEXT NOT NULL UNIQUE,"
            "password_hash TEXT NOT NULL,"
            "privilege TEXT NOT NULL DEFAULT 'viewer'"
            ");";
        char* err = nullptr;
        int rc = sqlite3_exec(db, create_sql, nullptr, nullptr, &err);
        if (rc != SQLITE_OK) {
            std::cerr << "❌ Error creando tabla users: " << (err?err:"") << std::endl;
            sqlite3_free(err);
        } else {
            std::cout << "✅ Tabla 'users' verificada/creada" << std::endl;
        }

        // Insertar usuarios por defecto si la tabla está vacía
        const char* count_sql = "SELECT COUNT(*) FROM users;";
        sqlite3_stmt* stmt = nullptr;
        rc = sqlite3_prepare_v2(db, count_sql, -1, &stmt, nullptr);
        if (rc == SQLITE_OK) {
            rc = sqlite3_step(stmt);
            int count = 0;
            if (rc == SQLITE_ROW) {
                count = sqlite3_column_int(stmt, 0);
            }
            sqlite3_finalize(stmt);

            if (count == 0) {
                std::cout << "👥 Creando usuarios por defecto..." << std::endl;
                const char* insert_sql =
                    "INSERT INTO users (username, password_hash, privilege) VALUES"
                    "('ADMIN','ADMIN','admin'),"
                    "('USER','USER','user'),"
                    "('VIEWER','VIEWER','viewer');";
                rc = sqlite3_exec(db, insert_sql, nullptr, nullptr, &err);
                if (rc != SQLITE_OK) {
                    std::cerr << "❌ Error insertando usuarios por defecto: " << (err?err:"") << std::endl;
                    sqlite3_free(err);
                } else {
                    std::cout << "✅ Usuarios por defecto creados (ADMIN/USER/VIEWER)" << std::endl;
                }
            } else {
                std::cout << "📊 Usuarios en BD: " << count << std::endl;
            }
        }
    }
}

Login::~Login() {
    if (db) {
        std::cout << "🔌 Cerrando conexión a BD..." << std::endl;
        sqlite3_close(db);
    }
}

bool Login::isConnected() const { 
    return db != nullptr; 
}

Login::AuthResult Login::authenticate(const std::string& username, const std::string& password) {
    AuthResult result{false, "viewer", "", "Credenciales inválidas"};
    
    if (!db) {
        result.message = "Error de conexión a la base de datos";
        std::cerr << "❌ Intento de login sin conexión a BD" << std::endl;
        return result;
    }

    std::cout << "🔐 Autenticando usuario: '" << username << "' con password: '" << password << "'" << std::endl;

    // CONSULTA CORREGIDA - comparación exacta (sin UPPER)
    const char* sql = "SELECT privilege FROM users WHERE username = ? AND password_hash = ? LIMIT 1;";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        result.message = sqlite3_errmsg(db);
        std::cerr << "❌ Error preparando consulta: " << result.message << std::endl;
        return result;
    }

    // Bind de parámetros - CORREGIDO
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, password.c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    std::cout << "📊 Resultado SQLite: " << rc << " (SQLITE_ROW=" << SQLITE_ROW << ")" << std::endl;
    
    if (rc == SQLITE_ROW) {
        // USUARIO VÁLIDO - existe en la base de datos
        const char* privilege = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        result.success = true;
        result.privilege = privilege ? privilege : "viewer";
        result.token = generateToken();
        // Guardar token activo -> usuario
        {
            std::lock_guard<std::mutex> l(token_mtx);
            tokens[result.token] = TokenInfo{username, result.privilege};
        }
        result.message = "Login exitoso";
        std::cout << "✅ Login exitoso - Usuario: " << username << ", Privilegio: " << result.privilege << std::endl;
        logger.logEvent("auth", std::string("Login exitoso usuario:") + username + std::string(" privilegio:") + result.privilege);
    } else {
        // USUARIO INVÁLIDO - no existe o credenciales incorrectas
        std::cout << "❌ Login fallido - Usuario: " << username << " no encontrado o password incorrecto" << std::endl;
        result.success = false;
        result.message = "Usuario o contraseña incorrectos";
        logger.logEvent("auth", std::string("Login fallido usuario:") + username);
    }

    sqlite3_finalize(stmt);
    return result;
}

std::string Login::usernameForToken(const std::string& token) {
    std::lock_guard<std::mutex> l(token_mtx);
    auto it = tokens.find(token);
    if (it != tokens.end()) return it->second.username;
    return std::string();
}

std::string Login::privilegeForToken(const std::string& token) {
    std::lock_guard<std::mutex> l(token_mtx);
    auto it = tokens.find(token);
    if (it != tokens.end()) return it->second.privilege;
    return std::string();
}
