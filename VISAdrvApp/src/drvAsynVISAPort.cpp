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
#include "asynOption.h"
#include "asynInterposeCom.h"
#include "asynInterposeEos.h"

#include <epicsExport.h>

#include "drvAsynVISAPort.h"

/// driver private data structure
typedef struct {
    asynUser          *pasynUser; 
    char              *portName;  ///< asyn port name
	ViSession 		   defaultRM;
	ViSession          vi;    ///< current session handle
	bool               connected;
    char              *resourceName; ///< VISA resource name
    unsigned long      nReadBytes;  ///< number of bytes read from this resource name
    unsigned long      nWriteBytes; ///< number of bytes written to this resource
    unsigned long      nReadCalls;  ///< number of read calls from this resource name
    unsigned long      nWriteCalls; ///< number of written calls to this resource
	double 			   timeout;
	bool               isSerial;
	bool               isGPIB;
	bool               SuccessIsEOM;   ///< signal ASYN_EOM_END on all reads > 0 bytes and VI_SUCCESS returned. 
	                                   ///< Devices like GPIB signal END and this is reflected in VI_SECCESS. Serial
									   ///< devices return VI_SUCCESS if read < requested, which may not be the true end 
    ViAttrState		   readIntTimeout; ///< internal read timeout (ms), used for second part of two stage read
    ViUInt8            termCharIn;     ///< read termination character, if specified improves read efficiency
    asynInterface      common;
    asynInterface      option;
    asynInterface      octet;
} visaDriver_t;

/// translate VISA error code to readable string 
static std::string errMsg(ViSession vi, ViStatus err)
{
    char err_msg[1024]={0};
    viStatusDesc (vi, err, err_msg);
    return std::string(err_msg);
}

#define VI_CHECK_ERROR(__command, __err) \
    if (__err < 0) \
    { \
        epicsSnprintf(pasynUser->errorMessage,pasynUser->errorMessageSize, \
                              "%s: %s %s", driver->resourceName, __command, errMsg(driver->vi, err).c_str()); \
        return asynError; \
    }

/*
 * asynOption methods
 */
static asynStatus
getOption(void *drvPvt, asynUser *pasynUser,
                              const char *key, char *val, int valSize)
{
    visaDriver_t *driver = (visaDriver_t*)drvPvt;
    assert(driver);
	if (!driver->connected)
	{
            epicsSnprintf(pasynUser->errorMessage,pasynUser->errorMessageSize,
                          "%s disconnected:", driver->resourceName);
            return asynError;
	}
	if (!driver->isSerial)
	{
            epicsSnprintf(pasynUser->errorMessage,pasynUser->errorMessageSize,
                          "%s getOption - not a serial device", driver->resourceName);
            return asynError;
	}
	ViUInt32 viu32;
	ViUInt16 viu16, flow;
	int l = -1;
	ViStatus err = viGetAttribute(driver->vi, VI_ATTR_ASRL_FLOW_CNTRL, &flow);
	VI_CHECK_ERROR(key, err);
    if (epicsStrCaseCmp(key, "baud") == 0) {
		if ( (err = viGetAttribute(driver->vi, VI_ATTR_ASRL_BAUD, &viu32)) == VI_SUCCESS ) {
            l = epicsSnprintf(val, valSize, "%u", (unsigned)viu32);		
		}
    }
    else if (epicsStrCaseCmp(key, "bits") == 0) {
		if ( (err = viGetAttribute(driver->vi, VI_ATTR_ASRL_DATA_BITS, &viu16)) == VI_SUCCESS ) {
            l = epicsSnprintf(val, valSize, "%u", (unsigned)viu16);		
		}
    }
    else if (epicsStrCaseCmp(key, "parity") == 0) {
		if ( (err = viGetAttribute(driver->vi, VI_ATTR_ASRL_PARITY, &viu16)) == VI_SUCCESS ) {
            switch (viu16) {
                case VI_ASRL_PAR_NONE:
                    l = epicsSnprintf(val, valSize, "none");
                    break;
                case VI_ASRL_PAR_ODD:
                    l = epicsSnprintf(val, valSize, "odd");
                    break;
                case VI_ASRL_PAR_EVEN:
                    l = epicsSnprintf(val, valSize, "even");
                    break;
                case VI_ASRL_PAR_MARK:
                    l = epicsSnprintf(val, valSize, "mark");
                    break;
                case VI_ASRL_PAR_SPACE:
                    l = epicsSnprintf(val, valSize, "space");
                    break;
				default:
                    l = epicsSnprintf(val, valSize, "unknown");
                    break;
			}
        }
    }
    else if (epicsStrCaseCmp(key, "stop") == 0) {
		if ( (err = viGetAttribute(driver->vi, VI_ATTR_ASRL_STOP_BITS, &viu16)) == VI_SUCCESS ) {
            switch (viu16) {
                case VI_ASRL_STOP_ONE:
                    l = epicsSnprintf(val, valSize, "1");
                    break;
                case VI_ASRL_STOP_ONE5:
                    l = epicsSnprintf(val, valSize, "1.5");
                    break;
                case VI_ASRL_STOP_TWO:
                    l = epicsSnprintf(val, valSize, "2");
                    break;
				default:
                    l = epicsSnprintf(val, valSize, "unknown");
                    break;
			}
        }
    }
    else if (epicsStrCaseCmp(key, "clocal") == 0) {
        l = epicsSnprintf(val, valSize, "%c",  (flow & VI_ASRL_FLOW_DTR_DSR) ? 'N' : 'Y');
    }
    else if (epicsStrCaseCmp(key, "crtscts") == 0) {
        l = epicsSnprintf(val, valSize, "%c",  (flow & VI_ASRL_FLOW_RTS_CTS) ? 'Y' : 'N');
    }
    else if (epicsStrCaseCmp(key, "ixon") == 0) {
        l = epicsSnprintf(val, valSize, "%c",  (flow & VI_ASRL_FLOW_XON_XOFF) ? 'Y' : 'N');
    }
    else if (epicsStrCaseCmp(key, "ixany") == 0) {
        l = epicsSnprintf(val, valSize, "%c",  'N'); // ixany not supported on windows?
    }
    else if (epicsStrCaseCmp(key, "ixoff") == 0) {
        l = epicsSnprintf(val, valSize, "%c",  (flow & VI_ASRL_FLOW_XON_XOFF) ? 'Y' : 'N');
    }
    else {
        epicsSnprintf(pasynUser->errorMessage,pasynUser->errorMessageSize,
                                                "Unsupported key \"%s\"", key);
        return asynError;
    }
	VI_CHECK_ERROR(key, err);
    if (l >= valSize) {
        epicsSnprintf(pasynUser->errorMessage,pasynUser->errorMessageSize,
                            "Value buffer for key '%s' is too small.", key);
        return asynError;
    }
    asynPrint(driver->pasynUser, ASYN_TRACEIO_DRIVER,
              "%s getOption, key=%s, val=%s\n",
              driver->portName, key, val);
    return asynSuccess;
}

