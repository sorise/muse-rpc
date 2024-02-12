//
// Created by remix on 23-8-27.
//

#include "serializer/binaryDeserializeView.hpp"

namespace muse::serializer{
    class IBinarySerializable;
/* 进行反序列合法性检测 */
#define MUSE_VIEW_CHECK_LEGITIMACY(TYPENAME, realName)                            \
    auto needLength = sizeof(BinaryDataType::TYPENAME) + sizeof(realName );        \
    if((byteStreamSize - readPosition) < needLength )                     \
        throw BinarySerializerException("remaining memory is not enough ", ErrorNumber::InsufficientRemainingMemory); \
                                                                                    \
    if (byteStream[readPosition] != (char)BinaryDataType::TYPENAME)                       \
        throw BinarySerializerException("read type error", ErrorNumber::DataTypeError);   \


/* 反序列化过程化中：如果主机是大端序，则转换为大端序 */
#define MUSE_VIEW_CONVERT_TO_BIG_ENDIAN(realDataType)            \
    if (byteSequence == ByteSequence::BigEndian)            \
    {                                                       \
        char* first = (char*)&value;                        \
        char* last = first + sizeof(realDataType);          \
        std::reverse(first, last);                          \
    }                                                       \
    readPosition += static_cast<int>(sizeof(realDataType));


#define MUSE_VIEW_PREVENT_CROSSING_BOUNDARIES()       \
    if (readPosition == byteStreamSize){              \
        throw BinarySerializerException("remaining memory is not enough ", ErrorNumber::InsufficientRemainingMemory); \
    }

    BinaryDeserializeView::BinaryDeserializeView():byteStream(nullptr), readPosition(0), byteStreamSize(0){
        byteSequence = getByteSequence();
    };

    BinaryDeserializeView::~BinaryDeserializeView(){
        this->byteStream = nullptr;
    };

    BinaryDeserializeView::BinaryDeserializeView(BinaryDeserializeView &&other) noexcept  {
        byteStream = other.byteStream;
        readPosition = other.readPosition;
        byteSequence = other.byteSequence;
        byteStreamSize = other.byteStreamSize;
        other.byteStream = nullptr;
        other.readPosition = 0;
    }

    bool BinaryDeserializeView::read(char *data, const unsigned int &len) noexcept{
        if (readPosition + len > byteStreamSize){
            return false;
        }
        std::memcpy(data, byteStream + readPosition, len);
        readPosition += (int)len;
        return true;
    }

    void BinaryDeserializeView::clear() {
        byteStream = nullptr;
        readPosition = 0;
    }

    void BinaryDeserializeView::reset() {
        readPosition = 0;
    }

    BinaryDeserializeView::Container_type::size_type BinaryDeserializeView::byteCount() const{
        return byteStreamSize;
    }


    BinaryDeserializeView& BinaryDeserializeView::output(bool & value) {
        MUSE_VIEW_CHECK_LEGITIMACY(BOOL,bool)
        value = (bool)byteStream[++readPosition];
        ++readPosition;
        return *this;
    }

    BinaryDeserializeView& BinaryDeserializeView::output(char & value) {
        MUSE_VIEW_CHECK_LEGITIMACY(CHAR,char)
        value = (char)byteStream[++readPosition];
        ++readPosition;
        return *this;
    }

    BinaryDeserializeView& BinaryDeserializeView::output(unsigned char & value) {
        MUSE_VIEW_CHECK_LEGITIMACY(UINT8,unsigned char)
        value = (unsigned char)byteStream[++readPosition];
        ++readPosition;
        return *this;
    }

    BinaryDeserializeView& BinaryDeserializeView::output(int16_t & value) {
        MUSE_VIEW_CHECK_LEGITIMACY(INT16,int16_t)
        value = *((int16_t *)(&byteStream[++readPosition]));
        //如果主机是大端序 将其转换为大端序列
        MUSE_VIEW_CONVERT_TO_BIG_ENDIAN(int16_t)
        return *this;
    }

