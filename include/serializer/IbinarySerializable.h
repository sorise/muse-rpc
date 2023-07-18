//
// Created by remix on 23-5-30.
//

#ifndef SERIALIZER_I_BINARY_SERIALIZABLE_H
#define SERIALIZER_I_BINARY_SERIALIZABLE_H 2
#include "binarySerializer.h"

//接口类
namespace muse::serializer{
    class BinarySerializer;
    //用户自定义类型必须具有
    class IBinarySerializable{
    public:
        virtual void serialize(BinarySerializer &serializer) const = 0;
        virtual void deserialize(BinarySerializer &serializer) = 0;
    };
}

#endif //SERIALIZER_I_BINARY_SERIALIZABLE_H
