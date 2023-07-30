//
// Created by remix on 23-7-26.
//
#define CATCH_CONFIG_MAIN
#include "catch.hpp"
#include "rpc/server/registry.hpp"
#include "rpc/server/synchronous_registry.hpp"

using namespace muse::rpc;

class Normal{
public:
    Normal(int _value, std::string  _name)
            :value(_value), name(std::move( _name)){}

    std::string setValueAndGetName(int _new_value){
        this->value = _new_value;
        return this->name;
    }

    int getValue() const{
        return this->value;
    }

    void addValue(){
        this->value++;
    }

private:
    int value;
    std::string name;
};

int test_fun1(int value){
    int i  = 10;
    return i + value;
}

void test_fun2(int value){
    printf("value:%d\n", value);
}


template<typename ...Argc>
BinarySerializer package(Argc&& ...argc){
    BinarySerializer serializer;
    std::tuple<Argc...> tpl(argc...);
    serializer.input(tpl);
    return serializer;
}


TEST_CASE("Bind function-pointer", "[Registry]"){
    int value = 100;
    BinarySerializer serializer = package(value);
    Registry proxy;
    proxy.Bind("run", test_fun1);

    proxy.runSafely("run", &serializer);

    REQUIRE(value == 100);
}

TEST_CASE("Bind class number-function", "[Registry]"){
    Normal normal(50, "remix");
    int para = 100;
    BinarySerializer serializer = package(para);
    Registry proxy;

    proxy.Bind("normal/setValueAndGetName", &Normal::setValueAndGetName, &normal);
    proxy.runSafely("normal/setValueAndGetName", &serializer);

    std::string r_name;
    RpcResponseHeader header;
    serializer.output(header);
    serializer.output(r_name);
    REQUIRE(r_name == "remix");
}

TEST_CASE("class Number", "[SynchronousRegistry]"){
    Normal normal(0, "remix");

    SynchronousRegistry registry;
    registry.Bind("Normal::addValue", &Normal::addValue, &normal);

    std::thread t1([&](){
        for (int i = 0; i < 500; ++i) {
            BinarySerializer serializer;
            registry.runEnsured("Normal::addValue", &serializer);
        }
    });

    std::thread t2([&](){
        for (int i = 0; i < 500; ++i) {
            BinarySerializer serializer;
            registry.runEnsured("Normal::addValue", &serializer);
        }
    });

    t1.join();
    t2.join();

    REQUIRE(normal.getValue() == 1000);
}


TEST_CASE("function pointer", "[SynchronousRegistry]"){
    Normal normal(0, "remix");

    SynchronousRegistry registry;
    REQUIRE_NOTHROW(registry.Bind("test", test_fun1));

}