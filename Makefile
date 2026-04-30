CC = gcc
CFLAGS = -Wall -Wextra -O3 -Iinclude
LDFLAGS = -lm

# dossiers
SRC_DIR = src
INC_DIR = include
BUILD_DIR = build
BIN_DIR = bin

# pour tous les .c dans src/ on définit le nom des .o correspondants
SRC = $(wildcard $(SRC_DIR)/*.c)
OBJ = $(BUILD_DIR)/graph.o \
      $(BUILD_DIR)/tas.o \
      $(BUILD_DIR)/dijkstra.o \
      $(BUILD_DIR)/a_star.o \
      $(BUILD_DIR)/alt.o \
      $(BUILD_DIR)/ch.o \
      $(BUILD_DIR)/analyzer.o \
      $(BUILD_DIR)/main.o

EXEC = $(BIN_DIR)/moteur_gps

all: directories $(EXEC)
directories:
	@mkdir -p $(BUILD_DIR) $(BIN_DIR)

# liens
$(EXEC): $(OBJ)
	$(CC) $(OBJ) -o $(EXEC) $(LDFLAGS)

# compilation : pour faire build/bidule.o, on compile src/bidule.c
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# nettoyage
clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)