static asynStatus
setOption(void *drvPvt, asynUser *pasynUser, const char *key, const char *val)
{
    visaDriver_t *driver = (visaDriver_t*)drvPvt;
    assert(driver);
	if (!driver->connected)
	{
            epicsSnprintf(pasynUser->errorMessage,pasynUser->errorMessageSize,
                          "%s disconnected:", driver->resourceName);
            return asynError;
	}
	if (!driver->isSerial)
	{
            epicsSnprintf(pasynUser->errorMessage,pasynUser->errorMessageSize,
                          "%s setOption - not a serial device", driver->resourceName);
            return asynError;
	}
    asynPrint(pasynUser, ASYN_TRACE_FLOW,
                    "%s setOption key %s val %s\n", driver->portName, key, val);
	ViUInt16 flow, old_flow;
	ViStatus err = viGetAttribute(driver->vi, VI_ATTR_ASRL_FLOW_CNTRL, &flow);
	VI_CHECK_ERROR(key, err);
	old_flow = flow;
    if (epicsStrCaseCmp(key, "baud") == 0) {
        int baud;
        if(sscanf(val, "%d", &baud) != 1) {
            epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize,
                                                                "Bad number");
            return asynError;
        }
        err = viSetAttribute(driver->vi, VI_ATTR_ASRL_BAUD, baud);
    }
    else if (epicsStrCaseCmp(key, "bits") == 0) {
        int bits;
        if(sscanf(val, "%d", &bits) != 1) {
            epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize,
                                                                "Bad number");
            return asynError;
        }
        err = viSetAttribute(driver->vi, VI_ATTR_ASRL_DATA_BITS, bits);
    }
    else if (epicsStrCaseCmp(key, "parity") == 0) {
        if (epicsStrCaseCmp(val, "none") == 0) {
            err = viSetAttribute(driver->vi, VI_ATTR_ASRL_PARITY, VI_ASRL_PAR_NONE);
        }
        else if (epicsStrCaseCmp(val, "odd") == 0) {
            err = viSetAttribute(driver->vi, VI_ATTR_ASRL_PARITY, VI_ASRL_PAR_ODD);
        }
        else if (epicsStrCaseCmp(val, "even") == 0) {
            err = viSetAttribute(driver->vi, VI_ATTR_ASRL_PARITY, VI_ASRL_PAR_EVEN);
        }
        else if (epicsStrCaseCmp(val, "mark") == 0) {
            err = viSetAttribute(driver->vi, VI_ATTR_ASRL_PARITY, VI_ASRL_PAR_MARK);
        }
        else if (epicsStrCaseCmp(val, "space") == 0) {
            err = viSetAttribute(driver->vi, VI_ATTR_ASRL_PARITY, VI_ASRL_PAR_SPACE);
        }
        else {
            epicsSnprintf(pasynUser->errorMessage,pasynUser->errorMessageSize,
                                                            "Invalid parity.");
            return asynError;
        }
    }
    else if (epicsStrCaseCmp(key, "stop") == 0) {
        if (epicsStrCaseCmp(val, "1") == 0) {
            err = viSetAttribute(driver->vi, VI_ATTR_ASRL_STOP_BITS, VI_ASRL_STOP_ONE);
        }
        else if (epicsStrCaseCmp(val, "1.5") == 0) {
            err = viSetAttribute(driver->vi, VI_ATTR_ASRL_STOP_BITS, VI_ASRL_STOP_ONE5);
        }
        else if (epicsStrCaseCmp(val, "2") == 0) {
            err = viSetAttribute(driver->vi, VI_ATTR_ASRL_STOP_BITS, VI_ASRL_STOP_TWO);
        }
        else {
            epicsSnprintf(pasynUser->errorMessage,pasynUser->errorMessageSize,
                                                "Invalid number of stop bits.");
            return asynError;
        }
    }
    else if (epicsStrCaseCmp(key, "clocal") == 0) {
        if (epicsStrCaseCmp(val, "Y") == 0) {
			flow &= ~VI_ASRL_FLOW_DTR_DSR;
        }
        else if (epicsStrCaseCmp(val, "N") == 0) {
			flow |= VI_ASRL_FLOW_DTR_DSR;
        }
        else {
            epicsSnprintf(pasynUser->errorMessage,pasynUser->errorMessageSize,
                                                    "Invalid clocal value.");
            return asynError;
        }
    }
    else if (epicsStrCaseCmp(key, "crtscts") == 0) {
        if (epicsStrCaseCmp(val, "Y") == 0) {
			flow |= VI_ASRL_FLOW_RTS_CTS;
        }
        else if (epicsStrCaseCmp(val, "N") == 0) {
			flow &= ~VI_ASRL_FLOW_RTS_CTS;
        }
        else {
            epicsSnprintf(pasynUser->errorMessage,pasynUser->errorMessageSize,
                                                      "Invalid crtscts value.");
            return asynError;
        }
    }
    else if ( (epicsStrCaseCmp(key, "ixon") == 0) || (epicsStrCaseCmp(key, "ixoff") == 0 ) ) {
        if (epicsStrCaseCmp(val, "Y") == 0) {
			flow |= VI_ASRL_FLOW_XON_XOFF;
        }
        else if (epicsStrCaseCmp(val, "N") == 0) {
			flow &= ~VI_ASRL_FLOW_XON_XOFF;
        }
        else {
            epicsSnprintf(pasynUser->errorMessage,pasynUser->errorMessageSize,
                                                    "Invalid %s value.", key);
            return asynError;
        }
    }
    else if (epicsStrCaseCmp(key, "ixany") == 0) {
        epicsSnprintf(pasynUser->errorMessage,pasynUser->errorMessageSize,
                                                    "Option ixany not supported on Windows");
        return asynError;       
    }
    else if (epicsStrCaseCmp(key, "") != 0) {
        epicsSnprintf(pasynUser->errorMessage,pasynUser->errorMessageSize,
                                                "Unsupported key \"%s\"", key);
        return asynError;
    }
	if (err == VI_SUCCESS && flow != old_flow)
	{
	    err = viSetAttribute(driver->vi, VI_ATTR_ASRL_FLOW_CNTRL, flow);
	}
	VI_CHECK_ERROR(key, err);
    asynPrint(driver->pasynUser, ASYN_TRACEIO_DRIVER,
              "%s setOption, key=%s, val=%s\n",
              driver->portName, key, val);
    return asynSuccess;
}

