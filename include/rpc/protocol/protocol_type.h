//
// Created by remix on 23-7-23.
//

#ifndef MUSE_RPC_PROTOCOL_TYPE_H
#define MUSE_RPC_PROTOCOL_TYPE_H
namespace muse::rpc{

    //协议类型
    enum class ProtocolType:unsigned char{
        RequestSend = 0,                    // 纯数据报文
        HearBeat = 1,                       // 判断对象机器是否正常运行  发 HearBeat 回  HearBeat
        ReceiverACK = 2,                    // 接收方确认数据
        RequestACK = 3,                     // 确认 ACK  关心阶段
        TimedOutRequestHeartbeat = 4,       // 心跳请求 报文还在处理吗？
        TimedOutResponseHeartbeat = 5,      // 心跳响应
        UnsupportedNetworkProtocol = 6,     // 网络数据格式不正确
        StateReset = 7,                     // 链接已经重置了
        TheServerResourcesExhausted = 8,    // 服务器资源已经耗尽
    };
}
#endif //MUSE_RPC_PROTOCOL_TYPE_H
