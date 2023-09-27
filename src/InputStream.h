// This is a personal academic project. Dear PVS-Studio, please check it.

// PVS-Studio Static Code Analyzer for C, C++, C#, and Java:
// https://pvs-studio.com

#ifndef LIBAV_TEST_INPUTSTREAM_H
#define LIBAV_TEST_INPUTSTREAM_H

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <unordered_map>
#include "FormatContext.h"
#include "Options.h"
#include "CodecContext.h"
#include "Packet.h"
#include "FormatContext.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/time.h>
#include <libavformat/avformat.h>
#include <libavutil/common.h>
#include <libavutil/timestamp.h>
#include <libavutil/parseutils.h>
}
using namespace std;


void open_rtsp_stream(AVFormatContext * context, std::string & url, AVDictionary * options)
{
    int error_code = avformat_open_input(&context, url.c_str(), nullptr, &options);
    if (error_code < 0)
    {
        fprintf(stderr, "Could not open input file '%s'", url.c_str());
        auto * error_message = new std::string;
        error_message->resize(1024);
        av_strerror(error_code, error_message->data(), error_message->size());
        LOG(error_message)
        throw std::runtime_error(error_message->c_str());
    }
}
class InputStream
{
public:
    unique_ptr<FormatContext> context;
    unique_ptr<Options> format_options;
    AVPixelFormat pixel_format;
    int video_stream_index = 0;
    int audio_stream_index = 0;
    bool stream_info_was_read_ = false;
    int64_t latest_video_pts = 0;
    string source_;
    string name;
    unordered_map<int, unique_ptr<CodecContext>> decode_contexts = {};

    InputStream(string source, unique_ptr<Options> format_options_) : source_(source), format_options(std::move(format_options_))
    {
        this->context = std::make_unique<FormatContext>();
        LOG("InputStream constructor")
    }

    /**
     *
     * @param codec_id codec
     * @param stream_id stream
     */
    void createDecodeContext(AVCodecID codec_id, int stream_id)
    {
        auto * codec = avcodec_find_decoder(codec_id);

        if (codec == nullptr)
            exit(1);

        auto codec_context = std::make_unique<CodecContext>(codec);
        avcodec_parameters_to_context(codec_context->cntx, context->cntx->streams[stream_id]->codecpar);

        if (avcodec_open2(codec_context->cntx, codec, nullptr) < 0)
            exit(1);

        decode_contexts[stream_id] = std::move(codec_context);
    }

    ~InputStream()
    {
        Logger l(std::cout);
        LOG("Input stream destructor")
        avformat_close_input(&context->cntx);
    }

