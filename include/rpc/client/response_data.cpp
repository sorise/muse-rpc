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

    ResponseDataFactory::ResponseDataFactory(ResponseDataFactory &&other) noexcept
    :pool(std::move(other.pool))
    {

    }

    ResponseDataFactory::ResponseDataFactory(const ResponseDataFactory &other) noexcept {
        this->pool = other.pool;
    }

    std::shared_ptr<std::pmr::synchronized_pool_resource> ResponseDataFactory::getPool() {
        return this->pool;
    }

    bool ResponseData::isOk() const { return this->isSuccess;};
    uint32_t ResponseData::getSize() const {return this->total_size;};
    FailureReason ResponseData::getFailureReason() const { return this->reason;};

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

            auto store = pool->allocate(_total_size);
            std::shared_ptr<char[]> dt((char*)store, [&, total=_total_size](char *ptr){
                pool->deallocate(ptr, total);
            });
            data = dt;
            is_initial = true;
        }
    }

    uint16_t ResponseData::setPieceState(const uint16_t & _index, bool value) {
        if (_index > piece_count) {
            SPDLOG_ERROR("Class Request getPieceState index Cross boundary error!, @_index {} vector size {}", _index, piece_state.size());
            throw ClientException("[Request]", ClientError::PieceStateIncorrect);
        }else{
            piece_state[_index] = value;
            return getAckNumber();
        }
    }

    bool ResponseData::getPieceState(const uint16_t & _index) const {
        if (_index > piece_count) {
            SPDLOG_ERROR("Class Request getPieceState index Cross boundary error!, @_index {} vector size {}", _index, piece_state.size());
            throw ClientException("[Request]", ClientError::PieceStateIncorrect);
        }else{
            return piece_state[_index];
        }
    }

    ResponseData::ResponseData(const ResponseData &other)
    : pool(other.pool),
    piece_state(other.piece_state),
    message_id(other.message_id),
    total_size(other.total_size),
    data(other.data),
    isSuccess(other.isSuccess),
    reason(other.reason),
      is_initial(other.is_initial),
    piece_count(other.piece_count){

    }

    ResponseData::ResponseData(ResponseData &&other) noexcept
    :pool(std::move(other.pool)),
    piece_state(std::move(other.piece_state)),
    message_id(other.message_id),
    total_size(other.total_size),
     data(std::move(other.data)),
     reason(other.reason),
     isSuccess(other.isSuccess),
     is_initial(other.is_initial),
    piece_count(other.piece_count){
        is_initial = other.is_initial;
    }

    uint16_t ResponseData::getAckNumber() {
        uint16_t result = piece_state.size();
        for (int i = 0; i < piece_state.size(); ++i) {
            if (!piece_state[i]){
                result = i;
                break;
            }
        }
        return result;
    }

    ResponseData::ResponseData()
    : pool(MemoryPoolSingleton()), piece_state(0),message_id(0),total_size(0), piece_count(0){

    }

    void ResponseData::set_success(const bool &suc) {
        this->isSuccess = suc;
    }

    bool ResponseData::get_success() const {
        return this->isSuccess;
    }

}