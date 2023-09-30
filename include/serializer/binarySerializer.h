// 二进制序列化库
// Created by remix on 23-5-26.
// 字节编码顺序：小端序

#ifndef MUSE_SERIALIZER_BINARY_SERIALIZER_H
#define MUSE_SERIALIZER_BINARY_SERIALIZER_H
#include "util.h"
#include "IbinarySerializable.h"
#include <algorithm>
#include <climits>
#include <list>
#include <map>
#include <unordered_map>
#include <set>
#include <fstream>
#include <string>
#include <sstream>
#include <cstdint>
#include <array>

static_assert(__cplusplus >= 201103L, "C++ version is not right" );

#define MUSE_TEMPLATE_CONVERT_TO_LITTLE_ENDIAN()        \
    if (byteSequence == ByteSequence::BigEndian){       \
        auto last = &byteStream[byteStream.size()];     \
        auto first = last - sizeof(uint32_t);           \
        std::reverse(first, last);                      \
    }

#define MUSE_PREVENT_BOUNDARY() \
    if (readPosition == byteStream.size()){ \
        throw BinarySerializerException("remaining memory is not enough ", ErrorNumber::InsufficientRemainingMemory); \
    }

namespace muse::serializer{
    class IBinarySerializable; //define

    class BinarySerializer final{
    public:
        using Position_Type = long;
        using Element_Length = uint32_t ;
        using Tuple_Element_Length = uint16_t ;
    private:
        template<unsigned int N>
        struct TupleWriter {
            template<typename... Args>
            static void write(const std::tuple<Args...>& t, BinarySerializer& serializer){
                serializer.input(std::get<N>(t));
                TupleWriter<N-1>::write(t, serializer);
            }
        };

        template<unsigned int N>
        struct TupleReader{
            template<typename... Args>
            static void read(std::tuple<Args...>& t, BinarySerializer& serializer){
                //typename std::tuple_element<N,std::tuple<Args...>>::type first;
                serializer.output(std::get<N>(t));
                TupleReader<N-1>::read(t, serializer);
            }
        };

        /* 存储字节流数据 */
        std::vector<char> byteStream;
        /* 读取指针 */
        Position_Type readPosition;
        /* 字节序 */
        ByteSequence byteSequence;
        /* 如果剩余空间不够了，就进行扩容，否则直接返回 */
        void expansion(size_t len);
        /* 读取 */
        bool read(char* data, const unsigned int& len) noexcept ;

        template<typename Tuple, std::size_t... Index>
        void  input_tuple_helper(Tuple&& t, std::index_sequence<Index...>){
            inputArgs(std::get<Index>(std::forward<Tuple>(t))...);
        }

        template<typename Tuple, std::size_t... Index>
        void  output_tuple_helper(Tuple&& t, std::index_sequence<Index...>){
            outputArgs(std::get<Index>(std::forward<Tuple>(t))...);
        }
    public:
        /* 最基础的写入方法 */
        BinarySerializer& write(const char* data, const unsigned int& len);
        using Container_type = std::vector<char>;
        void clear();                            //清除所有
        void reset();                            //重新从开始读取
        [[nodiscard]] Container_type::size_type byteCount() const;                //多少字节数量
        [[nodiscard]] Position_Type getReadPosition() const;                      //获得当前指针所在位置
        bool checkDataTypeRightForward(BinaryDataType type);              //检查类型是否正确，如果正确，移动 readPosition
        BinarySerializer();                                         //默认构造函数
        BinarySerializer(const BinarySerializer& other) = delete;   //禁止复制
        BinarySerializer(BinarySerializer &&other) noexcept;        //支持移动操作
        [[nodiscard]] const char* getBinaryStream() const;                        //返回二进制流的指针

        void saveToFile(const std::string& path) const;    //二进制数据存储到文件中
        void loadFromFile(const std::string& path);  //从文件中加载二进制数据

        ~BinarySerializer();

