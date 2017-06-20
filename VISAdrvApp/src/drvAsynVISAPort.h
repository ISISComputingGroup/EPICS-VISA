/// @file drvAsynVISAPort.h ASYN driver for National Instruments VISA 

#ifndef DRVASYNVISAPORT_H
#define DRVASYNVISAPORT_H

#include <shareLib.h>  

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

epicsShareFunc int drvAsynVISAPortConfigure(const char *portName,
                         const char *resourceName, 
                         unsigned int priority,
                         int noAutoConnect,
                         int noProcessEos);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
#endif  /* DRVASYNVISAPORT_H */
