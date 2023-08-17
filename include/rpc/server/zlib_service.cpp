//
// Created by remix on 23-7-30.
//


#include "zlib_service.hpp"

namespace muse::rpc{

    //解压
    std::tuple<std::shared_ptr<char[]>, size_t, std::shared_ptr<std::pmr::synchronized_pool_resource>>
    ZlibService::In(std::shared_ptr<char[]> data, size_t data_size,
                    std::shared_ptr<std::pmr::synchronized_pool_resource> _memory_pool) {
        uint32_t originalSize = *(reinterpret_cast<uint32_t*>(data.get())); //极限就是这么大 UINT32_MAX
        if (protocol.get_byte_sequence() == serializer::ByteSequence::BigEndian){
            char* first = reinterpret_cast<char*>(&originalSize); //特殊转换 变成
            char* last = first + sizeof(uint32_t);
            std::reverse(first, last);
        }
        std::shared_ptr<char[]> buffer( (char*)_memory_pool->allocate(originalSize), [=](char *ptr){
            _memory_pool->deallocate(ptr, originalSize);
        });
        uLongf outDesSize = originalSize;
        int ret = uncompress((Bytef*)(buffer.get()), &outDesSize, (Bytef*)(data.get() + sizeof(uint32_t)), data_size);
        if (ret != Z_OK) {
            throw std::runtime_error("ya suo cuo wu");
        }
        return std::make_tuple(buffer ,outDesSize ,_memory_pool);
    }
    // 11 40 00 40
    //压缩
    std::tuple<std::shared_ptr<char[]>, size_t, std::shared_ptr<std::pmr::synchronized_pool_resource>>
    ZlibService::Out(std::shared_ptr<char[]> data, size_t data_size, std::shared_ptr<std::pmr::synchronized_pool_resource> _memory_pool){
        uLongf desSize = compressBound(data_size);
        auto len_flag_size = sizeof(uint32_t);
        std::shared_ptr<char[]> destOutBuffer( (char*)_memory_pool->allocate(desSize + len_flag_size), [=](char *ptr){
            _memory_pool->deallocate(ptr, desSize + len_flag_size);
        });

        uint32_t save_size = data_size; //极限就是这么大 UINT32_MAX
        if (protocol.get_byte_sequence() == serializer::ByteSequence::BigEndian){
            std::memcpy(destOutBuffer.get(), (char *)&save_size, len_flag_size);
            char* first = destOutBuffer.get(); //特殊转换 变成
            char* last = first + sizeof(uint32_t);
            std::reverse(first, last);
        }else{
            std::memcpy(destOutBuffer.get(), (char *)&save_size, len_flag_size);
        }
        uLongf inDesSize = desSize;
        auto ret = compress(reinterpret_cast<Bytef *>(destOutBuffer.get() + len_flag_size), &inDesSize, reinterpret_cast<const Bytef *>(data.get()), data_size);
        if (inDesSize > desSize){
            throw std::runtime_error("ya suo cuo wu memory call error!");
        }
        if (ret != Z_OK) {
            throw std::runtime_error("ya suo cuo wu");
        }
        return std::make_tuple(destOutBuffer ,inDesSize + len_flag_size ,_memory_pool);
    }
}