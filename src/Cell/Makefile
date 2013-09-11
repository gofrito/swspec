########################################################################
#  Radioastronomic Spectrometer
#  Computational core for IBM Cell Broadband Engine, SPE core unit
#
#  Uses the IBM Cell SDK 2.x
#
#  (C) 2008 Jan Wagner
#      Licensed according to GNU GPL
########################################################################

# SPU_COMPILER = gcc
PROGRAMS_spu    := spectrometer_spu
LIBRARY_embed64 := spectrometer_spu.a

INCLUDE = -I$(CELL_SDK_ROOT)/usr/spu/include
LDFLAGS = -L$(CELL_SDK_ROOT)/sysroot/usr/spu/lib/ -L/usr/spu/lib/ -lmisc -lgmath -lsimdmath -B -lsimdmath
CPPFLAGS = -g -O3
CFLAGS = -g -O3

########################################################################
# note: for libmisc.a in SDK2.0 you will need to run
#       a 'make' inside /opt/ibm/cell-sdk/prototype/src/lib/misc/
#       to first compile that library...
########################################################################

include $(CELL_SDK_ROOT)/make.footer
