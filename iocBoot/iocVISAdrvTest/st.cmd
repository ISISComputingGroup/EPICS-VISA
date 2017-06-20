## @file st.cmd Example VISAdrv program

#!../../bin/windows-x64/VISAdrvTest

## You may have to change VISAdrvTest to something else
## everywhere it appears in this file

# Increase this if you get <<TRUNCATED>> or discarded messages warnings in your errlog output
errlogInit2(65536, 256)

< envPaths

cd "${TOP}"

## Register all support components
dbLoadDatabase "dbd/VISAdrvTest.dbd"
VISAdrvTest_registerRecordDeviceDriver pdbbase

## after device is mapped in NI MAX under devices and interfaces, and right click "scan for instruments"
drvAsynVISAPortConfigure("visa", "GPIB0::3::INSTR")
## instead if device is on another machine and you add the computer as a remote system in NI MAX
#drvAsynVISAPortConfigure("visa", "visa://ndximat/GPIB0::3::INSTR")

asynOctetSetOutputEos("visa",0,"\n")
asynOctetSetInputEos("visa",0,"\n")

# trace flow
#asynSetTraceMask("visa",0,0x11) 
# trace I/O
#asynSetTraceMask("visa",0,0x9) 
#asynSetTraceIOMask("visa",0,0x2)

epicsEnvSet ("STREAM_PROTOCOL_PATH", "$(TOP)/data")

## Load our record instances
dbLoadRecords("db/VISAdrvTest.db","P=$(MYPVPREFIX)Q=VISA:,PORT=visa")
dbLoadRecords("$(ASYN)/db/asynRecord.db","P=$(MYPVPREFIX),R=VISA:ASYNREC,PORT=visa,ADDR=0,OMAX=80,IMAX=80")

cd "${TOP}/iocBoot/${IOC}"
iocInit

## Start any sequence programs
#seq sncxxx,"user=faa59Host"

