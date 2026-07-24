// SPDX-License-Identifier: MIT
#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define GYRO_HOOK_IOC_MAGIC 'G'
#define GYRO_HOOK_INIT_TYPE 100U
#define GYRO_HOOK_VECTOR_TYPE 0U
#define GYRO_API_AIDL 1U
#define GYRO_API_HIDL_V10 2U

struct gyro_hook_init {
    uint32_t type;
    uint32_t convert_offset;
    uint32_t api_version;
    uint32_t reserved;
};

struct gyro_vector {
    uint32_t type;
    float x;
    float y;
    uint32_t active;
};

#define GYRO_IOC_SET_HOOK _IOW(GYRO_HOOK_IOC_MAGIC, 1, struct gyro_hook_init)
#define GYRO_IOC_SET_VEC _IOW(GYRO_HOOK_IOC_MAGIC, 2, struct gyro_vector)
#define GYRO_IOC_DISABLE _IO(GYRO_HOOK_IOC_MAGIC, 3)

static const char *const kSensorServicePath =
    "/system/lib64/libsensorservice.so";
static const char *const kDevicePath = "/dev/gyro_uprobe";
static const char *const kAidlConvertSymbol =
    "_ZN7android8hardware7sensors14implementation20convertToSensorEventERKN4aidl7android8hardware7sensors5EventEP15sensors_event_t";
static const char *const kHidlConvertSymbol =
    "_ZN7android8hardware7sensors4V1_014implementation20convertToSensorEventERKNS2_5EventEP15sensors_event_t";

static void print_usage(const char *program)
{
    fprintf(stderr,
            "usage:\n"
            "  %s init [device]\n"
            "  %s set <x> <y> [device]\n"
            "  %s off [device]\n"
            "  %s daemon <x> <y> <interval-ms> [device]\n",
            program, program, program, program);
}

static void *resolve_convert_function(uint32_t *api_version,
                                      uintptr_t *base_address)
{
    Dl_info info;
    void *handle;
    void *address;

    handle = dlopen(kSensorServicePath, RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        fprintf(stderr, "dlopen(%s): %s\n", kSensorServicePath, dlerror());
        return NULL;
    }

    address = dlsym(handle, kAidlConvertSymbol);
    *api_version = GYRO_API_AIDL;
    if (!address) {
        address = dlsym(handle, kHidlConvertSymbol);
        *api_version = GYRO_API_HIDL_V10;
    }

    if (!address) {
        fprintf(stderr, "convertToSensorEvent symbol not found\n");
        dlclose(handle);
        return NULL;
    }

    memset(&info, 0, sizeof(info));
    if (dladdr(address, &info) == 0 || !info.dli_fbase) {
        fprintf(stderr, "dladdr failed\n");
        dlclose(handle);
        return NULL;
    }

    *base_address = (uintptr_t)info.dli_fbase;
    dlclose(handle);
    return address;
}

static int open_device(const char *path)
{
    int fd = open(path, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "open(%s): %s\n", path, strerror(errno));
    }
    return fd;
}

static int command_init(const char *device_path)
{
    uintptr_t base_address = 0;
    uintptr_t address;
    uint32_t api_version = 0;
    void *convert;
    int fd;
    struct gyro_hook_init init = {
        .type = GYRO_HOOK_INIT_TYPE,
        .reserved = 0,
    };

    convert = resolve_convert_function(&api_version, &base_address);
    if (!convert) {
        return 1;
    }

    address = (uintptr_t)convert;
    if (address < base_address || address - base_address > UINT32_MAX) {
        fprintf(stderr, "invalid convert offset\n");
        return 1;
    }

    init.convert_offset = (uint32_t)(address - base_address);
    init.api_version = api_version;

    fd = open_device(device_path);
    if (fd < 0) {
        return 1;
    }

    if (ioctl(fd, GYRO_IOC_SET_HOOK, &init) < 0) {
        fprintf(stderr, "GYRO_IOC_SET_HOOK: %s\n", strerror(errno));
        close(fd);
        return 1;
    }

    printf("hook installed: api=%u offset=0x%08" PRIx32 "\n", api_version,
           init.convert_offset);
    close(fd);
    return 0;
}

static int command_set(const char *device_path, float x, float y, bool active)
{
    int fd = open_device(device_path);
    struct gyro_vector vector = {
        .type = GYRO_HOOK_VECTOR_TYPE,
        .x = x,
        .y = y,
        .active = active ? 1U : 0U,
    };

    if (fd < 0) {
        return 1;
    }

    if (ioctl(fd, GYRO_IOC_SET_VEC, &vector) < 0) {
        fprintf(stderr, "GYRO_IOC_SET_VEC: %s\n", strerror(errno));
        close(fd);
        return 1;
    }

    close(fd);
    return 0;
}

static int command_off(const char *device_path)
{
    int fd = open_device(device_path);
    if (fd < 0) {
        return 1;
    }

    if (ioctl(fd, GYRO_IOC_DISABLE) < 0) {
        fprintf(stderr, "GYRO_IOC_DISABLE: %s\n", strerror(errno));
        close(fd);
        return 1;
    }

    close(fd);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "init") == 0) {
        return command_init(argc >= 3 ? argv[2] : kDevicePath);
    }

    if (strcmp(argv[1], "set") == 0) {
        float x;
        float y;
        char *end_x;
        char *end_y;
        const char *device_path;

        if (argc < 4) {
            print_usage(argv[0]);
            return 1;
        }

        x = strtof(argv[2], &end_x);
        y = strtof(argv[3], &end_y);
        if (*end_x != '\0' || *end_y != '\0') {
            fprintf(stderr, "invalid float value\n");
            return 1;
        }
        device_path = argc >= 5 ? argv[4] : kDevicePath;
        return command_set(device_path, x, y, true);
    }

    if (strcmp(argv[1], "off") == 0) {
        return command_off(argc >= 3 ? argv[2] : kDevicePath);
    }

    if (strcmp(argv[1], "daemon") == 0) {
        float x;
        float y;
        long interval_ms;
        char *end_x;
        char *end_y;
        char *end_interval;
        const char *device_path;

        if (argc < 5) {
            print_usage(argv[0]);
            return 1;
        }

        x = strtof(argv[2], &end_x);
        y = strtof(argv[3], &end_y);
        interval_ms = strtol(argv[4], &end_interval, 10);
        if (*end_x != '\0' || *end_y != '\0' || *end_interval != '\0' ||
            interval_ms <= 0) {
            fprintf(stderr, "invalid daemon arguments\n");
            return 1;
        }
        device_path = argc >= 6 ? argv[5] : kDevicePath;

        for (;;) {
            if (command_set(device_path, x, y, true) != 0) {
                return 1;
            }
            usleep((useconds_t)interval_ms * 1000U);
        }
    }

    print_usage(argv[0]);
    return 1;
}
