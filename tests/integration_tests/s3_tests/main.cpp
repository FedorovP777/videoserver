// This is a personal academic project. Dear PVS-Studio, please check it.

// PVS-Studio Static Code Analyzer for C, C++, C#, and Java:
// https://pvs-studio.com
#include <uv.h>
#include "utils.h"
#include "S3Service.h"
using namespace std;

void handle_upload(uv_async_t * handle)
{
    cout << "end upload" << endl;
    auto * result = reinterpret_cast<LibuvCurlCpp::response<int *> *>(handle->data);
    std::cout << result->http_code << std::endl;
    std::cout << result->body << std::endl;
    std::cout << *result->user_payload << std::endl;
    uv_close(reinterpret_cast<uv_handle_t *>(handle), [](uv_handle_t * t) { delete t; });
}

int main(int /*argc*/, char ** /*argv*/)
{
    app::print_mem_usage();
    auto * s3_profile = new S3Profile;
    s3_profile->access_key_id = getenv_str("S3_ACCESS_KEY_ID", "minioadmin");
    s3_profile->secret_key = getenv_str("S3_SECRET_KEY", "minioadmin");
    s3_profile->s3_folder = getenv_str("S3_FOLDER", "test");
    s3_profile->bucket_name = getenv_str("S3_BUCKET", "vod-bucket");
    s3_profile->endpoint_url = getenv_str("S3_ENDPOINT_URL", "localhost:9000");
    s3_profile->host = getenv_str("S3_HOST", "localhost:9000");
    s3_profile->verify_ssl = false;
    string demo_file = "../tests/integration_tests/s3_tests/file-unmuxing-2022-10-10-1665424500.ts";
    auto * payload = new int(123);
    for (int i = 0; i < 10; i++)
    {
        cout << i << endl;
        S3Service::putObject(demo_file, "", s3_profile, handle_upload, payload);
        S3Service::putObject(demo_file, "", s3_profile, handle_upload, payload);
    }

    uv_run(uv_default_loop(), UV_RUN_DEFAULT);
    return 0;
}