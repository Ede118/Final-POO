CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -pthread
LIBS = -lsqlite3
TARGET = servidor

SRCS = servidor.cpp login.cpp administrador_sistema.cpp robot_controller_simple.cpp comunicacion_controlador_simple.cpp

$(TARGET): $(SRCS) json.hpp
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRCS) $(LIBS)

clean:
	rm -f $(TARGET)

.PHONY: clean