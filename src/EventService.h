// This is a personal academic project. Dear PVS-Studio, please check it.

// PVS-Studio Static Code Analyzer for C, C++, C#, and Java:
// https://pvs-studio.com

#ifndef MY_PROJECT_SRC_EVENTSERVICE_H_
#define MY_PROJECT_SRC_EVENTSERVICE_H_
#include <future>
#include <memory>
#include <filesystem>
#include <string>
#include <uv.h>
#include <Logger.h>
#include <ConfigService.h>
#include "S3Service.h"
#include "NotificationService.h"
using async_fn = void (*)(uv_async_s *);
using timer_end_fn = void (*)();
struct UploadToS3Task
{
    std::string filename;
    std::string current_server_name;
    std::string filename_template;
    std::string camera_name;
    std::string dest;
    std::string dest_name;
    S3Profile * s3_profile = nullptr;
    NotificationEndpoint * notification_endpoint = nullptr;
    bool del_after_upload;
};


class EventService
{
public:
    static void afterUploadFileAsync(uv_async_t * handle)
    {
        LOG("AfterUploadFileAsync")
        auto * result = reinterpret_cast<LibuvCurlCpp::response<UploadToS3Task *> *>(handle->data);
        std::cout << result->http_code << std::endl;
        std::cout << result->body << std::endl;
        UploadToS3Task * upload_task = result->user_payload;

        if (upload_task->del_after_upload)
        {
            deleteFile(upload_task->filename);
        }
        
        uv_close(reinterpret_cast<uv_handle_t *>(handle), [](uv_handle_t * t) { delete t; });
    }
    static void startUploadFileDone(uv_work_t * req, int /*status*/)
    {
        cout << "startUploadFileDone" << endl;
        auto * upload_to_s3task = static_cast<UploadToS3Task *>(req->data);
        delete upload_to_s3task;
        delete req;
        uv_print_all_handles(uv_default_loop(), stderr);
    }
    /**
     * Delete file async
     * @param path file path
     * @return
     */
    static int deleteFile(const std::filesystem::path & path)
    {
        LOG("Delete file:" << path)
        if (!std::filesystem::exists(path))
        {
            LOG("File not exist")
            return -1;
        }

        auto * req = new uv_fs_t;
        req->data = (void *)path.c_str();
        uv_fs_unlink(uv_default_loop(), req, path.c_str(), [](uv_fs_t * req) {
            LOG("Done delete file:" << (const char *)req->data);
            delete req;
        });

        return 1;
    }

    /**
     * Upload file to s3, run in thread pool
     * @param req
     */
    static void uploadFile(uv_work_t * req)
    {
        auto * task = static_cast<UploadToS3Task *>(req->data);
        LOG("Start upload"
            << "thread:" << std::this_thread::get_id() << endl
            << "filename" << task->filename)

        auto * result_upload_file
            = S3Service::putObject<UploadToS3Task *>(task->filename, "", task->s3_profile, afterUploadFileAsync, task);
        LOG("Upload done.")
        if (task->notification_endpoint == nullptr)
        {
            delete result_upload_file;
            return;
        }
        auto * notification_context = new NotificationContext;
        notification_context->result_upload_file = result_upload_file;
        notification_context->s3_profile = task->s3_profile;
        notification_context->dest_name = task->dest_name;
        notification_context->camera_name = task->camera_name;
        notification_context->notification_endpoint = task->notification_endpoint;
        sendAsync<NotificationContext *>(notification_context, sendNotifyAfterUploadAsync);
    }


    static void finishWriteFragmentAsync(uv_async_t * msg)
    {
        auto * task = static_cast<UploadToS3Task *>(msg->data);
        LOG("finishWriteFragmentAsync" << task->filename << " thread id: " << std::this_thread::get_id())
        uv_close(reinterpret_cast<uv_handle_t *>(msg), [](uv_handle_t * t) { delete t; });

        auto * req = new uv_work_t;
        req->data = reinterpret_cast<void *>(task);
        uv_queue_work(uv_default_loop(), req, EventService::uploadFile, EventService::startUploadFileDone);
    }

    static void signalHandler(uv_signal_t * /*req*/, int /*signum*/)
    {
        cout << "Signal received!" << endl;
        //TODO (me): update config by signal
    }

    static void registerReloadSignal()
    {
        uv_signal_t sig;
        uv_signal_init(uv_default_loop(), &sig);
        uv_signal_start(&sig, signalHandler, SIGUSR1);
    }

    template <typename T>
    static void writeJob(uv_work_t * req)
    {
        T data = static_cast<T>(req->data);
        data->stream_worker->processingOnePacketFromQueue();
    }

    template <typename T1>
    static void sendAsync(T1 data, async_fn fn)
    {
        auto * async = new uv_async_t;
        async->data = (void *)data;
        uv_async_init(uv_default_loop(), async, fn);
        uv_async_send(async);
    }
    template <typename T>
    static void startWriteJob(uv_async_t * handle)
    {
        uv_close(reinterpret_cast<uv_handle_t *>(handle), NULL);
        T data = static_cast<T>(handle->data);

        int timeout = 500;
        if (data->stream_worker->getWriteQueueSize() > 0)
        {
            timeout = 0;
        }
        uv_timer_stop(&data->uv_timer);
        uv_timer_init(uv_default_loop(), &data->uv_timer);
        uv_timer_start(
            &data->uv_timer,
            [](uv_timer_t * handle_timer) {
                T data = static_cast<T>(handle_timer->data);

                data->stream_worker->processingOnePacketFromQueue();

                //                writeJob<T>(data);
                uv_async_init(uv_default_loop(), &data->uv_async, startWriteJob<T>);
                uv_async_send(&data->uv_async);
                //                uv_queue_work(uv_default_loop(), &data->uv_work, writeJob<T>, [](uv_work_t * req, int status) {
                //                    T data = static_cast<T>(req->data);
                //                    uv_async_init(uv_default_loop(), &data->uv_async, startWriteJob<T>);
                //                    uv_async_send(&data->uv_async);
                //                });
            },
            timeout,
            0);
    }

