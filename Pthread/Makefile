# Makefile 

CC       := gcc
CFLAGS   := -std=c11 -D_GNU_SOURCE -D_POSIX_C_SOURCE=199309L -O2
LDFLAGS  := -lpthread

BINDIR   := pthread
COMPBIN  := $(BINDIR)/compress_all
DECBIN   := $(BINDIR)/decompress_all

COMPSRC  := $(BINDIR)/compress_pthread_all.c
DECSRC   := $(BINDIR)/decompress_pthread_all.c

.PHONY: all pall prun pdecomp pclean

all: pall pdecomp

pall: $(COMPBIN) $(DECBIN)

$(COMPBIN): $(COMPSRC)
	@echo "🔨 pthread: compilar compresión → $@"
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

$(DECBIN): $(DECSRC)
	@echo "🔨 pthread: compilar descompresión → $@"
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

prun: $(COMPBIN)
	@echo "🚀 pthread: compresión…"
	mkdir -p salida
	$(COMPBIN) pthread/parte1/*.txt pthread/parte2/*.txt

pdecomp: $(DECBIN)
	@echo "🚀 pthread: descompresión…"
	mkdir -p salida/descomp
	$(DECBIN)

pclean:
	@echo "🧹 pthread: limpiando…"
	rm -f $(COMPBIN) $(DECBIN)
	rm -rf salida
