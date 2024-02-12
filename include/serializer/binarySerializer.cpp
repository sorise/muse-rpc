//
// Created by remix on 23-5-26.
//
#include <cstdint>
#include "serializer/binarySerializer.h"

namespace muse::serializer{
    class IBinarySerializable;
/* 进行反序列合法性检测 */
#define MUSE_CHECK_LEGITIMACY(TYPENAME, realName)                            \
    auto needLength = sizeof(BinaryDataType::TYPENAME) + sizeof(realName );        \
    if((byteStream.size() - readPosition) < needLength )                     \
        throw BinarySerializerException("remaining memory is not enough ", ErrorNumber::InsufficientRemainingMemory); \
                                                                                    \
    if (byteStream[readPosition] != (char)BinaryDataType::TYPENAME)                       \
        throw BinarySerializerException("read type error", ErrorNumber::DataTypeError);   \


/* 反序列化过程化中：如果主机是大端序，则转换为大端序 */
#define MUSE_CONVERT_TO_BIG_ENDIAN(realDataType)            \
    if (byteSequence == ByteSequence::BigEndian)            \
    {                                                       \
        char* first = (char*)&value;                        \
        char* last = first + sizeof(realDataType);          \
        std::reverse(first, last);                          \
    }                                                       \
    readPosition += static_cast<int>(sizeof(realDataType));


/* 序列化过程化中：如果主机是大端序，则转换为小端序 */
#define MUSE_CONVERT_TO_LITTLE_ENDIAN()                 \
    write((char*)&type, sizeof(type));                  \
    write((char*)&value, (int)dataTypeSize);            \
    if (byteSequence == ByteSequence::BigEndian){       \
        auto last = &byteStream[byteStream.size()];     \
        auto first = last - dataTypeSize;               \
        std::reverse(first, last);                      \
    }


#define MUSE_PREVENT_CROSSING_BOUNDARIES()              \
    if (readPosition == byteStream.size()){             \
        throw BinarySerializerException("remaining memory is not enough ", ErrorNumber::InsufficientRemainingMemory); \
    }

    BinarySerializer::BinarySerializer():readPosition(0){
        byteStream.reserve(MUSE_DEFAULT_CAPACITY); //先给一个初始容量 32 字节
        byteSequence = getByteSequence();
    };
    BinarySerializer::~BinarySerializer()  = default;

    BinarySerializer::BinarySerializer(BinarySerializer &&other) noexcept  {
        byteStream = std::move(other.byteStream); //容器必须支持移动
        readPosition = other.readPosition;
        byteSequence = other.byteSequence;

        other.readPosition = 0;
    }

    void BinarySerializer::expansion(size_t len){
        if (byteStream.capacity() - byteStream.size() < len ){
            auto hasSize= byteStream.size();
            auto cap = byteStream.capacity();
            while ((cap - hasSize) < len){
                cap = cap * 2;
            };
            byteStream.reserve(cap);
        }
    }

    BinarySerializer& BinarySerializer::write(const char *data, const unsigned int& len) {
        if (len > 0){
            expansion(len); //先扩容
            auto length = byteStream.size();   //长度
            byteStream.resize(length + len);  //重新设置长度
            std::memcpy((char *)&byteStream[length], data, len); //内存拷贝
        }
        return *this;
    }

    bool BinarySerializer::read(char *data, const unsigned int &len) noexcept{
        if (readPosition + len > byteStream.size()){
            return false;
        }
        std::memcpy(data, byteStream.data() + readPosition, len);
        readPosition += (int)len;
        return true;
    }

    void BinarySerializer::clear() {
        Store_Element_Type type;
        byteStream.swap(type);
        byteStream.reserve(MUSE_DEFAULT_CAPACITY);
        readPosition = 0;
    }

    void BinarySerializer::reset() {
        readPosition = 0;
    }

    BinarySerializer::Container_type::size_type BinarySerializer::byteCount() const{
        return byteStream.size();
    }

    BinarySerializer& BinarySerializer::input(const bool &value) {
        auto type = BinaryDataType::BOOL;
        write((char*)&type, sizeof(type)); //类型
        write((char*)&value, sizeof(char)); //值
        return *this;
    }

