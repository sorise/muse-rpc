//
// Created by remix on 23-7-27.
//

#ifndef MUSE_RPC_CLIENT_HPP
#define MUSE_RPC_CLIENT_HPP
#include "serializer/binarySerializer.h"
#include "invoker.hpp"
#include "outcome.hpp"
#include <string>

namespace muse::rpc{
    using namespace muse::serializer;

    class Client {
    private:
        Invoker invoker;
        ResponseDataFactory factory;
    private:

        //将参数打包
        template<typename ...Argc>
        BinarySerializer package(Argc&& ...argc){
            BinarySerializer serializer;
            std::tuple<Argc...> tpl(argc...);
            serializer.input(tpl);
            return serializer;
        }

    public:
        Client(const char * _ip_address, const uint16_t& _port, std::shared_ptr<std::pmr::synchronized_pool_resource> _pool);
        //返回值 非空 void
        template<typename R, typename ...Argc>
        typename std::enable_if<!std::is_same<R, void>::value, Outcome<R>>::type
        call(const std::string& Name, Argc&&...argc){
            Outcome<R> result;
            BinarySerializer serializer;
            std::tuple<Argc...> tpl(argc...);
            serializer.input(Name);
            serializer.input(tpl);
            ResponseData responseData = invoker.request(serializer.getBinaryStream(), serializer.byteCount(), factory);
            if (responseData.isOk()){
                //写入数据
                try {
                    serializer.clear();
                    serializer.write(responseData.data.get(), responseData.getSize());
                    serializer.output(result.response);
                    if (result.response.getOkState()){
                        serializer.output(result.value);
                    }
                } catch (...) {
                    //读取错误，返回值非预期
                    result.response.setOkState(false);
                    result.response.setReason(RpcFailureReason::UnexpectedReturnValue);
                    return result;
                }
            }else{
                //这是协议层的错误，向上报告错误
                result.protocolReason == responseData.getFailureReason();
            }
            return result;
        }

        template<typename R, typename ...Argc>
        typename std::enable_if<std::is_same<R, void>::value, Outcome<void>>::type
        call( const std::string& name, Argc&&...argc){
            Outcome<void> result;
            BinarySerializer serializer;
            std::tuple<Argc...> tpl(argc...);
            serializer.input(name);
            serializer.input(tpl);
            ResponseData responseData = invoker.request(serializer.getBinaryStream(), serializer.byteCount(), factory);

            if (responseData.isOk()){
                //写入数据
                try {
                    serializer.clear();
                    serializer.write(responseData.data.get(), responseData.getSize());
                    serializer.output(result.response);
                } catch (...) {
                    //读取错误，返回值非预期
                    result.response.setOkState(false);
                    result.response.setReason(RpcFailureReason::UnexpectedReturnValue);
                    return result;
                }
            }else{
                //这是协议层的错误
                result.protocolReason = responseData.getFailureReason();
            }
            return result;
        }

        void Bind(uint16_t _local_port);
    };
}
#endif //MUSE_RPC_CLIENT_HPP
