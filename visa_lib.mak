## link visa to an IOC
## assumes $(APPNAME) already defined

ifneq ($(findstring windows,$(EPICS_HOST_ARCH)),)
$(APPNAME)_LIBS += visa64
endif
ifneq ($(findstring win32,$(EPICS_HOST_ARCH)),)
$(APPNAME)_LIBS += visa32
endif
ifneq ($(findstring linux,$(EPICS_HOST_ARCH)),)
## Linux: location of National Instruments libvisa.so library
$(APPNAME)_LDFLAGS += -L/usr/lib/x86_64-linux-gnu
$(APPNAME)_SYS_LIBS_Linux += visa
endif
