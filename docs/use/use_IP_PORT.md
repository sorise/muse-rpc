### 服务端获取客户端IP地址方法

服务端参数后置
```cpp
// @context/test_ip_client
int get_client_ip_port(int age, uint32_t client_ip_address, uint16_t client_port){
    std::cout << "client >>" << client_ip_address <<":" << client_port << std::endl;
    return 100 + age;
}

// @context/test_ip_client_test
int test_client_ip_port(uint32_t client_ip_address, uint16_t client_port){
    std::cout << "client >>" << client_ip_address <<":" << client_port << std::endl;
    return 100;
}

muse_bind_sync( Disposition::Prefix +"test_ip_client", get_client_ip_port);
muse_bind_sync( Disposition::Prefix + "test_ip_client_test", test_client_ip_port);
```

客户端请求
```cpp
//方法的路由
Client remix("127.0.0.1", 15000, MemoryPoolSingleton());
//调用远程方法
Outcome<int> result = remix.call<int>("@context/test_ip_client_test");

if (result.isOK()){
    std::cout << "result: " <<result.value << std::endl;
}else{
    if (result.protocolReason == FailureReason::OK){
        std::printf("rpc error\n");
    }else{
        std::printf("protocol error\n");
    }
}
```