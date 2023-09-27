// This is a personal academic project. Dear PVS-Studio, please check it.

// PVS-Studio Static Code Analyzer for C, C++, C#, and Java:
// https://pvs-studio.com

#ifndef LIBAV_TEST_OUTPUTSTREAM_H
#define LIBAV_TEST_OUTPUTSTREAM_H

#include <future>
#include <memory>
#include <vector>
#include <uv.h>
#include <filesystem>
#include <optional>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/common.h>
#include <libavutil/timestamp.h>
#include <libavutil/opt.h>
}

#include "FormatContext.h"
#include "Options.h"
#include "FormatFilename.h"
#include "Rescale.h"

struct OutStreamConfig
{
    string name;
    string src_name;
    optional<int> duration;
    optional<AVRational> time_base;
    bool rescale_ts_audio = false;
    unique_ptr<Options> options;
    S3Profile * s3_profile = nullptr;
    NotificationEndpoint * notification_endpoint = nullptr;
    bool del_after_upload;
};

struct CodecOptions
{
    optional<int64_t> bitrate;
    optional<AVRational> time_base;
    optional<int> max_b_frames;
    optional<int> gop_size;
    optional<int> qscale;
    optional<int> global_quality;
    optional<int> framerate;
    unique_ptr<Options> options;
    unordered_map<std::string, std::string> codec_params = {};
    int original_width = 0;
    int original_height = 0;
    AVPixelFormat original_pix_format;
    int height = 0;
    int width = 0;
    bool is_need_transcode = false;
    bool is_need_rescale = false;
};

namespace fs = std::filesystem;

class OutputStream
{
public:
    shared_ptr<FormatContext> output_context;
    string dest;
    std::function<string()> get_filename_fn;
    AVPacket * pkt_out;
    AVCodec * codec_;
    //    unique_ptr<Options> dst_steam_options_;
    unique_ptr<OutStreamConfig> dst_steam_config_;
    unique_ptr<CodecContext> enc_ctx;
    unique_ptr<Rescale> rescale;
    optional<AVRational> time_base;

    bool needCloseFile = false;
    bool is_transcoding = false;
    bool is_rescale = false;
    string outFilenameTemplate;

    unsigned long long prevIntervalSec = 0;
    unsigned long long currentTimestampSec = 0;
    string filename;
    const CodecOptions * transcodingOptions;

    OutputStream(
        const CodecOptions & transcodingOptions_,
        unique_ptr<OutStreamConfig> dst_steam_config,
        const std::function<string()> & get_filename_fn_,
        const vector<AVCodecParameters *> & codec_params)
        : transcodingOptions(&transcodingOptions_), get_filename_fn(get_filename_fn_), dst_steam_config_(std::move(dst_steam_config))
    {
        this->pkt_out = av_packet_alloc();

        if (!this->pkt_out)
            exit(1);

        this->output_context = std::make_shared<FormatContext>();

        is_transcoding = transcodingOptions_.is_need_transcode;
        is_rescale = transcodingOptions_.is_need_rescale;
        if (dst_steam_config_->time_base)
        {
            time_base = dst_steam_config_->time_base;
        }
        if (is_rescale)
        {
            this->rescale = make_unique<Rescale>(
                this->transcodingOptions->original_pix_format,
                this->transcodingOptions->original_width,
                this->transcodingOptions->original_height,
                this->transcodingOptions->width,
                this->transcodingOptions->height);
        }


        if (this->is_transcoding)
        {
            this->prepareEncContext(this->transcodingOptions->width, this->transcodingOptions->height);
        }

        LOG("OutputStream constructor")
        this->dest = get_filename_fn();
        fs::path path = this->dest;

        if (!fs::exists(path.parent_path()))
        {
            fs::create_directory(path.parent_path());
        }
        avformat_alloc_output_context2(&output_context->cntx, NULL, NULL, this->dest.c_str());

        if (!output_context->cntx)
        {
            fprintf(stderr, "Could not create output context\n");
        }

        for (const auto & i : codec_params)
        {
            AVStream * out_stream = avformat_new_stream(this->output_context->cntx, NULL);

            if (!out_stream)
            {
                fprintf(stderr, "Failed allocating output stream\n");
                exit(1);
            }
            if (avcodec_parameters_copy(out_stream->codecpar, i) < 0)
            {
                fprintf(stderr, "Failed to copy codec parameters\n");
            }
            out_stream->codecpar->codec_tag = 0;
        }

        this->output_context->cntx->oformat->flags = dst_steam_config_->options->getAVFlags();

        av_dump_format(this->output_context->cntx, 0, this->dest.c_str(), 1);
    }

    ~OutputStream()
    {
        Logger l(std::cout);
        LOG("OutputStream destructor")

        LOG("OutputStream av_write_trailer")
        av_write_trailer(this->output_context->cntx);

        if (this->output_context->cntx && !(this->output_context->cntx->flags & AVFMT_NOFILE))
        {
            LOG("OutputStream avio_closep")
            avio_closep(&this->output_context->cntx->pb);
        }
    }

