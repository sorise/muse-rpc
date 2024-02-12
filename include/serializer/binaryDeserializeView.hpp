//
// Created by remix on 23-8-27.
//

#ifndef SERIALIZER_BINARY_DESERIALIZE_VIEW_HPP
#define SERIALIZER_BINARY_DESERIALIZE_VIEW_HPP
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

#define MUSE_VIEW_PREVENT_BOUNDARY() \
    if (readPosition == byteStreamSize){ \
        throw BinarySerializerException("remaining memory is not enough ", ErrorNumber::InsufficientRemainingMemory); \
    }

namespace muse::serializer{
    class IBinarySerializable; //define

    class BinaryDeserializeView {
    public:
        using Position_Type = long;
        using Element_Length = uint32_t ;
        using Tuple_Element_Length = uint16_t ;
    private:
        /* 存储字节流数据 */
        const char *byteStream;
        /* 字节数量 */
        size_t byteStreamSize;
        /* 读取指针 */
        Position_Type readPosition;
        /* 字节序 */
        ByteSequence byteSequence;
        /* 读取 */
        bool read(char* data, const unsigned int& len) noexcept ;

        template<typename Tuple, std::size_t... Index>
        void  output_tuple_helper(Tuple&& t, std::index_sequence<Index...>){
            outputArgs(std::get<Index>(std::forward<Tuple>(t))...);
        }
    public:
        using Container_type = std::vector<char>;
        void clear();                            //清除所有
        void reset();                            //重新从开始读取
        void reset_bytestream(const char* _byteStream, size_t _byteStreamSize);
        [[nodiscard]] Container_type::size_type byteCount() const;            //多少字节数量
        [[nodiscard]] Position_Type getReadPosition() const;                  //获得当前指针所在位置
        bool checkDataTypeRightForward(BinaryDataType type);                  //检查类型是否正确，如果正确，移动 readPosition
        BinaryDeserializeView();                                              //默认构造函数
        BinaryDeserializeView(const char* _byteStream, size_t _byteStreamSize);       //默认构造函数
        BinaryDeserializeView(const BinaryDeserializeView& other) = delete;   //禁止复制
        BinaryDeserializeView(BinaryDeserializeView &&other) noexcept;        //支持移动操作
        [[nodiscard]] const char* getBinaryStream() const;                          //返回二进制流的指针

        void saveToFile(const std::string& path) const;    //二进制数据存储到文件中

        ~BinaryDeserializeView();
        //反序列化
        BinaryDeserializeView& output(bool &);
        BinaryDeserializeView& output(char &);
        BinaryDeserializeView& output(unsigned char &);
        BinaryDeserializeView& output(int16_t &);
        BinaryDeserializeView& output(int32_t &);
        BinaryDeserializeView& output(int64_t &);
        BinaryDeserializeView& output(uint16_t &);
        BinaryDeserializeView& output(uint32_t &);
        BinaryDeserializeView& output(uint64_t &);
        BinaryDeserializeView& output(float &);
        BinaryDeserializeView& output(double &);
        BinaryDeserializeView& output(std::string &);
        BinaryDeserializeView& output(char * , unsigned int ); /* 可能存在有复制消耗*/
        BinaryDeserializeView& output(unsigned char * , unsigned int ); /* 可能存在有复制消耗*/

        template<class T>
        T output(){
            T t;
            this->output(t);
            return t;
        }

        template<typename T, typename = typename std::enable_if_t<std::is_default_constructible_v<T>>>
        BinaryDeserializeView& output(std::vector<T>& value)
        {
            //防止越界
            MUSE_VIEW_PREVENT_BOUNDARY()
            //类型检测
            if (byteStream[readPosition] != (char)BinaryDataType::VECTOR)
                throw BinarySerializerException("read type error", ErrorNumber::DataTypeError);

            auto defaultPosition = readPosition;
            value.clear(); //先清除数据
            //读取长度信息
            readPosition++;
            //获取 vector 长度
            auto leftSize =  byteStreamSize - readPosition;
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
        BinaryDeserializeView& output(std::array<T, N>& value)
        {
            //防止越界
            MUSE_VIEW_PREVENT_BOUNDARY()
            //类型检测
            if (byteStream[readPosition] != (char)BinaryDataType::ARRAY)
                throw BinarySerializerException("read type error", ErrorNumber::DataTypeError);

            auto defaultPosition = readPosition;
            //读取长度信息
            readPosition++;
            //获取 array 长度
            auto leftSize =  byteStreamSize - readPosition;
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
        BinaryDeserializeView& output(std::list<T>& value){
            //防止越界
            MUSE_VIEW_PREVENT_BOUNDARY()
            //类型检测
            if (byteStream[readPosition] != (char)BinaryDataType::LIST)
                throw BinarySerializerException("read type error", ErrorNumber::DataTypeError);
            auto defaultPosition = readPosition;
            value.clear(); //先清除数据
            //读取长度信息
            readPosition++;
            //获取 list 长度
            auto leftSize =  byteStreamSize - readPosition;
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
        BinaryDeserializeView& output(std::set<T>& value){
            //防止越界
            MUSE_VIEW_PREVENT_BOUNDARY()
            //类型检测
            if (byteStream[readPosition] != (char)BinaryDataType::SET)
                throw BinarySerializerException("read type error", ErrorNumber::DataTypeError);
            auto defaultPosition = readPosition;
            value.clear();
            //读取长度信息
            readPosition++;
            //获取 list 长度
            auto leftSize =  byteStreamSize - readPosition;
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
        BinaryDeserializeView& output(std::map<K,V> &value){
            //防止越界
            MUSE_VIEW_PREVENT_BOUNDARY()
            //类型检测
            if (byteStream[readPosition] != (char)BinaryDataType::MAP)
                throw BinarySerializerException("read type error", ErrorNumber::DataTypeError);
            auto defaultPosition = readPosition;
            readPosition++;
            value.clear();
            //获取 list 长度
            auto leftSize =  byteStreamSize - readPosition;
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
        BinaryDeserializeView& output(std::unordered_map<K,V> &value){
            //防止越界
            MUSE_VIEW_PREVENT_BOUNDARY()
            //类型检测
            if (byteStream[readPosition] != (char)BinaryDataType::HASHMAP)
                throw BinarySerializerException("read type error", ErrorNumber::DataTypeError);
            auto defaultPosition = readPosition;
            readPosition++;
            value.clear();
            //获取 hashmap 数量
            auto leftSize =  byteStreamSize - readPosition;
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
        BinaryDeserializeView& outputArgs(T& value, Args&... args){
            output(value);
            outputArgs(args...);
            return *this;
        }
        BinaryDeserializeView& outputArgs() { return *this; };

        template<typename... Args>
        BinaryDeserializeView& output(std::tuple<Args...>& tpl){
            //防止越界
            MUSE_VIEW_PREVENT_BOUNDARY()
            //类型检测
            if (byteStream[readPosition] != (char)BinaryDataType::TUPLE)
                throw BinarySerializerException("read type error", ErrorNumber::DataTypeError);
            auto defaultPosition = readPosition;
            readPosition++;
            //获取 tuple 元素个数
            auto leftSize =  byteStreamSize - readPosition;
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

        BinaryDeserializeView& output(std::tuple<> & tpl);

        BinaryDeserializeView &output(muse::serializer::IBinarySerializable &serializable);

    };
}


#endif //SERIALIZER_BINARY_DESERIALIZE_VIEW_HPP
