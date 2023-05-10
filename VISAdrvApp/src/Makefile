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
# possible other location of an installation if no local one
ifeq ($(wildcard $(NIVISADIR)/*),)
NIVISADIR=C:\Program Files\IVI Foundation\VISA\Win64
endif
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
ifneq ($(findstring windows-x64-mingw,$(EPICS_HOST_ARCH)),)
VISALIB = NIVISA/Lib_x64/mingw
LIB_INSTALLS_WIN32 += $(VISALIB)/libvisa64.dll.a
VISAdrv_SYS_LIBS_WIN32 += visa64
VISAdrv_LDFLAGS_WIN32 += -L$(VISALIB)/mingw
else
VISALIB = NIVISA/Lib_x64/msc
LIB_INSTALLS_WIN32 += $(VISALIB)/visa64.lib
VISAdrv_SYS_LIBS_WIN32 += $(VISALIB)/visa64
#BIN_INSTALLS_WIN32 += $(NIVISADIR)/Bin_x64/*.dll
endif
endif
ifneq ($(findstring win32,$(EPICS_HOST_ARCH)),)
VISALIB = NIVISA/lib/msc
LIB_INSTALLS_WIN32 += $(VISALIB)/visa32.lib
VISAdrv_SYS_LIBS_WIN32 += $(VISALIB)/visa32
#BIN_INSTALLS_WIN32 += $(NIVISADIR)/Bin/*.dll
endif
ifneq ($(findstring linux,$(EPICS_HOST_ARCH)),)
## set path to visa on Linux in visa_lib.mak
APPNAME=VISAdrv
include $(TOP)/visa_lib.mak
endif

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

ifdef T_A

# we need to make a copy of system visa files due to spaces in the absolute path
# also in mingw we need to create a .dll.a import library for visa64.dll
# as we cannot use the .lib supplied for visual studio

../drvAsynVISAPort.cpp : NIVISA

NIVISA :
	-mkdir NIVISA
ifneq ($(findstring windows-x64-mingw,$(EPICS_HOST_ARCH)),)
	cp -r "$(NIVISADIR)\Include" "$@\include"
	gendef /c/Windows/System32/visa64.dll
	-mkdir $@/Lib_x64
	-mkdir $@/Lib_x64/mingw
	dlltool -k --output-lib $@/Lib_x64/mingw/libvisa64.dll.a --def visa64.def
else
ifeq (WIN32,$(OS_CLASS))
	xcopy /D /I /E /Q /Y /R "$(NIVISADIR)\Include" "$@\include"
	xcopy /D /I /E /Q /Y /R "$(NIVISADIR)\Lib_x64" "$@\Lib_x64"
	xcopy /D /I /E /Q /Y /R "$(NIVISADIR)\Lib" "$@\Lib"
endif
#ifeq (Linux,$(OS_CLASS))
#	cp -r $(NIVISA)/include $@
#	cp -r $(NIVISA)/extlib $@
#endif
endif

endif