        BinarySerializer& input(const bool&);
        BinarySerializer& input(const char &);
        BinarySerializer& input(const unsigned char &);
        BinarySerializer& input(const int16_t &);
        BinarySerializer& input(const int32_t &);
        BinarySerializer& input(const int64_t &);
        BinarySerializer& input(const uint16_t &);
        BinarySerializer& input(const uint32_t &);
        BinarySerializer& input(const uint64_t &);
        BinarySerializer& input(const float &);
        BinarySerializer& input(const double &);
        BinarySerializer& input(const std::string &);
        BinarySerializer& input(const char*, unsigned int);
        BinarySerializer& input(const unsigned char*, unsigned int);
        BinarySerializer &input(const muse::serializer::IBinarySerializable &serializable);

        template<typename T, typename = typename std::enable_if_t<std::is_default_constructible_v<T>>>
        BinarySerializer& input(const std::vector<T>& value){
            auto type = BinaryDataType::VECTOR;
            //写入类型
            write((char*)&type, sizeof(type));
            //获得成员数量,不能超过某个限制
            auto elementCount = value.size();
            if (elementCount > MUSE_MAX_VECTOR_COUNT){
                throw BinarySerializerException("the vector count exceeds the limit", ErrorNumber::TheVectorCountExceedsTheLimit);
            }
            //写入成员数量
            write((char*)&elementCount, sizeof(uint32_t));
            //存储长度, 注意大小端问题
            MUSE_TEMPLATE_CONVERT_TO_LITTLE_ENDIAN()
            //写入成员
            for (auto it = value.begin(); it != value.end(); it++)
            {
                input(*it);
            }
            return *this;
        }

        template<typename T,  std::size_t N,typename = typename std::enable_if_t<std::is_default_constructible_v<T>>>
        BinarySerializer& input(const std::array<T,N>& value){
            auto type = BinaryDataType::ARRAY;
            //写入类型
            write((char*)&type, sizeof(type));
            //获得成员数量,不能超过某个限制
            static_assert(N < MUSE_MAX_ARRAY_COUNT, "the array count exceeds the limit");
            //写入成员数量
            uint32_t tm = N;
            write((char*)&tm, sizeof(uint32_t));
            //存储长度, 注意大小端问题
            MUSE_TEMPLATE_CONVERT_TO_LITTLE_ENDIAN()
            //写入成员
            for (auto it = value.begin(); it != value.end(); it++)
            {
                input(*it);
            }
            return *this;
        }

        template<typename T, typename = typename std::enable_if_t<std::is_default_constructible_v<T>>>
        BinarySerializer& input(const std::list<T> &value){
            auto type = BinaryDataType::LIST;
            //写入类型
            write((char*)&type, sizeof(type));
            auto elementCount = value.size();
            if (elementCount > MUSE_MAX_LIST_COUNT){
                throw BinarySerializerException("the list count exceeds the limit", ErrorNumber::TheListCountExceedsTheLimit);
            }
            //写入成员数量
            write((char*)&elementCount, sizeof(uint32_t));
            //存储长度, 注意大小端问题
            MUSE_TEMPLATE_CONVERT_TO_LITTLE_ENDIAN()
            //写入成员
            for (auto it = value.begin(); it != value.end(); it++)
            {
                input(*it);
            }
            return *this;
        }

        template<typename K, typename V,
                typename = typename std::enable_if_t<std::is_default_constructible_v<K>>,
                typename = typename std::enable_if_t<std::is_default_constructible_v<V>>
        >
        BinarySerializer& input(const std::map<K,V> &value){
            auto type = BinaryDataType::MAP;
            //写入类型
            write((char*)&type, sizeof(type));
            auto elementCount = value.size();
            if (elementCount > MUSE_MAX_MAP_COUNT){
                throw BinarySerializerException("the map key-value pairs count exceeds the limit", ErrorNumber::TheMapKVCountExceedsTheLimit);
            }
            //写入成员数量
            write((char*)&elementCount, sizeof(uint32_t));
            //存储长度, 注意大小端问题
            MUSE_TEMPLATE_CONVERT_TO_LITTLE_ENDIAN()
            for (auto kv = value.begin();  kv!=value.end(); kv++)
            {
                input(kv->first);
                input(kv->second);
            }
            return *this;
        }

