/**********************************************************************
* Asyn device support using VISA           *
**********************************************************************/

#ifndef DRVASYNVISAPORT_H
#define DRVASYNVISAPORT_H

#include <shareLib.h>  

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

epicsShareFunc int drvAsynVISAPortConfigure(const char *portName,
                                            const char *resourceName);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
#endif  /* DRVASYNVISAPORT_H */
