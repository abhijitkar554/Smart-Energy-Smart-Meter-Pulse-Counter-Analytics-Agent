#ifndef PULSE_IOCTL_H
#define PULSE_IOCTL_H

#include <linux/ioctl.h>   

#define PULSE_IOC_MAGIC  'P'
#define PULSE_IOC_GET    _IOR(PULSE_IOC_MAGIC, 0, long)   /* get count  */
#define PULSE_IOC_RESET  _IO (PULSE_IOC_MAGIC, 1)          /* reset to 0 */
#define PULSE_IOC_ADD    _IOW(PULSE_IOC_MAGIC, 2, long)    /* add N      */

#endif 
