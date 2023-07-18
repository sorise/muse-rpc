#include "conf.hpp"

namespace muse::rpc{

    //减少复制成本
    bool Protocol::initiateSenderProtocolHeader(char *protocol, uint64_t timePoint, uint32_t totalSize, size_t protocolSize) noexcept {
        if (protocolSize < Protocol::protocolHeaderSize){
            return false;
        }
        // synchronousWord 设置同步字
        (*reinterpret_cast<unsigned char*>(protocol)) = Protocol::defaultSynchronousWord;
        // versionAndType 设置
        auto version = static_cast<unsigned char>(ProtocolVersion::VERSION1) << 4;
        auto type = static_cast<unsigned char>(ProtocolType::SenderSend);
        unsigned char versionAndType = version | type;
        *((unsigned char*)(protocol + 1 )) = versionAndType;

        // pieceOrder 当前分片的序号 有待后续修改
        *(reinterpret_cast<uint16_t*>(protocol + 2)) = 0;
        // 当pieceSize 当前分片的大小 有待后续修改
        *(reinterpret_cast<uint16_t *>(protocol + 4)) = Protocol::protocolHeaderSize;
        // timePoint 设置时间点
        *(reinterpret_cast<uint64_t*>(protocol + 6)) = timePoint;
        // totalCount 设置分片总数 count
        uint16_t count = totalSize/Protocol::defaultPieceLimitSize + (((totalSize % Protocol::defaultPieceLimitSize) == 0)?0:1);
        *(reinterpret_cast<uint16_t *>(protocol + 14)) = count;
        // pieceStandardSize 分片的标准大小
        *(reinterpret_cast<uint16_t *>(protocol + 16)) = Protocol::defaultPieceLimitSize;
        // totalSize UDP 完整大小
        *(reinterpret_cast<uint32_t *>(protocol + 18)) = totalSize;
        // acceptMinOrder 已经确认号 有待修改
        *(reinterpret_cast<uint16_t *>(protocol + 22)) = 0;
        // acceptOrder  发送方设置 确认号
        *(reinterpret_cast<uint16_t *>(protocol + 24)) = 0;
        return true;
    }

    ProtocolHeader Protocol::parse(char * protocol, size_t protocolSize, bool &success) noexcept{
        ProtocolHeader header{};
        if (protocolSize < Protocol::protocolHeaderSize){
            success = false;
            return header;
        }
        //同步字
        header.synchronousWord = *(uint8_t *)(protocol);
        if (header.synchronousWord != Protocol::defaultSynchronousWord){
            success = false;
            return header;
        }
        unsigned char versionAndType = *((unsigned char*)(protocol + 1 ));
        unsigned char version = versionAndType >> 4;
        unsigned char type = versionAndType & 00001111;

        //获得版本号
        switch (version) {
            case (unsigned char )ProtocolVersion::VERSION1 :
                header.version = ProtocolVersion::VERSION1;
                break;
            default:
                success = false;
                return header;
        }

        //获得协议类型
        switch (type) {
            case (unsigned char )ProtocolType::SenderSend :
                header.type = ProtocolType::SenderSend;
                break;
            case (unsigned char )ProtocolType::ReceiverACK :
                header.type = ProtocolType::ReceiverACK;
                break;
            case (unsigned char )ProtocolType::RequestACK :
                header.type = ProtocolType::RequestACK;
                break;
            case (unsigned char )ProtocolType::TimedOutRequestHeartbeat :
                header.type = ProtocolType::TimedOutRequestHeartbeat;
                break;
            case (unsigned char )ProtocolType::TimedOutResponseHeartbeat :
                header.type = ProtocolType::TimedOutResponseHeartbeat;
                break;
            case (unsigned char )ProtocolType::UnsupportedNetworkProtocol :
                header.type = ProtocolType::UnsupportedNetworkProtocol;
                break;
            default:
                success = false;
                return header;
        }

        header.pieceOrder = *(reinterpret_cast<uint16_t*>(protocol + 2));
        header.pieceSize = *(reinterpret_cast<uint16_t *>(protocol + 4));
        header.timePoint = *(reinterpret_cast<uint64_t *>(protocol + 6));
        header.totalCount = *(reinterpret_cast<uint16_t *>(protocol + 14));
        header.pieceStandardSize = *(reinterpret_cast<uint16_t *>(protocol + 16));
        header.totalSize = *(reinterpret_cast<uint32_t *>(protocol + 18));
        header.acceptMinOrder = *(reinterpret_cast<uint16_t *>(protocol + 22));
        header.acceptOrder = *(reinterpret_cast<uint16_t *>(protocol + 24));
        success = true; //解析成功
        return header;
    }

