// This is a personal academic project. Dear PVS-Studio, please check it.

// PVS-Studio Static Code Analyzer for C, C++, C#, and Java:
// https://pvs-studio.com

#ifndef MY_PROJECT_SRC_S3SERVICE_H_
#define MY_PROJECT_SRC_S3SERVICE_H_
#include <filesystem>
#include <fstream>
#include <iostream>
#include <fmt/core.h>

#include "ConfigService.h"
#include "S3CurlClient.h"
namespace fs = std::filesystem;
int leak_counter_result_upload_file = 0;

struct ResultUploadFile
{
    std::string filename;
    uintmax_t size;
    std::string dst;
    std::string dst_name;
    bool is_success = true;
    std::string error_message;
    ResultUploadFile()
    {
        leak_counter_result_upload_file++;
        LOG("constructor ResultUploadFile leak counter: " << leak_counter_result_upload_file)
    }
    ~ResultUploadFile()
    {
        leak_counter_result_upload_file--;
        LOG("destructor ResultUploadFile leak counter: " << leak_counter_result_upload_file)
    }
};
class S3Service
{
public:
    template <typename T>
    static ResultUploadFile *
    putObject(std::string srcPath, std::string dstPath, const S3Profile * s3Profile, cb_async_t cb_async, T user_payload)
    {
        S3Profile s3_profile = *s3Profile;
        if (s3_profile.bucket_name.empty() || s3_profile.endpoint_url.empty())
        {
            LOG("S3 profile not filled.")
            return nullptr;
        }
        if (srcPath.empty() || !fs::exists(fs::path(srcPath)))
        {
            LOG("File not exist.")
            return nullptr;
        }
        std::string upload_path = dstPath;
        if (dstPath.empty())
        {
            upload_path = fs::path(srcPath).filename();
        }
        auto s3client = S3CurlClient<T>(s3_profile.endpoint_url, s3_profile.host, s3_profile.access_key_id, s3_profile.secret_key);
        if (!s3_profile.s3_folder.empty())
        {
            upload_path = fmt::format("{}{}", s3_profile.s3_folder, upload_path);
        }

        s3client.upload(s3_profile.bucket_name, upload_path, srcPath, cb_async, user_payload);
        return nullptr;
    }
};
#endif // MY_PROJECT_SRC_S3SERVICE_H_
