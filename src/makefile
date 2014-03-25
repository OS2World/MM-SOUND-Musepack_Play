#
#  mpcplay.dll makefile for gnu MAKE and g++
#

# build environment
#-----------------------
#  gcc -v
# 
# Reading specs from J:/usr/gcc/3.3.5-csd1/lib/gcc-lib/i386-pc-os2-emx/3.3.5/specs
# Configured with: D:/CODING/LIBC/0.6/src/gcc/configure --enable-clh 
# --enable-threads=os2 --enable-shared=libgcc,bfd,opcodes --enable-nls 
# --without-included-gettext --with-local-prefix=D:/CODING/LIBC/0.6/TOOLS/x86.os2/gcc/staged 
# --prefix=/gcc --with-gnu-as --disable-libgcj --enable-languages=c,c++
# Thread model: os2
# gcc version 3.3.5 (Bird Build 2006-03-18 05:37)
# 
#  make -v
# GNU Make version 3.77, by Richard Stallman and Roland McGrath.
# Copyright (C) 1988, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98
# 	Free Software Foundation, Inc.
# This is free software; see the source for copying conditions.
# There is NO warranty; not even for MERCHANTABILITY or FITNESS FOR A
# PARTICULAR PURPOSE.
# 
# Report bugs to <bug-make@gnu.org>.
# 
#

TARGET = ../mpcplay

CC = g++ -c
LD = g++
RM = del

MPCDECLIB = ../../libmpcdec-1.2.2/src

CFLAGS   = -Zmts -O3
INCLUDE  = -I./include -I./mpcdec
LIB      =

all: sample.exe $(TARGET).dll
	cp $(TARGET).dll J:/Progs/MM/PM123_/
	cd J:/Progs/MM/PM123_/ && pwd && ./pm123 2> r

DEF      = mpcplay.def
OBJECTS  = mpcplay.o id3tag.o wav.o
LIBS     = $(MPCDECLIB)/.libs/mpcdec.a 

$(TARGET).dll : $(OBJECTS) $(DEF)
	$(LD) -Zdll -Zmts $(CFLAGS) -o $(TARGET).dll $(OBJECTS) $(LIBS) $(DEF)

clean:
	-@$(RM) ..\*.dll $(OBJECTS) *.bak ..\*.bak 2> nul

mpcplay.o : mpcplay.cpp wav.h
	$(CC) $(CFLAGS) $(INCLUDE) -o $@ mpcplay.cpp

id3tag.o : id3tag.cpp wav.h
	$(CC) $(CFLAGS) $(INCLUDE) -o $@ id3tag.cpp

wav.o : wav.cpp wav.h
	$(CC) $(CFLAGS) $(INCLUDE) -o $@ wav.cpp

sample.o : sample.cpp
	$(CC) $(CFLAGS) $(INCLUDE) -o $@ sample.cpp

sample.exe : sample.o
	$(LD) -o $@ $(CFLAGS) sample.o $(LIBS)
	