    void readStreamInfo()
    {
        AVFormatContext * context_ptr = context->cntx;
        context_ptr->flags = format_options->getAVFlags();
        context_ptr->flags |= AVFMT_FLAG_GENPTS;
        context_ptr->flags |= AVFMT_FLAG_IGNDTS;
        open_rtsp_stream(context_ptr, source_, format_options->av_format_options_);


        if (avformat_find_stream_info(context_ptr, nullptr) < 0)
        {
            fprintf(stderr, "Failed to retrieve input stream information");
        }

        av_dump_format(context_ptr, 0, source_.c_str(), 0);

        this->decode_contexts.reserve(context_ptr->nb_streams);

        for (int i = 0; i < context_ptr->nb_streams; i++)
        {
            if (context_ptr->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
            {
                this->createDecodeContext(context_ptr->streams[i]->codecpar->codec_id, i);
                audio_stream_index = i;
                continue;
            }

            if (context_ptr->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
            {
                this->createDecodeContext(context_ptr->streams[i]->codecpar->codec_id, i);
                video_stream_index = i;
                continue;
            }
        }

        this->pixel_format = static_cast<AVPixelFormat>(context_ptr->streams[this->video_stream_index]->codecpar->format);
        this->decode_contexts[video_stream_index].get()->cntx->pix_fmt = this->pixel_format;
        this->decode_contexts[video_stream_index].get()->cntx->width = context_ptr->streams[this->video_stream_index]->codecpar->width;
        this->decode_contexts[video_stream_index].get()->cntx->height = context_ptr->streams[this->video_stream_index]->codecpar->height;

        const AVDictionaryEntry * tag = nullptr;

        while ((tag = av_dict_get(context_ptr->metadata, "", tag, AV_DICT_IGNORE_SUFFIX)))
            printf("%s=1%s\n", tag->key, tag->value);

        stream_info_was_read_ = true;
    }

    bool isVideoPaket(AVPacket * paket) const { return paket->stream_index == this->video_stream_index; }

    bool isAudioPaket(AVPacket * paket) const { return paket->stream_index == this->audio_stream_index; }

    void setVideoLatestPTS(int64_t pts) { this->latest_video_pts = pts; }

    AVRational getTimeBase(int idx) { return context->cntx->streams[idx]->time_base; }

    int64_t getVideoLatestPTS() const { return this->latest_video_pts; }

    [[nodiscard]] float getWidthHeightRelation() const
    {
        return static_cast<float>(this->context->cntx->streams[this->video_stream_index]->codecpar->width)
            / static_cast<float>(this->context->cntx->streams[this->video_stream_index]->codecpar->height);
    }

    int calculateWidth(int height) const
    {
        int width = ceil(height * this->getWidthHeightRelation());
        if (width % 2 != 0)
        {
            width++;
        }
        return width;
    }

    [[nodiscard]] vector<int> getMapStreams()
    {
        /**
         * In input stream may by any type stream, but need write only audio and video.
         * Index array is dst steam index, value array is source_ stream index
         */
        vector<int> result;
        set<int> allow_codec_type = {AVMEDIA_TYPE_VIDEO, AVMEDIA_TYPE_AUDIO};
        AVFormatContext * context_ptr = context->cntx;
        for (int i = 0; i < context_ptr->nb_streams; i++)
        {
            if (allow_codec_type.contains(context_ptr->streams[i]->codecpar->codec_type))
            {
                result.push_back(i);
            }
        }
        return result;
    }


    [[nodiscard]] vector<int> getReversedMapStreams()
    {
        /**
         * In input stream may by any type stream, but need write only audio and video.
         * Index array is src steam index, value array is dst stream index
         */
        vector<int> result;
        set<int> allow_codec_type = {AVMEDIA_TYPE_VIDEO, AVMEDIA_TYPE_AUDIO};
        AVFormatContext * context_ptr = context->cntx;
        int dst_index = 0;
        for (int i = 0; i < context_ptr->nb_streams; i++)
        {
            if (allow_codec_type.contains(context_ptr->streams[i]->codecpar->codec_type))
            {
                result.push_back(dst_index++);
            }
            else
            {
                result.push_back(-1);
            }
        }
        return result;
    }

    template <typename F>
    void decodePacketLambda(AVPacket * pkt, const F & f)
    {
        int ret = avcodec_send_packet(this->decode_contexts[pkt->stream_index]->cntx, pkt);
        if (ret < 0)
        {
            char err[1024] = {0};
            av_strerror(ret, err, 1024);
            fprintf(stderr, "Error sending a packet for decoding %d\n", ret);
            return;
        }
        while (ret >= 0)
        {
            AVFrame * decode_frame = av_frame_alloc();
            ret = avcodec_receive_frame(this->decode_contexts[pkt->stream_index]->cntx, decode_frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                return;
            else if (ret < 0)
            {
                fprintf(stderr, "Error during decoding\n");
                exit(1);
            }


            f(decode_frame);
        }
    }

    vector<AVCodecParameters *> makeOutCodecPar()
    {
        auto map_streams = this->getMapStreams();
        auto codec_pars = vector<AVCodecParameters *>();
        for (const auto i : map_streams)
        {
            codec_pars.push_back(context->cntx->streams[i]->codecpar);
        }
        return codec_pars;
    }

    std::tuple<int, int, AVPixelFormat> getOriginalResolution()
    {
        return {
            this->decode_contexts[video_stream_index]->cntx->width,
            this->decode_contexts[video_stream_index]->cntx->height,
            this->decode_contexts[video_stream_index]->cntx->pix_fmt};
    }

    int readFrame(Packet & packet)
    {
        if (!stream_info_was_read_)
        {
            throw std::runtime_error("Stream info not was read. Call \"readStreamInfo\" method.");
        }
        int result = av_read_frame(context->cntx, packet.pkt);
        if (result >= 0)
        {
            //Todo: inc av ref counter
        }
        return result;
    }
};

#endif // LIBAV_TEST_INPUTSTREAM_H
