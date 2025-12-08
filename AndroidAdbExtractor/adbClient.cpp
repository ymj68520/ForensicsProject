#include "adbClient.h"
#include <cstdint>
#include <cstdlib>

void ADBClient::initSocket()
{
    // Socket initialization moved to constructor/connect logic
}

void ADBClient::cleanupSocket()
{
    if (sock != -1) {
#ifdef _WIN32
        closesocket(sock);
#else
        close(sock);
#endif
        sock = -1;
    }
}

ADBClient::ADBClient(const std::string &h, int p)
    : host(h), port(p), connected(false), sock(-1)
{
#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
}

ADBClient::~ADBClient()
{
    disconnect();
#ifdef _WIN32
    WSACleanup();
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
    if (!receiveExact(buffer.data(), length))
        return "";
    return std::string(buffer.data(), length);
}

bool ADBClient::receiveExact(char *buffer, int length)
{
    int total = 0;
    while (total < length)
    {
        int received = recv(sock, buffer + total, length - total, 0);
        if (received <= 0) {
            return false;
        }
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
    
    std::string s(status);
    if (s == "OKAY") return true;
    
    if (s == "FAIL") {
        char len_buf[5] = {0};
        if (receiveExact(len_buf, 4)) {
            unsigned int length = strtoul(len_buf, nullptr, 16);
            std::string error = receiveData(length);
            std::cerr << "ADB Error: " << error << std::endl;
        }
    } else {
         std::cerr << "ADB Unknown Status: " << s << std::endl;
    }
    return false;
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
    return true;
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
    // Parse device list
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

bool ADBClient::receiveFile(const std::string &remote_path, const std::string &local_path, uint32_t total_file_size)
{
    // Send STAT command to check file
    std::string stat_cmd = "STAT";
    stat_cmd += std::string(4, '\0'); // Placeholder for length
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

    // Send RECV command to receive file
    std::string recv_cmd = "RECV";
    recv_cmd += std::string(4, '\0'); // Placeholder for length
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

    // Receive data
    std::ofstream outfile(local_path, std::ios::binary);
    if (!outfile.is_open())
    {
        std::cerr << "Failed to open local file for writing: " << local_path << std::endl;
        return false;
    }
    
    uint64_t total_received_bytes = 0; // Use uint64_t for large files

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
            total_received_bytes += chunk_size;

            if (total_file_size > 0) {
                int progress = (total_received_bytes * 100) / total_file_size;
                std::cout << "\rReceiving: " << remote_path << " - " << progress << "% (" 
                          << total_received_bytes << "/" << total_file_size << " bytes)";
                std::cout.flush();
            }
        }
    }
    outfile.close();
    
    if (total_file_size > 0) {
        std::cout << std::endl; // New line after progress bar
    }
    return true;
}

bool ADBClient::statFile(const std::string& remote_path, uint32_t& mode, uint32_t& size, uint32_t& time) {
    char len_buf[4];
    uint32_t len = remote_path.length();
    len_buf[0] = (len >> 0) & 0xFF;
    len_buf[1] = (len >> 8) & 0xFF;
    len_buf[2] = (len >> 16) & 0xFF;
    len_buf[3] = (len >> 24) & 0xFF;
    
    std::string msg = "STAT" + std::string(len_buf, 4) + remote_path;

    if (!sendData(msg)) return false;

    char header[4];
    if (!receiveExact(header, 4)) return false;
    if (std::string(header, 4) != "STAT") return false;

    char stat_data[12];
    if (!receiveExact(stat_data, 12)) return false;

    mode = *(uint32_t*)(stat_data);
    size = *(uint32_t*)(stat_data + 4);
    time = *(uint32_t*)(stat_data + 8);
    
    return true;
}

std::vector<ADBClient::SyncEntry> ADBClient::listDirectory(const std::string &path)
{
    std::vector<SyncEntry> entries;
    
    char len_buf[4];
    uint32_t len = path.length();
    len_buf[0] = (len >> 0) & 0xFF;
    len_buf[1] = (len >> 8) & 0xFF;
    len_buf[2] = (len >> 16) & 0xFF;
    len_buf[3] = (len >> 24) & 0xFF;
    
    std::string msg = "LIST" + std::string(len_buf, 4) + path;
    if (!sendData(msg)) return entries;

    while(true) {
        char header[4];
        if (!receiveExact(header, 4)) break;
        std::string id(header, 4);
        
        if (id == "DONE") break;
        if (id == "FAIL") {
            char len_buf[4];
            if (receiveExact(len_buf, 4)) {
                uint32_t len = *(uint32_t*)len_buf;
                receiveData(len); // Consume error message
            }
            break;
        }
        if (id != "DENT") break;

        char stat_data[16];
        if (!receiveExact(stat_data, 16)) break;

        SyncEntry entry;
        entry.mode = *(uint32_t*)(stat_data);
        entry.size = *(uint32_t*)(stat_data + 4);
        entry.time = *(uint32_t*)(stat_data + 8);
        uint32_t name_len = *(uint32_t*)(stat_data + 12);

        if (name_len > 0) {
            std::vector<char> name_buf(name_len);
            if (!receiveExact(name_buf.data(), name_len)) break;
            entry.name = std::string(name_buf.data(), name_len);
            entries.push_back(entry);
        }
    }
    return entries;
}
