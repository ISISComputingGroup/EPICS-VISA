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

drvAsynVISAPortConfigure("visa","visa://ndximat/GPIB0::3::INSTR")

asynOctetSetOutputEos("visa",0,"\\r")
asynOctetSetInputEos("visa",0,"\\r")

epicsEnvSet ("STREAM_PROTOCOL_PATH", "$(TOP)/data")

## Load our record instances
dbLoadRecords("db/VISAdrvTest.db","P=$(MYPVPREFIX)Q=VISA:,PORT=visa")
dbLoadRecords("$(ASYN)/db/asynRecord.db","P=$(MYPVPREFIX),R=VISA:ASYNREC,PORT=visa,ADDR=0,OMAX=80,IMAX=80")

cd "${TOP}/iocBoot/${IOC}"
iocInit

## Start any sequence programs
#seq sncxxx,"user=faa59Host"

