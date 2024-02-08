//
// Created by remix on 24-2-9.
//
#include "rpc.hpp"

namespace muse::rpc{
    void MUSE_RPC::Configure(){
        //注册中间件
        MiddlewareChannel::configure<ZlibService>();  //解压缩
        MiddlewareChannel::configure<RouteService>(Singleton<Registry>(), Singleton<SynchronousRegistry>()); //方法的路由
        //启动线程池
        GetThreadPoolSingleton();
        // 启动日志
        muse::InitSystemLogger("");
    }

    void MUSE_RPC::Configure(const size_t &minThreadCount, const size_t &maxThreadCount, const size_t &taskQueueLength, const std::chrono::milliseconds &dynamicThreadVacantMillisecond, const std::string& logfile_directory){
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
        muse::InitSystemLogger(logfile_directory);
    }
}