        template<typename K, typename V,
                typename = typename std::enable_if_t<std::is_default_constructible_v<K>>,
                typename = typename std::enable_if_t<std::is_default_constructible_v<V>>
        >
        BinarySerializer& input(const std::unordered_map<K,V> &value){
            auto type = BinaryDataType::HASHMAP;
            //写入类型
            write((char*)&type, sizeof(type));
            auto elementCount = value.size();
            if (elementCount > MUSE_MAX_MAP_COUNT){
                throw BinarySerializerException("the hash map key-value pairs count exceeds the limit", ErrorNumber::TheHashMapKVCountExceedsTheLimit);
            }
            //写入成员数量
            write((char*)&elementCount, sizeof(uint32_t));
            MUSE_TEMPLATE_CONVERT_TO_LITTLE_ENDIAN()
            for (auto kv = value.begin();  kv!=value.end(); kv++)
            {
                input(kv->first);
                input(kv->second);
            }
            return *this;
        }

        template<typename T, typename = typename std::enable_if_t<std::is_default_constructible_v<T>>>
        BinarySerializer& input(const std::set<T> &value){
            auto type = BinaryDataType::SET;
            //写入类型
            write((char*)&type, sizeof(type));
            auto elementCount = value.size();
            if (elementCount > MUSE_MAX_LIST_COUNT){
                throw BinarySerializerException("the set count exceeds the limit", ErrorNumber::TheSetSizeExceedsTheLimit);
            }
            //写入成员数量
            write((char*)&elementCount, sizeof(uint32_t));
            //存储长度, 注意大小端问题
            MUSE_TEMPLATE_CONVERT_TO_LITTLE_ENDIAN()
            //写入成员
            for (auto it = value.begin(); it != value.end(); it++)
            {
                input(*it);
            }
            return *this;
        }

        template<typename... Args>
        BinarySerializer &input(const std::tuple<Args...>& tpl) {
            auto type = BinaryDataType::TUPLE;
            //写入类型
            write((char*)&type, sizeof(type));
            //获得长度
            uint16_t tupleSize = getTupleElementCount(tpl);
            if (tupleSize > MUSE_MAX_TUPLE_COUNT){
                throw BinarySerializerException("the tuple size exceeds the limit", ErrorNumber::TheTupleCountExceedsTheLimit);
            }
            //写入长度
            write((char*)&tupleSize, sizeof(uint16_t));
            //防止大小端问题
            if (byteSequence == ByteSequence::BigEndian){
                auto last = &byteStream[byteStream.size()];
                auto first = last - sizeof(uint16_t);
                std::reverse(first, last);
            }
            input_tuple_helper(tpl, std::make_index_sequence<sizeof...(Args)>());
            return *this;
        }

        BinarySerializer& input(const std::tuple<> & tpl);

        template<typename T, typename... Args>
        BinarySerializer& inputArgs(const T& value, const Args&... args) {
            input(value);
            inputArgs(args...);
            return *this;
        };
        BinarySerializer& inputArgs() { return *this; };

        /* 防止无端的类型转换错误 */
        BinarySerializer& input(const char*) = delete;
        BinarySerializer& input(char*) = delete;

        //反序列化
        BinarySerializer& output(bool &);
        BinarySerializer& output(char &);
        BinarySerializer& output(unsigned char &);
        BinarySerializer& output(int16_t &);
        BinarySerializer& output(int32_t &);
        BinarySerializer& output(int64_t &);
        BinarySerializer& output(u_int16_t &);
        BinarySerializer& output(u_int32_t &);
        BinarySerializer& output(u_int64_t &);
        BinarySerializer& output(float &);
        BinarySerializer& output(double &);
        BinarySerializer& output(std::string &);
        BinarySerializer& output(char * , unsigned int ); /* 可能存在有复制消耗*/
        BinarySerializer& output(unsigned char * , unsigned int ); /* 可能存在有复制消耗*/

        template<class T>
        T output(){
            T t;
            this->output(t);
            return t;
        }

