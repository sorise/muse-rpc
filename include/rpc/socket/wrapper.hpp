#include <iostream>
#include <exception>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/un.h>

#ifndef MUSE_WRAPPER_HPP
#define MUSE_WRAPPER_HPP

namespace muse::rpc{
    int Socket(int domain,int type,int protocol){
        auto socket_fd = socket(domain, type, protocol);
        if (socket_fd == -1) {
            throw std::runtime_error("create The socket failed! - errno: "+ std::to_string(errno));
        }
        return socket_fd;
    }

    int Bind(int socket_fd, const struct sockaddr *addr, socklen_t addrlen){
        auto result = bind(socket_fd, addr, addrlen);
        if (result == -1) {
            throw std::runtime_error("Bind occur error, please check the sockaddr data! - errno: "+ std::to_string(errno));
        }
        return result;
    }

    int Close(int fd){
        auto result = close(fd);
        if (result == -1) {
            throw std::runtime_error("Close the socket failed! - errno: "+ std::to_string(errno));
        }
        return result;
    }

}

#endif //MUSE_WRAPPER_HPP
