//
// Created by remix on 23-6-23.
// linux 不是有定时器信号
#include <iostream>
#include <chrono>
#include <limits>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <zlib.h>
#include <functional>
#include <cstring>

int main()
{
    //原始数据
    const unsigned char strSrc[]="48asd25da2s3d3sdsdadasdwe48wed74asdqsdwasewwsdrasfdasdfasdghgasdasddjqw5e4fqw894e528as3asd813asd515h5i132jg8h23yrg8gev17123891235479465h56as34d56as1c8e3";

    unsigned char destOutBuffer[4096];
    uLongf desSize = compressBound(sizeof(strSrc));

    std::cout << desSize << std::endl;

    compress(destOutBuffer,&desSize,strSrc, sizeof(strSrc));

    printf("%lu \n", desSize);

    unsigned char jiayaOutBuffer[4096];
    uLongf jiayadesSize = 4096;
    int ret = uncompress(jiayaOutBuffer, &jiayadesSize, destOutBuffer, desSize);

    if (ret != Z_OK) {
        printf("Error decompressing data: %d\n", ret);
        return ret;
    }
    printf("%lu \n", jiayadesSize);


    return 0;
}