    BinaryDeserializeView& BinaryDeserializeView::output(int32_t & value) {
        MUSE_VIEW_CHECK_LEGITIMACY(INT32,int32_t)
        value = *((int32_t *)(&byteStream[++readPosition]));
        //如果主机是大端序 将其转换为大端序列
        MUSE_VIEW_CONVERT_TO_BIG_ENDIAN(int32_t)
        return *this;
    }

    BinaryDeserializeView& BinaryDeserializeView::output(int64_t & value) {
        MUSE_VIEW_CHECK_LEGITIMACY(INT64,int64_t)
        value = *((int64_t *)(&byteStream[++readPosition]));
        //如果主机是大端序 将其转换为大端序列
        MUSE_VIEW_CONVERT_TO_BIG_ENDIAN(int64_t)
        return *this;
    }

    BinaryDeserializeView& BinaryDeserializeView::output(uint16_t & value) {
        MUSE_VIEW_CHECK_LEGITIMACY(UINT16,uint16_t)
        value = *((uint16_t *)(&byteStream[++readPosition]));
        //如果主机是大端序 将其转换为大端序列
        MUSE_VIEW_CONVERT_TO_BIG_ENDIAN(uint16_t)
        return *this;
    }

    BinaryDeserializeView& BinaryDeserializeView::output(uint32_t & value) {
        MUSE_VIEW_CHECK_LEGITIMACY(UINT32,uint32_t)
        value = *((uint32_t *)(&byteStream[++readPosition]));
        //如果主机是大端序 将其转换为大端序列
        MUSE_VIEW_CONVERT_TO_BIG_ENDIAN(uint32_t)
        return *this;
    }

    BinaryDeserializeView& BinaryDeserializeView::output(uint64_t & value) {
        MUSE_VIEW_CHECK_LEGITIMACY(UINT64,uint64_t)
        value = *((uint64_t *)(&byteStream[++readPosition]));
        //如果主机是大端序 将其转换为大端序列
        MUSE_VIEW_CONVERT_TO_BIG_ENDIAN(uint64_t)
        return *this;
    }

    BinaryDeserializeView& BinaryDeserializeView::output(float & value) {
        MUSE_VIEW_CHECK_LEGITIMACY(FLOAT,float)
        value = *((float *)(&byteStream[++readPosition]));
        //如果主机是大端序 将其转换为大端序列
        MUSE_VIEW_CONVERT_TO_BIG_ENDIAN(float)
        return *this;
    }

    BinaryDeserializeView& BinaryDeserializeView::output(double & value) {
        MUSE_VIEW_CHECK_LEGITIMACY(DOUBLE,double)
        value = *((double *)(&byteStream[++readPosition]));
        //如果主机是大端序 将其转换为大端序列
        MUSE_VIEW_CONVERT_TO_BIG_ENDIAN(double)
        return *this;
    }

    BinaryDeserializeView& BinaryDeserializeView::output(std::string & str) {
        //防止访问越界
        MUSE_VIEW_PREVENT_CROSSING_BOUNDARIES()
        auto defaultPosition = readPosition;
        //类型检测
        if (byteStream[readPosition] != (char)BinaryDataType::STRING)
            throw BinarySerializerException("read type error", ErrorNumber::DataTypeError);
        //剩余空间检测
        readPosition++;
        auto leftSize =  byteStreamSize - readPosition;
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
        leftSize =  byteStreamSize - defaultPosition;
        if (leftSize < stringLength){
            readPosition = defaultPosition;
            throw BinarySerializerException("remaining memory is not enough ", ErrorNumber::InsufficientRemainingMemory);
        }
        //正确读取
        str.assign((char*)&byteStream[readPosition], stringLength);
        readPosition += static_cast<int>(stringLength); //移动
        return *this;
    }

