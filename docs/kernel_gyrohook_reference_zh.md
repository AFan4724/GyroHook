# GyroHook 内核参考实现

`kernel/` 和 `userspace/` 是一套独立的内核陀螺仪 Hook 参考实现，不依赖现有 Xposed 模块。

## 原理

内核模块对以下文件和函数偏移注册 uprobe：

```text
/system/lib64/libsensorservice.so
android::hardware::sensors::implementation::convertToSensorEvent
```

前置回调执行时读取 AIDL/HIDL `Event`：

```text
sensorType 不等于 4：直接返回
sensorType 等于 4：把保存的 X/Y 加到陀螺仪前两个轴
```

两种事件布局：

```text
AIDL：X = Event + 0x18，Y = Event + 0x1c
HIDL：X = Event + 0x10，Y = Event + 0x14
```

回调只修改正在转换的原始 `Event`，原 `convertToSensorEvent` 随后继续执行。

## ioctl 协议

初始化参数：

```c
struct gyro_hook_setup {
    int convert_offset; // convertToSensorEvent 相对库基址的偏移
    int sensor_api;     // 1 = AIDL，2 = HIDL
};
```

向量参数：

```c
struct gyro_motion_vector {
    float x;
    float y;
    int enabled; // 1 = 启用，0 = 停止叠加
};
```

设备节点：

```text
/dev/gyrohook
```

## 内核要求

目标内核需要支持：

```text
CONFIG_UPROBES=y
CONFIG_UPROBE_EVENTS=y
```

模块使用 `uprobe_register`。该符号通常是 GPL-only 导出，因此 `kernel/gyrohook.c`
必须使用 `MODULE_LICENSE("GPL")`，源码也按 GPL-2.0 声明。不同 Android/GKI 内核可能限制该符号的正常模块导出，构建前必须确认目标内核允许当前模块解析该符号。

## 许可证边界

- `kernel/gyrohook.c`：GPL-2.0。
- `kernel/include/gyro_ioctl.h`：GPL-2.0 WITH Linux-syscall-note。
- `userspace/gyroctl.c`：普通用户态程序，继续使用项目现有 MIT 协议。
- `app/` 与 `test/` 的 Xposed 方案：继续使用项目现有 MIT 协议。

## 构建

内核模块需要目标设备的内核源码树或匹配的 GKI 头文件：

```bash
make -C kernel KDIR=/path/to/kernel/source
```

用户态工具可以在 Android NDK 环境中构建：

```bash
make -C userspace CC=<android-ndk-toolchain>/bin/clang
```

## 使用

加载模块：

```bash
insmod kernel/gyrohook.ko
```

安装 Hook：

```bash
./userspace/gyroctl init
```

设置二维叠加值：

```bash
./userspace/gyroctl set 0.25 -0.10
```

关闭叠加：

```bash
./userspace/gyroctl off
```

卸载模块：

```bash
rmmod gyrohook
```

## 限制

- 当前参考实现只叠加陀螺仪 X/Y，不修改 Z。
- 只能用于 `libsensorservice.so` 存在对应转换函数且布局一致的设备。
- 不同厂商内核的 uprobe API、SELinux、GKI/KMI、模块签名策略可能不同。
- 这是参考实现，不应未经适配直接刷入生产设备。
