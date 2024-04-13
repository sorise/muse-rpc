//
// Created by remix on 23-5-27.
//

#ifndef SERIALIZER_UTIL_H
#define SERIALIZER_UTIL_H

#include <iostream>
#include <cstring>
#include <vector>
#include <exception>
#include <tuple>

#ifndef _WIN32
    #define MUSE_Serializer_API
#else
    #ifdef serializer_EXPORTS
        #define MUSE_Serializer_API __declspec(dllexport)   //库项目调用
    #else
        #define MUSE_Serializer_API __declspec(dllimport)  //调用库库项目调用
    #endif
#endif

namespace muse::serializer{
    //初始化容量大小
    #define MUSE_DEFAULT_CAPACITY 32
    //字符串的最大长度, 1MB 等于 1048576 字节， 0.5M 524288
    #define MUSE_MAX_STRING_LENGTH 1048576
    //Byte 长度 上限
    #define MUSE_MAX_BYTE_COUNT 65536
    //Vector 长度 数量上限
    #define MUSE_MAX_VECTOR_COUNT 16777216
    //Vector 长度 数量上限
    #define MUSE_MAX_ARRAY_COUNT 16777216
    //List 长度 数量上限
    #define MUSE_MAX_LIST_COUNT 16777216
    //Map 健值对 数量上限
    #define MUSE_MAX_MAP_COUNT 16777216
    //Tuple 数量上限
    #define MUSE_MAX_TUPLE_COUNT 64
    //SET 成员数量上限
    #define MUSE_MAX_SET_COUNT 16777216

    /*
     *  type: enum class BinaryDataType : char
     *  des: 支持序列化的数据类型
     *  大小必须是一个字节
     */
    enum class BinaryDataType : char
    {
        BOOL = 0,   //bool
        INT16,      //short
        INT32,      //int
        INT64,      //long
        UINT16,     //unsigned short
        UINT32,     //unsigned int
        UINT64,     //unsigned long
        FLOAT,      //float 浮点数
        DOUBLE,     //double 双精度浮点数
        CHAR,       //char 字符
        STRING,     //string 字服串
        BYTES,      //char[] 纯粹的二进制写入
        UBYTES,     //unsigned char[] 纯粹的二进制写入
        VECTOR,     //顺序表  std::vector<T>
        LIST,       //链表   std::list<T>
        MAP,        //字典 - 红黑树 map
        TUPLE,      //元组 - tuple
        HASHMAP,    //字典 - Hash map
        SET,        //集合 - Set
        MUSECLASS,  //用户自定义类型
        ARRAY,      //array<_tp, _nm> 类型
        UINT8,      //unsigned char
    };

    enum class ByteSequence: short
    {
        BigEndian = 0,      /* 大端序 网络字节序 */
        LittleEndian = 1,   /* 小端序 主机字节序 */
    };

    /* 错误号 */
    enum class ErrorNumber: short {
        DataTypeError = 0,                  /* 读取类型错误 */
        InsufficientRemainingMemory,        /* 剩余空间不足 */
        TheStringLengthExceedsTheLimit,     /* 字符串长度超过 1048576 */
        IllegalStringLength,                /* 字符串长度非法 1048576*/
        TheBytesCountExceedsTheLimit,       /* 字节数量超过 65536 */
        IllegalBytesCount,                  /* 字节数量非法 */
        ReadIllegalBytesCount,              /* 你存储了X个字节，但是读取了Y个， (X！=Y) */
        TheVectorCountExceedsTheLimit,      /* Vector长度超过 16777216   */
        IllegalVectorCount,                 /* Vector数量非法 */
        TheListCountExceedsTheLimit,        /* List长度超过 16777216   */
        IllegalListCount,                   /* List数量非法 */
        TheMapKVCountExceedsTheLimit,       /* Map长度超过 16777216   */
        IllegalMapKVCount,                  /* Map数量非法 */
        TheTupleCountExceedsTheLimit,       /* Tuple长度超过 64   */
        IllegalTupleCount,                  /* Tuple数量非法 */
        IllegalParameterTupleCount,         /* 传递的Tuple 元素数量不一致 */
        TheHashMapKVCountExceedsTheLimit,   /* HashMap长度超过 16777216   */
        IllegalHashMapKVCount,              /* HashMap数量非法 */
        ErrorReadingSubElements,            /* 读取子元素失败 */
        TheSetSizeExceedsTheLimit,          /* Set 成员数量超过 16777216   */
        IllegalSetCount,                    /* HashMap数量非法 */
        NoDataToStoreInFile,                /* 没有数据可以存储到文件中 */
        TheArrayCountExceedsTheLimit,      /* Array长度超过 16777216   */
        IllegalArrayCount,                 /* Array数量非法 */
    };

    /* 获取当前主机采用的字节序 */
    MUSE_Serializer_API ByteSequence getByteSequence();

    //获取元组长度 01
    template<typename T>
    auto getTupleElementCount(T tp) -> decltype(std::tuple_size<T>::value) {
        return std::tuple_size<decltype(tp)>::value;
    }

    //获取元组长度 02
    template<typename ...Args>
    auto getTupleElementCount(std::tuple<Args...> tp) -> decltype(std::tuple_size<std::tuple<Args...>>::value) {
        return std::tuple_size<decltype(tp)>::value;
    }

    /* 错误异常消息 */
    class MUSE_Serializer_API BinarySerializerException: public std::logic_error{
    public:
        explicit BinarySerializerException(const std::string &arg, ErrorNumber err);
        ~BinarySerializerException() override ;
        ErrorNumber getErrorNumber(); //返回错误号
    private:
        ErrorNumber errorNumber;
    };
}

#endif //SERIALIZER_UTIL_H
