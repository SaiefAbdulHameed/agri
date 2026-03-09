CC = gcc
CFLAGS = -Wall -Wextra -Iinclude
LIBS = -lsqlite3 -lssl -lcrypto -lcurl

SRC = src/main.c  src/auth.c src/utils.c \
      src/soil_data.c src/fertilizer.c 

OUT = agri_server

all:
	$(CC) $(CFLAGS) $(SRC) -o $(OUT) $(LIBS)

run:
	./$(OUT)

clean:
	rm -f $(OUT)
