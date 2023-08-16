#include <algorithm>
#include "protocol.hpp"

using namespace muse::serializer;

namespace muse::rpc{

    //减少复制成本, 协议字段采用 网络字节序 也就是大端序
    bool Protocol::initiateSenderProtocolHeader(char *protocol, uint64_t timePoint, uint32_t totalSize, size_t protocolSize) noexcept {
        if (protocolSize < Protocol::protocolHeaderSize){
            return false;
        }
        // synchronousWord 设置同步字
        (*reinterpret_cast<unsigned char*>(protocol)) = Protocol::defaultSynchronousWord;
        // versionAndType 设置
        auto phase = static_cast<unsigned char>(CommunicationPhase::Request) << 4;
        auto type = static_cast<unsigned char>(ProtocolType::RequestSend);
        unsigned char phaseAndType = phase | type;
        *((unsigned char*)(protocol + 1 )) = phaseAndType;
        // pieceOrder 当前分片的序号 有待后续修改
        *(reinterpret_cast<uint16_t*>(protocol + 2)) =  0; //是 0 就不转换了
        //判断当前主机的字节序，如果是小端序 / 主机序 === 转换为 ===> 大端序
        uint16_t count = totalSize/Protocol::defaultPieceLimitSize + (((totalSize % Protocol::defaultPieceLimitSize) == 0)?0:1);
        // acceptMinOrder  已被确认号
        *(reinterpret_cast<uint16_t *>(protocol + 6)) = 0;
        if (by == ByteSequence::LittleEndian){
            //转换为大端序
            // 当pieceSize 当前分片的大小 有待后续修改
            *(reinterpret_cast<uint16_t *>(protocol + 4)) = htons(Protocol::protocolHeaderSize);
            // timePoint 设置时间点
            *(reinterpret_cast<uint64_t*>(protocol + 8)) = timePoint;
            char* first = protocol + 8; //特殊转换 变成
            char* last = first + sizeof(uint64_t);
            std::reverse(first, last);

            // totalCount 设置分片总数 count
            *(reinterpret_cast<uint16_t *>(protocol + 16)) = htons(count);
            // pieceStandardSize 分片的标准大小
            *(reinterpret_cast<uint16_t *>(protocol + 18)) = htons(Protocol::defaultPieceLimitSize);
            // totalSize UDP 完整大小
            *(reinterpret_cast<uint32_t *>(protocol + 20)) = htonl(totalSize);
            // acceptMinOrder 已经确认号 有待修改
        }else{ //已经大端序
            *(reinterpret_cast<uint16_t *>(protocol + 4)) = Protocol::protocolHeaderSize;
            *(reinterpret_cast<uint64_t*>(protocol + 8)) = timePoint;
            *(reinterpret_cast<uint16_t *>(protocol + 16)) = count;
            *(reinterpret_cast<uint16_t *>(protocol + 18)) = Protocol::defaultPieceLimitSize;
            *(reinterpret_cast<uint32_t *>(protocol + 20)) = totalSize;
        }
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
        unsigned char phaseAndType = *((unsigned char*)(protocol + 1 ));
        unsigned char phaseAndType_ = *((unsigned char*)(protocol + 1 ));
        unsigned char phase = phaseAndType >> 4;
        unsigned char type = phaseAndType_ & 0b00001111;

        //获得版本号
        switch (phase) {
            case (unsigned char )CommunicationPhase::Request :
                header.phase = CommunicationPhase::Request;
                break;
            case (unsigned char )CommunicationPhase::Response :
                header.phase = CommunicationPhase::Response;
                break;
            default:
                success = false;
                return header;
        }
        //获得协议类型
        switch (type) {
            case (unsigned char )ProtocolType::RequestSend :
                header.type = ProtocolType::RequestSend;
                break;
            case (unsigned char )ProtocolType::HearBeat :
                header.type = ProtocolType::HearBeat;
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
            case (unsigned char )ProtocolType::StateReset :
                header.type = ProtocolType::StateReset;
                break;
            case (unsigned char )ProtocolType::TheServerResourcesExhausted :
                header.type = ProtocolType::TheServerResourcesExhausted;
                break;
            default:
                success = false;
                return header;
        }

        if (by == ByteSequence::LittleEndian){
            header.pieceOrder = htons(*(reinterpret_cast<uint16_t*>(protocol + 2)));
            header.pieceSize = htons(*(reinterpret_cast<uint16_t *>(protocol + 4)));
            header.acceptMinOrder = htons(*(reinterpret_cast<uint16_t *>(protocol + 6)));
            header.timePoint =  *(reinterpret_cast<uint64_t *>(protocol + 8));
            //特殊转换
            char* first = (char*)&(header.timePoint);
            char* last = first + sizeof(uint64_t);
            std::reverse(first, last);

            header.totalCount = htons(*(reinterpret_cast<uint16_t *>(protocol + 16)));
            header.pieceStandardSize = htons(*(reinterpret_cast<uint16_t *>(protocol + 18)));
            header.totalSize = htonl(*(reinterpret_cast<uint32_t *>(protocol + 20)));
            header.acceptOrder = htons(*(reinterpret_cast<uint16_t *>(protocol + 24)));
        }else{
            header.pieceOrder = *(reinterpret_cast<uint16_t*>(protocol + 2));
            header.pieceSize = *(reinterpret_cast<uint16_t *>(protocol + 4));
            header.acceptMinOrder = *(reinterpret_cast<uint16_t *>(protocol + 6));
            header.timePoint = *(reinterpret_cast<uint64_t *>(protocol + 8));
            header.totalCount = *(reinterpret_cast<uint16_t *>(protocol + 16));
            header.pieceStandardSize = *(reinterpret_cast<uint16_t *>(protocol + 18));
            header.totalSize = *(reinterpret_cast<uint32_t *>(protocol + 20));
            header.acceptOrder = *(reinterpret_cast<uint16_t *>(protocol + 24));
        }
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
        unsigned char phaseAndType = *((unsigned char*)(protocol + 1 ));
        unsigned char phase = phaseAndType >> 4;
        unsigned char type = phaseAndType & 0b00001111;

        //获得版本号
        switch(phase) {
            case (unsigned char )CommunicationPhase::Request :
                header->phase = CommunicationPhase::Request;
                break;
            case (unsigned char )CommunicationPhase::Response :
                header->phase = CommunicationPhase::Response;
                break;
            default:
                return nullptr;
        }

        //获得协议类型
        switch (type) {
            case (unsigned char )ProtocolType::RequestSend :
                header->type = ProtocolType::RequestSend;
                break;
            case (unsigned char )ProtocolType::HearBeat :
                header->type = ProtocolType::HearBeat;
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
            case (unsigned char )ProtocolType::StateReset :
                header->type = ProtocolType::StateReset;
                break;
            case (unsigned char )ProtocolType::TheServerResourcesExhausted :
                header->type = ProtocolType::TheServerResourcesExhausted;
                break;
            default:
                return nullptr;
        }
        if (by == ByteSequence::LittleEndian){
            header->pieceOrder = htons(*(reinterpret_cast<uint16_t*>(protocol + 2)));
            header->pieceSize = htons(*(reinterpret_cast<uint16_t *>(protocol + 4)));
            header->acceptMinOrder = htons(*(reinterpret_cast<uint16_t *>(protocol + 6)));
            header->timePoint =  *(reinterpret_cast<uint64_t *>(protocol + 8));
            //特殊转换
            char* first = (char*)&((header)->timePoint);
            char* last = first + sizeof(uint64_t);
            std::reverse(first, last);

            header->totalCount = htons(*(reinterpret_cast<uint16_t *>(protocol + 16)));
            header->pieceStandardSize = htons(*(reinterpret_cast<uint16_t *>(protocol + 18)));
            header->totalSize = htonl(*(reinterpret_cast<uint32_t *>(protocol + 20)));
            header->acceptOrder = htons(*(reinterpret_cast<uint16_t *>(protocol + 24)));
        }else{
            header->pieceOrder = *(reinterpret_cast<uint16_t*>(protocol + 2));
            header->pieceSize = *(reinterpret_cast<uint16_t *>(protocol + 4));
            header->acceptMinOrder = *(reinterpret_cast<uint16_t *>(protocol + 6));
            header->timePoint = *(reinterpret_cast<uint64_t *>(protocol + 8));
            header->totalCount = *(reinterpret_cast<uint16_t *>(protocol + 16));
            header->pieceStandardSize = *(reinterpret_cast<uint16_t *>(protocol + 18));
            header->totalSize = *(reinterpret_cast<uint32_t *>(protocol + 20));
            header->acceptOrder = *(reinterpret_cast<uint16_t *>(protocol + 24));
        }
        return header;
    }

