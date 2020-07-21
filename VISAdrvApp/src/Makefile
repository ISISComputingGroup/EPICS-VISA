TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#==================================================

# build a support library
LIBRARY_IOC += VISAdrv

ifeq (WIN32,$(OS_CLASS))
# set NIVISADIR to the location, if on windows, of the National Instruments VISA library
# the subst make sure it uses a windows style rather than uxix style path
# At the ISIS facility this library is in a subdirectory of the ICPBINARYDIR common library area
NIVISADIR=$(subst /,\,$(ICPBINARYDIR)/NIVISA)
# possible other location of a local installation
#NIVISADIR=C:\Program Files\IVI Foundation\VISA\Win64
USR_INCLUDES += -INIVISA/include
else
## Linux: the National Instruments visa.h header file location
USR_INCLUDES += -I/usr/include/ni-visa
endif

DBD += VISAdrv.dbd

# specify all source files to be compiled and added to the library
VISAdrv_SRCS += drvAsynVISAPort.cpp

VISAdrv_LIBS += asyn
VISAdrv_LIBS += $(EPICS_BASE_IOC_LIBS)

## we don't install Visa DLLs as they may conflict with local ones
ifneq ($(findstring windows,$(EPICS_HOST_ARCH)),)
VISALIB = NIVISA/Lib_x64/msc
LIB_INSTALLS_WIN32 += $(VISALIB)/visa64.lib
VISAdrv_SYS_LIBS_WIN32 += $(VISALIB)/visa64
#BIN_INSTALLS_WIN32 += $(NIVISADIR)/Bin_x64/*.dll
endif
ifneq ($(findstring win32,$(EPICS_HOST_ARCH)),)
VISALIB = NIVISA/lib/msc
LIB_INSTALLS_WIN32 += $(VISALIB)/visa32.lib
VISAdrv_SYS_LIBS_WIN32 += $(VISALIB)/visa32
#BIN_INSTALLS_WIN32 += $(NIVISADIR)/Bin/*.dll
endif
ifneq ($(findstring linux,$(EPICS_HOST_ARCH)),)
## Linux: location of National Instruments libvisa.so library
VISAdrv_LDFLAGS += -L/usr/lib/x86_64-linux-gnu
VISAdrv_SYS_LIBS_Linux += visa
endif

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

# we ned to make a copy due to spaces in the absolute path
../drvAsynVISAPort.cpp : NIVISA
NIVISA :
	-mkdir NIVISA
ifeq (WIN32,$(OS_CLASS))
	xcopy /D /I /E /Q /Y /R $(NIVISADIR)\Include $@\include
	xcopy /D /I /E /Q /Y /R $(NIVISADIR)\Lib_x64 $@\Lib_x64
	xcopy /D /I /E /Q /Y /R $(NIVISADIR)\Lib $@\Lib
endif
#ifeq (Linux,$(OS_CLASS))
#	cp -r $(NIVISA)/include $@
#	cp -r $(NIVISA)/extlib $@
#endif

