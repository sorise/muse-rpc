//
// Created by remix on 23-7-22.
//

#ifndef MUSE_RPC_RESPONSE_DATA_HPP
#define MUSE_RPC_RESPONSE_DATA_HPP
#include <iostream>
#include <vector>
#include <memory>
#include <memory_resource>
#include "invoker_exception.hpp"
#include "../logger/conf.hpp"
#include "../memory/conf.hpp"
#include "../protocol/communication_phase.h"
#include "../protocol/protocol.hpp"

namespace muse::rpc{

    enum class FailureReason: short {
        OK, //没有失败
        TheServerResourcesExhausted, //服务器资源耗尽，请勿链接
        NetworkTimeout, //网络连接超时
        TheRunningLogicOfTheServerIncorrect, //服务器运行逻辑错误，返回的报文并非所需
    };

    class ResponseData{
    public:
        uint64_t            message_id;
        std::vector<bool>   piece_state;    //各个分片的状态
        uint16_t            piece_count;
        uint32_t            total_size;
        bool is_initial{false};
        bool isSuccess {false};
        FailureReason reason{ FailureReason::OK };
        std::shared_ptr<std::pmr::synchronized_pool_resource> pool;
        void initialize(uint64_t _message_id, uint16_t _piece_count,uint32_t _total_size);
        uint16_t getAckNumber();
        bool getPieceState(const uint16_t & _idx) const;
        uint16_t setPieceState(const uint16_t & _index, bool value);
    public:
        void set_success(const bool & suc);
        bool get_success() const;

        std::shared_ptr<char[]>  data {nullptr};

        ResponseData();
        explicit ResponseData(std::shared_ptr<std::pmr::synchronized_pool_resource> _pool);
        ResponseData(const ResponseData& other);
        ResponseData(ResponseData&& other) noexcept;

        bool isOk() const;
        uint32_t getSize() const;
        FailureReason getFailureReason() const;
    };

    class ResponseDataFactory{
    private:
        std::shared_ptr<std::pmr::synchronized_pool_resource> pool;
    public:
        ResponseDataFactory(size_t _largest_required_pool_block, size_t _max_blocks_per_chunk);
        ResponseDataFactory(ResponseDataFactory &&) noexcept ;
        ResponseDataFactory(const ResponseDataFactory &other) noexcept ;
        explicit ResponseDataFactory(std::shared_ptr<std::pmr::synchronized_pool_resource> _pool);
        std::shared_ptr<std::pmr::synchronized_pool_resource> getPool();
        ResponseData getResponseData();
    };


}

#endif //MUSE_RPC_RESPONSE_DATA_HPP
