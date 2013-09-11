BASEFILES = PCal.cpp #test.cpp

BASEOBJS=$(BASEFILES:.cpp=.o)
CC = g++
CFLAGS = -g -O3 -c -Wall -pthread -funroll-all-loops -DUNIT_TEST=1

all: pcal
clean:
	rm -f ${BASEOBJS} pcal

LBITS := $(shell getconf LONG_BIT)
ifeq ($(LBITS),64)
	intel_LDINCL = -lguide -lm -lippsem64t -B -lguide -lippcoreem64t -pthread
else
	intel_LDINCL = -lm -lippcore -lipps -lguide -pthread
endif
intel_CFLAGS   = -msse3 $(CFLAGS) -I${IPP_PATH}/include/ -I.
intel_LDFLAGS  = -L${IPP_PATH}/sharedlib/ -L${IPP_PATH}/lib/

pcal: $(BASEOBJS)
	$(CC) $(intel_LDFLAGS) $(intel_LDINCL) $(BASEOBJS) -o pcal

.cpp.o:
	$(CC) $(intel_CFLAGS) $< -o $@

zip:
	mkdir ./pcal_extractor_v10/
	cp Makefile $(BASEFILES) PCal.h ./pcal_extractor_v10/
	zip -9r pcal_extractor_v10.zip ./pcal_extractor_v10
	rm -rf ./pcal_extractor_v10
