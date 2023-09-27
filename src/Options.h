// This is a personal academic project. Dear PVS-Studio, please check it.

// PVS-Studio Static Code Analyzer for C, C++, C#, and Java:
// https://pvs-studio.com

#ifndef LIBAV_TEST_OPTIONS_H
#define LIBAV_TEST_OPTIONS_H

#include "Logger.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/common.h>
#include <libavutil/timestamp.h>
}

#include <iostream>


/**
 * See https://ffmpeg.org/ffmpeg-formats.html#Format-Options
 */
class Options
{
public:
    AVDictionary * av_format_options_;
    int av_flags_ = 0;

    std::unordered_map<std::string, int> av_fmt_flags = {
        {"AVFMT_FLAG_GENPTS", AVFMT_FLAG_GENPTS},
        {"AVFMT_FLAG_IGNIDX", AVFMT_FLAG_IGNIDX},
        {"AVFMT_FLAG_NONBLOCK", AVFMT_FLAG_NONBLOCK},
        {"AVFMT_FLAG_IGNDTS", AVFMT_FLAG_IGNDTS},
        {"AVFMT_FLAG_NOFILLIN", AVFMT_FLAG_NOFILLIN},
        {"AVFMT_FLAG_NOPARSE", AVFMT_FLAG_NOPARSE},
        {"AVFMT_FLAG_NOBUFFER", AVFMT_FLAG_NOBUFFER},
        {"AVFMT_FLAG_CUSTOM_IO", AVFMT_FLAG_CUSTOM_IO},
        {"AVFMT_FLAG_DISCARD_CORRUPT", AVFMT_FLAG_DISCARD_CORRUPT},
        {"AVFMT_FLAG_FLUSH_PACKETS", AVFMT_FLAG_FLUSH_PACKETS},

        {"AVFMT_NOFILE", AVFMT_NOFILE},
        {"AVFMT_NEEDNUMBER", AVFMT_NEEDNUMBER},
        {"AVFMT_SHOW_IDS", AVFMT_SHOW_IDS},
        {"AVFMT_GLOBALHEADER", AVFMT_GLOBALHEADER},
        {"AVFMT_NOTIMESTAMPS", AVFMT_NOTIMESTAMPS},
        {"AVFMT_GENERIC_INDEX", AVFMT_GENERIC_INDEX},
        {"AVFMT_GENERIC_INDEX", AVFMT_GENERIC_INDEX},
        {"AVFMT_TS_DISCONT", AVFMT_TS_DISCONT},
        {"AVFMT_VARIABLE_FPS", AVFMT_VARIABLE_FPS},
        {"AVFMT_NODIMENSIONS", AVFMT_NODIMENSIONS},
        {"AVFMT_NOSTREAMS", AVFMT_NOSTREAMS},
        {"AVFMT_NOBINSEARCH", AVFMT_NOBINSEARCH},
        {"AVFMT_NOGENSEARCH", AVFMT_NOGENSEARCH},
        {"AVFMT_NO_BYTE_SEEK", AVFMT_NO_BYTE_SEEK},
        {"AVFMT_NO_BYTE_SEEK", AVFMT_NO_BYTE_SEEK},
        {"AVFMT_NO_BYTE_SEEK", AVFMT_NO_BYTE_SEEK},
        {"AVFMT_ALLOW_FLUSH", AVFMT_ALLOW_FLUSH},
        {"AVFMT_TS_NONSTRICT", AVFMT_TS_NONSTRICT},

        {"AV_CODEC_FLAG_UNALIGNED", AV_CODEC_FLAG_UNALIGNED},
        {"AV_CODEC_FLAG_QSCALE", AV_CODEC_FLAG_QSCALE},
        {"AV_CODEC_FLAG_4MV", AV_CODEC_FLAG_4MV},
        {"AV_CODEC_FLAG_OUTPUT_CORRUPT", AV_CODEC_FLAG_OUTPUT_CORRUPT},
        {"AV_CODEC_FLAG_QPEL", AV_CODEC_FLAG_QPEL},
        {"AV_CODEC_FLAG_DROPCHANGED", AV_CODEC_FLAG_DROPCHANGED},
        {"AV_CODEC_FLAG_LOOP_FILTER", AV_CODEC_FLAG_LOOP_FILTER},
        {"AV_CODEC_FLAG_GRAY", AV_CODEC_FLAG_GRAY},
        {"AV_CODEC_FLAG_PSNR", AV_CODEC_FLAG_PSNR},
        {"AV_CODEC_FLAG_TRUNCATED", AV_CODEC_FLAG_TRUNCATED},
        {"AV_CODEC_FLAG_INTERLACED_DCT", AV_CODEC_FLAG_INTERLACED_DCT},
        {"AV_CODEC_FLAG_GLOBAL_HEADER", AV_CODEC_FLAG_GLOBAL_HEADER},
        {"AV_CODEC_FLAG_BITEXACT", AV_CODEC_FLAG_BITEXACT},
        {"AV_CODEC_FLAG_AC_PRED", AV_CODEC_FLAG_AC_PRED},
        {"AV_CODEC_FLAG_INTERLACED_ME", AV_CODEC_FLAG_INTERLACED_ME},
        {"AV_CODEC_FLAG_CLOSED_GOP", AV_CODEC_FLAG_CLOSED_GOP},

    };

    Options()
    {
        LOG("Options:constructor")
        av_format_options_ = NULL;
    }

    void setAVOption(const char * key, const char * value) { av_dict_set(&av_format_options_, key, value, AV_DICT_APPEND); }

    void setAVFlag(const std::string & av_flag) { av_flags_ |= av_fmt_flags[av_flag]; };

    int getAVFlags() { return av_flags_; };

    ~Options()
    {
        LOG("Options:destructor")
        av_dict_free(&av_format_options_);
    }
};

#endif // LIBAV_TEST_OPTIONS_H
