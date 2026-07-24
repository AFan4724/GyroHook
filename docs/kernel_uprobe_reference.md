# Kernel uprobe reference implementation

`kernel/` and `userspace/` provide a standalone kernel-side gyroscope hook reference implementation. It does not depend on the existing Xposed module.

## How it works

The kernel module registers a uprobe at a function offset inside:

```text
/system/lib64/libsensorservice.so
android::hardware::sensors::implementation::convertToSensorEvent
```

The pre-handler reads the input AIDL/HIDL `Event`:

```text
sensorType != 4: return
sensorType == 4: add the saved X/Y vector to the first two gyroscope axes
```

Supported layouts:

```text
AIDL:       X = Event + 0x18, Y = Event + 0x1c
HIDL V1_0:  X = Event + 0x10, Y = Event + 0x14
```

The handler modifies the original event before the normal conversion function continues.

## ioctl protocol

Initialization packet:

```c
struct gyro_hook_init {
    __u32 type;           // 100
    __u32 convert_offset; // convertToSensorEvent offset from library base
    __u32 api_version;    // 1 = AIDL, 2 = HIDL V1_0
    __u32 reserved;
};
```

Vector packet:

```c
struct gyro_vector {
    __u32 type;   // 0
    float x;
    float y;
    __u32 active; // 1 = enabled, 0 = disabled
};
```

Device node:

```text
/dev/gyro_uprobe
```

## Kernel requirements

The target kernel must support:

```text
CONFIG_UPROBES=y
CONFIG_UPROBE_EVENTS=y
```

The module calls `uprobe_register`. That symbol is commonly exported as GPL-only, so `kernel/gyro_uprobe.c` must use `MODULE_LICENSE("GPL")` and is declared as GPL-2.0. Android/GKI kernels may also restrict normal module access to this symbol, so symbol resolution must be verified for the exact target kernel.

## License boundaries

- `kernel/gyro_uprobe.c`: GPL-2.0.
- `kernel/include/gyro_ioctl.h`: GPL-2.0 WITH Linux-syscall-note.
- `userspace/gyroctl.c`: ordinary userspace program, remains under the project's MIT license.
- `app/` and `test/`: the existing Xposed implementation remains under the project's MIT license.

## Build

Build the module with the kernel source tree or compatible GKI headers:

```bash
make -C kernel KDIR=/path/to/kernel/source
```

Build the userspace tool with an Android NDK toolchain:

```bash
make -C userspace CC=<android-ndk-toolchain>/bin/clang
```

## Usage

Load the module:

```bash
insmod kernel/gyro_uprobe.ko
```

Install the hook:

```bash
./userspace/gyroctl init
```

Set the two-axis additive vector:

```bash
./userspace/gyroctl set 0.25 -0.10
```

Disable it:

```bash
./userspace/gyroctl off
```

Unload the module:

```bash
rmmod gyro_uprobe
```

## Limitations

- The current reference implementation modifies gyroscope X/Y only, not Z.
- It requires a compatible `libsensorservice.so` and event layout.
- Vendor kernels may differ in uprobe APIs, SELinux, GKI/KMI rules, and module signing.
- This is a reference implementation and should not be flashed to a production device without device-specific validation.
