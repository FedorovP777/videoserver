// This is a personal academic project. Dear PVS-Studio, please check it.

// PVS-Studio Static Code Analyzer for C, C++, C#, and Java:
// https://pvs-studio.com

#ifndef LIBAV_TEST_CODECCONTEXT_H
#define LIBAV_TEST_CODECCONTEXT_H

#include <iostream>
#include <sstream>
#include <string>
#include "Logger.h"

extern "C" {
#include <libavcodec/avcodec.h>
}

class CodecContext {
public:
    AVCodecContext *cntx = nullptr;

    CodecContext(AVCodec *codec_id) {
        LOG("CodecContext:constructor")
        cntx = avcodec_alloc_context3(codec_id);
    }

    ~CodecContext() {
        LOG("CodecContext:destructor")
        if (cntx != nullptr) {
            avcodec_free_context(&cntx);
            cntx = nullptr;
        }
    }
};

#endif // LIBAV_TEST_CODECCONTEXT_H