    BinarySerializer &BinarySerializer::input(const char & value) {
        auto type = BinaryDataType::CHAR;
        write((char*)&type, sizeof(type)); //类型
        write((char*)&value, sizeof(char)); //值
        return *this;
    }

    BinarySerializer &BinarySerializer::input(const unsigned char & value) {
        auto type = BinaryDataType::UINT8;
        write((char*)&type, sizeof(type)); //类型
        write((char*)&value, sizeof(unsigned char)); //值
        return *this;
    }

    BinarySerializer &BinarySerializer::input(const int16_t & value) {
        auto type = BinaryDataType::INT16;
        auto dataTypeSize = sizeof(int16_t);
        MUSE_CONVERT_TO_LITTLE_ENDIAN()
        return *this;
    }

    BinarySerializer &BinarySerializer::input(const int32_t & value) {
        auto type = BinaryDataType::INT32;
        auto dataTypeSize = sizeof(int32_t);
        MUSE_CONVERT_TO_LITTLE_ENDIAN()
        return *this;
    }

    BinarySerializer &BinarySerializer::input(const int64_t & value) {
        auto type = BinaryDataType::INT64;
        auto dataTypeSize = sizeof(int64_t);
        MUSE_CONVERT_TO_LITTLE_ENDIAN()
        return *this;
    }

    BinarySerializer &BinarySerializer::input(const uint16_t & value) {
        auto type = BinaryDataType::UINT16;
        auto dataTypeSize = sizeof(uint16_t);
        MUSE_CONVERT_TO_LITTLE_ENDIAN()
        return *this;
    }

    BinarySerializer &BinarySerializer::input(const uint32_t & value) {
        auto type = BinaryDataType::UINT32;
        auto dataTypeSize = sizeof(uint32_t);
        MUSE_CONVERT_TO_LITTLE_ENDIAN()
        return *this;
    }

    BinarySerializer &BinarySerializer::input(const uint64_t & value) {
        auto type = BinaryDataType::UINT64;
        auto dataTypeSize = sizeof(uint64_t);
        MUSE_CONVERT_TO_LITTLE_ENDIAN()
        return *this;
    }

    BinarySerializer &BinarySerializer::input(const float & value) {
        auto type = BinaryDataType::FLOAT;
        auto dataTypeSize = sizeof(float);
        MUSE_CONVERT_TO_LITTLE_ENDIAN()
        return *this;
    }

    BinarySerializer &BinarySerializer::input(const double & value) {
        auto type = BinaryDataType::DOUBLE;
        auto dataTypeSize = sizeof(double);
        MUSE_CONVERT_TO_LITTLE_ENDIAN()
        return *this;
    }

    BinarySerializer &BinarySerializer::input(const std::string & value) {
        uint32_t len = value.length();              //存储字符串长度
        //字符串长度不能超过  MUSE_MAX_STRING_LENGTH 规定
        if (len > MUSE_MAX_STRING_LENGTH){
            throw BinarySerializerException("the string length exceeds the limit", ErrorNumber::TheStringLengthExceedsTheLimit);
        }
        auto type = BinaryDataType::STRING;
        write((char*)&type, sizeof(type)); //类型
        //存储长度
        write((char*)&len, sizeof(uint32_t));
        //存储长度
        if (byteSequence == ByteSequence::BigEndian){
            auto last = &byteStream[byteStream.size()];
            auto first = last - sizeof(uint32_t);
            std::reverse(first, last);
        }
        //存储数据
        write(value.c_str(), (int)len);
        return *this;
    }

    /*  BYTES
     * [  1byte 类型  ][   4byte 字节长度   ][ 存储字节数据....  ]
     */
    BinarySerializer &BinarySerializer::input(const char * value, unsigned int len) {
        if (len > MUSE_MAX_BYTE_COUNT){
            throw BinarySerializerException("the bytes count exceeds the limit", ErrorNumber::TheBytesCountExceedsTheLimit);
        }
        auto type = BinaryDataType::BYTES;
        write((char*)&type, sizeof(type)); //类型
        //存储长度
        write((char*)&len, sizeof(uint32_t));
        //存储长度 注意大小端问题
        if (byteSequence == ByteSequence::BigEndian){
            auto last = &byteStream[byteStream.size()];
            auto first = last - sizeof(uint32_t);
            std::reverse(first, last);
        }
        //写入数据
        write((char*)value, len); //值
        return *this;
    }

