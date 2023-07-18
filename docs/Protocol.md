```cpp
 ProtocolHeader{
    // 六 字节
    uint8_t synchronousWord;          // 1 同步字  全1111 0000
    uint8_t versionAndType;           // 1 协议版本 和 类型
    uint16_t pieceOrder;              // 2 序号
    uint16_t pieceSize;               // 2 当前分片多少个字节数
    // 八 字节
    uint64_t timePoint;               // 8 区分不同的包 报文 ID
    // 八 字节
    uint16_t totalCount;                   // 2 总共有多少个包
    uint16_t pieceStandardSize ;      // 2 标准大小
    uint32_t totalSize;               // 4 UDP完整报文的大小
    // 四 字节
    uint16_t acceptMinOrder;          // 2 已被确认序号
    uint16_t acceptOrder;             // 2 确认序号
};
```