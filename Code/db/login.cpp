#include <sqlite3.h>
#include <iostream>
#include <string>
#include <optional>

static void check_rc(int rc, sqlite3* db, const char* msg) {
    if (rc != SQLITE_OK) {
        std::cerr << msg << ": " << (db ? sqlite3_errmsg(db) : "(no db)") << "\n";
        exit(1);
    }
}

bool initialize_db(sqlite3* db) {
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
        std::cerr << "Error creando tabla users: " << (err?err:"") << "\n";
        sqlite3_free(err);
        return false;
    }

    // Insertar usuarios por defecto si la tabla está vacía
    const char* count_sql = "SELECT COUNT(*) FROM users;";
    sqlite3_stmt* stmt = nullptr;
    rc = sqlite3_prepare_v2(db, count_sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Error preparando COUNT stmt: " << sqlite3_errmsg(db) << "\n";
        return false;
    }
    rc = sqlite3_step(stmt);
    int count = 0;
    if (rc == SQLITE_ROW) {
        count = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);

    if (count == 0) {
        const char* insert_sql =
            "INSERT INTO users (username, password_hash, privilege) VALUES"
            "('ADMIN','ADMIN','admin'),"
            "('USER','USER','user'),"
            "('VIEWER','VIEWER','viewer');";
        rc = sqlite3_exec(db, insert_sql, nullptr, nullptr, &err);
        if (rc != SQLITE_OK) {
            std::cerr << "Error insertando usuarios por defecto: " << (err?err:"") << "\n";
            sqlite3_free(err);
            return false;
        }
    }
    return true;
}

std::optional<std::string> try_login(sqlite3* db, const std::string& username, const std::string& password) {
    // Usamos UPPER(username) para permitir coincidencia case-insensitive en el nombre.
    const char* select_sql = "SELECT privilege FROM users WHERE UPPER(username)=UPPER(?) AND password_hash=? LIMIT 1;";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, select_sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Error preparando select: " << sqlite3_errmsg(db) << "\n";
        return std::nullopt;
    }
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, password.c_str(), -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        const unsigned char* priv = sqlite3_column_text(stmt, 0);
        std::string privilege = priv ? reinterpret_cast<const char*>(priv) : "viewer";
        sqlite3_finalize(stmt);
        return privilege;
    }
    sqlite3_finalize(stmt);
    return std::nullopt;
}

int main(int argc, char** argv) {
    const char* dbpath = "db/users.sqlite3";
    sqlite3* db = nullptr;
    int rc = sqlite3_open(dbpath, &db);
    if (rc != SQLITE_OK) {
        std::cerr << "No se puede abrir la base de datos: " << sqlite3_errmsg(db) << "\n";
        return 1;
    }

    if (!initialize_db(db)) {
        sqlite3_close(db);
        return 1;
    }

    std::string username, password;
    if (argc >= 3) {
        username = argv[1];
        password = argv[2];
    } else {
        std::cout << "Usuario: ";
        if (!std::getline(std::cin, username)) {
            sqlite3_close(db);
            return 1;
        }
        std::cout << "Contraseña: ";
        if (!std::getline(std::cin, password)) {
            sqlite3_close(db);
            return 1;
        }
    }

    auto res = try_login(db, username, password);
    if (res) {
        std::cout << "LOGIN_SUCCESS:" << *res << "\n";
        sqlite3_close(db);
        return 0;
    } else {
        std::cout << "LOGIN_FAILED" << "\n";
        sqlite3_close(db);
        return 2;
    }
}
