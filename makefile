CC=clang
CFLAGS= -Wall -g -c
AR= ar
ARFLAGS= -cvrs
AFILES= libmythreads.a
CFILES= libmythreads.c
OFILES= libmythreads.o
HEADERS= mythreads.h

default: libmythreads.c
	$(CC) $(CFLAGS) $(CFILES)
	$(AR) $(ARFLAGS) $(AFILES) $(OFILES)

clean:
	rm $(AFILES) $(OFILES)

tar:
	tar -czvf project2.tgz $(CFILES) $(HEADERS) makefile README