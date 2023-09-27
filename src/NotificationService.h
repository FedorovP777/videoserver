#ifndef CLANG_TIDY_NOTIFICATIONSERVICE_H
#define CLANG_TIDY_NOTIFICATIONSERVICE_H
#include "LibuvCurlCpp.h"
#include "third-party/json.hpp"
using json = nlohmann::json;
struct NotificationContext
{
    ResultUploadFile * result_upload_file;
    S3Profile * s3_profile;
    string dest_name;
    string camera_name;
    NotificationEndpoint * notification_endpoint;
    NotificationContext() { LOG("constructor NotificationContext") }
    ~NotificationContext()
    {
        LOG("destructor NotificationContext")
        delete result_upload_file;
    }
};
class NotificationService
{
public:
    /**
     * Call HTTP request after upload file to S3
     * method inside delete notification_context
     * @param notification_context
     */
    static void successUploadFile(NotificationContext * notification_context)
    {
        LibuvCurlCpp::request_options options;
        std::unordered_map<std::string, std::string> headers;

        headers["Accept"] = "application/json";
        headers["Content-Type"] = "application/json; charset=utf-8";
        headers["Authorization"] = fmt::format("\"Bearer: {}\"", notification_context->notification_endpoint->access_key);
        options["method"] = "POST";
        options["url"] = notification_context->notification_endpoint->url;
        options["headers"] = headers;

        json j;
        j[0]["type"] = "success_upload";
        if (notification_context->result_upload_file != nullptr)
        {
            j[0]["msg"]["size"] = notification_context->result_upload_file->size;
            j[0]["msg"]["dst"] = notification_context->result_upload_file->dst;
        }

        j[0]["msg"]["dst_name"] = notification_context->dest_name;
        j[0]["msg"]["camera_name"] = notification_context->camera_name;
        options["body"] = j.dump();
        cout << j.dump() << endl;


        auto i = new int(123);
        LibuvCurlCpp::LibuvCurlCpp<int *>::request(
            options, [](uv_async_t * handle) {}, i);

        delete notification_context;
    }
};
#endif //CLANG_TIDY_NOTIFICATIONSERVICE_H
