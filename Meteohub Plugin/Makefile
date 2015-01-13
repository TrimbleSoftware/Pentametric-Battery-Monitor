# make file for meteohub/meteoplug plig-in for Pentametric serial interface
# for both meteohub on Iomega iConnect hardware running Debian 4 Linux AND meteoplug on TP-LINK MR3020 hardware running OpenWRT Linux
# FTRIMBLE 2-Feb-2013

CC = $(CC)
LD = $(LD)
CFLAGS = $(CFLAGS)
LDFLAGS = $(LDFLAGS)

# get machine name from OS
UNAME := $(shell uname -m)

# OpenWRT centric make file settings for TP-LINK MR-3020 Meteoplug Pentrametric plug-in
# requires OpenWRT SDK to be installed/built on the host OS, in this case debian 6 running in a virtualbox VM on Win7 host OS.
# Assumes that the OpenWRT gcc MIPS crosscompiler/linker is located on the PATH

# gcc settings for OpenWRT MIPS gcc crosscompiler/linker running on VMBox with Debian 6
ifeq ($(UNAME),i686)
	# override CC and LD with OpenWRT MIPS specific cross complier
	CC	= mips-openwrt-linux-uclibc-gcc
	LD	= mips-openwrt-linux-uclibc-gcc
	# override CFLAGS and LDFLAGS to use OpenWRT includes and libs
	CFLAGS  = -s -Wall -O2 -U DEBUG -I /home/meteoplug/openwrt/trunk/staging_dir/toolchain-mips_r2_gcc-4.6-linaro_uClibc-0.9.33.2/usr/include
	LDFLAGS = -s -L /home/meteoplug/openwrt/trunk/staging_dir/toolchain-mips_r2_gcc-4.6-linaro_uClibc-0.9.33.2/usr/lib
endif

# gcc settings for gcc running on Debian 4 in meteohub iConnect kernel
ifeq ($(UNAME),armv5tel)
	# user standard gcc
	CC	= gcc 
	LD	= gcc
	# c and linker flags
	CFLAGS  = -Wall -O2 -U DEBUG
	LDFLAGS = -s 
endif

#gcc settings for gcc running on raspbery pi meteohub kernel
ifeq ($(UNAME),armv6l)
	# user standard gcc
	CC 	= gcc
	LD 	= gcc
	# c and linker flags
	CFLAGS	= -Wall -O2 -U DEBUG
	LDFLAGS	= -s -lm
endif

all:	mhpmpi

debug: clean debug_compile mhpmpi

mhpmpi:	mhpmpi.o config.o
	$(LD) $(LDFLAGS) mhpmpi.o config.o -o mhpmpi

static:	mhpmpi.o config.o
	$(LD) $(LDFLAGS) -static -o mhpmpi mhpmpi.o

debug_compile:	config.c mhpmpi.c mhpmpi.h
	$(CC) $(CFLAGS) -g3 -D DEBUG -c mhpmpi.c -c config.c

mhpmpi.o:	config.c mhpmpi.c mhpmpi.h
	$(CC) $(CFLAGS) -c mhpmpi.c -o mhpmpi.o

config.o:	config.c mhpmpi.h
	$(CC) $(CFLAGS) -c config.c -o config.o	

clean:
	rm -rf mhpmpi *.o *~