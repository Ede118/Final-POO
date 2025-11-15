# ---------- Configuración básica ----------
CXX      := g++
INCDIR   := inc
SRCDIR   := src
OBJDIR   := build
BIN_DIR  := bin
TARGET   := $(BIN_DIR)/servidor

CXXFLAGS := -std=c++17 -Wall -Wextra -g -I$(INCDIR)
LDFLAGS  := -lsqlite3

# ---------- Fuentes y objetos (automático por carpeta) ----------
# Todos los .cpp que haya en src/
SRCS := $(wildcard $(SRCDIR)/*.cpp)

# Por cada src/xxx.cpp -> build/xxx.o
OBJS := $(SRCS:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)

# ---------- Reglas principales ----------
all: $(TARGET)

$(TARGET): $(OBJS) | $(BIN_DIR)
	$(CXX) $(OBJS) $(LDFLAGS) -o $@

# Compilar cada .cpp a .o (en build/)
$(OBJDIR)/%.o: $(SRCDIR)/%.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Crear directorios si no existen
$(OBJDIR):
	mkdir -p $(OBJDIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# ---------- Limpieza ----------
.PHONY: clean
clean:
	rm -rf $(OBJDIR) $(BIN_DIR)
