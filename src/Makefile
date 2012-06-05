obj-y := sysWatchDef.o
obj-m := kWatch.o

CFLAGS_kWatch.o := -DDEBUG
CC		=	gcc
CCFLAGS		=	-Wall -Werror

KVERSION = $(shell uname -r)

all:
	$(CC) -o uWatch $(CCFLAGS) uWatch.c
	$(CC) -o testscripts/inotifyTest testscripts/inotifyTest.c
	$(CC) -o testscripts/kWatchTest testscripts/kWatchTest.c

clean:
	rm -f *.o
	rm -f *.ko
	rm -f *.mod.c
	rm -f modules.*
	rm -f Module.symvers
	rm -f uWatch
	rm -f testscripts/inotifyTest
	rm -f testscripts/kWatchTest
