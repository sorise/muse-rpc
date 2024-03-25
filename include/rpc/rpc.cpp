//
// Created by remix on 24-2-9.
//
#include "rpc.hpp"

namespace muse::rpc{

    std::string Disposition::Prefix = {"@context/"};

    void Disposition::Server_Configure(){
        //注册中间件
        MiddlewareChannel::configure<ZlibService>();  //解压缩
        MiddlewareChannel::configure<RouteService>(Singleton<Registry>(), Singleton<SynchronousRegistry>()); //方法的路由
        //启动线程池
        GetThreadPoolSingleton();
        // 启动日志
        muse::InitSystemLogger("");
    }

    void Disposition::Server_Configure(
            const size_t &minThreadCount,
            const size_t &maxThreadCount,
            const size_t &taskQueueLength,
            const std::chrono::milliseconds &dynamicThreadVacantMillisecond,
            const std::string& logfile_directory,
            bool console_open_state
    ){
        //目录不正确
        if (!std::filesystem::is_directory(logfile_directory)){
            throw std::logic_error("The log directory is incorrect!");
        }
        if (!std::filesystem::exists(logfile_directory)){
            throw std::logic_error("The log directory is not exist!");
        }
        //注册中间件
        MiddlewareChannel::configure<ZlibService>();  //解压缩
        MiddlewareChannel::configure<RouteService>(Singleton<Registry>(), Singleton<SynchronousRegistry>()); //方法的路由
        // 设置线程池
        ThreadPoolSetting::MinThreadCount = minThreadCount;
        ThreadPoolSetting::MaxThreadCount = maxThreadCount;
        ThreadPoolSetting::TaskQueueLength = taskQueueLength;
        ThreadPoolSetting::DynamicThreadVacantMillisecond = dynamicThreadVacantMillisecond;
        //启动线程池
        GetThreadPoolSingleton();
        // 启动日志
        muse::InitSystemLogger(logfile_directory, console_open_state);
    }
    /* 并未设置线程池 */
    void Disposition::Client_Configure() {
        MiddlewareChannel::configure<ZlibService>();  //解压缩
        MiddlewareChannel::configure<RouteService>(Singleton<Registry>(), Singleton<SynchronousRegistry>()); //方法的路由
    }


}

