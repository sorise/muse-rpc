//
// Created by remix on 23-7-18.
//

#ifndef MUSE_LOGGER_CONF_HPP
#define MUSE_LOGGER_CONF_HPP
#include "spdlog/spdlog.h"
#include "spdlog/async.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include <memory>
#include <thread>
#include <iostream>

#define LOGGER_NAME "museLogger"
#define MUSE_LOG spdlog::get("museLogger")

namespace muse::rpc{
    static void InitSystemLogger(){
        //开启日志
        try
        {
            spdlog::init_thread_pool(8192, 1);
            //开两个线程取记录日志
            std::vector<spdlog::sink_ptr> sinks;
            //输出到文件，旋转日志
            auto rotating = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                    "muse.log", 1024 * 1024 * 512, 3, false
            );
            //输出到控制台
            auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            sinks.push_back(rotating);
            sinks.push_back(consoleSink);
            //创造一个日志记录器
            auto logger =
                    std::make_shared<spdlog::async_logger>(
                            LOGGER_NAME,
                            begin(sinks), end(sinks),
                            spdlog::thread_pool(),
                            spdlog::async_overflow_policy::block
                    );
            //设置日志格式
            logger->set_pattern("[%Y-%m-%d %H:%M:%S %e] %^[%n] [%l] [thread:%t] [%s:%#]%$  %v");
            //立即刷新
            logger->flush_on(spdlog::level::err);
            //全局注册
            spdlog::register_logger(logger);
            //变成全局日志
            spdlog::set_default_logger(logger);
        }
        catch (const spdlog::spdlog_ex& ex)
        {
            throw std::runtime_error("start failed because the Logger initialization failed in Reactor constructor");
        }
    }

};

#endif //MUSE_LOGGER_CONF_HPP
