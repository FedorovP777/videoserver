// This is a personal academic project. Dear PVS-Studio, please check it.

// PVS-Studio Static Code Analyzer for C, C++, C#, and Java:
// https://pvs-studio.com

#ifndef MY_PROJECT_SRC_STREAMWORKER_H_
#define MY_PROJECT_SRC_STREAMWORKER_H_
#include "SharedQueue.h"
using namespace std;

class StreamWorker
{
private:
    StreamSetting stream_setting;
    atomic<bool> exit_flag = false;
    shared_ptr<future<void>> receive_thread;
    shared_ptr<future<void>> write_thread;
    shared_ptr<Packet> input_packet;
    shared_ptr<InputStream> input_stream;
    vector<shared_ptr<OutputStream>> output_streams;
    SharedQueue<AVPacket *> queue_write;
    vector<int> map_streams;
    vector<int> reversed_map_streams;
    bool is_need_transcode = false;


public:
    atomic_int64_t latest_receive_paket_pts;
    atomic_int64_t latest_write_paket_pts;

    int getWriteQueueSize() { return this->queue_write.getSize(); }

    explicit StreamWorker(StreamSetting _ss) : stream_setting(_ss)
    {
        LOG("StreamWorker constructor")
        input_packet = make_shared<Packet>();
        auto input_stream_options = make_unique<Options>();
        for (const auto [key, value] : stream_setting.input_stream_setting.format_options)
        {
            input_stream_options->setAVOption(key.c_str(), value.c_str());
        }
        input_stream = make_shared<InputStream>(stream_setting.input_stream_setting.src, std::move(input_stream_options));
    }
    void init()
    {
        input_stream->readStreamInfo();
        reversed_map_streams = input_stream->getReversedMapStreams();
        map_streams = input_stream->getMapStreams();
        auto original_size = input_stream->getOriginalResolution();

        for (const auto & os_stream : stream_setting.output_stream_settings)
        {
            auto os = make_unique<OutStreamConfig>();
            os->duration = os_stream.file_duration_sec;
            os->s3_profile = os_stream.s3_profile;
            os->name = os_stream.name;
            os->src_name = input_stream->name;
            os->notification_endpoint = os_stream.notification_endpoint;
            os->del_after_upload = os_stream.del_after_upload;
            auto encoder_options = make_unique<Options>();
            CodecOptions transcoding_options;
            transcoding_options.original_width = std::get<0>(original_size);
            transcoding_options.original_height = std::get<1>(original_size);
            transcoding_options.original_pix_format = std::get<2>(original_size);
            os->rescale_ts_audio = os_stream.rescale_ts_audio;
            if (os_stream.time_base)
            {
                auto i = AVRational{1, 1};
                av_parse_video_rate(&i, os_stream.time_base.value().c_str());
                os->time_base = i;
            }

            for (const auto [key, value] : os_stream.codec_options)
            {
                if (key == "bitrate")
                {
                    transcoding_options.is_need_transcode = true;
                }
                if (key == "preset")
                {
                    transcoding_options.codec_params["preset"] = value;
                    transcoding_options.is_need_transcode = true;
                }
                if (key == "height")
                {
                    transcoding_options.height = stoi(value);
                    transcoding_options.is_need_transcode = true;
                }

                if (key == "width")
                {
                    transcoding_options.width = stoi(value);
                    transcoding_options.is_need_transcode = true;
                }
            }
            if (transcoding_options.height != 0 && transcoding_options.width == 0)
            {
                transcoding_options.width = input_stream->calculateWidth(transcoding_options.height);
                if (transcoding_options.height != transcoding_options.original_height)
                {
                    transcoding_options.is_need_rescale = true;
                    transcoding_options.is_need_transcode = true;
                }
            }
            if (transcoding_options.height == 0 && transcoding_options.width == 0)
            {
                transcoding_options.height = transcoding_options.original_height;
                transcoding_options.width = transcoding_options.original_width;
            }
            auto dst_format_options = make_unique<Options>();

            for (const auto [key, value] : os_stream.format_options)
            {
                dst_format_options->setAVOption(key.c_str(), value.c_str());
            }

            for (const auto value : os_stream.format_flags)
            {
                dst_format_options->setAVFlag(value);
            }

            for (const auto value : os_stream.codec_flags)
            {
                encoder_options->setAVFlag(value);
                transcoding_options.is_need_transcode = true;
            }

            if (transcoding_options.is_need_transcode)
            {
                is_need_transcode = true;
            }

            transcoding_options.options = std::move(encoder_options);
            os->options = std::move(dst_format_options);

            std::function<string()> get_filename_fn = [&os_stream]() { return FormatFilename::formatting(os_stream.path, "camera_name"); };

            auto output_stream
                = make_shared<OutputStream>(transcoding_options, std::move(os), get_filename_fn, input_stream->makeOutCodecPar());
            output_streams.push_back(std::move(output_stream));
        }

        for (const auto & dst_stream : output_streams)
        {
            dst_stream->updateOutputFilename(0);
        }
    }
    void stop()
    {
        LOG("StreamWorker stop")
        exit_flag = true;
        receive_thread->wait();
        write_thread->wait();
        LOG("StreamWorker stop done")
    }
    void runBackground()
    {
        //Запуск
        this->receive_thread = make_shared<std::future<void>>(std::async(std::launch::async, [this]() {
            while (true)
            {
                if (exit_flag)
                {
                    queue_write.justNotify();
                    return;
                }
                if (av_read_frame(input_stream->context->cntx, input_packet->pkt) < 0)
                    break;


                if (input_packet->pkt->pts < 1)
                {
                    LOG("PTS < 1")
                    av_packet_unref(input_packet->pkt);
                    continue;
                }

                if (input_packet->pkt->size <= 0)
                {
                    LOG("Size < 0")
                    av_packet_unref(input_packet->pkt);
                    continue;
                }
                auto * pkt_clone = av_packet_clone(input_packet->pkt);
                av_packet_unref(input_packet->pkt);
                //                latest_receive_paket_pts = pkt_clone->pts;
                queue_write.push(pkt_clone);
            }
        }));
    }
    void processingOnePacketFromQueue()
    {
        if (exit_flag)
            return;

        auto * pkt = queue_write.popIfExist();

        if (exit_flag || pkt == nullptr)
            return;
        processPacket(pkt);
        delete pkt;
    }
    void processPacket(AVPacket * pkt)
    {
        auto dst_stream_index = reversed_map_streams[pkt->stream_index];

        if (dst_stream_index < 0)
        {
            return;
        }
        //        if (pkt->pts < 0)
        //        {
        //            return;
        //        }
        pkt->pos = -1;

        if (input_stream->isVideoPaket(pkt))
        {
            input_stream->setVideoLatestPTS(pkt->pts);
        }
        //        if (input_stream->isAudioPaket(pkt))
        //        {
        //            pkt->pts = input_stream->getVideoLatestPTS();
        //            pkt->dts = input_stream->getVideoLatestPTS() - 1;
        //        }
        pkt->stream_index = dst_stream_index;
        cout << input_stream->getTimeBase(pkt->stream_index).den << endl;

        if (input_stream->isAudioPaket(pkt) || !is_need_transcode)
        {
            for (const auto & dst_stream : output_streams)
            {
                if(input_stream->isAudioPaket(pkt) && dst_stream->dst_steam_config_->rescale_ts_audio){
                    av_packet_rescale_ts(pkt, input_stream->getTimeBase(pkt->stream_index), dst_stream->getTimeBase());
                }
                dst_stream->writeFrame(pkt);
            }
            latest_write_paket_pts = pkt->pts;
        }


        if (is_need_transcode)
        {
            input_stream->decodePacketLambda(pkt, [this](AVFrame * frame) {
                for (const auto & dst_stream : output_streams)
                {
                    if (dst_stream->is_rescale)
                    {
                        dst_stream->rescaleEncodeWrite(frame);
                    }
                    else
                    {
                        dst_stream->encodeAndWrite(frame);
                    }
                }
                av_frame_free(&frame);
            });
        }

        for (const auto & dst_stream : output_streams)
        {
            auto currentTimestampSec = pkt->pts / 100000;
            if (input_stream->isVideoPaket(pkt))
            {
                cout << currentTimestampSec << endl;
            }


            if (input_stream->isVideoPaket(pkt) && dst_stream->isNeedUpdateFilename(pkt->pts))
            {
                dst_stream->updateOutputFilename(pkt->pts / 100000);
            }
        }
        latest_write_paket_pts = pkt->pts;
        av_packet_unref(pkt);
    }
    void run()
    {
        auto packet = Packet();

        while (true)
        {
            if (input_stream->readFrame(packet) < 0)
            {
                LOG("Error read frame")
                return;
            }
            processPacket(packet.pkt);
        }
    }
};
#endif // MY_PROJECT_SRC_STREAMWORKER_H_