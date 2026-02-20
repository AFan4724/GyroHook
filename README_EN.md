# GyroHook

[中文](README.md) | English

## Introduction

GyroHook is a project that allows users to modify gyroscope sensor data on Android devices. It consists of an Android app, an Xposed module, and a C++ command-line tool that work together to enable custom control over gyroscope data.

## Features

- **Custom Gyroscope Offset**: Set X, Y, Z axis rotation offsets via the Android app.
- **Real-time Data Modification**: The Xposed module reads these offsets and applies them to gyroscope events in real time.
- **Language Switch**: The app supports English and Chinese. Tap the language icon in the Toolbar to switch.
- **Multiple Input Methods**:
  - **Android App UI**: Enter and save settings directly in the app.
  - **Socket Communication**: The app can start a Socket server to receive gyroscope data from external sources.
  - **C++ CLI Tool**: Connect as a Socket client or directly modify the config file.

## Project Components

1. **Android App (`app` directory)**:
   - Provides a UI (`MainActivity.kt`) to input X, Y, Z offsets and the Socket server port.
   - Saves settings to SharedPreferences (`/data/user/0/com.example.gyrohook/shared_prefs/gyro_settings.xml`).
   - Built-in Socket server to receive external gyroscope data and update settings.

2. **Xposed Module (`GyroHook.kt` in `app` directory)**:
   - Runs under the Xposed framework.
   - Reads gyroscope offsets from the SharedPreferences file saved by the app.
   - Hooks the system-level sensor event dispatch method (`dispatchSensorEvent`).
   - When a gyroscope event is detected, adds the configured offsets to the raw sensor data.

3. **C++ CLI Tool (`test` directory)**:
   - `main.cpp`: Main program with `socket` and `file` operation modes.
   - `GyroHook.cpp` / `GyroHook.hpp`: Socket communication logic and file operation logic.

## How It Works

1. **Setup**: The user sets X, Y, Z offsets and the Socket port in the Android app. Settings are saved to SharedPreferences.
2. **Hook & Modify**: The Xposed module reads the settings, intercepts gyroscope data, applies the offsets, and passes the modified data to requesting apps.
3. **External Control (optional)**:
   - **Via Socket**: Start the Socket server in the app, then run the C++ tool in `socket` mode to send new data.
   - **Via File**: Run the C++ tool in `file` mode to directly modify the SharedPreferences file.

## C++ CLI Tool Usage

Compile `main.cpp` and `GyroHook.cpp` in the `test` directory.

**Basic usage:**

```bash
./main <mode> [options]
```

**Socket mode (`./main socket [ip] [port] [x y z]`):**

```bash
# Simulate continuous sending
./main socket 192.168.1.100 12345

# Send fixed values once
./main socket 127.0.0.1 16384 1.0 2.5 -0.5
```

**File mode (`./main file [file_path] <x> <y> <z> [port]`):**

```bash
# Use default path
./main file 0.1 0.2 0.3

# Use custom path and port
./main file /sdcard/my_settings.json 0.1 0.2 0.3 12345
```

## Notes

- **Xposed Required**: The `GyroHook.kt` module requires Xposed framework to be installed and activated on the device.
- **File Permissions**: Ensure the config file has correct read/write permissions.
- **Config File Format**: The C++ tool writes standard SharedPreferences XML format, readable directly by `XSharedPreferences`.

## License

This project is licensed under the MIT License. See the `LICENSE` file for details.
