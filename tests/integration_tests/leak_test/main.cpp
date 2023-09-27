// This is a personal academic project. Dear PVS-Studio, please check it.

// PVS-Studio Static Code Analyzer for C, C++, C#, and Java:
// https://pvs-studio.com
#include <uv.h>


#include "S3Service.h"
#include "EventService.h"
#include "utils.h"

#include "NotificationService.h"
using namespace std;

int queue_work_counter = 0;

int main(int /*argc*/, char ** /*argv*/)
{
    auto * timer_req = new uv_timer_t;
    auto s3_profile = new S3Profile{"1", "1", "11"};
    auto notification_endpoint = new NotificationEndpoint;
    notification_endpoint->url = "http://192.168.2.91:9001/login";
    notification_endpoint->access_key = "123";
    notification_endpoint->enable_notification = true;
    uv_timer_init(uv_default_loop(), timer_req);
    auto i = new UploadToS3Task;
    i->s3_profile = s3_profile;
    i->camera_name = "123";
    i->dest_name = "123";
    i->notification_endpoint = notification_endpoint;
    i->current_server_name = "123";
    timer_req->data = (void *)i;

    uv_timer_start(
        timer_req,
        [](uv_timer_t * handle) {
            queue_work_counter++;
            uv_queue_work(
                uv_default_loop(),
                new uv_work_t,
                [](uv_work_t * req) {
                    cout << "uv_queue_work start" << endl;
                    cout << "counter" << queue_work_counter << endl;
                    LOG("thread id: " << std::this_thread::get_id())
                    uv_print_all_handles(uv_default_loop(), stderr);
                    usleep(500 * 1000);
                    app::print_mem_usage();
                },
                [](uv_work_t * req, int /*status*/
                ) {
                    cout << "uv_queue_work stop" << endl;
                    queue_work_counter--;
                    delete req;
                });

            double vm, rss;
            app::process_mem_usage(vm, rss);
            app::print_libuv_info();
        },
        0,
        100);
    auto * timer_req_stop_t = new uv_timer_t;
    uv_timer_init(uv_default_loop(), timer_req_stop_t);
    timer_req_stop_t->data = (void *)timer_req;
    uv_timer_start(
        timer_req_stop_t,
        [](uv_timer_t * handle) {
            uv_timer_stop((uv_timer_t *)handle->data);
            delete handle;
        },
        10 * 1000,
        0);

    return uv_run(uv_default_loop(), UV_RUN_DEFAULT);
}