TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#==================================================
# build a support library

USR_INCLUDES += -INIVISA/include

LIBRARY_IOC += VISAdrv

ifeq (WIN32,$(OS_CLASS))
NIVISADIR=$(subst /,\,$(ICPBINARYDIR)/NIVISA)
else
NIVISADIR=$(ICPBINARYDIR)/NIVISA
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

