#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <osiUnistd.h>
#include <cantProceed.h>
#include <errlog.h>
#include <iocsh.h>
#include <epicsAssert.h>
#include <epicsExit.h>
#include <epicsStdio.h>
#include <epicsString.h>
#include <epicsThread.h>
#include <epicsTime.h>
#include <osiUnistd.h>

#include <iostream>
#include <string>

#include <visa.h>

#include "asynDriver.h"
#include "asynOctet.h"
#include "asynInterposeCom.h"
#include "asynInterposeEos.h"

#include <epicsExport.h>

#include "drvAsynVISAPort.h"

typedef struct {
    asynUser          *pasynUser; 
    char              *portName;
	ViSession          vi;
	bool               connected;
    char              *resourceName;
    unsigned long      nRead;
    unsigned long      nWritten;
    asynInterface      common;
    asynInterface      octet;
} visaDriver_t;

static std::string errMsg(ViSession vi, ViStatus err)
{
    char err_msg[1024]={0};
    viStatusDesc (vi, err, err_msg);
    return std::string(err_msg);
}

/*
 * Close a connection
 */
static void
closeConnection(asynUser *pasynUser, visaDriver_t *driver, const char* reason)
{
    asynPrint(pasynUser, ASYN_TRACE_FLOW,
              "Close %s connection %s\n", driver->resourceName, reason);
    if (!driver->connected) {
        epicsSnprintf(pasynUser->errorMessage,pasynUser->errorMessageSize,
                              "%s: Link already closed!", driver->resourceName);
	    return;
    }
	ViStatus err;
	if ( (err = viClose(driver->vi)) != VI_SUCCESS )
	{
        epicsSnprintf(pasynUser->errorMessage,pasynUser->errorMessageSize,
                              "%s: viClose error", driver->resourceName);
	}
    driver->connected = false;	
	driver->vi = VI_NULL;
}


/*Beginning of asynCommon methods*/
/*
 * Report link parameters
 */
static void
asynCommonReport(void *drvPvt, FILE *fp, int details)
{
    visaDriver_t *driver = (visaDriver_t*)drvPvt;

    assert(driver);
    if (details >= 1) {
        fprintf(fp, "    Port %s: %sonnected\n",
                                                driver->resourceName,
                                                (driver->connected ? "C" : "Disc"));
    }
    if (details >= 2) {
        fprintf(fp, "    Characters written: %lu\n", driver->nWritten);
        fprintf(fp, "       Characters read: %lu\n", driver->nRead);
    }
}

static void
visaCleanup (void *arg)
{
    asynStatus status;
    visaDriver_t *driver = (visaDriver_t*)arg;
	
	//delete default rm?

    if (!arg) return;
    status=pasynManager->lockPort(driver->pasynUser);
    if(status!=asynSuccess)
        asynPrint(driver->pasynUser, ASYN_TRACE_ERROR, "%s: cleanup locking error\n", driver->portName);

    if(status==asynSuccess)
        pasynManager->unlockPort(driver->pasynUser);
}

static void
driverCleanup(visaDriver_t *driver)
{
	if (driver)
	{
        free(driver->portName);
        free(driver->resourceName);
        free(driver);
    }
}
/*
 * Create a link
*/
static asynStatus
connectIt(void *drvPvt, asynUser *pasynUser)
{
    visaDriver_t *driver = (visaDriver_t*)drvPvt;
    assert(driver);
    asynPrint(pasynUser, ASYN_TRACE_FLOW,
              "Open connection to %s  reason: %d\n", driver->resourceName,
                                                           pasynUser->reason);

    if (driver->connected) {
        epicsSnprintf(pasynUser->errorMessage,pasynUser->errorMessageSize,
                              "%s: Link already open!", driver->resourceName);
        return asynError;
    }
	ViSession defaultRM;
	ViStatus err;
	if ( (err = viOpenDefaultRM(&defaultRM)) != VI_SUCCESS )
	{
        epicsSnprintf(pasynUser->errorMessage,pasynUser->errorMessageSize,
                              "%s: viOpenDerfaultRm", driver->resourceName);
		return asynError; // close default rm
		
	}
	if ( (err = viOpen(defaultRM, driver->resourceName,VI_NULL,VI_NULL,&(driver->vi))) != VI_SUCCESS )
	{
        epicsSnprintf(pasynUser->errorMessage,pasynUser->errorMessageSize,
                              "%s: viOpen %s", driver->resourceName, errMsg(defaultRM, err).c_str());
		return asynError; // close default rm
	}
    asynPrint(pasynUser, ASYN_TRACE_FLOW,
                          "Opened connection to %s\n", driver->resourceName);
    driver->connected = true;
	std::cerr << "Connected to " << driver->resourceName << std::endl;
    return asynSuccess;
}

static asynStatus
asynCommonConnect(void *drvPvt, asynUser *pasynUser)
{
    asynStatus status = asynSuccess;

    status = connectIt(drvPvt, pasynUser);
    if (status == asynSuccess)
        pasynManager->exceptionConnect(pasynUser);
    return status;
}