    std::shared_ptr<ProtocolHeader> Protocol::memory_parse(char *protocol, size_t protocolSize) noexcept {
        auto header = std::make_shared<ProtocolHeader>();
        if (protocolSize < Protocol::protocolHeaderSize){
            return nullptr;
        }
        //同步字
        header->synchronousWord = *(uint8_t *)(protocol);
        if (header->synchronousWord != Protocol::defaultSynchronousWord){
            return nullptr;
        }
        unsigned char versionAndType = *((unsigned char*)(protocol + 1 ));
        unsigned char version = versionAndType >> 4;
        unsigned char type = versionAndType & 00001111;

        //获得版本号
        switch(version) {
            case (unsigned char )ProtocolVersion::VERSION1 :
                header->version = ProtocolVersion::VERSION1;
                break;
            default:
                return nullptr;
        }

        //获得协议类型
        switch (type) {
            case (unsigned char )ProtocolType::SenderSend :
                header->type = ProtocolType::SenderSend;
                break;
            case (unsigned char )ProtocolType::ReceiverACK :
                header->type = ProtocolType::ReceiverACK;
                break;
            case (unsigned char )ProtocolType::RequestACK :
                header->type = ProtocolType::RequestACK;
                break;
            case (unsigned char )ProtocolType::TimedOutRequestHeartbeat :
                header->type = ProtocolType::TimedOutRequestHeartbeat;
                break;
            case (unsigned char )ProtocolType::TimedOutResponseHeartbeat :
                header->type = ProtocolType::TimedOutResponseHeartbeat;
                break;
            case (unsigned char )ProtocolType::UnsupportedNetworkProtocol :
                header->type = ProtocolType::UnsupportedNetworkProtocol;
                break;
            default:
                return nullptr;
        }

        header->pieceOrder = *(reinterpret_cast<uint16_t*>(protocol + 2));
        header->pieceSize = *(reinterpret_cast<uint16_t *>(protocol + 4));
        header->timePoint = *(reinterpret_cast<uint64_t *>(protocol + 6));
        header->totalCount = *(reinterpret_cast<uint16_t *>(protocol + 14));
        header->pieceStandardSize = *(reinterpret_cast<uint16_t *>(protocol + 16));
        header->totalSize = *(reinterpret_cast<uint32_t *>(protocol + 18));
        header->acceptMinOrder = *(reinterpret_cast<uint16_t *>(protocol + 22));
        header->acceptOrder = *(reinterpret_cast<uint16_t *>(protocol + 24));
        //解析成功
        return header;
    }

    void Protocol::setProtocolType(char *protocol, ProtocolType type) {
         uint8_t vt = (*(reinterpret_cast<uint8_t *>(protocol + 1))) & 0b11110000;
        //获得协议类型
        switch (type) {
            case ProtocolType::SenderSend:
                vt = vt | (unsigned char )ProtocolType::SenderSend;
                break;
            case ProtocolType::ReceiverACK :
                vt = vt | (unsigned char )ProtocolType::ReceiverACK;
                break;
            case ProtocolType::RequestACK :
                vt = vt | (unsigned char )ProtocolType::RequestACK;
                break;
            case ProtocolType::TimedOutRequestHeartbeat:
                vt = vt | (unsigned char )ProtocolType::TimedOutRequestHeartbeat;
                break;
            case ProtocolType::TimedOutResponseHeartbeat :
                vt = vt | (unsigned char )ProtocolType::TimedOutResponseHeartbeat;
                break;
            default:
                return;
        }
        *(reinterpret_cast<uint8_t*>(protocol + 1 )) = vt;
    }

    ProtocolType Protocol::getProtocolType(char *protocol) {
        uint8_t type = (*(reinterpret_cast<uint8_t *>(protocol + 1))) & 0b00001111;
        switch (type) {
            case (unsigned char )ProtocolType::SenderSend :
                return ProtocolType::SenderSend;
                break;
            case (unsigned char )ProtocolType::ReceiverACK :
                return ProtocolType::ReceiverACK;
                break;
            case (unsigned char )ProtocolType::RequestACK :
                return ProtocolType::RequestACK;
                break;
            case (unsigned char )ProtocolType::TimedOutRequestHeartbeat :
                return ProtocolType::TimedOutRequestHeartbeat;
                break;
            case (unsigned char )ProtocolType::TimedOutResponseHeartbeat :
                return ProtocolType::TimedOutResponseHeartbeat;
                break;
            case (unsigned char )ProtocolType::UnsupportedNetworkProtocol :
                return ProtocolType::UnsupportedNetworkProtocol;
                break;
            default:
                throw std::runtime_error("type error");
        }
    };



}