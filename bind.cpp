//
// Created by remix on 23-6-23.
// linux 不是有定时器信号
#include <iostream>
#include <chrono>
#include <limits>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <thread>
#include <zlib.h>
#include <functional>
#include <cstring>
#include <future>

int main()
{
    auto future = std::async(std::launch::async,[](){
        std::cout << "what" << std::endl;
    });


    std::cin.get();
    return 0;
}
