#include <iostream>
#include <random>
#include <exception>
#include <chrono>
#include <zlib.h>
#include "rpc/protocol/conf.hpp"
#include "timer/timer_wheel.hpp"
#include "rpc/server/reactor.hpp"
#include "logger/conf.hpp"
#include "rpc/server/reactor.hpp"
#include "rpc/protocol/conf.hpp"
#include "serializer/binarySerializer.h"
#include "timer/timer_wheel.hpp"

using namespace muse::logger;
using namespace muse::rpc;
using namespace muse::timer;
using namespace std::chrono_literals;

void creatClient(){
    std::cout << "start client:" << "\n";
    auto client_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (client_fd == -1) throw std::runtime_error("create the socket failed!");

    //服务器端口 + IP
    sockaddr_in server_address{
            AF_INET,
            htons(15000),
    };
    if (inet_aton("127.0.0.1",&server_address.sin_addr) == 0){
        throw std::runtime_error("ip address is not right!");
    };
    socklen_t len = sizeof(server_address);

    int timers = 1;
    while (timers -- > 0){
        struct timeval timeout = {3,0};
        setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(struct timeval));

        //创建一个二进制序列化器
        muse::serializer::BinarySerializer serializer;
        int32_t number = 10;
        std::list<std::string> names{"remix", "muse", "coco" , "tome", "alice" };
        std::vector<double> scores = {84.01,98.1,15.2,98.2,15.89,84.01,98.1,15.2,98.2,15.89};
        //序列化
        serializer.inputArgs(number, names, scores);
        auto dataCount = serializer.byteCount();

        //头部
        char sendData[1024];
        Protocol::initiateSenderProtocolHeader(sendData, TimerWheel::GetTick().count(), dataCount, Protocol::protocolHeaderSize);

        std::memcpy(sendData + Protocol::protocolHeaderSize, serializer.getBinaryStream(), dataCount); //body

        auto result = sendto(client_fd, sendData, dataCount + Protocol::protocolHeaderSize,
                             0 , (struct sockaddr*)&server_address, sizeof(server_address));

        if (result == -1){
            throw std::runtime_error("sendto error!");
        }else{
            std::cout << "send success!" << std::endl;
        }

        char buffer[4096];
        auto read_count = recvfrom(client_fd, buffer, 4096, 0, (struct sockaddr*)&server_address, &len);
        if (read_count < 0) {
            std::cout << "the connect has been end!" << std::endl;
        }
        for (int i = 0; i < read_count; ++i) {
            if (buffer[i] == '\0') break;
            printf("%c", buffer[i]);
        }
        std::this_thread::sleep_for(std::chrono::seconds(10));
    }
    close(client_fd);
}


int main(int argc, char *argv[]){
    creatClient();
    return 0;
}