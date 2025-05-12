#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <stdexcept>
#include <cmath>
#include "GyroHook.hpp"

const std::string DEFAULT_IP = "127.0.0.1";
const int DEFAULT_PORT = 16384;
const std::string DEFAULT_FILE_PATH = "/data/user/0/com.example.gyrohook/shared_prefs/gyro_settings.xml";

void printUsage(const char* progName) {
    std::cerr << "用法: " << progName << " <mode> [options]\n"
              << "模式 (mode):\n"
              << "  socket          通过Socket发送数据。\n"
              << "  file            直接修改配置文件。\n"
              << "选项 (options) - Socket模式:\n"
              << "  <ip_address>    服务器IP地址 (默认: " << DEFAULT_IP << ")\n"
              << "  <port>          服务器端口 (默认: " << DEFAULT_PORT << ")\n"
              << "  <x> <y> <z>     要发送的固定X, Y, Z值。如果提供，则只发送一次。\n"
              << "                  如果不提供，则进入模拟持续发送模式。\n"
              << "选项 (options) - 文件模式:\n"
              << "  <file_path>     配置文件的完整路径 (默认: " << DEFAULT_FILE_PATH << ")\n"
              << "  <x> <y> <z>     要写入文件的X, Y, Z值。\n"
              << "  [port_value]    要写入文件的端口值 (可选, 默认: " << DEFAULT_PORT << ")\n"
              << "示例:\n"
              << "  " << progName << " socket 192.168.1.100 12345         (模拟发送到指定IP和端口)\n"
              << "  " << progName << " socket 127.0.0.1 16384 1.0 2.5 -0.5 (发送一次固定值)\n"
              << "  " << progName << " file /path/to/settings.json 0.1 0.2 0.3 12345 \n"
              << "  " << progName << " file 0.1 0.2 0.3                      (使用默认文件路径和端口)\n"
              << std::endl;
}

void simulateGyroMovement(GyroClient& client) {
    float x = 0.0f, y = 0.0f, z = 0.0f;
    const float step = 0.1f;
    int iteration = 0;

    std::cout << "Socket模式: 开始模拟持续发送陀螺仪数据... (按Ctrl+C停止)" << std::endl;
    while (client.isConnected()) {
        x += step;
        if (x > 5.0f) x = -5.0f;

        y = 2.0f * std::sin(static_cast<double>(x));

        z = 1.5f * std::cos(static_cast<double>(x));

        std::cout << "发送: X=" << x << ", Y=" << y << ", Z=" << z << std::endl;
        if (!client.sendGyroData(x, y, z)) {
            std::cerr << "发送数据失败，断开连接。" << std::endl;
            client.disconnect();
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        iteration++;
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    std::string mode = argv[1];

    try {
        if (mode == "socket") {
            std::string ip = DEFAULT_IP;
            int port = DEFAULT_PORT;
            bool sendOnce = false;
            float x = 0, y = 0, z = 0;

            if (argc > 2) ip = argv[2];
            if (argc > 3) port = std::stoi(argv[3]);
            
            if (argc > 6) {
                sendOnce = true;
                x = std::stof(argv[4]);
                y = std::stof(argv[5]);
                z = std::stof(argv[6]);
            } else if (argc > 4 && argc <=6) {
                 std::cerr << "Socket模式下，如果提供了X,Y,Z值，则必须同时提供IP和端口。" << std::endl;
                 printUsage(argv[0]);
                 return 1;
            } else if (argc == 5 || argc == 6){
                 sendOnce = true;
                 x = std::stof(argv[4]);
                 y = std::stof(argv[5]);
                 if(argc == 7) z = std::stof(argv[6]);
                 else z = 0.0f;
            }

            std::cout << "Socket模式: 尝试连接到 " << ip << ":" << port << std::endl;
            GyroClient client(ip, port);
            if (!client.connect()) {
                std::cerr << "无法连接到服务器。" << std::endl;
                return 1;
            }

            if (sendOnce) {
                std::cout << "Socket模式: 发送一次固定数据: X=" << x << ", Y=" << y << ", Z=" << z << std::endl;
                if (client.sendGyroData(x, y, z)) {
                    std::cout << "数据发送成功。" << std::endl;
                } else {
                    std::cerr << "数据发送失败。" << std::endl;
                }
                client.disconnect();
            } else {
                simulateGyroMovement(client);
            }

        } else if (mode == "file") {
            if (argc < 5) {
                std::cerr << "文件模式: 参数不足。至少需要 X, Y, Z 值。" << std::endl;
                printUsage(argv[0]);
                return 1;
            }

            std::string filePath = DEFAULT_FILE_PATH;
            float x, y, z;
            int filePort = DEFAULT_PORT;

            int valueStartIndex = 2;

            if (argc > 5 && (std::string(argv[2]).find('/') != std::string::npos || std::string(argv[2]).find('.') != std::string::npos)) {
                if (argc < 6) {
                    printUsage(argv[0]);
                    return 1;
                }
                filePath = argv[2];
                valueStartIndex = 3;
                x = std::stof(argv[valueStartIndex]);
                y = std::stof(argv[valueStartIndex + 1]);
                z = std::stof(argv[valueStartIndex + 2]);
                if (argc > valueStartIndex + 3) {
                    filePort = std::stoi(argv[valueStartIndex + 3]);
                }
            } else {
                 if (argc < 5) {
                    printUsage(argv[0]);
                    return 1;
                }
                x = std::stof(argv[2]);
                y = std::stof(argv[3]);
                z = std::stof(argv[4]);
                if (argc > 5) {
                    filePort = std::stoi(argv[5]);
                }
            }

            std::cout << "文件模式: 尝试更新文件 " << filePath << std::endl;
            std::cout << "    设置值: X=" << x << ", Y=" << y << ", Z=" << z << ", Port=" << filePort << std::endl;
            if (GyroFileUtils::updateGyroSettingsInFile(filePath, x, y, z, filePort)) {
                std::cout << "文件更新成功。" << std::endl;
            } else {
                std::cerr << "文件更新失败。" << std::endl;
                return 1;
            }

        } else {
            std::cerr << "错误: 未知模式 '" << mode << "'" << std::endl;
            printUsage(argv[0]);
            return 1;
        }

    } catch (const std::invalid_argument& e) {
        std::cerr << "参数转换错误: " << e.what() << std::endl;
        printUsage(argv[0]);
        return 1;
    } catch (const std::out_of_range& e) {
        std::cerr << "参数超出范围: " << e.what() << std::endl;
        printUsage(argv[0]);
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "发生错误: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}