// This is a personal academic project. Dear PVS-Studio, please check it.

// PVS-Studio Static Code Analyzer for C, C++, C#, and Java:
// https://pvs-studio.com

#ifndef LIBAV_FORMAT_CONTEXT_H
#define LIBAV_FORMAT_CONTEXT_H

#include <iostream>
#include <sstream>
#include <string>
#include "Logger.h"

using namespace std;

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/common.h>
#include <libavutil/timestamp.h>
}

class FormatContext
{
public:
    AVFormatContext * cntx;

    FormatContext()
    {
        Logger l(std::cout);
        LOG("FormatContext:constructor")
        cntx = avformat_alloc_context();
    }


    ~FormatContext()
    {
        Logger l(std::cout);
        LOG("FormatContext:destructor")

        if (cntx != nullptr)
        {
            avformat_close_input(&cntx);
            avformat_free_context(cntx);
            cntx = nullptr;
        }
    }
};

#endif // LIBAV_FORMAT_CONTEXT_H
