#ifndef GYROHOOK_HPP
#define GYROHOOK_HPP

#include <string>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

class GyroClient {
public:
    GyroClient(const std::string& serverIp = "127.0.0.1", int serverPort = 16384);
    ~GyroClient();

    bool connect();
    void disconnect();
    bool sendGyroData(float x, float y, float z);
    bool isConnected() const;

private:
    int sock;
    std::string ip;
    int port;
    bool connected;
};

namespace GyroFileUtils {
    bool updateGyroSettingsInFile(const std::string& filePath, float x, float y, float z, int portVal = 16384);
}

#endif