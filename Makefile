all:
		cd mark5access; ./build.sh
		cd src; make

install:
		cd mark5access; make install
		cd src; make install
clean:
		rm src/*.o src/swspectrometer src/intel_swspectrometer
		rm src/IA-32/*.o 
                

