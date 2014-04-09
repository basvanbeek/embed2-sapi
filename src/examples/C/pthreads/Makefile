SRCS=embed2_test.cpp
OBJS=embed2_test.o
CC=gcc
CPPFLAGS=`php-config --includes`
LDFLAGS=-lpthread -lphp5 `php-config --libs`
all:	embed2_test

$(SRCS):
	$(CC) $(CPPFLAGS) -c $(SRCS)

embed2_test: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS)

clean:
	-rm $(BASEDIR)*.o embed2_test

