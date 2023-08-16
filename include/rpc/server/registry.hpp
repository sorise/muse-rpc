#ifndef MUSE_RPC_REGISTRY_HPP
#define MUSE_RPC_REGISTRY_HPP
#include <iostream>
#include <unordered_map>
#include <functional>
#include "serializer/binarySerializer.h"
#include "rpc_response_header.hpp"

using namespace muse::serializer;
//RPC 服务器方法的注册中心
namespace muse::rpc{

#define MUSE_REGISTRY_CHECK_PARAMETERS()                    \
    RpcResponseHeader header;                               \
    std::tuple<typename std::decay<Params>::type...> tpl;   \
    try {                                                   \
        serializer->output(tpl);                            \
    }catch (const BinarySerializerException &bet){          \
        header.setReason(RpcFailureReason::ParameterError); \
        serializer->clear();                                \
        serializer->input(header);                          \
        return;                                             \
    }catch (const std::exception &ex) {                     \
        header.setReason(RpcFailureReason::ServerInnerException); \
        serializer->clear();                 \
        serializer->input(header);           \
        return;                              \
    }

#define MUSE_REGISTRY_METHOD_CATCH()        \
    catch(...) {                            \
        header.setReason(RpcFailureReason::MethodExecutionError);   \
        serializer->clear();                                        \
        serializer->input(header);                                  \
        return;                                                     \
    }

    class Registry {
    private:
        //方法存储中心，使用hash 列表 存储, 参数是 BinarySerializer
        std::unordered_map<std::string, std::function<void(BinarySerializer*)>> dictionary;
    public:
        Registry() = default;
    private:

        // 用tuple做参数调用函数模板类
        template<typename Function, typename Tuple, std::size_t... Index>
        decltype(auto) invoke_impl(Function&& func, Tuple&& t, std::index_sequence<Index...>)
        {
            return func(std::get<Index>(std::forward<Tuple>(t))...);
        }

        // 用tuple做参数调用函数模板类
        template<typename Function, typename C, typename Tuple, std::size_t... Index>
        decltype(auto) invoke_impl(Function&& func, C*c,  Tuple&& t, std::index_sequence<Index...>)
        {
            return ((*c).*func)(std::get<Index>(std::forward<Tuple>(t))...);
        }

        /* 非成员函数 有返回值 */
        template<typename R, typename ...Params>
        typename std::enable_if<!std::is_same<R, void>::value, void>::type
        callBind(std::function<R(Params...)> func, BinarySerializer *serializer){
            //防止参数解析失败
            MUSE_REGISTRY_CHECK_PARAMETERS()
            try {
                R result = invoke_impl(func, tpl, std::make_index_sequence<sizeof...(Params)>{});
                serializer->clear();
                header.setOkState(true); //执行成功
                serializer->input(header); //先写入头，再写入结果
                serializer->input(result);
            }MUSE_REGISTRY_METHOD_CATCH()
        }

        /* 非成员函数 无返回值 */
        template<typename R, typename ...Params>
        typename std::enable_if<std::is_same<R, void>::value, void>::type
        callBind(std::function<R(Params...)> func, BinarySerializer *serializer){
            //防止参数解析失败
            MUSE_REGISTRY_CHECK_PARAMETERS()
            try {
                invoke_impl(func, tpl, std::make_index_sequence<sizeof...(Params)>{});
                serializer->clear();
                header.setOkState(true); //执行成功
                serializer->input(header);
            }MUSE_REGISTRY_METHOD_CATCH()
        }

        /* 成员函数有返回值 */
        template<typename R, typename C, typename ...Params>
        typename std::enable_if<!std::is_same<R, void>::value, void>::type
        callBind(R(C::*func)(Params...), C* c, BinarySerializer *serializer){
            //防止参数解析失败
            MUSE_REGISTRY_CHECK_PARAMETERS()
            try {
                R result = invoke_impl(func, c, tpl, std::make_index_sequence<sizeof...(Params)>{});
                serializer->clear();
                header.setOkState(true); //执行成功
                serializer->input(header);
                serializer->input(result);
            }MUSE_REGISTRY_METHOD_CATCH()
        }

        /* 成员函数无返回值 */
        template<typename R, typename C, typename ...Params>
        typename std::enable_if<std::is_same<R, void>::value, void>::type
        callBind(R(C::*func)(Params...), C* c, BinarySerializer *serializer){
            //防止参数解析失败
            MUSE_REGISTRY_CHECK_PARAMETERS()
            try {
                invoke_impl(func, c, tpl, std::make_index_sequence<sizeof...(Params)>{});
                serializer->clear();
                header.setOkState(true); //执行成功
                serializer->input(header);
            }MUSE_REGISTRY_METHOD_CATCH()
        }

        // 函数指针
        template<typename R, typename... Params>
        void callProxy(R(*func)(Params...), BinarySerializer* serializer) {
            callProxy(std::function<R(Params...)>(func), serializer);
        }

        //函数成员指针
        template<typename R, typename... Params>
        void callProxy(std::function<R(Params...)> func, BinarySerializer* serializer){
            callBind(func, serializer);
        }

        //函数成员指针
        template<typename R, typename C, typename... Params>
        void callProxy(R(C::* func)(Params...), C* c, BinarySerializer* serializer){
            callBind(func, c ,serializer);
        }

        template<typename F>
        inline void Proxy(F func, BinarySerializer *serializer)
        {
            callProxy(func, serializer);
        }

        template<typename F, typename C>
        inline void Proxy(F fun, C * c, BinarySerializer * serializer)
        {
            callProxy(fun, c, serializer);
        }

        template<typename F>
        void lambdaProxy(F func, BinarySerializer* serializer){
            lambdaHelper(std::function(func), serializer);
        }

        //函数成员指针
        template<typename R, typename... Params>
        void lambdaHelper(std::function<R(Params...)> func, BinarySerializer* serializer){
            callBind(func, serializer);
        }

    public:
        /* 普通可调用对象注册 */
        template<typename F>
        bool Bind(const std::string& name, F& func){
            if (dictionary.find(name) == dictionary.end()){
                dictionary[name] = std::bind(&Registry::Proxy<F>, this, func, std::placeholders::_1);
                return true;
            }
            return false;
        }

        /* lambda 可调用对象注册 */
        template<typename F>
        bool Bind(const std::string& name, F&& func){
            if (dictionary.find(name) == dictionary.end()){
               dictionary[name] = std::bind(&Registry::lambdaProxy<F>, this, std::forward<F>(func), std::placeholders::_1);
                return true;
            }
            return false;
        }

        /* 成员函数注册 */
        template<typename F, typename C>
        bool Bind(const std::string& name, F func, C *c){
            if (dictionary.find(name) == dictionary.end()){
                dictionary[name] = std::bind(&Registry::Proxy<F,C>, this, func, c, std::placeholders::_1);
                return true;
            }
            return false;
        }

        /*查看某个名称下是否具有方法*/
        bool check(const std::string& name);

        /* 执行方法 */
        void runSafely(const std::string& name, BinarySerializer *serializer);

        /* 执行这方法之前，请先调用 check 确认执行的方法已经存在 */
        void runEnsured(const std::string& name, BinarySerializer *serializer);
    };
}
#endif //MUSE_RPC_REGISTRY_HPP
