# GyroHook 项目

## 简介

GyroHook 是一个允许用户修改安卓设备陀螺仪传感器数据的项目。它包含一个安卓应用、一个 Xposed 模块和一个 C++ 命令行工具，它们共同协作以实现对陀螺仪数据的自定义控制。

## 主要功能

*   **自定义陀螺仪偏移**: 用户可以通过安卓应用设置 X, Y, Z 轴的旋转偏移量。
*   **实时数据修改**: Xposed 模块会读取这些偏移量，并实时应用到系统的陀螺仪事件中。
*   **多种数据输入方式**:
    *   **安卓应用界面**: 直接在应用内输入和保存设置。
    *   **Socket 通信**: 安卓应用可以启动一个 Socket 服务器，接收外部发送的陀螺仪数据并更新设置。
    *   **C++ 命令行工具**:
        *   可以作为 Socket 客户端连接到安卓应用的服务器，发送固定的或模拟的陀螺仪数据。
        *   可以直接修改配置文件 (SharedPreferences) 来更新陀螺仪设置。

## 项目组件

1.  **安卓应用 (`app` 目录)**:
    *   提供用户界面 (`MainActivity.kt`) 来输入陀螺仪的 X, Y, Z 偏移值和 Socket 服务器端口。
    *   将设置保存到 SharedPreferences 文件 (`/data/user/0/com.example.gyrohook/shared_prefs/gyro_settings.xml`)。
    *   内置一个 Socket 服务器，用于接收外部发送的陀螺仪数据，并更新应用的设置。

2.  **Xposed 模块 (`app` 目录中的 `GyroHook.kt`)**:
    *   在 Xposed 框架下运行。
    *   读取由安卓应用保存的 SharedPreferences 文件中的陀螺仪偏移值。
    *   Hook 系统底层的传感器事件分发方法 (`dispatchSensorEvent`)。
    *   当检测到陀螺仪传感器事件时，将配置的偏移量添加到原始传感器数据上。

3.  **C++ 命令行工具 (`test` 目录)**:
    *   `main.cpp`: 主程序，提供了两种操作模式。
        *   **`socket` 模式**:
            *   作为客户端连接到安卓应用中运行的 Socket 服务器。
            *   可以发送一次性的固定 X, Y, Z 值。
            *   可以模拟连续的陀螺仪运动并周期性发送数据。
        *   **`file` 模式**:
            *   直接读取并修改 SharedPreferences 文件 (默认路径: `/data/user/0/com.example.gyrohook/shared_prefs/gyro_settings.xml`)。
            *   写入标准 SharedPreferences XML 格式，可被 `XSharedPreferences` 直接读取。
    *   `GyroHook.cpp` / `GyroHook.hpp`: 包含了 C++ 客户端的 Socket 通信逻辑 (`GyroClient` 类) 和文件操作逻辑 (`GyroFileUtils` 命名空间)。

## 工作流程

1.  **设置**: 用户通过安卓应用 `MainActivity` 设置陀螺仪的 X, Y, Z 偏移值和 Socket 通信端口。这些设置被保存到应用的 SharedPreferences 文件中。
2.  **Hook 与修改**:
    *   Xposed 模块 `GyroHook` 在系统层面加载。
    *   它会读取 SharedPreferences 中的设置。
    *   当任何应用请求陀螺仪数据时，`GyroHook` 会截获原始数据，并根据用户的设置进行修改，然后将修改后的数据传递给请求的应用。
3.  **外部控制 (可选)**:
    *   **通过 Socket**:
        1.  安卓应用启动 Socket 服务器。
        2.  C++ 命令行工具以 `socket` 模式运行，连接到该服务器。
        3.  C++ 工具发送新的 X, Y, Z 数据。
        4.  安卓应用收到数据后，更新界面显示，并将新设置保存回 SharedPreferences。Xposed 模块随后会读取到新的设置。
    *   **通过文件修改**:
        1.  C++ 命令行工具以 `file` 模式运行。
        2.  直接修改 SharedPreferences 文件内容。
        3.  Xposed 模块在下次读取配置时（例如，传感器事件触发时重新加载配置）会获取到新的设置。

## 如何使用 C++ 命令行工具 (`main`)

编译 `test` 目录下的 `main.cpp` 和 `GyroHook.cpp` (例如使用 g++)。

**基本用法:**

```bash
./main <mode> [options]
```

**模式 (`mode`):**

*   `socket`: 通过 Socket 发送数据。
*   `file`: 直接修改配置文件。

**Socket 模式选项 (`./main socket [ip_address] [port] [x y z]`):**

*   `ip_address`: 服务器 IP 地址 (默认: `127.0.0.1`)。
*   `port`: 服务器端口 (默认: `16384`)。
*   `x y z`: (可选) 要发送的固定 X, Y, Z 值。如果提供，则只发送一次。如果不提供，则进入模拟持续发送模式。

    *   示例 (模拟发送到指定 IP 和端口):
        ```bash
        ./main socket 192.168.1.100 12345
        ```
    *   示例 (发送一次固定值到默认 IP 和端口):
        ```bash
        ./main socket 127.0.0.1 16384 1.0 2.5 -0.5
        ```
    *   示例 (发送一次固定值 X,Y，Z 默认为 0，到默认 IP 和端口):
        ```bash
        ./main socket 127.0.0.1 16384 1.0 2.5
        ```


**文件模式选项 (`./main file [file_path] <x> <y> <z> [port_value]`):**

*   `file_path`: (可选) 配置文件的完整路径 (默认: `/data/user/0/com.example.gyrohook/shared_prefs/gyro_settings.xml`)。
*   `x y z`: 要写入文件的 X, Y, Z 值 (必需)。
*   `port_value`: (可选) 要写入文件的端口值 (默认: `16384`)。

    *   示例 (使用自定义路径、值和端口):
        ```bash
        ./main file /sdcard/my_settings.json 0.1 0.2 0.3 12345
        ```
    *   示例 (使用默认路径，提供 X, Y, Z 值和端口):
        ```bash
        ./main file 0.1 0.2 0.3 12345
        ```
    *   示例 (使用默认路径和端口，提供 X, Y, Z 值):
        ```bash
        ./main file 0.1 0.2 0.3
        ```

## 注意事项

*   **Xposed 依赖**: `GyroHook.kt` 模块的运行需要设备上已安装并激活 Xposed 框架。
*   **文件权限**: 确保配置文件具有正确的读写权限，以便应用、Xposed 模块和 C++ 工具能够正常访问。安卓应用在保存设置时会尝试设置文件权限。
*   **配置文件格式**: C++ 工具写入标准 SharedPreferences XML 格式，`XSharedPreferences` 可以直接读取。

## 许可

本项目采用 MIT 许可证。详细信息请参阅 `LICENSE` 文件。 