//
// Created by remix on 23-7-22.
//

#include "response_data.hpp"

#include <utility>

namespace muse::rpc{

    ResponseDataFactory::ResponseDataFactory(size_t _largest_required_pool_block, size_t _max_blocks_per_chunk){
        std::pmr::pool_options option;
        option.largest_required_pool_block = _largest_required_pool_block; //5M
        option.max_blocks_per_chunk = _max_blocks_per_chunk; //每一个chunk有多少个block
        pool = std::make_shared<std::pmr::synchronized_pool_resource>(option);
    }

    ResponseDataFactory::ResponseDataFactory(std::shared_ptr<std::pmr::synchronized_pool_resource> _pool)
    :pool(std::move(_pool)){

    }

    ResponseData ResponseDataFactory::getResponseData() {
        return ResponseData(pool);
    }


    ResponseData::ResponseData(std::shared_ptr<std::pmr::synchronized_pool_resource> _pool)
    : pool(std::move(_pool)), piece_state(0),message_id(0),total_size(0), piece_count(0){
        is_initial = false;
    }

    void ResponseData::initialize(uint64_t _message_id, uint16_t _piece_count,uint32_t _total_size) {
        if (!is_initial){
            piece_state.reserve(_piece_count);
            piece_state.resize(_piece_count, false);

            this->message_id =_message_id;
            this->piece_count = _piece_count;
            this->total_size =_total_size;

            is_initial = true;
        }
    }

    ResponseData::ResponseData(const ResponseData &other)
    : pool(other.pool), piece_state(other.piece_state),message_id(other.message_id),total_size(other.total_size), piece_count(other.piece_count){
        is_initial = other.is_initial;
    }

    ResponseData::ResponseData(ResponseData &&other) noexcept
    :pool(std::move(other.pool)), piece_state(std::move(other.piece_state)),message_id(other.message_id),total_size(other.total_size), piece_count(other.piece_count){
        is_initial = other.is_initial;
    }

}