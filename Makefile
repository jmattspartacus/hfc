CC = g++

GRROOT_DIR := $(shell if [ -z $(GRROOT_DIR) ]; then echo "../.."; else echo $(GRROOT_DIR); fi)
LIB_DIR = $(shell if [ -z $(GRROOT_LIBDIR) ]; then echo "$(GRROOT_DIR)/lib/lib32"; else echo $(GRROOT_LIBDIR); fi)
BIN_DIR = $(shell if [ -z $(GRROOT_BINDIR) ]; then echo "$(GRROOT_DIR)/bin/bin32"; else echo $(GRROOT_BINDIR); fi)

FLAG =  -O3
MFILE_LIB = 
RUNFILE = GEB_HFC
INSTALLDIR=${HOME}/.local/bin/

INCLUDE =
LIBS = -Wl,--no-as-needed -lz -lbz2

OBJFILES = GEB_HFC.o HFC.o

$(RUNFILE): src/GEB_HFC.cpp $(OBJFILES) 
	$(CC) $(FLAG) $(OBJFILES) -o $(RUNFILE) $(LIBS) 

GEB_HFC.o: src/GEB_HFC.cpp src/HFC.h
	$(CC) $(FLAG) -c $<

HFC.o: src/HFC.cpp src/HFC.h
	$(CC) $(FLAG) -c $<


clean:
	rm -f $(RUNFILE) $(OBJFILES) 

install:
	cp $(RUNFILE) $(INSTALLDIR)$(RUNFILE)
