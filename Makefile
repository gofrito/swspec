all:
		cd mark5access; ./build.sh
		cd src; make

install:
		cd mark5access; make install
		cd src; make install
clean:
		cd src; rm *.o swspectrometer intel_swspectrometer