static const struct asynOption asynOptionMethods = { setOption, getOption };

/// close a VISA session
static asynStatus
closeConnection(asynUser *pasynUser, visaDriver_t *driver, const char* reason)
{
    asynPrint(pasynUser, ASYN_TRACE_FLOW,
              "Close %s connection %s\n", driver->resourceName, reason);
    if (!driver->connected) {
        epicsSnprintf(pasynUser->errorMessage,pasynUser->errorMessageSize,
                              "%s: session already closed", driver->resourceName);
        return asynError;
    }
	ViStatus err;
	if ( (err = viClose(driver->vi)) != VI_SUCCESS )
	{
        epicsSnprintf(pasynUser->errorMessage,pasynUser->errorMessageSize,
                              "%s: viClose error", driver->resourceName);
        return asynError;
	}
    driver->connected = false;
	driver->vi = VI_NULL;
	pasynManager->exceptionDisconnect(pasynUser);
    return asynSuccess;
}


/*Beginning of asynCommon methods*/
/*
 * Report link parameters
 */
static void
asynCommonReport(void *drvPvt, FILE *fp, int details)
{
    visaDriver_t *driver = (visaDriver_t*)drvPvt;
    char termChar[16]; // bit of space for encoding an escape sequence
    assert(driver);
    if (details >= 1) {
        fprintf(fp, "    Port %s: %sonnected\n",
                                                driver->resourceName,
                                                (driver->connected ? "C" : "Disc"));
    }
	if (driver->termCharIn != 0)
	{
	    epicsStrnEscapedFromRaw(termChar, sizeof(termChar), reinterpret_cast<const char*>(&(driver->termCharIn)), 1);
	}
	else
	{
		strncpy(termChar, "<none>", sizeof(termChar));
	}
    if (details >= 2) {
        fprintf(fp, "    Characters written: %lu\n", driver->nWriteBytes);
        fprintf(fp, "       Characters read: %lu\n", driver->nReadBytes);
        fprintf(fp, "      write operations: %lu\n", driver->nWriteCalls);
        fprintf(fp, "       read operations: %lu\n", driver->nReadCalls);
        fprintf(fp, "      Is serial device: %c\n", (driver->isSerial ? 'Y' : 'N'));
        fprintf(fp, "      Is GPIB device: %c\n", (driver->isGPIB ? 'Y' : 'N'));
        fprintf(fp, "  Input term char hint: \"%s\" (0x%x)\n", termChar, (unsigned)driver->termCharIn);
        fprintf(fp, "Internal read tmo (ms): %d\n", (driver->readIntTimeout == VI_TMO_IMMEDIATE ? 0 : (int)driver->readIntTimeout));
    }
}

