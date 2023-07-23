#define CATCH_CONFIG_MAIN
#include "catch.hpp"
#include "rpc/protocol/protocol.hpp"
#include "timer/timer_wheel.hpp"

using namespace muse::rpc;

TEST_CASE("Protocol - init", "[protocol]"){
    char header[26];

    Protocol Protocol;

    Protocol.initiateSenderProtocolHeader(
           header,
           muse::timer::TimerWheel::GetTick().count(),
           512,
           Protocol::protocolHeaderSize
    );

    bool result = false;
    ProtocolHeader protocolHeader = Protocol.parse(header, Protocol::protocolHeaderSize, result);

    REQUIRE(result == true);
    REQUIRE(protocolHeader.synchronousWord == Protocol::defaultSynchronousWord);
    REQUIRE(protocolHeader.type == ProtocolType::RequestSend);
    REQUIRE(protocolHeader.pieceStandardSize == Protocol::defaultPieceLimitSize);
}


TEST_CASE("Protocol - type", "[protocol]"){
    char data[26];
    Protocol Protocol;

    Protocol.initiateSenderProtocolHeader(
            data,
            muse::timer::TimerWheel::GetTick().count(),
            512,
            Protocol::protocolHeaderSize
    );

    try {
        Protocol.setProtocolType(data, ProtocolType::TimedOutRequestHeartbeat);
        ProtocolType type = Protocol.getProtocolType(data);
        REQUIRE(type == ProtocolType::TimedOutRequestHeartbeat);

        Protocol.setProtocolType(data, ProtocolType::ReceiverACK);
        ProtocolType type1 = Protocol.getProtocolType(data);
        REQUIRE(type1 == ProtocolType::ReceiverACK);


        Protocol.setProtocolType(data, ProtocolType::StateReset);
        ProtocolType type2 = Protocol.getProtocolType(data);
        REQUIRE(type2 == ProtocolType::StateReset);

    }
    catch(...){
        REQUIRE(false);
    }
}

TEST_CASE("Protocol - parse", "[protocol]"){
    char data[26];
    Protocol protocol;

    protocol.initiateSenderProtocolHeader(
            data,
            muse::timer::TimerWheel::GetTick().count(),
            512,
            Protocol::protocolHeaderSize
    );

    try {
        protocol.setProtocolType(data, ProtocolType::TimedOutRequestHeartbeat);
        ProtocolType type = protocol.getProtocolType(data);
        bool is;
        auto header = protocol.parse(data, 26, is);
        REQUIRE( header.type  == type);

        protocol.setProtocolType(data, ProtocolType::ReceiverACK);
        ProtocolType type1 = protocol.getProtocolType(data);
        header = protocol.parse(data, 26, is);
        REQUIRE(header.type == type1);


        protocol.setProtocolType(data, ProtocolType::StateReset);
        ProtocolType type2 = protocol.getProtocolType(data);
        header = protocol.parse(data, 26, is);
        REQUIRE(header.type == type2);

    }
    catch(...){
        REQUIRE(false);
    }
}

TEST_CASE("Protocol - order/size", "[protocol]"){
    char data[26];
    Protocol protocol;
    protocol.initiateSenderProtocolHeader(
            data,
            muse::timer::TimerWheel::GetTick().count(),
            512,
            Protocol::protocolHeaderSize
    );

    try {
        bool isSuccess = false;
        protocol.setProtocolOrderAndSize(data,15, 511);
        ProtocolHeader header = protocol.parse(data, 26, isSuccess);
        REQUIRE(header.pieceOrder == 15);
        REQUIRE(header.pieceSize == 511);
        REQUIRE(isSuccess);
    }
    catch(...){
        REQUIRE(false);
    }
}


TEST_CASE("Protocol - phase", "[protocol]"){
    char data[26];
    Protocol protocol;
    protocol.initiateSenderProtocolHeader(
            data,
            muse::timer::TimerWheel::GetTick().count(),
            512,
            Protocol::protocolHeaderSize
    );

    protocol.setProtocolPhase(data, CommunicationPhase::Response);
    REQUIRE_NOTHROW(protocol.getProtocolPhase(data) == CommunicationPhase::Response);

}