    /**
         * Periodic health check task
         *
         * @param handle
         */
    template <typename T>
    static void timerWriterJob(uv_timer_t * handle)
    {
        EventService::sendAsync<void *>(handle->data, startWriteJob<T>);
    }

    /**
         * Periodic health check task
         *
         * @param handle
         */
    template <typename T>
    static void streamHealthCheckerJob(uv_timer_t * handle)
    {
        LOG("Start stream health checker job")
        T * data = static_cast<T *>(handle->data);
        data->stream_health_checker->checkStreams(data->stream_workers);
        LOG("Stream health checker job done")
    }
    template <typename T1>
    static void registerWriteJob(T1 job_writer_struct)
    {
        job_writer_struct->uv_timer.data = job_writer_struct;
        job_writer_struct->uv_work.data = job_writer_struct;
        job_writer_struct->uv_async.data = job_writer_struct;
        uv_timer_init(uv_default_loop(), &job_writer_struct->uv_timer);

        const int TIMER_JOB_TIMEOUT_MS = 10 * 100;
        const int TIMER_JOB_INTERVAL_MS = 0;
        uv_timer_start(&job_writer_struct->uv_timer, EventService::timerWriterJob<T1>, TIMER_JOB_TIMEOUT_MS, TIMER_JOB_INTERVAL_MS);
    }
    template <typename T1, typename T2, typename T3>
    static void registerHealthCheckJob(T2 & stream_workers, T3 * stream_health_checker)
    {
        auto * timer_req = new uv_timer_t;
        uv_timer_init(uv_default_loop(), timer_req);
        timer_req->data = new T1{stream_health_checker, &stream_workers};
        const int HEALTH_CHECK_JOB_TIMEOUT_MS = 10 * 1000;
        const int HEALTH_CHECK_JOB_INTERVAL_MS = 10 * 1000;

        uv_timer_start(timer_req, EventService::streamHealthCheckerJob<T1>, HEALTH_CHECK_JOB_TIMEOUT_MS, HEALTH_CHECK_JOB_INTERVAL_MS);
    }

    static void sendNotifyAfterUploadAsync(uv_async_t * req)
    {
        LOG("sendNotifyAfterUploadAsync")
        auto * task = static_cast<NotificationContext *>(req->data);
        NotificationService::successUploadFile(task);
        uv_close(reinterpret_cast<uv_handle_t *>(req), [](uv_handle_t * t) { delete t; });
    }
    template <typename T>
    static void timer(int start_timeout, int threshold, int interval, void (*timer_fn)(T), timer_end_fn timer_end_fn, T value)
    /** Handy timer wrap around uv timer.
         * Based on two uv timer, then first for start and interval timer, second timer need for stop first timer.
         *
         * @tparam T
         * @param start_timeout
         * @param threshold
         * @param interval
         * @param timer_fn
         * @param timer_end_fn
         * @param value
         */
    {
        struct TimerDto
        {
            uv_timer_t timer_req_start;
            uv_timer_t timer_req_stop;
            void (*timer_end_fn)();
            void (*timer_fn)(T);
            T payload;
            ~TimerDto() { cout << "TimerDto destructor" << endl; }
        };

        auto * timer_dto = new TimerDto;
        timer_dto->timer_end_fn = timer_end_fn;
        timer_dto->timer_fn = timer_fn;
        timer_dto->payload = value;
        uv_timer_init(uv_default_loop(), &timer_dto->timer_req_start);
        uv_timer_start(
            &timer_dto->timer_req_start,
            [](uv_timer_t * handle) {
                auto timer_dto = reinterpret_cast<TimerDto *>(handle->data);
                timer_dto->timer_fn(timer_dto->payload);
            },
            start_timeout,
            interval);

        if (threshold == 0)
        {
            return;
        }


        auto stop_two_timers = [](uv_timer_t * handle) {
            /* Stop start timer */
            auto timer_dto = reinterpret_cast<TimerDto *>(handle->data);
            uv_timer_stop(&timer_dto->timer_req_start);
            uv_close(reinterpret_cast<uv_handle_t *>(&timer_dto->timer_req_start), [](uv_handle_t * handle) {
                /* Stop stop timer */
                auto timer_dto = reinterpret_cast<TimerDto *>(handle->data);
                uv_timer_stop(reinterpret_cast<uv_timer_t *>(&timer_dto->timer_req_stop));
                uv_close(reinterpret_cast<uv_handle_t *>(&timer_dto->timer_req_stop), [](uv_handle_t * handle) {
                    /* Call callback and free mem*/
                    auto timer_dto = reinterpret_cast<TimerDto *>(handle->data);
                    timer_dto->timer_end_fn();
                    delete timer_dto;
                });
            });
        };

        timer_dto->timer_req_start.data = reinterpret_cast<void *>(timer_dto);
        timer_dto->timer_req_stop.data = reinterpret_cast<void *>(timer_dto);
        uv_timer_init(uv_default_loop(), &timer_dto->timer_req_stop);
        uv_timer_start(&timer_dto->timer_req_stop, stop_two_timers, threshold, 0);
    }
};

#endif // MY_PROJECT_SRC_EVENTSERVICE_H_
