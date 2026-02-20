# GyroHook

[English](README_EN.md) | 中文

## 简介

GyroHook 是一个允许用户修改安卓设备陀螺仪传感器数据的项目。它包含一个安卓应用、一个 Xposed 模块和一个 C++ 命令行工具，它们共同协作以实现对陀螺仪数据的自定义控制。

## 主要功能

- **自定义陀螺仪偏移**: 用户可以通过安卓应用设置 X, Y, Z 轴的旋转偏移量。
- **实时数据修改**: Xposed 模块会读取这些偏移量，并实时应用到系统的陀螺仪事件中。
- **语言切换**: 应用支持中英文切换，点击 Toolbar 右上角的语言图标即可切换。
- **多种数据输入方式**:
  - **安卓应用界面**: 直接在应用内输入和保存设置。
  - **Socket 通信**: 安卓应用可以启动一个 Socket 服务器，接收外部发送的陀螺仪数据并更新设置。
  - **C++ 命令行工具**: 可以作为 Socket 客户端连接到安卓应用的服务器，或直接修改配置文件。

## 项目组件

1. **安卓应用 (`app` 目录)**:
   - 提供用户界面 (`MainActivity.kt`) 来输入陀螺仪的 X, Y, Z 偏移值和 Socket 服务器端口。
   - 将设置保存到 SharedPreferences 文件 (`/data/user/0/com.example.gyrohook/shared_prefs/gyro_settings.xml`)。
   - 内置一个 Socket 服务器，用于接收外部发送的陀螺仪数据，并更新应用的设置。

2. **Xposed 模块 (`app` 目录中的 `GyroHook.kt`)**:
   - 在 Xposed 框架下运行。
   - 读取由安卓应用保存的 SharedPreferences 文件中的陀螺仪偏移值。
   - Hook 系统底层的传感器事件分发方法 (`dispatchSensorEvent`)。
   - 当检测到陀螺仪传感器事件时，将配置的偏移量添加到原始传感器数据上。

3. **C++ 命令行工具 (`test` 目录)**:
   - `main.cpp`: 主程序，提供 `socket` 和 `file` 两种操作模式。
   - `GyroHook.cpp` / `GyroHook.hpp`: 包含 Socket 通信逻辑和文件操作逻辑。

## 工作流程

1. **设置**: 用户通过安卓应用设置陀螺仪的 X, Y, Z 偏移值和 Socket 通信端口，保存到 SharedPreferences。
2. **Hook 与修改**: Xposed 模块读取 SharedPreferences 中的设置，截获陀螺仪数据并按设置修改后传递给请求的应用。
3. **外部控制 (可选)**:
   - **通过 Socket**: 启动 Socket 服务器后，C++ 工具以 `socket` 模式连接并发送新数据。
   - **通过文件修改**: C++ 工具以 `file` 模式直接修改 SharedPreferences 文件。

## 如何使用 C++ 命令行工具

编译 `test` 目录下的 `main.cpp` 和 `GyroHook.cpp`。

**基本用法:**

```bash
./main <mode> [options]
```

**Socket 模式 (`./main socket [ip] [port] [x y z]`):**

```bash
# 模拟持续发送
./main socket 192.168.1.100 12345

# 发送一次固定值
./main socket 127.0.0.1 16384 1.0 2.5 -0.5
```

**文件模式 (`./main file [file_path] <x> <y> <z> [port]`):**

```bash
# 使用默认路径
./main file 0.1 0.2 0.3

# 使用自定义路径和端口
./main file /sdcard/my_settings.json 0.1 0.2 0.3 12345
```

## 注意事项

- **Xposed 依赖**: `GyroHook.kt` 模块的运行需要设备上已安装并激活 Xposed 框架。
- **文件权限**: 确保配置文件具有正确的读写权限。
- **配置文件格式**: C++ 工具写入标准 SharedPreferences XML 格式，`XSharedPreferences` 可以直接读取。

## 许可

本项目采用 MIT 许可证。详细信息请参阅 `LICENSE` 文件。
