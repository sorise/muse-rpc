//
// Created by remix on 23-7-16.
//

#ifndef MUSE_PROTOCOL_CONF_HPP
#define MUSE_PROTOCOL_CONF_HPP
#include <iostream>
#include <memory>
/*
 * RPC 协议信息
 *    超时、确认、重传、序号协议
      结构体最小 2 字节 总大小为 30 字节
 * */

namespace muse::rpc{
    //协议类型
    enum class ProtocolType:unsigned char{
        SenderSend = 1, //发送方发送数据
        ReceiverACK = 2,  //接收方确认数据
        RequestACK = 3,   //请求重传
        TimedOutRequestHeartbeat = 4, // 心跳请求
        TimedOutResponseHeartbeat = 5, // 心跳响应
        UnsupportedNetworkProtocol = 6, // 网络数据格式不正确
    };

    //协议版本号
    enum class ProtocolVersion:unsigned char {
        VERSION1 = 1, //首个版本
    };

    //协议头 结构体
    struct ProtocolHeader{
        // 六 字节
        uint8_t synchronousWord;          // 1 同步字  全1111 0000
        ProtocolVersion version;          // 1 协议版本
        ProtocolType type;                // 1  协议类型
        uint16_t pieceOrder;              // 2 序号
        uint16_t pieceSize;               // 2 当前分片多少个字节数
        // 八 字节
        uint64_t timePoint;               // 8 区分不同的包 报文 ID
        // 八 字节
        uint16_t totalCount;                   // 2 总共有多少个包
        uint16_t pieceStandardSize ;      // 2 标准大小
        uint32_t totalSize;               // 4 UDP完整报文的大小
        // 四 字节
        uint16_t acceptMinOrder;          // 2 已被确认序号
        uint16_t acceptOrder;             // 2 确认序号
    };

    class Protocol{
    public:
        // 协议头大小
        static constexpr const unsigned int protocolHeaderSize = 26;
        // 每一个分片的数据部分容量
        static constexpr const unsigned int defaultPieceLimitSize = 548 - protocolHeaderSize; // MTU 576 - 8 - 20 - 26 = 522
        // 默认协议同步字
        static constexpr const unsigned char defaultSynchronousWord = 0b11110000;

        //协议初始化, 一般是 client 发送方调用！
        static bool initiateSenderProtocolHeader(char *protocol, uint64_t timePoint, uint32_t totalSize, size_t protocolSize) noexcept;
        //解析 一般是 server 调用
        static ProtocolHeader parse(char * protocol, size_t protocolSize, bool &success) noexcept;

        static std::shared_ptr<ProtocolHeader> memory_parse(char * protocol, size_t protocolSize) noexcept;
        // 获得协议类型 随意使用会有越界错误
        static void setProtocolType(char * protocol, ProtocolType type);
        // 获得协议类型 随意使用会有越界错误, 用于单元测试
        static ProtocolType getProtocolType(char *protocol);
    private:
        Protocol() = default;
    public:
        ~Protocol() = default;
    };

}

#endif //MUSE_PROTOCOL_CONF_HPP
