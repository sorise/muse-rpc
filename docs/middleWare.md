```c++
class zlibService: public middleware_service{
public:
    zlibService() = default;
    ~zlibService() override = default;

    //数据输入 解压、解密
    std::tuple<std::shared_ptr<char[]>, size_t , std::shared_ptr<std::pmr::synchronized_pool_resource>>
    In(std::shared_ptr<char[]> data, size_t data_size, std::shared_ptr<std::pmr::synchronized_pool_resource> _memory_pool) override{
        printf("jie ya\n");
        return std::make_tuple(data, data_size, _memory_pool);
    };

    //数据输出 压缩、加密
    std::tuple<std::shared_ptr<char[]>, size_t , std::shared_ptr<std::pmr::synchronized_pool_resource>>
    Out(std::shared_ptr<char[]> data, size_t data_size, std::shared_ptr<std::pmr::synchronized_pool_resource> _memory_pool) override{
        printf("ya suo\n");
        return std::make_tuple(data, data_size, _memory_pool);
    };
};


class secretService: public middleware_service{
public:
    secretService() = default;
    ~secretService() override = default;

    //数据输入 解压、解密
    std::tuple<std::shared_ptr<char[]>, size_t , std::shared_ptr<std::pmr::synchronized_pool_resource>>
    In(std::shared_ptr<char[]> data, size_t data_size, std::shared_ptr<std::pmr::synchronized_pool_resource> _memory_pool) override{
        printf("jie mi\n");
        return std::make_tuple(data, data_size, _memory_pool);
    };

    //数据输出 压缩、加密
    std::tuple<std::shared_ptr<char[]>, size_t , std::shared_ptr<std::pmr::synchronized_pool_resource>>
    Out(std::shared_ptr<char[]> data, size_t data_size, std::shared_ptr<std::pmr::synchronized_pool_resource> _memory_pool) override{
        printf("jia mi\n");
        return std::make_tuple(data, data_size, _memory_pool);
    };
};
```