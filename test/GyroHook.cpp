#include "GyroHook.hpp"
#include <iostream>
#include <string>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>

GyroClient::GyroClient(const std::string& serverIp, int serverPort)
    : ip(serverIp), port(serverPort), connected(false), sock(-1) {}

GyroClient::~GyroClient() {
    disconnect();
}

bool GyroClient::connect() {
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        std::cerr << "GyroClient: 无法创建Socket" << std::endl;
        return false;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip.c_str(), &server_addr.sin_addr) <= 0) {
        std::cerr << "GyroClient: 无效的IP地址 " << ip << std::endl;
        close(sock);
        sock = -1;
        return false;
    }

    if (::connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "GyroClient: 连接失败到 " << ip << ":" << port << std::endl;
        close(sock);
        sock = -1;
        return false;
    }

    connected = true;
    std::cout << "GyroClient: 已连接到服务器 " << ip << ":" << port << std::endl;
    return true;
}

void GyroClient::disconnect() {
    if (sock != -1) {
        close(sock);
        sock = -1;
        connected = false;
        std::cout << "GyroClient: 已断开连接" << std::endl;
    }
}

bool GyroClient::sendGyroData(float x, float y, float z) {
    if (!connected) {
        std::cerr << "GyroClient: 未连接到服务器" << std::endl;
        return false;
    }

    std::ostringstream oss;
    oss << x << "," << y << "," << z << "\\n";
    std::string message = oss.str();

    if (send(sock, message.c_str(), message.length(), 0) != static_cast<ssize_t>(message.length())) {
        std::cerr << "GyroClient: 发送数据失败" << std::endl;
        return false;
    }
    return true;
}

bool GyroClient::isConnected() const {
    return connected;
}

namespace GyroFileUtils {
    bool updateGyroSettingsInFile(const std::string& filePath, float x, float y, float z, int portVal) {
        std::ofstream outFile(filePath, std::ios::out | std::ios::trunc);
        if (!outFile.is_open()) {
            std::cerr << "GyroFileUtils: 无法打开文件进行写入: " << filePath << std::endl;
            return false;
        }

        std::ostringstream xmlStream;
        xmlStream << std::fixed << std::setprecision(6);

        xmlStream << "<?xml version='1.0' encoding='utf-8' standalone='yes' ?>\n";
        xmlStream << "<map>\n";
        xmlStream << "    <float name=\"x\" value=\"" << x << "\" />\n";
        xmlStream << "    <float name=\"y\" value=\"" << y << "\" />\n";
        xmlStream << "    <float name=\"z\" value=\"" << z << "\" />\n";
        xmlStream << "    <int name=\"socket_port\" value=\"" << portVal << "\" />\n";
        xmlStream << "</map>";

        outFile << xmlStream.str();
        
        if (outFile.fail()) {
            std::cerr << "GyroFileUtils: 写入文件失败: " << filePath << std::endl;
            outFile.close();
            return false;
        }

        outFile.close();
        std::cout << "GyroFileUtils: 成功更新文件: " << filePath << std::endl;
        std::cout << "    新设置: X=" << x << ", Y=" << y << ", Z=" << z << ", Port=" << portVal << std::endl;
        return true;
    }
}