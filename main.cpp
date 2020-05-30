#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <algorithm>
#include <set>
#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <vector>
#include <sstream>
#include <fstream>

//#define ERROR -1
#define CLIENT_IP "192.168.15.1"

int ctrl_socket, data_socket;

bool isIpCorrect(char *ip) {
    ushort count = 0;
    char sym = ip[0];
    while (sym != '\0') {
        if (sym == '.') {
            count++;
        }
        if (count > 3) {
            return false;
        }
        sym++;
    }
    return true;
}

void initCntrlSock() {
    char ip[15];
    printf("Enter IP >> ");
    scanf("%s", ip);
    if (!isIpCorrect(ip)) {
        printf("Incorrect IP\n");
    }
    sockaddr_in address;
    ctrl_socket = socket(AF_INET, SOCK_STREAM, 0);
    address.sin_family = AF_INET;
    address.sin_port = htons(21);
    address.sin_addr.s_addr = inet_addr(ip);
    if (connect(ctrl_socket, (sockaddr *) &address, sizeof(address)) < 0) {
        perror("socket");
    }
}

void send_request(int sock, const char *buf_request) {
//    std::cout << "request: " << buf_request;
    const char *buf = (char *) buf_request;    // can't do pointer arithmetic on void*
    int send_size; // size in bytes sent or -1 on error
    size_t size_left; // size left to send
    const int flags = 0;

    size_left = strlen(buf_request);
    while (size_left > 0) {
        if ((send_size = send(sock, buf, size_left, flags)) == -1) {
            std::cout << "send error: " << std::endl;
        }
        size_left -= send_size;
        buf += send_size;
    }
}

std::string get_response(int sock) {
    int recv_size; // size in bytes received or -1 on error
    const int flags = 0;
    const int size_buf = 1024;
    char buf[size_buf];

    if ((recv_size = recv(sock, buf, size_buf, flags)) == -1) {
        std::cout << "recv error: " << strerror(errno) << std::endl;
    }

    std::string str(buf, recv_size);
    return str;
}

void login() {
    printf("Enter login >> ");
    char name[64];
    scanf("%s", name);
    char str[512];
    sprintf(str, "USER %s\r\n", name);
    send_request(ctrl_socket, str);
    std::cout << get_response(ctrl_socket);
    printf("Enter password >> ");
    char pass[64];
    scanf("%s", pass);
    sprintf(str, "PASS %s\r\n", pass);
    send_request(ctrl_socket, str);
    std::cout << get_response(ctrl_socket);
}

std::string open_PORT_listening_socket(bool printPort) {
    data_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (data_socket < 0) {
        perror("dataSock");
        exit(3);
    }
    struct sockaddr_in dataAddr;
    dataAddr.sin_family = AF_INET;
    dataAddr.sin_port = htons(0);
    inet_aton(CLIENT_IP, &dataAddr.sin_addr);

    if (bind(data_socket, (struct sockaddr *) &dataAddr, sizeof(dataAddr)) < 0) {
        perror("bind");
        exit(4);
    }
    socklen_t len = sizeof(dataAddr);
    int dataPort = 0;
    if (getsockname(data_socket, (sockaddr *) &dataAddr, &len) == -1) {
        perror("getsockname");
        exit(5);
    } else {
        dataPort = ntohs(dataAddr.sin_port);
        if (printPort) {
            std::printf("Listening port %d...\n", dataPort);
        }
    }
    listen(data_socket, 2);
    int portP1 = dataPort / 256, portP2 = dataPort % 256;
    std::stringstream ss;
    std::string addrPORT(inet_ntoa(dataAddr.sin_addr));
    std::replace(addrPORT.begin(), addrPORT.end(), '.', ',');
    ss << "PORT " << addrPORT << "," << portP1 << "," << portP2 << "\r\n";
    return ss.str();
}

std::string getDataSockRespose() {
    int sock2 = accept(data_socket, nullptr, nullptr);
    if (sock2 < 0) {
        perror("accept");
        exit(3);
    }
    return get_response(sock2);
}

