#pragma once
// adb_client.h
#ifndef ADB_CLIENT_H
#define ADB_CLIENT_H

#include <string>
#include <vector>
#include <cstring>
#include <iostream>
#include <fstream>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
typedef SOCKET socket_t;
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
typedef int socket_t;
#endif

class ADBClient
{
private:
    socket_t sock;
    std::string host;
    int port;
    bool connected;

    // 初始化socket库（Windows only）
    void initSocket();

    // 清理socket
    void cleanupSocket();

    // 发送数据
    bool sendData(const std::string &data);

    // 接收数据
    std::string receiveData(int length);

    // 接收固定长度数据
    bool receiveExact(char *buffer, int length);

    // 发送ADB命令格式：4字节长度（hex）+ 命令
    bool sendADBCommand(const std::string &cmd);

    // 接收abd响应状态
    bool receiveADBStatus();

public:
    ADBClient(const std::string &h = "127.0.0.1", int p = 5037)
        : host(h), port(p), connected(false), sock(-1)
    {
        initSocket();
    }

    ~ADBClient()
    {
        disconnect();
    }

    // 连接ABD服务器
    bool connect();

    // 断开连接
    bool disconnect();

    // 获取设备列表
    std::vector<std::string> getDevices();

    // 选择设备（用于后续命令）
    bool selectDevice(const std::string &serial);

    // 执行shell命令
    std::string executeShell(const std::string &command);

    // 同步连接(传输文件)
    bool syncConnect();

    // 接收文件
    bool receiveFile(const std::string &remote_path, const std::string &local_path);

    // 列出目录
    std::vector<std::string> listDirectory(const std::string &path);
};

#endif // ADB_CLIENT_H