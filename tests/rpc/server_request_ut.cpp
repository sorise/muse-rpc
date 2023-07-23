#define CATCH_CONFIG_MAIN
#include "catch.hpp"
#include "rpc/server/reqeust.hpp"
#include <netinet/in.h>
#include <arpa/inet.h>

using namespace muse::rpc;

TEST_CASE("Request - find and search", "[Request]"){
    sockaddr_in address1{};
    sockaddr_in address2{};
    sockaddr_in address3{};
    inet_pton(AF_INET, "114.109.20.18",&address1.sin_addr);
    inet_pton(AF_INET, "114.109.70.18",&address2.sin_addr);
    inet_pton(AF_INET, "144.109.20.18",&address3.sin_addr);

    std::map<Request, int ,std::less<>> connections;

    Request request1(1111000, 25000, ntohl(address1.sin_addr.s_addr),10,256);
    Request request2(1111001, 15000, ntohl(address2.sin_addr.s_addr),12,256);
    Request request3(1111002, 14000, ntohl(address3.sin_addr.s_addr),14,256);

    Servlet rb1(1111000, 25000, ntohl(address1.sin_addr.s_addr));
    Servlet rb_not(1111100, 25000, ntohl(address1.sin_addr.s_addr));
    connections.insert_or_assign(request1, 1);
    connections.insert_or_assign(request2, 1);
    connections.insert_or_assign(request3, 1);

    auto it = connections.find(rb1);

    REQUIRE(it != connections.end());
    REQUIRE(connections.find(rb_not) == connections.end());
    REQUIRE(connections.begin()->first.getID() == 1111000);
}