CC = gcc
CFLAGS = -Wall -O3 -Ien-tetes
LDFLAGS = -lm

# dossiers
SRC_DIR = sources
INC_DIR = en-tetes
BUILD_DIR = compilation

# pour tous les .c dans sources/ on définit le nom des .o correspondants
SRC = $(wildcard $(SRC_DIR)/*.c)
OBJ = $(BUILD_DIR)/graph.o \
      $(BUILD_DIR)/tas.o \
      $(BUILD_DIR)/dijkstra.o \
      $(BUILD_DIR)/a_star.o \
      $(BUILD_DIR)/alt.o \
      $(BUILD_DIR)/ch.o \
      $(BUILD_DIR)/analyzer.o \
      $(BUILD_DIR)/main.o

EXEC = moteur_gps

all: directories $(EXEC)

directories:
	@mkdir -p $(BUILD_DIR)

# liens
$(EXEC): $(OBJ)
	$(CC) $(OBJ) -o $(EXEC) $(LDFLAGS)

# compilation : pour faire bcompilationuild/bidule.o, on compile sources/bidule.c
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# nettoyage
clean:
	rm -rf $(BUILD_DIR) $(EXEC)

run: all
	./moteur_gps
	python3 scripts/analyze_results.py
	gnuplot scripts/generate_graphs.p