    /*  UBYTES
     * [  1byte 类型  ][   4byte 字节长度   ][ 存储字节数据....  ]
     */
    BinarySerializer &BinarySerializer::input(const unsigned char * value, unsigned int len) {
        if (len > MUSE_MAX_BYTE_COUNT){
            throw BinarySerializerException("the bytes count exceeds the limit", ErrorNumber::TheBytesCountExceedsTheLimit);
        }
        auto type = BinaryDataType::UBYTES;
        write((char*)&type, sizeof(type)); //类型
        //存储长度
        write((char*)&len, sizeof(uint32_t));
        //存储长度, 注意大小端问题
        if (byteSequence == ByteSequence::BigEndian){
            auto last = &byteStream[byteStream.size()];
            auto first = last - sizeof(uint32_t);
            std::reverse(first, last);
        }
        //写入数据
        write(( char*)value, len); //值
        return *this;
    }

    BinarySerializer& BinarySerializer::input(const std::tuple<> & tpl){
        return *this;
    }

    BinarySerializer& BinarySerializer::output(bool & value) {
        MUSE_CHECK_LEGITIMACY(BOOL,bool)
        value = (bool)byteStream[++readPosition];
        ++readPosition;
        return *this;
    }

    BinarySerializer& BinarySerializer::output(char & value) {
        MUSE_CHECK_LEGITIMACY(CHAR,char)
        value = (char)byteStream[++readPosition];
        ++readPosition;
        return *this;
    }

    BinarySerializer& BinarySerializer::output(unsigned char & value) {
        MUSE_CHECK_LEGITIMACY(UINT8,unsigned char)
        value = (unsigned char)byteStream[++readPosition];
        ++readPosition;
        return *this;
    }

    BinarySerializer& BinarySerializer::output(int16_t & value) {
        MUSE_CHECK_LEGITIMACY(INT16,int16_t)
        value = *((int16_t *)(&byteStream[++readPosition]));
        //如果主机是大端序 将其转换为大端序列
        MUSE_CONVERT_TO_BIG_ENDIAN(int16_t)
        return *this;
    }

    BinarySerializer& BinarySerializer::output(int32_t & value) {
        MUSE_CHECK_LEGITIMACY(INT32,int32_t)
        value = *((int32_t *)(&byteStream[++readPosition]));
        //如果主机是大端序 将其转换为大端序列
        MUSE_CONVERT_TO_BIG_ENDIAN(int32_t)
        return *this;
    }

    BinarySerializer& BinarySerializer::output(int64_t & value) {
        MUSE_CHECK_LEGITIMACY(INT64,int64_t)
        value = *((int64_t *)(&byteStream[++readPosition]));
        //如果主机是大端序 将其转换为大端序列
        MUSE_CONVERT_TO_BIG_ENDIAN(int64_t)
        return *this;
    }

    BinarySerializer& BinarySerializer::output(uint16_t & value) {
        MUSE_CHECK_LEGITIMACY(UINT16,uint16_t)
        value = *((uint16_t *)(&byteStream[++readPosition]));
        //如果主机是大端序 将其转换为大端序列
        MUSE_CONVERT_TO_BIG_ENDIAN(uint16_t)
        return *this;
    }

    BinarySerializer& BinarySerializer::output(uint32_t & value) {
        MUSE_CHECK_LEGITIMACY(UINT32,uint32_t)
        value = *((uint32_t *)(&byteStream[++readPosition]));
        //如果主机是大端序 将其转换为大端序列
        MUSE_CONVERT_TO_BIG_ENDIAN(uint32_t)
        return *this;
    }

    BinarySerializer& BinarySerializer::output(uint64_t & value) {
        MUSE_CHECK_LEGITIMACY(UINT64,uint64_t)
        value = *((uint64_t *)(&byteStream[++readPosition]));
        //如果主机是大端序 将其转换为大端序列
        MUSE_CONVERT_TO_BIG_ENDIAN(uint64_t)
        return *this;
    }