static asynStatus
asynCommonDisconnect(void *drvPvt, asynUser *pasynUser)
{
    visaDriver_t *driver = (visaDriver_t*)drvPvt;

    assert(driver);
    closeConnection(pasynUser,driver,"Disconnect request");
    pasynManager->exceptionDisconnect(pasynUser);
    return asynSuccess;
}

static asynStatus writeIt(void *drvPvt, asynUser *pasynUser,
    const char *data, size_t numchars,size_t *nbytesTransfered)
{
    visaDriver_t *driver = (visaDriver_t*)drvPvt;
    asynStatus status = asynSuccess;

    assert(driver);
    asynPrint(pasynUser, ASYN_TRACE_FLOW,
              "%s write.\n", driver->resourceName);
    asynPrintIO(pasynUser, ASYN_TRACEIO_DRIVER, data, numchars,
                "%s write %lu\n", driver->resourceName, (unsigned long)numchars);
    *nbytesTransfered = 0;
	if (!driver->connected)
	{
            epicsSnprintf(pasynUser->errorMessage,pasynUser->errorMessageSize,
                          "%s disconnected:", driver->resourceName);
            return asynError;
	}
    if (numchars == 0)
	{
        return asynSuccess;
	}
	unsigned long actual = 0;
	ViStatus err;
	if ( (err = viWrite(driver->vi, (ViBuf)data, numchars, &actual)) != VI_SUCCESS )
	{
            epicsSnprintf(pasynUser->errorMessage,pasynUser->errorMessageSize,
                          "%s write error %s", driver->resourceName, errMsg(driver->vi, err).c_str());
            closeConnection(pasynUser,driver,"Write error");
            return asynError;
		
	}
    driver->nWritten += actual;
    *nbytesTransfered += actual;
    numchars -= actual;
    data += actual;
	if (actual < numchars)
	{
            epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize,
                                     "%s write %d/%d", driver->resourceName, actual, numchars);
            closeConnection(pasynUser,driver,"partial Write error");
            status = asynError;
    }
    asynPrint(pasynUser, ASYN_TRACE_FLOW,
              "wrote %lu to %s, return %s.\n", (unsigned long)*nbytesTransfered,
                                               driver->resourceName,
                                               pasynManager->strStatus(status));
    return status;
}

static asynStatus readIt(void *drvPvt, asynUser *pasynUser,
    char *data, size_t maxchars,size_t *nbytesTransfered,int *gotEom)
{
    visaDriver_t *driver = (visaDriver_t*)drvPvt;
    int reason = 0;
    asynStatus status = asynSuccess;

    assert(driver);
    asynPrint(pasynUser, ASYN_TRACE_FLOW,
              "%s read.\n", driver->resourceName);
	if (!driver->connected)
	{
            epicsSnprintf(pasynUser->errorMessage,pasynUser->errorMessageSize,
                          "%s disconnected:", driver->resourceName);
            return asynError;
	}
    if (maxchars <= 0) {
        epicsSnprintf(pasynUser->errorMessage,pasynUser->errorMessageSize,
                  "%s maxchars %d. Why <=0?",driver->resourceName,(int)maxchars);
        return asynError;
    }
	unsigned long actual = 0;
	ViStatus err;
    *nbytesTransfered = 0;
    err = viRead(driver->vi, (ViBuf)data, maxchars, &actual);
	if (err < 0)
	{
            epicsSnprintf(pasynUser->errorMessage,pasynUser->errorMessageSize,
                          "%s read error %s", driver->resourceName, errMsg(driver->vi, err).c_str());
            closeConnection(pasynUser,driver,"Read error");
            return asynError;		
	}
	switch(err)
	{
		case VI_SUCCESS:
			reason |= ASYN_EOM_END;
		    break;
			
		case VI_SUCCESS_TERM_CHAR:
			reason |= ASYN_EOM_EOS;
			break;
			
		case VI_SUCCESS_MAX_CNT:
			reason |= ASYN_EOM_CNT;
			break;
			
		default:
			std::cerr << "Unknown error code " << err << std::endl;
			break;
	}
	if (actual > 0)
	{
        asynPrintIO(pasynUser, ASYN_TRACEIO_DRIVER, data, maxchars,
                   "%s read %d\n", driver->resourceName, actual);
        driver->nRead += (unsigned long)actual;
    }
	else
	{
            epicsSnprintf(pasynUser->errorMessage,pasynUser->errorMessageSize,
                          "%s read",
                          driver->resourceName);
            closeConnection(pasynUser,driver,"Read error");
            status = asynError;
    }
    *nbytesTransfered = actual;
    /* If there is room add a null byte */
    if (actual < (int) maxchars)
        data[actual] = 0;
    else
        reason |= ASYN_EOM_CNT;
    if (gotEom) *gotEom = reason;
    return status;
}

