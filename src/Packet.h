// This is a personal academic project. Dear PVS-Studio, please check it.

// PVS-Studio Static Code Analyzer for C, C++, C#, and Java:
// https://pvs-studio.com

#ifndef LIBAV_TEST_PACKET_H
#define LIBAV_TEST_PACKET_H

#include <iostream>
#include "Logger.h"

extern "C" {
#include <libavcodec/avcodec.h>
}

class Packet
{
public:
    AVPacket * pkt;

    Packet()
    {
        LOG("Packet:constructor"  << endl)
        this->pkt = av_packet_alloc();
        if (!this->pkt)
        {
            fprintf(stderr, "Could not allocate AVPacket\n");
        }
    }

    ~Packet()
    {
        LOG("Packet:destructor" << endl)
        av_packet_free(&pkt);
    }
};

#endif // LIBAV_TEST_PACKET_H