        template<typename T, typename = typename std::enable_if_t<std::is_default_constructible_v<T>>>
        BinarySerializer& output(std::vector<T>& value)
        {
            //防止越界
            MUSE_PREVENT_BOUNDARY()
            //类型检测
            if (byteStream[readPosition] != (char)BinaryDataType::VECTOR)
                throw BinarySerializerException("read type error", ErrorNumber::DataTypeError);

            auto defaultPosition = readPosition;
            value.clear(); //先清除数据
            //读取长度信息
            readPosition++;
            //获取 vector 长度
            auto leftSize =  byteStream.size() - readPosition;
            if (leftSize < sizeof(uint32_t)){
                readPosition = defaultPosition;
                throw BinarySerializerException("remaining memory is not enough ", ErrorNumber::InsufficientRemainingMemory);
            }
            //读取字符串长度 大小端处理
            auto vectorSize = *((uint32_t *)(&byteStream[readPosition]));
            if (byteSequence == ByteSequence::BigEndian)
            {
                char* first = (char*)&vectorSize;
                char* last = first + sizeof(uint32_t);
                std::reverse(first, last);
            }
            //检测长度是否非法
            if (vectorSize > MUSE_MAX_VECTOR_COUNT){
                readPosition = defaultPosition;
                throw BinarySerializerException("illegal vector count" , ErrorNumber::IllegalVectorCount);
            }
            //检测剩余空间是否足够
            readPosition += sizeof(uint32_t);
            try {
                for (unsigned int i =0; i < vectorSize;i++)
                {
                    T v;
                    output(v);
                    value.emplace_back(v);
                }
            }catch(BinarySerializerException &serializerException) {
                readPosition = defaultPosition;
                throw serializerException;
            }catch (std::exception &ex) {
                readPosition = defaultPosition;
                throw ex;
            }catch (...){
                readPosition = defaultPosition;
                throw BinarySerializerException("not know error", ErrorNumber::ErrorReadingSubElements);
            }
            return *this;
        }

        template<typename T,  std::size_t N,typename = typename std::enable_if_t<std::is_default_constructible_v<T>>>
        BinarySerializer& output(std::array<T, N>& value)
        {
            //防止越界
            MUSE_PREVENT_BOUNDARY()
            //类型检测
            if (byteStream[readPosition] != (char)BinaryDataType::ARRAY)
                throw BinarySerializerException("read type error", ErrorNumber::DataTypeError);

            auto defaultPosition = readPosition;
            //读取长度信息
            readPosition++;
            //获取 array 长度
            auto leftSize =  byteStream.size() - readPosition;
            if (leftSize < sizeof(uint32_t)){
                readPosition = defaultPosition;
                throw BinarySerializerException("remaining memory is not enough ", ErrorNumber::InsufficientRemainingMemory);
            }
            //读取字符串长度 大小端处理
            auto arraySize = *((uint32_t *)(&byteStream[readPosition]));
            if (byteSequence == ByteSequence::BigEndian)
            {
                char* first = (char*)&arraySize;
                char* last = first + sizeof(uint32_t);
                std::reverse(first, last);
            }
            //检测长度是否非法
            if (arraySize > MUSE_MAX_ARRAY_COUNT){
                readPosition = defaultPosition;
                throw BinarySerializerException("illegal vector count" , ErrorNumber::IllegalVectorCount);
            }
            //检测剩余空间是否足够
            readPosition += sizeof(uint32_t);
            try {
                for (unsigned int i =0; i < arraySize;i++)
                {
                    output(value[i]);
                }
            }catch(BinarySerializerException &serializerException) {
                readPosition = defaultPosition;
                throw serializerException;
            }catch (std::exception &ex) {
                readPosition = defaultPosition;
                throw ex;
            }catch (...){
                readPosition = defaultPosition;
                throw BinarySerializerException("not know error", ErrorNumber::ErrorReadingSubElements);
            }
            return *this;
        }

