BOOST_COMPILER=gcc41
BASEDIR=../
SRCS=embed2_cpp_test.cpp
OBJS=embed2_cpp_test.o
CC=g++
CPPFLAGS=`php-config --includes`
LDFLAGS=-lphp5 `php-config --libs` -lboost_thread-$(BOOST_COMPILER)-mt
all:	embed2_cpp_test

$(SRCS):
	$(CC) $(CPPFLAGS) -c $(BASEDIR)$(SRCS)

embed2_cpp_test: $(BASEDIR)$(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(BASEDIR)$(OBJS)

clean:
	-rm $(BASEDIR)*.o embed2_cpp_test

