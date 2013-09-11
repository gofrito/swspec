CC = g++
CFLAGS = -g -O3 -Wall -pthread -DHAVE_MK5ACCESS=1 -I../mark5access/

BASEFILES = swspectrometer.cpp TaskDispatcher.cpp FileSource.cpp FileSink.cpp TeeSink.cpp Buffer.cpp Helpers.cpp \
   DataSource.cpp DataSink.cpp VSIBSource.cpp IniParser.cpp LogFile.cpp IA-32/TaskCoreIPP.cpp IA-32/DataUnpackers.cpp IA-32/PhaseCal/PCal.cpp

# ##### ADD PLPLOT CAPABILITY(?)
FLAG_HAVE_PLPLOT =    # leave blank to not include PlPlot
ifeq ($(FLAG_HAVE_PLPLOT),1)
   BASEFILES := $(BASEFILES) PlplotSink.cpp
   CFLAGS := $(CFLAGS) -DHAVE_PLPLOT=1
endif

BASEOBJS=$(BASEFILES:.cpp=.o)
BUILD_NUMBER_FILE=build-number.txt

# ##### PLATFORM SELECT (edit manually...)

all: intel
clean: intel_clean

# ##### INTEL/AMD MAKE
IPP_PATH=/opt/intel/ipp/6.1.5.061/em64t

intel_CFLAGS  = $(CFLAGS) -DINTEL_IPP=1 -I${IPP_PATH}/include/ -I. -I./IA-32/
intel_LDFLAGS = -L${IPP_PATH}/sharedlib/ -L${IPP_PATH}/lib/ -L../mark5access/mark5access/.libs/ -Wl,-rpath,${IPP_PATH}/sharedlib/

LBITS := $(shell getconf LONG_BIT)
ifeq ($(LBITS),64)
   intel_LDINCL  =  -Wl,-Bdynamic -lguide -lippsem64t -lippvmem64t -lippcoreem64t -lX11 -lm -pthread
   intel_LDINCL_S  = -static -lippsmergedem64t -lippcoreem64t -lippacemergedem64t -lippccemergedem64t -lippchemergedem64t -lippcvemergedem64t \
       -lippdcemergedem64t -lippdiemergedem64t -lippgenemergedem64t -lippiemergedem64t -lippjemergedem64t -lippmemergedem64t \
       -lippremergedem64t -lippscemergedem64t -lippsemergedem64t -lippsremergedem64t -lippvcemergedem64t -lippvmemergedem64t
else
   intel_LDINCL  =  -Wl,-Bdynamic -lguide -lipps -lippcore -lX11 -lm -pthread
endif

mk5access_LDFLAGS = -Wl,-Bstatic -lmark5access -Wl,-Bdynamic

intel: $(BASEFILES) intel_swspectrometer
intel_clean:
	rm -f ${BASEOBJS} swspectrometer intel_swspectrometer

intel_swspectrometer: $(BASEOBJS) $(BUILD_NUMBER_FILE)
	$(CC) -g -O3 $(BASEOBJS) $(intel_LDFLAGS) $(mk5access_LDFLAGS) $(intel_LDINCL) $(BUILD_NUMBER_LDFLAGS) -o intel_swspectrometer
	cp intel_swspectrometer swspectrometer

.cpp.o:
	$(CC) $(intel_CFLAGS) -c $< -o $@

# ##### IBM CELL MAKE

cell:
	cd Cell; make; cd ..
	ppu-c++ -m64 -I. -I./Cell/ -DIBM_CELL=1 ${BASEFILES} Cell/TaskCoreCell.cpp -static -lspe -lpthread -o swspectrometer -L./Cell/ ./Cell/spectrometer_spu.a

cell_clean:
	cd Cell; make clean; cd ..
	rm -rf *.o
	rm -rf swspectrometer
	rm -rf *.*~

# ##### GENERIC

install: swspectrometer
	cp swspectrometer /usr/local/bin/

# ##### BUILD NUMBER

include Buildnumber.inc