static void
visaCleanup (void *arg)
{
    asynStatus status;
    visaDriver_t *driver = (visaDriver_t*)arg;
	
    if (!arg) return;
    status=pasynManager->lockPort(driver->pasynUser);
    if(status!=asynSuccess)
        asynPrint(driver->pasynUser, ASYN_TRACE_ERROR, "%s: cleanup locking error\n", driver->portName);

    if(status==asynSuccess)
        pasynManager->unlockPort(driver->pasynUser);

	viClose(driver->defaultRM); // this will automatically close all sessions 
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
              "Open connection to \"%s\"  reason: %d\n", driver->resourceName,
                                                           pasynUser->reason);

    if (driver->connected) {
        epicsSnprintf(pasynUser->errorMessage,pasynUser->errorMessageSize,
                              "%s: session already open.", driver->resourceName);
        return asynError;
    }
	ViStatus err;
	if ( (err = viOpen(driver->defaultRM, driver->resourceName,VI_NULL,VI_NULL,&(driver->vi))) != VI_SUCCESS )
	{
        epicsSnprintf(pasynUser->errorMessage,pasynUser->errorMessageSize,
                              "%s: viOpen %s", driver->resourceName, errMsg(driver->defaultRM, err).c_str());
		return asynError;
	}
	ViUInt16 intf_type;
	char intf_name[256];
	intf_name[0] = '\0';
	err = viGetAttribute(driver->vi, VI_ATTR_INTF_INST_NAME, intf_name);
	VI_CHECK_ERROR("intf_name", err);
	err = viGetAttribute(driver->vi, VI_ATTR_INTF_TYPE, &intf_type);
	VI_CHECK_ERROR("intf_type", err);
	
	if (intf_type == VI_INTF_ASRL) // is it a serial device?
	{
		// disable read/write exit on serial specific termination character, we use VI_ATTR_TERMCHAR_EN
		err = viSetAttribute(driver->vi, VI_ATTR_ASRL_END_IN, VI_ASRL_END_NONE);
	    VI_CHECK_ERROR("ASRL_END_IN", err);
		err = viSetAttribute(driver->vi, VI_ATTR_ASRL_END_OUT, VI_ASRL_END_NONE);
	    VI_CHECK_ERROR("ASRL_END_OUT", err);
		driver->isSerial = true;
	}
	else
	{
		driver->isSerial = false;		
	}
	if (intf_type == VI_INTF_GPIB)
	{
		driver->isGPIB = true;
		// we should make these configurable
		err = viSetAttribute(driver->vi, VI_ATTR_GPIB_READDR_EN, VI_TRUE);
	    VI_CHECK_ERROR("VI_ATTR_GPIB_READDR_EN", err);
// The LabVIEW driver set this to VI_TRUE (default is VI_FALSE) but causes problems for stress rig if we set it
//		err = viSetAttribute(driver->vi, VI_ATTR_GPIB_UNADDR_EN, VI_TRUE);
//	    VI_CHECK_ERROR("VI_ATTR_GPIB_UNADDR_EN", err);
		err = viSetAttribute(driver->vi, VI_ATTR_SEND_END_EN, VI_TRUE);
	    VI_CHECK_ERROR("VI_ATTR_SEND_END_EN", err);
	}
	else
	{
		driver->isGPIB = false;		
	}
	if (driver->termCharIn != 0)
	{
		// tell VISA to terminate a read early when this character is seen
	    err = viSetAttribute(driver->vi, VI_ATTR_TERMCHAR, driver->termCharIn);
	    VI_CHECK_ERROR("termchar", err);
	    err = viSetAttribute(driver->vi, VI_ATTR_TERMCHAR_EN, VI_TRUE);
	}
	else
	{
	    // disable read/write command exit on termination character VI_ATTR_TERMCHAR in general
	    err = viSetAttribute(driver->vi, VI_ATTR_TERMCHAR_EN, VI_FALSE);
	}
	VI_CHECK_ERROR("termchar_en", err);

    // these are the defaults, need to change?