        template<typename T, typename = typename std::enable_if_t<std::is_default_constructible_v<T>>>
        BinarySerializer& output(std::list<T>& value){
            //防止越界
            MUSE_PREVENT_BOUNDARY()
            //类型检测
            if (byteStream[readPosition] != (char)BinaryDataType::LIST)
                throw BinarySerializerException("read type error", ErrorNumber::DataTypeError);
            auto defaultPosition = readPosition;
            value.clear(); //先清除数据
            //读取长度信息
            readPosition++;
            //获取 list 长度
            auto leftSize =  byteStream.size() - readPosition;
            if (leftSize < sizeof(uint32_t)){
                readPosition = defaultPosition;
                throw BinarySerializerException("remaining memory is not enough ", ErrorNumber::InsufficientRemainingMemory);
            }
            //读取字符串长度 大小端处理
            auto listSize = *((uint32_t *)(&byteStream[readPosition]));
            if (byteSequence == ByteSequence::BigEndian)
            {
                char* first = (char*)&listSize;
                char* last = first + sizeof(uint32_t);
                std::reverse(first, last);
            }
            //检测长度是否非法
            if (listSize > MUSE_MAX_LIST_COUNT){
                readPosition = defaultPosition;
                throw BinarySerializerException("illegal list count" , ErrorNumber::IllegalListCount);
            }
            readPosition += sizeof(uint32_t);
            try {
                for (unsigned int i = 0; i < listSize;i++)
                {
                    T v;
                    output(v);
                    value.emplace_back(v);
                }
            }catch(BinarySerializerException &serializerException) {
                readPosition = defaultPosition;
                throw serializerException;
            }catch (std::exception &ex) {
                readPosition = defaultPosition;
                throw ex;
            }catch (...){
                readPosition = defaultPosition;
                throw BinarySerializerException("not know error", ErrorNumber::ErrorReadingSubElements);
            }
            return *this;
        }

        template<typename T, typename = typename std::enable_if_t<std::is_default_constructible_v<T>>>
        BinarySerializer& output(std::set<T>& value){
            //防止越界
            MUSE_PREVENT_BOUNDARY()
            //类型检测
            if (byteStream[readPosition] != (char)BinaryDataType::SET)
                throw BinarySerializerException("read type error", ErrorNumber::DataTypeError);
            auto defaultPosition = readPosition;
            value.clear();
            //读取长度信息
            readPosition++;
            //获取 list 长度
            auto leftSize =  byteStream.size() - readPosition;
            if (leftSize < sizeof(uint32_t)){
                readPosition = defaultPosition;
                throw BinarySerializerException("remaining memory is not enough ", ErrorNumber::InsufficientRemainingMemory);
            }
            //读取字符串长度 大小端处理
            auto setSize = *((uint32_t *)(&byteStream[readPosition]));
            if (byteSequence == ByteSequence::BigEndian)
            {
                char* first = (char*)&setSize;
                char* last = first + sizeof(uint32_t);
                std::reverse(first, last);
            }
            //检测长度是否非法
            if (setSize > MUSE_MAX_SET_COUNT){
                readPosition = defaultPosition;
                throw BinarySerializerException("illegal set count" , ErrorNumber::IllegalSetCount);
            }
            readPosition += sizeof(uint32_t);
            try {
                for (unsigned int i = 0; i < setSize;i++)
                {
                    T v;
                    output(v);
                    value.emplace(v);
                }
            }catch(BinarySerializerException &serializerException) {
                readPosition = defaultPosition;
                throw serializerException;
            }catch (std::exception &ex) {
                readPosition = defaultPosition;
                throw ex;
            }catch (...){
                readPosition = defaultPosition;
                throw BinarySerializerException("not know error", ErrorNumber::ErrorReadingSubElements);
            }
            return *this;
        }

