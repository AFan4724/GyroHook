// SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note
#ifndef GYRO_UPROBE_IOCTL_H
#define GYRO_UPROBE_IOCTL_H

#include <linux/ioctl.h>
#include <linux/types.h>

#define GYRO_HOOK_IOC_MAGIC 'G'

#define GYRO_HOOK_INIT_TYPE 100U
#define GYRO_HOOK_VECTOR_TYPE 0U

#define GYRO_API_AIDL 1U
#define GYRO_API_HIDL_V10 2U

struct gyro_hook_init {
    __u32 type;
    __u32 convert_offset;
    __u32 api_version;
    __u32 reserved;
};

struct gyro_vector {
    __u32 type;
    float x;
    float y;
    __u32 active;
};

#define GYRO_IOC_SET_HOOK \
    _IOW(GYRO_HOOK_IOC_MAGIC, 1, struct gyro_hook_init)
#define GYRO_IOC_SET_VEC \
    _IOW(GYRO_HOOK_IOC_MAGIC, 2, struct gyro_vector)
#define GYRO_IOC_DISABLE _IO(GYRO_HOOK_IOC_MAGIC, 3)

#endif