    BinarySerializer& BinarySerializer::output(float & value) {
        MUSE_CHECK_LEGITIMACY(FLOAT,float)
        value = *((float *)(&byteStream[++readPosition]));
        //如果主机是大端序 将其转换为大端序列
        MUSE_CONVERT_TO_BIG_ENDIAN(float)
        return *this;
    }

    BinarySerializer& BinarySerializer::output(double & value) {
        MUSE_CHECK_LEGITIMACY(DOUBLE,double)
        value = *((double *)(&byteStream[++readPosition]));
        //如果主机是大端序 将其转换为大端序列
        MUSE_CONVERT_TO_BIG_ENDIAN(double)
        return *this;
    }

    BinarySerializer& BinarySerializer::output(std::string & str) {
        //防止访问越界
        MUSE_PREVENT_CROSSING_BOUNDARIES()
        auto defaultPosition = readPosition;
        //类型检测
        if (byteStream[readPosition] != (char)BinaryDataType::STRING)
            throw BinarySerializerException("read type error", ErrorNumber::DataTypeError);
        //剩余空间检测
        readPosition++;
        auto leftSize =  byteStream.size() - readPosition;
        if (leftSize < sizeof(uint32_t)){
            readPosition = defaultPosition;
            throw BinarySerializerException("remaining memory is not enough ", ErrorNumber::InsufficientRemainingMemory);
        }
        //字符串长度,大小端处理
        auto stringLength = *((uint32_t *)(&byteStream[readPosition]));
        if (byteSequence == ByteSequence::BigEndian)
        {
            char* first = (char*)&stringLength;
            char* last = first + sizeof(uint32_t);
            std::reverse(first, last);
        }
        //检测长度是否非法
        if (stringLength > MUSE_MAX_STRING_LENGTH){
            readPosition = defaultPosition;
            throw BinarySerializerException("illegal string length" , ErrorNumber::IllegalStringLength);
        }
        //检测剩余空间是否足够
        readPosition += sizeof(uint32_t);
        leftSize =  byteStream.size() - defaultPosition;
        if (leftSize < stringLength){
            readPosition = defaultPosition;
            throw BinarySerializerException("remaining memory is not enough ", ErrorNumber::InsufficientRemainingMemory);
        }
        //正确读取
        str.assign((char*)&byteStream[readPosition], stringLength);
        readPosition += static_cast<int>(stringLength); //移动
        return *this;
    }

    BinarySerializer& BinarySerializer::output(char *value, unsigned int len) {
        //防止初次访问越界
        MUSE_PREVENT_CROSSING_BOUNDARIES()
        auto defaultPosition = readPosition;
        //类型检测，可能会有越界错误
        if (byteStream[readPosition] != (char)BinaryDataType::BYTES)
            throw BinarySerializerException("read type error", ErrorNumber::DataTypeError);
        //读取长度信息
        readPosition++;
        auto leftSize =  byteStream.size() - readPosition;
        if (leftSize < sizeof(uint32_t)){
            readPosition = defaultPosition;
            throw BinarySerializerException("remaining memory is not enough ", ErrorNumber::InsufficientRemainingMemory);
        }
        //字符串长度 大小端处理
        auto stringLength = *((uint32_t *)(&byteStream[readPosition]));
        if (byteSequence == ByteSequence::BigEndian)
        {
            char* first = (char*)&stringLength;
            char* last = first + sizeof(uint32_t);
            std::reverse(first, last);
        }
        //检测长度是否非法
        if (stringLength > MUSE_MAX_BYTE_COUNT){
            readPosition = defaultPosition;
            throw BinarySerializerException("illegal bytes count" , ErrorNumber::IllegalBytesCount);
        }
        if (stringLength != len){
            throw BinarySerializerException("read illegal bytes count" , ErrorNumber::ReadIllegalBytesCount);
        }
        //检测剩余空间是否足够
        readPosition += sizeof(uint32_t);
        leftSize =  byteStream.size() - defaultPosition;
        if (leftSize < stringLength){
            readPosition = defaultPosition;
            throw BinarySerializerException("remaining memory is not enough ", ErrorNumber::InsufficientRemainingMemory);
        }
        std::memcpy(value,(char*)&byteStream[readPosition], stringLength);
        readPosition += static_cast<int>(stringLength);
        return *this;
    }