    void Protocol::setProtocolType(char *protocol, ProtocolType type) {
         uint8_t vt = (*(reinterpret_cast<uint8_t *>(protocol + 1))) & 0b11110000;
        //获得协议类型
        switch (type) {
            case ProtocolType::RequestSend:
                vt = vt | (unsigned char )ProtocolType::RequestSend;
                break;
            case ProtocolType::HearBeat:
                vt = vt | (unsigned char )ProtocolType::HearBeat;
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
            case ProtocolType::StateReset :
                vt = vt | (unsigned char )ProtocolType::StateReset;
                break;
            case ProtocolType::UnsupportedNetworkProtocol :
                vt = vt | (unsigned char )ProtocolType::UnsupportedNetworkProtocol;
                break;
            case ProtocolType::TheServerResourcesExhausted :
                vt = vt | (unsigned char )ProtocolType::TheServerResourcesExhausted;
                break;
            default:
                return;
        }
        *(reinterpret_cast<uint8_t*>(protocol + 1 )) = vt;
    }

    ProtocolType Protocol::getProtocolType(char *protocol) {
        uint8_t type = (*(reinterpret_cast<uint8_t *>(protocol + 1))) & 0b00001111;
        switch (type) {
            case (unsigned char )ProtocolType::HearBeat :
                return ProtocolType::HearBeat;
            case (unsigned char )ProtocolType::RequestSend :
                return ProtocolType::RequestSend;
            case (unsigned char )ProtocolType::ReceiverACK :
                return ProtocolType::ReceiverACK;
            case (unsigned char )ProtocolType::RequestACK :
                return ProtocolType::RequestACK;
            case (unsigned char )ProtocolType::TimedOutRequestHeartbeat :
                return ProtocolType::TimedOutRequestHeartbeat;
            case (unsigned char )ProtocolType::TimedOutResponseHeartbeat :
                return ProtocolType::TimedOutResponseHeartbeat;
            case (unsigned char )ProtocolType::UnsupportedNetworkProtocol :
                return ProtocolType::UnsupportedNetworkProtocol;
            case (unsigned char )ProtocolType::StateReset :
                return ProtocolType::StateReset;
            case (unsigned char )ProtocolType::TheServerResourcesExhausted :
                return ProtocolType::TheServerResourcesExhausted;
            default:
                throw std::runtime_error("type error");
        }
    }