/*
 * Flush pending input
 */
static asynStatus
flushIt(void *drvPvt,asynUser *pasynUser)
{
    visaDriver_t *driver = (visaDriver_t*)drvPvt;
    assert(driver);
    asynPrint(pasynUser, ASYN_TRACE_FLOW, "%s flush\n", driver->resourceName);
    return asynSuccess;
}

/*
 * asynCommon methods
 */
static const struct asynCommon drvAsynVISAPortAsynCommon = {
    asynCommonReport,
    asynCommonConnect,
    asynCommonDisconnect
};

/*
 * Configure and register an IP socket from a hostInfo string
 */
epicsShareFunc int
drvAsynVISAPortConfigure(const char *portName,
                       const char *resourceName)
{
    visaDriver_t *driver;
    asynInterface *pasynInterface;
    asynStatus status;
    int nbytes;
    asynOctet *pasynOctet;
    static int firstTime = 1;

    /*
     * Check arguments
     */
    if (portName == NULL) {
        printf("Port name missing.\n");
        return -1;
    }
    if (resourceName == NULL) {
        printf("resourceName information missing.\n");
        return -1;
    }

    /*
     * Perform some one-time-only initializations
     */
    if (firstTime) {
        firstTime = 0;
		// defaultRM ?
    }

    /*
     * Create a driver
     */
	int priority = 0;
    nbytes = sizeof(visaDriver_t) + sizeof(asynOctet);
    driver = (visaDriver_t *)callocMustSucceed(1, nbytes,
          "drvAsyVISAPortConfigure()");
    pasynOctet = (asynOctet *)(driver+1);
    driver->connected = false;
    driver->resourceName = epicsStrDup(resourceName);
    driver->portName = epicsStrDup(portName);

    /*
     *  Link with higher level routines
     */
    pasynInterface = (asynInterface *)callocMustSucceed(2, sizeof(*pasynInterface), "drvAsynVISAPortConfigure");
    driver->common.interfaceType = asynCommonType;
    driver->common.pinterface  = (void *)&drvAsynVISAPortAsynCommon;
    driver->common.drvPvt = driver;
    if (pasynManager->registerPort(driver->portName,
                                   ASYN_CANBLOCK,
                                   true,
                                   priority,
                                   0) != asynSuccess) {
        printf("drvAsynVISAPortConfigure: Can't register myself.\n");
        visaCleanup(driver);
        return -1;
    }
    status = pasynManager->registerInterface(driver->portName,&driver->common);
    if(status != asynSuccess) {
        printf("drvAsynVISAPortConfigure: Can't register common.\n");
        visaCleanup(driver);
        return -1;
    }
    pasynOctet->read = readIt;
    pasynOctet->write = writeIt;
    pasynOctet->flush = flushIt;
    driver->octet.interfaceType = asynOctetType;
    driver->octet.pinterface  = pasynOctet;
    driver->octet.drvPvt = driver;
    status = pasynOctetBase->initialize(driver->portName,&driver->octet, 0, 0, 1);
    if(status != asynSuccess) {
        printf("drvAsynVISAPortConfigure: pasynOctetBase->initialize failed.\n");
        driverCleanup(driver);
        return -1;
    }
    driver->pasynUser = pasynManager->createAsynUser(0,0);
    status = pasynManager->connectDevice(driver->pasynUser,driver->portName,-1);
    if(status != asynSuccess) {
        printf("connectDevice failed %s\n",driver->pasynUser->errorMessage);
        driverCleanup(driver);
        return -1;
    }

    /*
     * Register for socket cleanup
     */
    epicsAtExit(visaCleanup, driver);
    return 0;
}

/*
 * IOC shell command registration
 */
static const iocshArg drvAsynVISAPortConfigureArg0 = { "port name",iocshArgString};
static const iocshArg drvAsynVISAPortConfigureArg1 = { "visa resource",iocshArgString};
static const iocshArg *drvAsynVISAPortConfigureArgs[] = {
    &drvAsynVISAPortConfigureArg0, &drvAsynVISAPortConfigureArg1
};
static const iocshFuncDef drvAsynVISAPortConfigureFuncDef =
                      {"drvAsynVISAPortConfigure",sizeof(drvAsynVISAPortConfigureArgs)/sizeof(iocshArg*),drvAsynVISAPortConfigureArgs};
static void drvAsynVISAPortConfigureCallFunc(const iocshArgBuf *args)
{
    drvAsynVISAPortConfigure(args[0].sval, args[1].sval);
}

extern "C"
{

static void
drvAsynVISAPortConfigureRegister(void)
{
    static int firstTime = 1;
    if (firstTime) {
        iocshRegister(&drvAsynVISAPortConfigureFuncDef,drvAsynVISAPortConfigureCallFunc);
        firstTime = 0;
    }
}

epicsExportRegistrar(drvAsynVISAPortConfigureRegister);

}