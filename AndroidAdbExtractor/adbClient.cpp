#include "adbClient.h"

void ADBClient::initSocket()
{
#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
}

void ADBClient::cleanupSocket()
{
#ifdef _WIN32
    closesocket(sock)
        WSACleanupp();
#else
    close(sock);
#endif
}

bool ADBClient::sendData(const std::string &data)
{
    int sent = send(sock, data.c_str(), data.length(), 0);
    return sent == (int)data.length();
}

std::string ADBClient::receiveData(int length)
{
    std::vector<char> buffer(length);
    int receive = recv(sock, buffer.data(), length, 0);
    if (receive <= 0)
        return "";
    return std::string(buffer.data(), receive);
}

bool ADBClient::receiveExact(char *buffer, int length)
{
    int total = 0;
    while (total < length)
    {
        int received = recv(sock, buffer + total, length - total, 0);
        if (received <= 0)
            return false;
        total += received;
    }
    return true;
}

bool ADBClient::sendADBCommand(const std::string &cmd)
{
    char length[5];
    snprintf(length, sizeof(length), "%04X", (unsigned int)cmd.length());
    std::string full_cmd = std::string(length) + cmd;
    return sendData(full_cmd);
}

bool ADBClient::receiveADBStatus()
{
    char status[5] = {0};
    if (!receiveExact(status, 4))
        return false;
    return (std::string(status) == "OKAY");
}

bool ADBClient::connect()
{
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        std::cerr << "Socket creation failed" << std::endl;
        return false;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, host.c_str(), &server_addr.sin_addr);

    if (::connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        std::cerr << "Connect ADB Server Failed! (" << host << ":" << port << ")" << std::endl;
        std::cerr << "Make sure ADB Server is running: adb start-server" << std::endl;
        cleanupSocket();
        return false;
    }

    connected = true;
    std::cout << "Connected to ADB Server at " << host << ":" << port << std::endl;
    return true;
}

bool ADBClient::disconnect()
{
    if (connected)
    {
        cleanupSocket();
        connected = false;
    }
}

std::vector<std::string> ADBClient::getDevices()
{
    std::vector<std::string> devices;
    if (!connected)
        return devices;

    if (!sendADBCommand("host:devices"))
    {
        std::cerr << "Failed to send devices command" << std::endl;
        return devices;
    }

    if (!receiveADBStatus())
    {
        std::cerr << "ADB Server returned error for devices command" << std::endl;
        return devices;
    }

    char length_buf[5] = {0};
    if (!receiveExact(length_buf, 4))
    {
        std::cerr << "Failed to receive devices response length" << std::endl;
        return devices;
    }

    unsigned int length = strtoul(length_buf, nullptr, 16);
    std::string response = receiveData(length);
    if (response.empty())
    {
        std::cerr << "Failed to receive devices response data" << std::endl;
        return devices;
    }
    // 解析设备列表
    size_t pos = 0;
    while (pos < response.length())
    {
        size_t tab_pos = response.find('\t', pos);
        if (tab_pos == std::string::npos)
            break;

        std::string device_id = response.substr(pos, tab_pos - pos);
        if (!device_id.empty())
        {
            devices.push_back(device_id);
        }

        pos = response.find('\n', tab_pos);
        if (pos == std::string::npos)
            break;
        pos++;
    }

    return devices;
}

bool ADBClient::selectDevice(const std::string &serial)
{
    std::string cmd = "host:transport:" + serial;
    if (!sendADBCommand(cmd))
        return false;
    return receiveADBStatus();
}

std::string ADBClient::executeShell(const std::string &command)
{
    std::string cmd = "shell:" + command;
    if (!sendADBCommand(cmd))
    {
        std::cerr << "Failed to send shell command" << std::endl;
        return "";
    }
    if (!receiveADBStatus())
        return "";

    std::string result;
    char buffer[4096];
    int received;

    while ((received = recv(sock, buffer, sizeof buffer, 0)) > 0)
    {
        result.append(buffer, received);
    }
    return result;
}

bool ADBClient::syncConnect()
{
    if (!sendADBCommand("sync:"))
        return false;
    return receiveADBStatus();
}

bool ADBClient::receiveFile(const std::string &remote_path, const std::string &local_path)
{
    // 发送STAT命令检查文件
    std::string stat_cmd = "STAT ";
    stat_cmd += std::string(4, '\0'); // 长度占位
    uint32_t path_len = remote_path.length();
    stat_cmd[4] = (path_len >> 0) & 0xFF;
    stat_cmd[5] = (path_len >> 8) & 0xFF;
    stat_cmd[6] = (path_len >> 16) & 0xFF;
    stat_cmd[7] = (path_len >> 24) & 0xFF;
    stat_cmd += remote_path;

    if (!sendData(stat_cmd))
    {
        std::cerr << "Failed to send STAT command" << std::endl;
        return false;
    }

    char response[16];
    if (!receiveExact(response, 16))
        return false;

    if (std::string(response, 4) != "STAT")
    {
        std::cerr << "File not found on device: " << remote_path << std::endl;
        return false;
    }

    // 发送RECV命令接收文件
    std::string recv_cmd = "RECV ";
    recv_cmd += std::string(4, '\0'); // 长度占位
    recv_cmd[4] = (path_len >> 0) & 0xFF;
    recv_cmd[5] = (path_len >> 8) & 0xFF;
    recv_cmd[6] = (path_len >> 16) & 0xFF;
    recv_cmd[7] = (path_len >> 24) & 0xFF;
    recv_cmd += remote_path;

    if (!sendData(recv_cmd))
    {
        std::cerr << "Failed to send RECV command" << std::endl;
        return false;
    }

    // 接收数据
    std::ofstream outfile(local_path, std::ios::binary);
    if (!outfile.is_open())
    {
        std::cerr << "Failed to open local file for writing: " << local_path << std::endl;
        return false;
    }
    while (true)
    {
        char chunk_header[8];
        if (!receiveExact(chunk_header, 8))
        {
            std::cerr << "Failed to receive chunk header" << std::endl;
            break;
        }

        std::string chunk_id(chunk_header, 4);
        uint32_t chunk_size = *(uint32_t *)(chunk_header + 4);
        if (chunk_id == "DONE")
            break;
        if (chunk_id == "FAIL")
        {
            std::cerr << "File transfer failed from device" << std::endl;
            outfile.close();
            return false;
        }
        if (chunk_id == "DATA")
        {
            std::vector<char> data(chunk_size);
            if (!receiveExact(data.data(), chunk_size))
            {
                std::cerr << "Failed to receive data chunk" << std::endl;
                break;
            }
            outfile.write(data.data(), chunk_size);
        }
    }
    outfile.close();
    return true;
}

std::vector<std::string> ADBClient::listDirectory(const std::string &path)
{
    std::vector<std::string> files;
    std::string result = executeShell("ls -l " + path);

    size_t pos = 0;
    while (pos < result.length())
    {
        size_t newline = result.find('\n', pos);
        if (newline == std::string::npos)
        {
            break;
        }
        std::string file = result.substr(pos, newline - pos);
        if (!file.empty() && file.find("Permisson denied") == std::string::npos)
        {
            files.push_back(file);
        }
        pos = newline + 1;
    }
    return files;
}