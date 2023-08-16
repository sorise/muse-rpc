//
// Created by remix on 23-7-16.
//

#ifndef MUSE_PROTOCOL_CONF_HPP
#define MUSE_PROTOCOL_CONF_HPP
#include <memory>
#include <netinet/in.h>
#include "serializer/util.h"
#include "protocol_type.h"
#include "communication_phase.h"
/*
 * RPC 协议信息
 *    超时、确认、重传、序号协议
      结构体最小 2 字节 总大小为 30 字节
 * */

namespace muse::rpc{

    //协议头 结构体
    struct ProtocolHeader{
        // 八 字节
        uint8_t synchronousWord;          // 1 同步字  全1111 0000
        CommunicationPhase phase;          // 1 协议阶段
        ProtocolType type;                // 1 协议类型
        uint16_t pieceOrder;              // 2 序号
        uint16_t pieceSize;               // 2 当前分片的数据部分有多少个字节
        uint16_t acceptMinOrder;          // 2 发送数据端希望收到的确认号
        // 八 字节
        uint64_t timePoint;               // 8 区分不同的包 报文 ID
        // 八 字节
        uint16_t totalCount;                   // 2 总共有多少个包
        uint16_t pieceStandardSize ;      // 2 标准大小
        uint32_t totalSize;               // 4 UDP完整报文的大小
        // 二 字节
        uint16_t acceptOrder;             // 2 确认序号
    };

    class Protocol{
    private:
        muse::serializer::ByteSequence by; //当前主机的字节序
    public:
        muse::serializer::ByteSequence get_byte_sequence();

        Protocol();
        // 协议头大小
        static constexpr const unsigned int protocolHeaderSize = 26;
        // 每一个分片的数据部分容量
        static constexpr const unsigned int defaultPieceLimitSize = 548 - protocolHeaderSize; // MTU 576 - 8 - 20 - 26 = 522
        // 协议头大小
        static constexpr const unsigned int FullPieceSize = protocolHeaderSize + defaultPieceLimitSize;
        // 默认协议同步字
        static constexpr const unsigned char defaultSynchronousWord = 0b11110000;

        //协议初始化, 一般是 client 发送方调用！
        bool initiateSenderProtocolHeader(char *protocol, uint64_t timePoint, uint32_t totalSize, size_t protocolSize) noexcept;
        //设置piece序号 和 大小
        void setProtocolOrderAndSize(char *protocol, uint16_t order, uint16_t size) noexcept;
        //解析 一般是 server 调用
        ProtocolHeader parse(char * protocol, size_t protocolSize, bool &success) noexcept;
        //解析完毕 存放到啊堆中
        std::shared_ptr<ProtocolHeader> memory_parse(char * protocol, size_t protocolSize) noexcept;
        // 获得协议类型 随意使用会有越界错误
        void setProtocolType(char * protocol, ProtocolType type);
        // 获得协议类型 随意使用会有越界错误, 用于单元测试
        ProtocolType getProtocolType(char *protocol);
        //设置协议阶段
        void setProtocolPhase(char * protocol,CommunicationPhase phase);
        //获取协议阶段
        CommunicationPhase getProtocolPhase(char * protocol);

        void setACKAcceptOrder(char *protocol, const uint16_t& order);

        void setAcceptMinOrder(char *protocol, const uint16_t& _accept_order);

        uint16_t getAcceptMinOrder(char *protocol);

        static int getSendCount(const uint16_t& piece_count);

        ~Protocol() = default;
    };

}

#endif //MUSE_PROTOCOL_CONF_HPP