    BinaryDeserializeView& BinaryDeserializeView::output(char *value, unsigned int len) {
        //防止初次访问越界
        MUSE_VIEW_PREVENT_CROSSING_BOUNDARIES()
        auto defaultPosition = readPosition;
        //类型检测，可能会有越界错误
        if (byteStream[readPosition] != (char)BinaryDataType::BYTES)
            throw BinarySerializerException("read type error", ErrorNumber::DataTypeError);
        //读取长度信息
        readPosition++;
        auto leftSize =  byteStreamSize - readPosition;
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
        leftSize =  byteStreamSize - defaultPosition;
        if (leftSize < stringLength){
            readPosition = defaultPosition;
            throw BinarySerializerException("remaining memory is not enough ", ErrorNumber::InsufficientRemainingMemory);
        }
        std::memcpy(value,(char*)&byteStream[readPosition], stringLength);
        readPosition += static_cast<int>(stringLength);
        return *this;
    }

    BinaryDeserializeView& BinaryDeserializeView::output(unsigned char *value, unsigned int len) {
        //防止初次访问越界
        MUSE_VIEW_PREVENT_CROSSING_BOUNDARIES()
        auto defaultPosition = readPosition;
        //类型检测
        if (byteStream[readPosition] != (char)BinaryDataType::UBYTES)
            throw BinarySerializerException("read type error", ErrorNumber::DataTypeError);
        //读取长度信息
        readPosition++;
        auto leftSize =  byteStreamSize - readPosition;
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
        leftSize =  byteStreamSize - defaultPosition;
        if (leftSize < stringLength){
            readPosition = defaultPosition;
            throw BinarySerializerException("remaining memory is not enough ", ErrorNumber::InsufficientRemainingMemory);
        }
        std::memcpy(value,(char*)&byteStream[readPosition], stringLength);
        readPosition += static_cast<int>(stringLength);
        return *this;
    }

    BinaryDeserializeView& BinaryDeserializeView::output(std::tuple<> & tpl){
        return *this;
    }

    BinaryDeserializeView::Position_Type BinaryDeserializeView::getReadPosition() const {
        return readPosition;
    }

    bool BinaryDeserializeView::checkDataTypeRightForward(BinaryDataType dt){
        //防止初次访问越界
        MUSE_VIEW_PREVENT_CROSSING_BOUNDARIES()
        if (byteStream[readPosition] == (char)dt){
            readPosition++;
            return true;
        }
        return false;
    }


    BinaryDeserializeView &BinaryDeserializeView::output(IBinarySerializable &serializable) {
        serializable.deserialize(*this);
        return *this;
    }

    const char *BinaryDeserializeView::getBinaryStream() const {
        return byteStream;
    }

    void BinaryDeserializeView::saveToFile(const std::string& path) const{
        if (byteStream == nullptr || byteStreamSize == 0){
            throw BinarySerializerException("no data to store in file!", ErrorNumber::NoDataToStoreInFile);
        }
        std::ofstream saver(path,std::ios_base::binary | std::ios_base::trunc | std::ios_base::out );
        if (!saver.fail() && saver.is_open()){
            //数据量过大，会出现问题 unsigned long  到 long 的转换！ 且 long 能表示的数据量有限！
            saver.write(byteStream, (long)byteStreamSize);
            saver.flush();
        }else {
            //文件路径错误
            throw std::logic_error("the file path is not right!");
        }
        saver.close();
    }

    BinaryDeserializeView::BinaryDeserializeView(const char *_byteStream, size_t _byteStreamSize)
    : byteStream(_byteStream), byteStreamSize(_byteStreamSize), readPosition(0) {
        byteSequence = getByteSequence();
    }

    void BinaryDeserializeView::reset_bytestream(const char *_byteStream, size_t _byteStreamSize) {
        byteStream = _byteStream;
        byteStreamSize = _byteStreamSize;
        readPosition = 0;
    }


}