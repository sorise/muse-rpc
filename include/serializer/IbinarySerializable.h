//
// Created by remix on 23-5-30.
//

#ifndef SERIALIZER_I_BINARY_SERIALIZABLE_H
#define SERIALIZER_I_BINARY_SERIALIZABLE_H 2
#include "binarySerializer.h"
#include "binaryDeserializeView.hpp"

//接口类
namespace muse::serializer{
    class BinarySerializer;

    class BinaryDeserializeView;

    //用户自定义类型必须具有
    class IBinarySerializable{
    public:
        virtual void serialize(BinarySerializer &serializer) const = 0; //序列化
        virtual void deserialize(BinarySerializer &serializer) = 0;  //反序列化
        virtual void deserialize(BinaryDeserializeView &serializer) = 0; //反序列化
    };
}

#endif //SERIALIZER_I_BINARY_SERIALIZABLE_H
