#ifndef DMOTOR_IOCTL_H
#define DMOTOR_IOCTL_H
#include <linux/ioctl.h>
#include <linux/types.h>
struct dmotor_access { __u32 offset; __u32 value; };
#define DMOTOR_READ _IOWR('M', 0x40, struct dmotor_access)
#define DMOTOR_WRITE _IOW('M', 0x41, struct dmotor_access)
#define DMOTOR_HALT _IO('M', 0x42)
/* Deassert the owned source only; does NOT acknowledge protective latches. */
#define DMOTOR_RELEASE_HALT _IO('M', 0x43)
#endif
