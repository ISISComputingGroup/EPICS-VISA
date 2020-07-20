# VISAdrv - EPICS asyn driver interface to National Instruments VISA library

The module creates an equivalent to the drvAsynSerialPortConfigure() command called drvAsynVISAPortConfigure()
which can be used to connect to serial devices served by National Instruments VISA. If the device is a
serial device then asynSetOption() etc. can be used as normal. You can also use drvAsynVISAPortConfigure() to
access National Instruments GPIB 488.2 devices such as GPIB/ENET 100

You will need to edit VISAdrvApp/src/Makefile and change NIVISADIR to point to the location of your
National Instruments VISA installation on WIndows. On Linux look at the Linux: entries in this Makefile that specify the location of Visa headers and libraries.
 
To add this module to an existing driver then you need to follow some of the steps taken in the
VISAdrvTestApp directory. You will need to (see VISAdrvTestApp/src/build.mak): 

Add    VISAdrv.dbd   to the _DBD list 

Add    VISAdrv  to the _LIBS list 

You should also add the line 

    include $(TOP)/visa_lib.mak

To link against the system NI VISA library (this is only strictly needed if EPICS is being built
statically i.e. STATIC_BUILD=YES in e.g. CONFIG_SITE). See VISAdrvTestApp/src/build.mak for an example.


Then to configure the driver in the IOC at boot time (see iocsBoot/iocVISAdrvtest/st.cmd) use a command like:

    # local serial port (NI MAX name)
    drvAsynVISAPortConfigure("L0", "ASRL5::INSTR")

    # local serial port (alias)
    drvAsynVISAPortConfigure("L0", "COM5")

    # GPIB device added to local device list 
    drvAsynVISAPortConfigure("L0", "GPIB0::3::INSTR")

    # Remote System: GPIB device added to remote PC and served from there using NI VISA Server 
    drvAsynVISAPortConfigure("L0", "visa://remotecomputer/GPIB0::3::INSTR")


where L0 is you asyn port name followed by the local or remote VISA name of your device (see NI Measurement and Automation explorer if you don't know the name)

The drvAsynVISAPortConfigure() command supports some additional options that may sometime be needed:

* readIntTmoMs

  this sets a timeout be be used when a zero timeout read is specified. Before a write operation stream 
  device will do a zero timout read to flush out any unused output from a previous command. With
  GPIB-ENET a zero timeout read caused issues so this can be set to either a small values or
  negative to mean "skip such reads".

* termCharIn

  this is just for optimisation, it tells VISA the termination character and so might 
  make reads a little more efficient in some situations. Set to 0 if not needed.
    
* deviceSendEOM 

  Setting this means the device indicates EOM and so NI-VISA will get complete messages. This is useful 
  for GPIB-ENET if there is no termination character, NI-VISA will always get a complete message and by
  asserting EOM in asyn it avoids needing to wait for e.g. the stream device ReadTimeout to otherwise occur 					

See drvAsynVISAPortConfigure() documentation at http://epics.isis.stfc.ac.uk/doxygen/main/support/VISAdrv/index.html for more details
