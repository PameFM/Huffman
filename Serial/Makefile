# Nombre de los ejecutables
COMPRESS_EXEC = compressSerial
DECOMPRESS_EXEC = decompressSerial

# Compilador y banderas
CC = gcc
CFLAGS = -Wall -Wextra -Wno-format-truncation -O2

# Archivos fuente
COMPRESS_SRC = compressSerial.c
DECOMPRESS_SRC = decompressSerial.c

# Reglas
all: $(COMPRESS_EXEC) $(DECOMPRESS_EXEC)

$(COMPRESS_EXEC): $(COMPRESS_SRC)
	$(CC) $(CFLAGS) -o $(COMPRESS_EXEC) $(COMPRESS_SRC)

$(DECOMPRESS_EXEC): $(DECOMPRESS_SRC)
	$(CC) $(CFLAGS) -o $(DECOMPRESS_EXEC) $(DECOMPRESS_SRC)

clean:
	rm -f $(COMPRESS_EXEC) $(DECOMPRESS_EXEC)

.PHONY: all clean