    void prepareEncContext(int width, int height)
    {
        this->codec_ = avcodec_find_encoder_by_name("libx264");
        if (!this->codec_)
        {
            fprintf(stderr, "Codec '%s' not found\n", "libx264");
            exit(1);
        }

        this->enc_ctx = std::make_unique<CodecContext>(this->codec_);
        if (!this->enc_ctx->cntx)
        {
            fprintf(stderr, "Could not allocate video codec context\n");
            exit(1);
        }

        if (transcodingOptions->bitrate)
        {
            /* put sample parameters */
            this->enc_ctx->cntx->bit_rate = transcodingOptions->bitrate.value();
        }

        /* resolution must be a multiple of two */
        this->enc_ctx->cntx->width = width;
        this->enc_ctx->cntx->height = height;

        if (transcodingOptions->time_base)
        {
            /* frames per second */
            this->enc_ctx->cntx->time_base = transcodingOptions->time_base.value();
            this->enc_ctx->cntx->framerate = AVRational{1, 25};
        }
        this->enc_ctx->cntx->flags = transcodingOptions->options->getAVFlags();

        if (transcodingOptions->global_quality)
        {
            this->enc_ctx->cntx->global_quality = FF_QP2LAMBDA * transcodingOptions->global_quality.value();
        }
        /* emit one intra frame every ten frames
         * check frame pict_type before passing frame
         * to encoder, if frame->pict_type is AV_PICTURE_TYPE_I
         * then gop_size is ignored and the output of encoder
         * will always be I frame irrespective to gop_size
         */
        if (transcodingOptions->gop_size)
        {
            this->enc_ctx->cntx->gop_size = transcodingOptions->gop_size.value();
        }

        if (transcodingOptions->max_b_frames)
        {
            this->enc_ctx->cntx->max_b_frames = transcodingOptions->max_b_frames.value();
        }

        this->enc_ctx->cntx->pix_fmt = transcodingOptions->original_pix_format;
        cout << this->codec_->id << " codec id" << endl;
        for (auto & [key, value] : transcodingOptions->codec_params)
        {
            av_opt_set(this->enc_ctx->cntx->priv_data, key.c_str(), value.c_str(), AV_OPT_SEARCH_CHILDREN);
        }

        /* open it */
        if (avcodec_open2(this->enc_ctx->cntx, this->codec_, &transcodingOptions->options->av_format_options_) < 0)
        {
            fprintf(stderr, "Could not open codec: \n");
            exit(1);
        }
    }

    void updateOutputFilename(int sec)
    {
        /**
         * Create new file and upload complete file to s3
         */
        string previous_filename = this->filename;
        this->filename = get_filename_fn();
        cout << this->filename << endl;
        this->setOutputFilename(this->filename);
        this->prevIntervalSec = sec;
        LOG("updateOutputFilename, thread id:" << std::this_thread::get_id())
        if (!previous_filename.empty() && dst_steam_config_->s3_profile != nullptr)
        {
            auto * upload_to_s3task = new UploadToS3Task;
            upload_to_s3task->filename = previous_filename;
            upload_to_s3task->camera_name = dst_steam_config_->src_name;
            upload_to_s3task->dest_name = dst_steam_config_->name;
            upload_to_s3task->dest = this->dest;
            upload_to_s3task->s3_profile = dst_steam_config_->s3_profile;
            upload_to_s3task->notification_endpoint = dst_steam_config_->notification_endpoint;
            upload_to_s3task->del_after_upload = dst_steam_config_->del_after_upload;

            /**
             * updateOutputFilename ->(async) finishWriteFragmentAsync -> uploadFile(tp) -> startUploadFileDone(main thread)
             *                                                                           \
             *                                                                            (async) sendNotifyAfterUploadAsync
             */
            EventService::sendAsync<UploadToS3Task *>(upload_to_s3task, EventService::finishWriteFragmentAsync);
        }
    }

    void setOutputFilename(const string & dest_) const
    {
        /**
         * Close current and open new file for write
         */
        av_write_trailer(this->output_context->cntx);
        avio_closep(&this->output_context->cntx->pb);

        if (!(this->output_context->cntx->oformat->flags & AVFMT_NOFILE))
        {
            if (avio_open(&this->output_context->cntx->pb, dest_.c_str(), AVIO_FLAG_WRITE) < 0)
            {
                fprintf(stderr, "Could not open output file '%s'", dest_.c_str());
            }
        }
        if (avformat_write_header(this->output_context->cntx, NULL) < 0)
        {
            fprintf(stderr, "Error occurred when opening output file\n");
        }
    }


    /**
     * Write a packet to an output media file.
     * @param pkt
     */
    void writeFrame(AVPacket * pkt) const { av_write_frame(this->output_context->cntx, pkt); }

    void encodeAndWrite(const AVFrame * frame) const
    {
        int ret;

        ret = avcodec_send_frame(enc_ctx->cntx, frame);
        if (ret < 0)
        {
            fprintf(stderr, "Error sending a frame for encoding\n");
            exit(1);
        }

        while (ret >= 0)
        {
            auto * av_packet_out = av_packet_alloc();
            ret = avcodec_receive_packet(enc_ctx->cntx, av_packet_out);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            {
                av_packet_free(&av_packet_out);
                return;
            }
            else if (ret < 0)
            {
                fprintf(stderr, "Error during encoding\n");
                exit(1);
            }
            this->writeFrame(av_packet_out);
            av_packet_free(&av_packet_out);
        }
    }

    void rescaleEncodeWrite(AVFrame * frame)
    {
        AVFrame * frame_tmp = av_frame_clone(frame);
        av_frame_copy(frame_tmp, frame);
        av_frame_make_writable(frame_tmp);
        this->rescale->rescale(frame, frame_tmp);
        this->encodeAndWrite(frame_tmp);
        av_frame_free(&frame_tmp);
    }

    void interleavedWriteFrame(AVPacket * pkt) const { av_interleaved_write_frame(this->output_context->cntx, pkt); }

    bool isNeedUpdateFilename(int64_t & millisecTimestamp)
    {
        auto currentTimestampSec1 = millisecTimestamp / 100000;
        return dst_steam_config_->duration.value() > 0 && currentTimestampSec1 > 0
            && currentTimestampSec1 > prevIntervalSec + dst_steam_config_->duration.value();
    }

    AVRational getTimeBase() { return time_base.value(); }
};

#endif // LIBAV_TEST_OUTPUTSTREAM_H