    void Protocol::setProtocolOrderAndSize(char *protocol, uint16_t order, uint16_t size) noexcept {
        if (by == ByteSequence::LittleEndian){
            *(reinterpret_cast<uint16_t *>(protocol + 2)) = htons(order);
            // pieceStandardSize 分片的标准大小
            *(reinterpret_cast<uint16_t *>(protocol + 4)) = htons(size);
        }else{
            *(reinterpret_cast<uint16_t *>(protocol + 2)) = order;
            // pieceStandardSize 分片的标准大小
            *(reinterpret_cast<uint16_t *>(protocol + 4)) = size;
        }
    }

    void Protocol::setACKAcceptOrder(char *protocol, const uint16_t &order) {
        if (by == ByteSequence::BigEndian){
            *(reinterpret_cast<uint16_t *>(protocol + 24)) = order;
        }else{
            *(reinterpret_cast<uint16_t *>(protocol + 24)) = htons(order);
        }
    }

    Protocol::Protocol(){
        by = muse::serializer::getByteSequence();
    }

    void Protocol::setProtocolPhase(char *protocol, CommunicationPhase _phase) {
        uint8_t type = (*(reinterpret_cast<uint8_t *>(protocol + 1))) & 0b00001111;
        uint8_t phase = ((uint8_t)_phase) << 4;
        *(reinterpret_cast<uint8_t *>(protocol + 1)) = phase | type;
    }

    CommunicationPhase Protocol::getProtocolPhase(char *protocol) {
        uint8_t phase = *(reinterpret_cast<uint8_t *>(protocol + 1)) >> 4;
        //获得版本号
        switch(phase) {
            case (unsigned char )CommunicationPhase::Request :
                return CommunicationPhase::Request;
            case (unsigned char )CommunicationPhase::Response :
                return CommunicationPhase::Response;
            default:
                throw std::runtime_error("type error");
        }
    }

    void Protocol::setAcceptMinOrder(char *protocol, const uint16_t &_accept_order) {
        if (by == ByteSequence::LittleEndian){
            *(reinterpret_cast<uint16_t *>(protocol + 6)) = htons(_accept_order);
        }else{
            *(reinterpret_cast<uint16_t *>(protocol + 6)) = _accept_order;
        }
    };

    uint16_t Protocol::getAcceptMinOrder(char *protocol) {
        if (by == ByteSequence::LittleEndian){
           auto order = *(reinterpret_cast<uint16_t *>(protocol + 6));
            return  htons(order);
        }else{
            return *(reinterpret_cast<uint16_t *>(protocol + 6));
        }
    }

    int Protocol::getSendCount(const uint16_t& piece_count) {
        if (piece_count <= 3){
            return 1;
        }else if (piece_count > 3 && piece_count <= 10){
            return 3;
        }else if (piece_count > 10 && piece_count <= 1024){
            return 5;
        }else{
            return 7;
        }
    }

    muse::serializer::ByteSequence Protocol::get_byte_sequence() {
        return this->by;
    }

}