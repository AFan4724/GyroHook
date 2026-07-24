// SPDX-License-Identifier: GPL-2.0
#include <linux/device.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/namei.h>
#include <linux/ptrace.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/uprobes.h>

#include "include/gyro_ioctl.h"

#define SENSOR_TYPE_GYROSCOPE 4
#define SENSOR_TYPE_OFFSET 0x0c
#define AIDL_GYRO_VECTOR_OFFSET 0x18
#define HIDL_GYRO_VECTOR_OFFSET 0x10
#define SENSOR_SERVICE_PATH "/system/lib64/libsensorservice.so"

struct gyro_state {
    spinlock_t lock;
    int sensor_api;
    int convert_offset;
    float x;
    float y;
    bool enabled;
    bool registered;
    struct uprobe_consumer consumer;
};

static struct gyro_state state;

static int gyro_uprobe_pre_handler(struct uprobe_consumer *consumer,
                                   struct pt_regs *regs)
{
    struct gyro_state *current;
    unsigned long event_address;
    int sensor_type = 0;
    size_t vector_offset;
    float vector[2];
    float x;
    float y;
    int sensor_api;
    bool enabled;

    if (!regs) {
        return 0;
    }

    current = container_of(consumer, struct gyro_state, consumer);
    spin_lock(&current->lock);
    x = current->x;
    y = current->y;
    enabled = current->enabled;
    sensor_api = current->sensor_api;
    spin_unlock(&current->lock);

    if (!enabled || (x == 0.0f && y == 0.0f)) {
        return 0;
    }

    event_address = regs->regs[0];
    if (copy_from_user(&sensor_type,
                       (const void __user *)(event_address + SENSOR_TYPE_OFFSET),
                       sizeof(sensor_type))) {
        return 0;
    }

    if (sensor_type != SENSOR_TYPE_GYROSCOPE) {
        return 0;
    }

    vector_offset = sensor_api == GYRO_SENSOR_AIDL
                        ? AIDL_GYRO_VECTOR_OFFSET
                        : HIDL_GYRO_VECTOR_OFFSET;

    if (copy_from_user(vector,
                       (const void __user *)(event_address + vector_offset),
                       sizeof(vector))) {
        return 0;
    }

    vector[0] += x;
    vector[1] += y;

    if (copy_to_user((void __user *)(event_address + vector_offset), vector,
                     sizeof(vector))) {
        return 0;
    }

    return 0;
}

static int gyro_register_uprobe(const struct gyro_hook_setup *setup)
{
    struct path path;
    struct inode *inode;
    int result;

    if (setup->convert_offset <= 0) {
        return -EINVAL;
    }

    if (setup->sensor_api != GYRO_SENSOR_AIDL &&
        setup->sensor_api != GYRO_SENSOR_HIDL) {
        return -EINVAL;
    }

    if (state.registered) {
        return -EBUSY;
    }

    result = kern_path(SENSOR_SERVICE_PATH, LOOKUP_FOLLOW, &path);
    if (result) {
        return result;
    }

    inode = d_backing_inode(path.dentry);
    if (!inode) {
        path_put(&path);
        return -ENODEV;
    }

    spin_lock(&state.lock);
    state.sensor_api = setup->sensor_api;
    state.convert_offset = setup->convert_offset;
    spin_unlock(&state.lock);

    memset(&state.consumer, 0, sizeof(state.consumer));
    state.consumer.handler = gyro_uprobe_pre_handler;
    result = uprobe_register(inode, setup->convert_offset, &state.consumer);
    path_put(&path);
    if (result) {
        return result;
    }

    state.registered = true;
    return 0;
}

static void gyro_unregister_uprobe(void)
{
    struct path path;
    struct inode *inode;
    int result;

    if (!state.registered) {
        return;
    }

    result = kern_path(SENSOR_SERVICE_PATH, LOOKUP_FOLLOW, &path);
    if (!result) {
        inode = d_backing_inode(path.dentry);
        if (inode) {
            uprobe_unregister(inode, state.convert_offset, &state.consumer);
        }
        path_put(&path);
    }
    state.registered = false;

    spin_lock(&state.lock);
    state.x = 0.0f;
    state.y = 0.0f;
    state.enabled = false;
    spin_unlock(&state.lock);
}

static long gyro_ioctl(struct file *file, unsigned int command,
                       unsigned long argument)
{
    struct gyro_hook_setup setup;
    struct gyro_motion_vector motion;

    switch (command) {
    case GYRO_IOC_SETUP:
        if (copy_from_user(&setup, (const void __user *)argument,
                           sizeof(setup))) {
            return -EFAULT;
        }
        return gyro_register_uprobe(&setup);

    case GYRO_IOC_SET_MOTION:
        if (copy_from_user(&motion, (const void __user *)argument,
                           sizeof(motion))) {
            return -EFAULT;
        }
        spin_lock(&state.lock);
        state.x = motion.x;
        state.y = motion.y;
        state.enabled = motion.enabled != 0;
        spin_unlock(&state.lock);
        return 0;

    case GYRO_IOC_DISABLE:
        spin_lock(&state.lock);
        state.x = 0.0f;
        state.y = 0.0f;
        state.enabled = false;
        spin_unlock(&state.lock);
        return 0;

    default:
        return -ENOTTY;
    }
}

static const struct file_operations gyro_fops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = gyro_ioctl,
    .compat_ioctl = gyro_ioctl,
};

static struct miscdevice gyro_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "gyro_uprobe",
    .fops = &gyro_fops,
    .mode = 0600,
};

static int __init gyro_module_init(void)
{
    spin_lock_init(&state.lock);
    return misc_register(&gyro_device);
}

static void __exit gyro_module_exit(void)
{
    gyro_unregister_uprobe();
    misc_deregister(&gyro_device);
}

module_init(gyro_module_init);
module_exit(gyro_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("AFan4724");
MODULE_DESCRIPTION("Kernel uprobe gyroscope vector hook");