        template<typename K, typename V,
                typename = typename std::enable_if_t<std::is_default_constructible_v<K>>,
                typename = typename std::enable_if_t<std::is_default_constructible_v<V>>
        >
        BinarySerializer& output(std::map<K,V> &value){
            //防止越界
            MUSE_PREVENT_BOUNDARY()
            //类型检测
            if (byteStream[readPosition] != (char)BinaryDataType::MAP)
                throw BinarySerializerException("read type error", ErrorNumber::DataTypeError);
            auto defaultPosition = readPosition;
            readPosition++;
            value.clear();
            //获取 list 长度
            auto leftSize =  byteStream.size() - readPosition;
            if (leftSize < sizeof(uint32_t)){
                readPosition = defaultPosition;
                throw BinarySerializerException("remaining memory is not enough ", ErrorNumber::InsufficientRemainingMemory);
            }
            //读取字符串长度 大小端处理
            auto mapKVSize = *((uint32_t *)(&byteStream[readPosition]));
            if (byteSequence == ByteSequence::BigEndian)
            {
                char* first = (char*)&mapKVSize;
                char* last = first + sizeof(uint32_t);
                std::reverse(first, last);
            }
            //检测长度是否非法
            if (mapKVSize > MUSE_MAX_MAP_COUNT){
                readPosition = defaultPosition;
                throw BinarySerializerException("illegal map k-v pair count" , ErrorNumber::IllegalMapKVCount);
            }
            readPosition += sizeof(uint32_t);
            try {
                for (unsigned int i = 0; i < mapKVSize;i++)
                {
                    K k;  //必须具备默认构造函数
                    V v;  //必须具备默认构造函数
                    output(k);
                    output(v);
                    value[k] = v;
                }
            }catch(BinarySerializerException &serializerException) {
                readPosition = defaultPosition;
                throw serializerException;
            }catch (std::exception &ex) {
                readPosition = defaultPosition;
                throw ex;
            }catch (...){
                readPosition = defaultPosition;
                throw BinarySerializerException("not know error", ErrorNumber::ErrorReadingSubElements);
            }
            return *this;
        }

        template<typename K, typename V,
                typename = typename std::enable_if_t<std::is_default_constructible_v<K>>,
                typename = typename std::enable_if_t<std::is_default_constructible_v<V>>
        >
        BinarySerializer& output(std::unordered_map<K,V> &value){
            //防止越界
            MUSE_PREVENT_BOUNDARY()
            //类型检测
            if (byteStream[readPosition] != (char)BinaryDataType::HASHMAP)
                throw BinarySerializerException("read type error", ErrorNumber::DataTypeError);
            auto defaultPosition = readPosition;
            readPosition++;
            value.clear();
            //获取 hashmap 数量
            auto leftSize =  byteStream.size() - readPosition;
            if (leftSize < sizeof(uint32_t)){
                readPosition = defaultPosition;
                throw BinarySerializerException("remaining memory is not enough ", ErrorNumber::InsufficientRemainingMemory);
            }
            //读取字符串长度 大小端处理
            auto mapKVSize = *((uint32_t *)(&byteStream[readPosition]));
            if (byteSequence == ByteSequence::BigEndian)
            {
                char* first = (char*)&mapKVSize;
                char* last = first + sizeof(uint32_t);
                std::reverse(first, last);
            }
            //检测长度是否非法
            if (mapKVSize > MUSE_MAX_MAP_COUNT){
                readPosition = defaultPosition;
                throw BinarySerializerException("illegal hash map k-v pair count" , ErrorNumber::IllegalHashMapKVCount);
            }
            readPosition += sizeof(uint32_t);
            try {
                for (unsigned int i = 0; i < mapKVSize;i++)
                {
                    K k;  //必须具备默认构造函数
                    V v;  //必须具备默认构造函数
                    output(k);
                    output(v);
                    value[k] = v;
                }
            }catch(BinarySerializerException &serializerException) {
                readPosition = defaultPosition;
                throw serializerException;
            }catch (std::exception &ex) {
                readPosition = defaultPosition;
                throw ex;
            }catch (...){
                readPosition = defaultPosition;
                throw BinarySerializerException("not know error", ErrorNumber::ErrorReadingSubElements);
            }
            return *this;
        }

        template<typename T, typename... Args>
        BinarySerializer& outputArgs(T& value, Args&... args){
            output(value);
            outputArgs(args...);
            return *this;
        }
        BinarySerializer& outputArgs() { return *this; };

