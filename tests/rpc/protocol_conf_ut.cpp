#define CATCH_CONFIG_MAIN
#include "catch.hpp"
#include "rpc/protocol/conf.hpp"
#include "timer/timer_wheel.hpp"

using namespace muse::rpc;

TEST_CASE("Protocol - initiateSenderProtocolHeader", "[protocol]"){
    char header[26];

    Protocol::initiateSenderProtocolHeader(
           header,
           muse::timer::TimerWheel::GetTick().count(),
           512,
           Protocol::protocolHeaderSize
    );

    bool result = false;
    ProtocolHeader protocolHeader = Protocol::parse(header, Protocol::protocolHeaderSize, result);

    REQUIRE(result == true);
    REQUIRE(protocolHeader.synchronousWord == Protocol::defaultSynchronousWord);
    REQUIRE(protocolHeader.type == ProtocolType::SenderSend);
    REQUIRE(protocolHeader.pieceStandardSize == Protocol::defaultPieceLimitSize);
}


TEST_CASE("Protocol - type", "[protocol]"){
    char data[26];

    Protocol::initiateSenderProtocolHeader(
            data,
            muse::timer::TimerWheel::GetTick().count(),
            512,
            Protocol::protocolHeaderSize
    );

    try {
        Protocol::setProtocolType(data, ProtocolType::TimedOutRequestHeartbeat);
        ProtocolType type = Protocol::getProtocolType(data);
        REQUIRE(type == ProtocolType::TimedOutRequestHeartbeat);
    }
    catch(...){
        REQUIRE(false);
    }
}



