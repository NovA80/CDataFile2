#
# Makefile for CDataFile
# 

CC      = egcs
PROF    = -O -ggdb
LFLAGS  = $(PROF) -rdynamic -lstdc++
CFLAGS  = -Wall $(PROF) -D_XOPEN_SOURCE 
#IPATH   = -I/usr/include/g++-2/
IPATH   =

OFILES =   CDataFile.o DataFileTest.o

all: cdf

cdf:  $(OFILES)
	@echo "-*-- Linking..."
	@$(CC) $(LFLAGS) -o cdfTest $(OFILES) 

.c.o: CDataFile.h
	@echo "Compiling $<..."
	@$(CC) -c $(CFLAGS) $(IPATH)  $<

