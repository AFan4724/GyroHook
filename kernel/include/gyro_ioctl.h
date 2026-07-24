// SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note
#ifndef GYRO_UPROBE_IOCTL_H
#define GYRO_UPROBE_IOCTL_H

#include <linux/ioctl.h>

#define GYRO_HOOK_IOC_MAGIC 'G'

#define GYRO_SENSOR_AIDL 1
#define GYRO_SENSOR_LEGACY 2

struct gyro_hook_setup {
    int convert_offset;
    int sensor_api;
};

struct gyro_motion_vector {
    float x;
    float y;
    int enabled;
};

#define GYRO_IOC_SETUP \
    _IOW(GYRO_HOOK_IOC_MAGIC, 1, struct gyro_hook_setup)
#define GYRO_IOC_SET_MOTION \
    _IOW(GYRO_HOOK_IOC_MAGIC, 2, struct gyro_motion_vector)
#define GYRO_IOC_DISABLE _IO(GYRO_HOOK_IOC_MAGIC, 3)

#endif
