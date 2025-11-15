#pragma once
#ifndef LOGIN_H
#define LOGIN_H

#include <sqlite3.h>
#include <string>
#include <random>
#include <iostream>

class Login {
private:
    sqlite3* db = nullptr;

    std::string generateToken() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(1000, 9999);
        return std::to_string(dis(gen));
    }

public:
    struct AuthResult {
        bool success;
        std::string privilege;
        std::string token;
        std::string message;
    };

    Login();
    ~Login();

    bool isConnected() const;
    AuthResult authenticate(const std::string& username, const std::string& password);
};

#endif