        template<typename... Args>
        BinarySerializer& output(std::tuple<Args...>& tpl){
            //防止越界
            MUSE_PREVENT_BOUNDARY()
            //类型检测
            if (byteStream[readPosition] != (char)BinaryDataType::TUPLE)
                throw BinarySerializerException("read type error", ErrorNumber::DataTypeError);
            auto defaultPosition = readPosition;
            readPosition++;
            //获取 tuple 元素个数
            auto leftSize =  byteStream.size() - readPosition;
            if (leftSize < sizeof(uint16_t)){
                readPosition = defaultPosition;
                throw BinarySerializerException("remaining memory is not enough ", ErrorNumber::InsufficientRemainingMemory);
            }
            auto tupleSize = *((uint16_t *)(&byteStream[readPosition]));
            //处理大小端
            if (byteSequence == ByteSequence::BigEndian)
            {
                char* first = (char*)&tupleSize;
                char* last = first + sizeof(uint16_t);
                std::reverse(first, last);
            }
            //检测长度是否非法
            if (tupleSize > MUSE_MAX_TUPLE_COUNT){
                readPosition = defaultPosition;
                throw BinarySerializerException("illegal tuple count" , ErrorNumber::IllegalTupleCount);
            }
            constexpr size_t N = sizeof...(Args);
            if (tupleSize != N){
                readPosition = defaultPosition;
                throw BinarySerializerException("tuple count not match" , ErrorNumber::IllegalParameterTupleCount);
            }
            readPosition += sizeof(uint16_t);
            try {
                output_tuple_helper(tpl, std::make_index_sequence<N>());
                //TupleReader<N - 1>::read(tpl,*this);
            }catch(BinarySerializerException &serializerException) {
                readPosition = defaultPosition;
                throw serializerException;
            }catch (std::exception &ex) {
                readPosition = defaultPosition;
                throw ex;
            }catch (...){
                readPosition = defaultPosition;
                throw BinarySerializerException("not know error", ErrorNumber::ErrorReadingSubElements);
            }
            return *this;
        }

        BinarySerializer& output(std::tuple<> & tpl);

        BinarySerializer &output(muse::serializer::IBinarySerializable &serializable);
    };

    template<>
    struct BinarySerializer::TupleWriter<0> {
        template<typename... Args>
        static void write(const std::tuple<Args...>& t, BinarySerializer& serializer){
            serializer.input(std::get<0>(t));
        }
    };

    template<>
    struct BinarySerializer::TupleReader<0>{
        template<typename... Args>
        static void read(std::tuple<Args...>& t, BinarySerializer& serializer){
            //typename std::tuple_element<0,decltype(t)>::type first;
            serializer.output(std::get<0>(t));
        }
    };

#define MUSE_IBinarySerializable(...)   \
    void serialize(muse::serializer::BinarySerializer &serializer) const override{     \
        auto type = muse::serializer::BinaryDataType::MUSECLASS;                             \
        serializer.write((char*)&type, sizeof(type));                \
        serializer.inputArgs(__VA_ARGS__);                           \
    };                                                               \
                                                                     \
    void deserialize(muse::serializer::BinarySerializer &serializer)  override {    \
        auto readPosition = serializer.getReadPosition();               \
        if (readPosition == serializer.byteCount()){                    \
            throw muse::serializer::BinarySerializerException("remaining memory is not enough ", muse::serializer::ErrorNumber::InsufficientRemainingMemory); \
        }                                                                                                                       \
        auto result = serializer.checkDataTypeRightForward(muse::serializer::BinaryDataType::MUSECLASS);                                          \
        if (!result) throw muse::serializer::BinarySerializerException("read type error", muse::serializer::ErrorNumber::DataTypeError);                      \
        serializer.outputArgs(__VA_ARGS__);                                                                                     \
    }                                                                    \
    void deserialize(muse::serializer::BinaryDeserializeView &serializer)  override {    \
        auto readPosition = serializer.getReadPosition();               \
        if (readPosition == serializer.byteCount()){                    \
            throw muse::serializer::BinarySerializerException("remaining memory is not enough ", muse::serializer::ErrorNumber::InsufficientRemainingMemory); \
        }                                                                                                                       \
        auto result = serializer.checkDataTypeRightForward(muse::serializer::BinaryDataType::MUSECLASS);                                          \
        if (!result) throw muse::serializer::BinarySerializerException("read type error", muse::serializer::ErrorNumber::DataTypeError);                      \
        serializer.outputArgs(__VA_ARGS__);                                                                                     \
    }
}
#endif //MUSE_SERIALIZER_BINARY_SERIALIZER_H