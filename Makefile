CC = gcc
FLAGS = -Wall
RM = rm -vrf
SRC = $(filter-out netfileserver.c, $(wildcard *.c))
OBJ = $(patsubst %.c, %.o, $(SRC))
PROG = $(patsubst %.c, %, $(SRC))

all: index

index:
	$(CC) $(FLAGS) $(SRC) -o netclient

clean:
	$(RM) $(OBJ)
	$(RM) $(PROG)

tar:
	tar -czvf asst3.tgz asst3

server:
	gcc netfileserver.c -pthread 