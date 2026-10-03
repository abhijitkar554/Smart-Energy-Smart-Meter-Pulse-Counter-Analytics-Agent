#ifndef PULSE_IOCTL_H
#define PULSE_IOCTL_H
/*
 * pulse_ioctl.h — Shared ioctl definitions between kernel driver and userspace.
 * Include this file in both pulse_counter_driver.c and HardwarePulseReader.cpp.
 */
#include <linux/ioctl.h>   /* kernel side */

#define PULSE_IOC_MAGIC  'P'
#define PULSE_IOC_GET    _IOR(PULSE_IOC_MAGIC, 0, long)   /* get count  */
#define PULSE_IOC_RESET  _IO (PULSE_IOC_MAGIC, 1)          /* reset to 0 */
#define PULSE_IOC_ADD    _IOW(PULSE_IOC_MAGIC, 2, long)    /* add N      */

#endif /* PULSE_IOCTL_H */