    BinarySerializer& BinarySerializer::output(unsigned char *value, unsigned int len) {
        //防止初次访问越界
        MUSE_PREVENT_CROSSING_BOUNDARIES()
        auto defaultPosition = readPosition;
        //类型检测
        if (byteStream[readPosition] != (char)BinaryDataType::UBYTES)
            throw BinarySerializerException("read type error", ErrorNumber::DataTypeError);
        //读取长度信息
        readPosition++;
        auto leftSize =  byteStream.size() - readPosition;
        if (leftSize < sizeof(uint32_t)){
            readPosition = defaultPosition;
            throw BinarySerializerException("remaining memory is not enough ", ErrorNumber::InsufficientRemainingMemory);
        }
        //字符串长度 大小端处理
        auto stringLength = *((uint32_t *)(&byteStream[readPosition]));
        if (byteSequence == ByteSequence::BigEndian)
        {
            char* first = (char*)&stringLength;
            char* last = first + sizeof(uint32_t);
            std::reverse(first, last);
        }
        //检测长度是否非法
        if (stringLength > MUSE_MAX_BYTE_COUNT){
            readPosition = defaultPosition;
            throw BinarySerializerException("illegal bytes count" , ErrorNumber::IllegalBytesCount);
        }
        if (stringLength != len){
            throw BinarySerializerException("read illegal bytes count" , ErrorNumber::ReadIllegalBytesCount);
        }
        //检测剩余空间是否足够
        readPosition += sizeof(uint32_t);
        leftSize =  byteStream.size() - defaultPosition;
        if (leftSize < stringLength){
            readPosition = defaultPosition;
            throw BinarySerializerException("remaining memory is not enough ", ErrorNumber::InsufficientRemainingMemory);
        }
        std::memcpy(value,(char*)&byteStream[readPosition], stringLength);
        readPosition += static_cast<int>(stringLength);
        return *this;
    }

    BinarySerializer& BinarySerializer::output(std::tuple<> & tpl){
        return *this;
    }

    BinarySerializer::Position_Type BinarySerializer::getReadPosition() const {
        return readPosition;
    }

    bool BinarySerializer::checkDataTypeRightForward(BinaryDataType dt){
        //防止初次访问越界
        MUSE_PREVENT_CROSSING_BOUNDARIES()
        if (byteStream[readPosition] == (char)dt){
            readPosition++;
            return true;
        }
        return false;
    }

    BinarySerializer &BinarySerializer::input(const IBinarySerializable &serializable) {
        serializable.serialize(*this);
        return *this;
    }

    BinarySerializer &BinarySerializer::output(IBinarySerializable &serializable) {
        serializable.deserialize(*this);
        return *this;
    }

    const char *BinarySerializer::getBinaryStream() const {
        return byteStream.data();
    }

    void BinarySerializer::saveToFile(const std::string& path) const{
        if (byteStream.empty()){
            throw BinarySerializerException("no data to store in file!", ErrorNumber::NoDataToStoreInFile);
        }
        std::ofstream saver(path,std::ios_base::binary | std::ios_base::trunc | std::ios_base::out );
        if (!saver.fail() && saver.is_open()){
            //数据量过大，会出现问题 unsigned long  到 long 的转换！ 且 long 能表示的数据量有限！
            saver.write(byteStream.data(), (long)byteStream.size());
            saver.flush();
        }else {
            //文件路径错误
            throw std::logic_error("the file path is not right!");
        }
        saver.close();
    }

    void BinarySerializer::loadFromFile(const std::string &path) {
        std::ifstream loader(path,std::ios_base::binary  | std::ios_base::in );
        if (!loader.fail() && loader.is_open()){
           byteStream.clear();
           readPosition = 0;
           constexpr size_t bufferSize = 1024;
           char loaderBuffer[bufferSize];
           size_t readBytes = loader.readsome(loaderBuffer, bufferSize);
           while (readBytes > 0){
               write(loaderBuffer, readBytes);
               readBytes = loader.readsome(loaderBuffer, bufferSize);
           }
        }else {
            //文件路径错误
            throw std::logic_error("the file path is not right!");
        }
    }
}