//	viSetAttribute(driver->vi, VI_ATTR_SEND_END_EN, VI_TRUE);
//	viSetAttribute(driver->vi, VI_ATTR_SUPPRESS_END_EN, VI_FALSE);

	// don't think we need to do anything about these as we using raw rather than buffered io
	//	VI_ATTR_RD_BUF_OPER_MODE
	//	VI_ATTR_WR_BUF_OPER_MODE    -> VI_FLUSH_ON_ACCESS
	
    asynPrint(pasynUser, ASYN_TRACE_FLOW,
                          "Opened connection to \"%s\" (%s) isSerial=%c isGPIB=%c\n", driver->resourceName, 
						  intf_name, (driver->isSerial ? 'Y' : 'N'), (driver->isGPIB ? 'Y' : 'N'));
    driver->connected = true;
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
    return closeConnection(pasynUser,driver,"Disconnect request");
}

static asynStatus writeIt(void *drvPvt, asynUser *pasynUser,
    const char *data, size_t numchars, size_t *nbytesTransfered)
{
    visaDriver_t *driver = (visaDriver_t*)drvPvt;
    asynStatus status = asynSuccess;
	bool timedout = false;
	epicsTimeStamp epicsTS1, epicsTS2;

    assert(driver);
    asynPrint(pasynUser, ASYN_TRACE_FLOW,
              "%s write.\n", driver->resourceName);
    asynPrintIO(pasynUser, ASYN_TRACEIO_DRIVER, data, numchars,
                "%s write %lu\n", driver->resourceName, (unsigned long)numchars);
	epicsTimeGetCurrent(&epicsTS1);
    *nbytesTransfered = 0;
	if (!driver->connected)
	{
            epicsSnprintf(pasynUser->errorMessage,pasynUser->errorMessageSize,
                          "%s disconnected:", driver->resourceName);
            return asynError;
	}
	++(driver->nWriteCalls);
    if (numchars == 0)
	{
        return asynSuccess;
	}
	ViStatus err;
	// always need to set timeout as use immediate as part of read
        driver->timeout = pasynUser->timeout;
		if (driver->timeout == 0)
		{			
			err = viSetAttribute(driver->vi, VI_ATTR_TMO_VALUE, VI_TMO_IMMEDIATE);
		}
		else
		{
			err = viSetAttribute(driver->vi, VI_ATTR_TMO_VALUE, static_cast<int>(driver->timeout * 1000.0));
		}
		VI_CHECK_ERROR("set timeout", err);
	unsigned long actual = 0;
	err = viWrite(driver->vi, (ViBuf)data, static_cast<ViUInt32>(numchars), &actual);
	if ( err == VI_ERROR_TMO )
	{
		timedout = true;
	}
	else if ( err != VI_SUCCESS )
	{
            closeConnection(pasynUser,driver,"Write error");
            epicsSnprintf(pasynUser->errorMessage,pasynUser->errorMessageSize,
                          "%s write error %s", driver->resourceName, errMsg(driver->vi, err).c_str());
            return asynError;		
	}
    driver->nWriteBytes += actual;
    *nbytesTransfered += actual;
    numchars -= actual;
    data += actual;
	if (timedout)
	{
		status = asynTimeout;
	}
	epicsTimeGetCurrent(&epicsTS2);
    asynPrint(pasynUser, ASYN_TRACE_FLOW,
              "wrote %lu to %s, return %s.\n", (unsigned long)*nbytesTransfered,
                                               driver->resourceName,
                                               pasynManager->strStatus(status));
	asynPrint(pasynUser, ASYN_TRACE_FLOW, "%s Write took %f timeout was %f\n", 
	          driver->resourceName, epicsTimeDiffInSeconds(&epicsTS2, &epicsTS1), pasynUser->timeout);
    return status;
}

