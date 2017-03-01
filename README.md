# VISAdrv - EPICS asyn driver interface to National Instruments VISA library

The module creates an equivalent to the drvAsynSerialPortConfigure() command called drvAsynVISAPortConfigure()
which can be used to connect to serial devices served by National Instruments VISA. If the device is a
serial device then asynSetOption() etc. can be used as normal. You can also use drvAsynVISAPortConfigure() to
access National Instruments GPIB 488.2 devices

To add this module to an existing driver then you need to follow some of the steps taken in the
VISAdrvTestApp directory. You will need to (see VISAdrvTestApp/src/build.mak): 

Add    VISAdrv.dbd   to the _DBD list 

Add    VISAdrv  to the _LIBS list 

You should also add the line 

    include $(TOP)/visa_lib.mak

To link against the system VISA library (this is only strictly needed for static builds)


Then to configure the driver in the IOC at boot time (see iocsBoot/iocVISAdrvtest/st.cmd) use a command like:

    # local serial port
    drvAsynVISAPortConfigure("L0", "ASRL1::INSTR")

or

    # remote GPIB
    drvAsynVISAPortConfigure("L0", "visa://ndximat/GPIB0::3::INSTR")


where L0 is you asyn port name followed by the local or remote VISA name of your device (see NI Measurement and Automation explorer if you don't know the name)

