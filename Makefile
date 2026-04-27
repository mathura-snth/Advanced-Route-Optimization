CC = gcc
CFLAGS = -Wall -Wextra -O3
LDFLAGS = -lm

OBJ = graph.o tas.o dijkstra.o a_star.o alt.o ch.o moteur_gps.o
EXEC = moteur_gps
all: $(EXEC)

$(EXEC): $(OBJ)
	$(CC) $(OBJ) -o $(EXEC) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o $(EXEC)