static asynStatus readIt(void *drvPvt, asynUser *pasynUser,
    char *data, size_t maxchars, size_t *nbytesTransfered, int *gotEom)
{
    visaDriver_t *driver = (visaDriver_t*)drvPvt;
    int reason = 0;
    asynStatus status = asynSuccess;
	epicsTimeStamp epicsTS1, epicsTS2;

    assert(driver);
    asynPrint(pasynUser, ASYN_TRACE_FLOW,
              "%s read.\n", driver->resourceName);
	epicsTimeGetCurrent(&epicsTS1);
    *nbytesTransfered = 0;
    if (gotEom) *gotEom = 0;
	if (!driver->connected)
	{
            epicsSnprintf(pasynUser->errorMessage,pasynUser->errorMessageSize,
                          "%s disconnected:", driver->resourceName);
            return asynError;
	}
	++(driver->nReadCalls);
    if (maxchars <= 0) {
        epicsSnprintf(pasynUser->errorMessage,pasynUser->errorMessageSize,
                  "%s maxchars %d. Why <=0?",driver->resourceName,(int)maxchars);
        return asynError;
    }
	unsigned long actual = 0, actualex = 0;
	ViStatus err;
//	ViUInt32 avail = 0;
	// should we only ever try and read one character?
//	if (driver->isSerial)
//	{
//		if (viGetAttribute(driver->vi, VI_ATTR_ASRL_AVAIL_NUM, &avail) == VI_SUCCESS)
//		{
//			if (avail > maxchars)
//			{
//				avail = maxchars;
//			}
//		}
//	}

// try and read one character, if we don't time out try and read more with an immediate timeout
// we always need to set timeout as we reset to immediate below
// we don't use driver->readIntTimeout
// whatever out timeout, we can get called with a timeout of 0 by higher levels to flush the input queue 
// prior to a write, hence we need to map to  readIntTimeout  to avois problems on GPIB-ENET
	driver->timeout = pasynUser->timeout;
	if (driver->timeout == 0)
	{
		err = viSetAttribute(driver->vi, VI_ATTR_TMO_VALUE, driver->readIntTimeout);
	}
	else
	{
		err = viSetAttribute(driver->vi, VI_ATTR_TMO_VALUE, static_cast<int>(driver->timeout * 1000.0));
	}
	VI_CHECK_ERROR("set timeout", err);
	err = viRead(driver->vi, (ViBuf)data, 1, &actual);
	if (err < 0 && err != VI_ERROR_TMO)
	{
		closeConnection(pasynUser, driver, "Read error (stage 1)");
		epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize,
			"%s read error %s", driver->resourceName, errMsg(driver->vi, err).c_str());
		return asynError;
	}
	if (actual > 0 && err == VI_SUCCESS_MAX_CNT)
	{
		// read anything else that might be there, originally this used VI_TMO_IMMEDIATE
		// but we had a few timeout issues with GPIP over ethernet so this is now 
		// configurable to a small finite value
		err = viSetAttribute(driver->vi, VI_ATTR_TMO_VALUE, driver->readIntTimeout);
		VI_CHECK_ERROR("set timeout", err);
		err = viRead(driver->vi, reinterpret_cast<ViBuf>(data + actual), static_cast<ViUInt32>(maxchars - actual), &actualex);
		if (err < 0 && err != VI_ERROR_TMO)
		{
			closeConnection(pasynUser, driver, "Read error (stage 2)");
			epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize,
				"%s read error %s", driver->resourceName, errMsg(driver->vi, err).c_str());
			return asynError;
		}
		actual += actualex;
		// a VI_SUCCESS on GPIB means we got the EOM, on serial it doesn't necessarily mean this
		// so on timeout don't convert to VI_SUCCESS but to something that will get ignored in the
		// next case statement
		// remove expected VI_ERROR_TMO, but leave VI_SUCCESS_TERM_CHAR etc.
	    if (err < 0)
		{
		    err = VI_WARN_UNKNOWN_STATUS;
		}
	}
	switch(err)
	{
		case VI_SUCCESS:
			if (driver->SuccessIsEOM && actual > 0)
			{
				reason |= ASYN_EOM_END;
			}
		    break;
			
		case VI_SUCCESS_TERM_CHAR:
			reason |= ASYN_EOM_EOS;  
			break;
			
		case VI_SUCCESS_MAX_CNT:
//			reason |= ASYN_EOM_CNT;  // we read avail not maxchars, add this later if needed
			break;
			
		case VI_ERROR_TMO:
			status = asynTimeout;
			break;
			
		default:
			break;
	}
	if (actual > 0)
	{
        asynPrintIO(pasynUser, ASYN_TRACEIO_DRIVER, data, actual,
                   "%s read %d\n", driver->resourceName, actual);
        driver->nReadBytes += (unsigned long)actual;
    }
