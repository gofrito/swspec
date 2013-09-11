CFLAGS = -g -O0 -Wall -pthread -DHAVE_MK5ACCESS=1 -I../mark5access/
CXXFLAGS = $(CFLAGS)
LDFLAGS = -L../mark5access/mark5access/.libs/ -Wl,-Bstatic -lmark5access -Wl,-Bdynamic

all: mk5access_testcase debug_MkIV

debug_MkIV: debug_MkIV.cpp
	g++ -c $(CFLAGS) debug_MkIV.cpp
	g++ debug_MkIV.o $(LDFLAGS) -o debug_MkIV

mk5access_testcase: mk5access_testcase.c
	g++ -c $(CFLAGS) mk5access_testcase.c
	g++ mk5access_testcase.o $(LDFLAGS) -o mk5access_testcase

clean:
	rm -f *.o
	rm -f mk5access_testcase
	rm -f debug_MkIV