std::vector<std::string> receive_list(int sock) {
    int recv_size; // size in bytes received or -1 on error
    const int flags = 0;
    const int size_buf = 255;
    char buf[size_buf];
    std::string str_nlst;

    int sock2 = accept(sock, nullptr, nullptr);
    if (sock2 < 0) {
        perror("accept");
        exit(3);
    }

    std::vector<std::string> list;

    while (true) {
        if ((recv_size = recv(sock2, buf, size_buf, flags)) == -1) {
            std::cout << "recv error: " << strerror(errno) << std::endl;
            exit(1);
        }
        if (recv_size == 0) {
            break;
        }
        for (int i = 0; i < recv_size; i++) {
            str_nlst += buf[i];
        }
    }

    size_t start = 0;
    size_t count = 0;
    for (size_t idx = 0; idx < str_nlst.size() - 1; idx++) {
        //detect CRLF
        if (str_nlst.at(idx) == '\r' && str_nlst.at(idx + 1) == '\n') {
            count = idx - start;
            std::string str = str_nlst.substr(start, count);
            start = idx + 2;
//            std::cout << str << std::endl;
            list.push_back(str);
        }
    }
    return list;
}

void nlst() {
    char buf_request[255];
    std::string str_rsp;
    std::vector<std::string> list;
    std::string portCommand = open_PORT_listening_socket(true);
    send_request(ctrl_socket, portCommand.c_str());
    std::cout << get_response(ctrl_socket);
    sprintf(buf_request, "NLST\r\n");
    send_request(ctrl_socket, buf_request);
    std::cout << get_response(ctrl_socket);
    list = receive_list(data_socket);
    for (auto item : list) {
        std::cout << item << std::endl;
    }
    std::cout << get_response(ctrl_socket);
    close(data_socket);
}

// 250 CWD command successful
void saveDataToDisk() {
    std::string folderName, str_rsp, code;
    uint count = 0;
    do {
        std::cout << "Введите название папки >> ";
        std::cin >> folderName;
        send_request(ctrl_socket, ("CWD " + folderName + "\r\n").c_str());
        str_rsp = get_response(ctrl_socket);
        std::cout << str_rsp;
        code = str_rsp.substr(0, 3);
    } while (code == "550");

    char buf_request[255];
    std::vector<std::string> list;
    std::string portCommand = open_PORT_listening_socket(false);
    send_request(ctrl_socket, portCommand.c_str());
    str_rsp = get_response(ctrl_socket);
    sprintf(buf_request, "NLST\r\n");
    send_request(ctrl_socket, buf_request);
    str_rsp = get_response(ctrl_socket);
    list = receive_list(data_socket);
    str_rsp = get_response(ctrl_socket);
    close(data_socket);

    std::ofstream fout;

    for (auto item : list) {
        if (item.find(".") != std::string::npos) {
            portCommand = open_PORT_listening_socket(false);
            send_request(ctrl_socket, portCommand.c_str());
            str_rsp = get_response(ctrl_socket);
            sprintf(buf_request, "%s", ("RETR " + item + "\r\n").c_str());
            send_request(ctrl_socket, buf_request);
            str_rsp = get_response(ctrl_socket);
            str_rsp = getDataSockRespose();
            fout.open(item);
            fout << str_rsp;
            fout.close();
            str_rsp = get_response(ctrl_socket);
            close(data_socket);
        }
    }
}

void quit() {
    char buff[256];
    sprintf(buff, "QUIT\r\n");
    send_request(ctrl_socket, buff);
    std::cout << get_response(ctrl_socket);
}

int main() {
    initCntrlSock();
    std::cout << get_response(ctrl_socket);
    login();
    int choice = -1;
    while (choice != 0) {
        std::cout
                << "Выберите действие:\n0. Выход\n1. Список файлов и папок в корневой директории\n"
                   "2. Скачать все файлы из папки на диск\n>> ";
        scanf("%d", &choice);
        switch (choice) {
            case 0:
                quit();
                break;
            case 1:
                nlst();
                break;
            case 2:
                saveDataToDisk();
                break;
            default:
                std::cout << "Unknown command." << std::endl;
                break;
        }
    }
    close(ctrl_socket);  ///закрытие соединения
    return 0;
}