//	else
//	{
//           epicsSnprintf(pasynUser->errorMessage,pasynUser->errorMessageSize,
//                          "%s read", driver->resourceName);
//            closeConnection(pasynUser,driver,"Read error");
//            status = asynError;
//    }
    *nbytesTransfered = actual;
    /* If there is room add a null byte */
    if (actual < (int) maxchars)
        data[actual] = 0;
    else
        reason |= ASYN_EOM_CNT;
    if (gotEom) *gotEom = reason;
	epicsTimeGetCurrent(&epicsTS2);
    asynPrint(pasynUser, ASYN_TRACE_FLOW,
              "read %lu from %s, return %s.\n", (unsigned long)*nbytesTransfered,
                                               driver->resourceName,
                                               pasynManager->strStatus(status));
	asynPrint(pasynUser, ASYN_TRACE_FLOW, "%s Read took %f timeout was %f\n", driver->resourceName, 
	          epicsTimeDiffInSeconds(&epicsTS2, &epicsTS1), pasynUser->timeout);
    return status;
}

static asynStatus
flushIt(void *drvPvt,asynUser *pasynUser)
{
	epicsTimeStamp epicsTS1, epicsTS2;
	epicsTimeGetCurrent(&epicsTS1);
    visaDriver_t *driver = (visaDriver_t*)drvPvt;
    assert(driver);
	if (!driver->connected)
	{
		epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize,
			"%s disconnected:", driver->resourceName);
		return asynError;
	}
	//	ViStatus err = viFlush(driver->vi, VI_WRITE_BUF | VI_IO_OUT_BUF);
	ViStatus err = viFlush(driver->vi, VI_READ_BUF_DISCARD | VI_IO_IN_BUF_DISCARD);
	VI_CHECK_ERROR("flush", err);
	epicsTimeGetCurrent(&epicsTS2);
    asynPrint(pasynUser, ASYN_TRACE_FLOW, "%s flush\n", driver->resourceName);
	asynPrint(pasynUser, ASYN_TRACE_FLOW, "%s flush took %f\n", driver->resourceName, 
	          epicsTimeDiffInSeconds(&epicsTS2, &epicsTS1));
    return asynSuccess;
}

static asynOctet asynOctetMethods = { writeIt, readIt, flushIt };

/*
 * asynCommon methods
 */
static const struct asynCommon asynCommonMethods = {
    asynCommonReport,
    asynCommonConnect,
    asynCommonDisconnect
};

/*
 * Configure and register an IP socket from a hostInfo string
 */
