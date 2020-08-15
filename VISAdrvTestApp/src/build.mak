## @file build.mak Makefile for VISADrvTest

TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

### NOTE: there should only be one build.mak for a given IOC family and this should be located in the ###-IOC-01 directory

#=============================
# Build the IOC application VISAdrvTest
# We actually use $(APPNAME) below so this file can be included by multiple IOCs

PROD_IOC = $(APPNAME)
# VISAdrvTest.dbd will be created and installed
DBD += $(APPNAME).dbd

# VISAdrvTest.dbd will be made up from these files:
$(APPNAME)_DBD += base.dbd
$(APPNAME)_DBD += asyn.dbd
$(APPNAME)_DBD += VISAdrv.dbd
$(APPNAME)_DBD += stream.dbd

# Add all the support libraries needed by this IOC
$(APPNAME)_LIBS += stream VISAdrv asyn
ifdef PCRE
$(APPNAME)_LIBS += pcre
endif

# this links the IOC against the system NI VISA libraries, which is needed for static EPICS builds
include $(TOP)/visa_lib.mak

# VISAdrvTest_registerRecordDeviceDriver.cpp derives from VISAdrvTest.dbd
$(APPNAME)_SRCS += $(APPNAME)_registerRecordDeviceDriver.cpp

# Build the main IOC entry point on workstation OSs.
$(APPNAME)_SRCS_DEFAULT += $(APPNAME)Main.cpp
$(APPNAME)_SRCS_vxWorks += -nil-

# Add support from base/src/vxWorks if needed
#$(APPNAME)_OBJS_vxWorks += $(EPICS_BASE_BIN)/vxComLibrary

# Finally link to the EPICS Base libraries
$(APPNAME)_LIBS += $(EPICS_BASE_IOC_LIBS)

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

