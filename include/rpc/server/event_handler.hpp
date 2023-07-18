//
// Created by remix on 23-7-17.
//

#ifndef MUSE_EVENT_HANDLER_HPP
#define MUSE_EVENT_HANDLER_HPP
#include <poll.h>
#include <arpa/inet.h>

namespace muse::rpc {
    /* 一个事件处理器 */
    class EventHandler {
    public:
        typedef void (*CallBack)(int fd, uint32_t events, void *arg);
    private:
        //要监听的文件描述符
        int socket_handle;
        //IPv4 地址
        struct sockaddr_in address;
        //上次接受到消息的时间
        uint64_t lastReceivedMessageTime;
        //什么时候向这个fd发送过消息
        uint64_t lastSendMessageTime;
        //监听的事件
        uint32_t events;
        //回调函数
        CallBack callBack;
        //是否在监听:1->在红黑树上(监听), 0->不在(不监听)
        bool status;
    public:
        EventHandler();
        //执行处理
        void runHandler();

    };
}
#endif //MUSE_EVENT_HANDLER_HPP