epicsShareFunc int
drvAsynVISAPortConfigure(const char *portName,
                         const char *resourceName, 
                         unsigned int priority,
                         int noAutoConnect,
                         int noProcessEos,
                         int readIntTmoMs,
                         const char* termCharIn,
						 int SuccessIsEOM)
{
    visaDriver_t *driver;
    asynStatus status;
    static int firstTime = 1;

    /*
     * Check arguments
     */
    if (portName == NULL) {
        printf("drvAsynVISAPortConfigure: Port name missing.\n");
        return -1;
    }
    if (resourceName == NULL) {
        printf("drvAsynVISAPortConfigure: resourceName information missing.\n");
        return -1;
    }

    /*
     * Perform some one-time-only initializations
     */
    if (firstTime) {
        firstTime = 0;
    }

    /*
     * Create a driver
     */
    driver = (visaDriver_t *)callocMustSucceed(1, sizeof(visaDriver_t), "drvAsyVISAPortConfigure()");
    driver->connected = false;
    driver->resourceName = epicsStrDup(resourceName);
    driver->portName = epicsStrDup(portName);
    driver->timeout = -0.1;
    driver->isSerial = false;
    driver->isGPIB = false;
	driver->SuccessIsEOM = (SuccessIsEOM != 0);
	if (readIntTmoMs != 0)
	{
        printf("drvAsynVISAPortConfigure: using internal read timeout of %d ms\n", readIntTmoMs);
        driver->readIntTimeout = readIntTmoMs;		
	}
	else
	{
        driver->readIntTimeout = VI_TMO_IMMEDIATE;
	}
    driver->termCharIn = 0;
    if (termCharIn != NULL)
    {
		char termChar[16];
	    epicsStrnRawFromEscaped(termChar, sizeof(termChar), termCharIn, strlen(termCharIn));
        if (strlen(termChar) == 1)
		{
			driver->termCharIn = termChar[0];
            printf("drvAsynVISAPortConfigure: using term char hint \"%s\" (0x%x)\n", termCharIn, (unsigned)driver->termCharIn);
		}
		else
		{
            printf("drvAsynVISAPortConfigure: termChar must be single character - NOT SET\n");
		}
	}
	if (viOpenDefaultRM(&(driver->defaultRM)) != VI_SUCCESS)
	{
		printf("drvAsynVISAPortConfigure: viOpenDefaultRM failed for port \"%s\"\n", driver->portName);
		driverCleanup(driver);
		return -1;
	}
    driver->pasynUser = pasynManager->createAsynUser(0,0);

    /*
     *  Link with higher level routines
     */
    driver->common.interfaceType = asynCommonType;
    driver->common.pinterface  = (void *)&asynCommonMethods;
    driver->common.drvPvt = driver;
    driver->option.interfaceType = asynOptionType;
    driver->option.pinterface  = (void *)&asynOptionMethods;
    driver->option.drvPvt = driver;

	if (pasynManager->registerPort(driver->portName,
                                   ASYN_CANBLOCK,
                                   !noAutoConnect,
                                   priority,
                                   0) != asynSuccess) {
        printf("drvAsynVISAPortConfigure: Can't register myself.\n");
        driverCleanup(driver);
        return -1;
    }
    status = pasynManager->registerInterface(driver->portName,&driver->common);
    if(status != asynSuccess) {
        printf("drvAsynVISAPortConfigure: Can't register common.\n");
        driverCleanup(driver);
        return -1;
    }
    status = pasynManager->registerInterface(driver->portName,&driver->option);
    if(status != asynSuccess) {
        printf("drvAsynVISAPortConfigure: Can't register option.\n");
        driverCleanup(driver);
        return -1;
    }
    driver->octet.interfaceType = asynOctetType;
    driver->octet.pinterface  = &asynOctetMethods;
    driver->octet.drvPvt = driver;
    status = pasynOctetBase->initialize(driver->portName,&driver->octet,
                             (noProcessEos ? 0 : 1),(noProcessEos ? 0 : 1),1);
    if(status != asynSuccess) {
        printf("drvAsynVISAPortConfigure: Can't register octet.\n");
        driverCleanup(driver);
        return -1;
    }
    status = pasynManager->connectDevice(driver->pasynUser,driver->portName,-1);
    if(status != asynSuccess) {
        printf("drvAsynVISAPortConfigure: connectDevice failed %s\n",driver->pasynUser->errorMessage);
        visaCleanup(driver);
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
static const iocshArg drvAsynVISAPortConfigureArg2 = { "priority",iocshArgInt};
static const iocshArg drvAsynVISAPortConfigureArg3 = { "noAutoConnect",iocshArgInt};
static const iocshArg drvAsynVISAPortConfigureArg4 = { "noProcessEos",iocshArgInt};
static const iocshArg drvAsynVISAPortConfigureArg5 = { "readIntTmoMs",iocshArgInt};
static const iocshArg drvAsynVISAPortConfigureArg6 = { "termCharIn",iocshArgString};
static const iocshArg drvAsynVISAPortConfigureArg7 = { "SuccessIsEOM",iocshArgInt};

static const iocshArg *drvAsynVISAPortConfigureArgs[] = {
    &drvAsynVISAPortConfigureArg0, &drvAsynVISAPortConfigureArg1, &drvAsynVISAPortConfigureArg2,
    &drvAsynVISAPortConfigureArg3, &drvAsynVISAPortConfigureArg4, &drvAsynVISAPortConfigureArg5,
    &drvAsynVISAPortConfigureArg6, &drvAsynVISAPortConfigureArg7

};

static const iocshFuncDef drvAsynVISAPortConfigureFuncDef =
                      {"drvAsynVISAPortConfigure",sizeof(drvAsynVISAPortConfigureArgs)/sizeof(iocshArg*),drvAsynVISAPortConfigureArgs};

static void drvAsynVISAPortConfigureCallFunc(const iocshArgBuf *args)
{
    drvAsynVISAPortConfigure(args[0].sval, args[1].sval, args[2].ival, args[3].ival,
                             args[4].ival, args[5].ival, args[6].sval, args[7].ival);
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