//
// Created by User on 30.01.2023.
//

#ifndef LIBAV_TEST_S3CURLCLIENT_H
#define LIBAV_TEST_S3CURLCLIENT_H
#include <curl/curl.h>
#include "crypto/SSLHmac.h"
#include "third-party/base64.h"
#include "LibuvCurlCpp.h"
using f_int_t = void (*)(string body, string header, int result, int statusCode);
using cb_async_t = void (*)(uv_async_t * handle);
template <typename T>
class S3CurlClient
{
    string key;
    string secret;
    string endpoint;
    string host;

public:
    S3CurlClient(string & _endpoint, string & _host, string & _key, string & _secret)
        : endpoint(_endpoint), host(_host), key(_key), secret(_secret){};

    void upload(const string & bucket, const string & target, const string & filename, cb_async_t cb_async, T callback_payload)
    {
        FILE * fd = fopen(filename.c_str(), "rb");
        if (!fd)
            throw string("File not found.");

        std::time_t t = std::time(nullptr);
        string date = fmt::format("{:%a, %d %b %Y %T %z}", fmt::localtime(t));

        struct curl_slist * list = NULL;
        list = curl_slist_append(list, "Content-Type: application/octet-stream");
        auto header_date = fmt::format("Date: {}", date);
        list = curl_slist_append(list, header_date.c_str());
        auto _signature = fmt::format("PUT\n\napplication/octet-stream\n{}\n/{}/{}", date, bucket, target);
        auto ssl_hmac = SSLHmac(secret);
        ssl_hmac.update(_signature);
        auto sign = ssl_hmac.get_raw_result();
        string sign_b64 = base64_encode(sign);
        auto authorization = fmt::format("AWS {}:{}", key, sign_b64);

        struct stat file_info;
        if (fstat(fileno(fd), &file_info) != 0)
            throw string("Error file read.");

        CURL * curl = curl_easy_init();

        CURLcode res;
        if (!curl)
        {
            throw string("Error curl init.");
        }
        string url = fmt::format("http://{}/{}/{}", endpoint, bucket, target);
        LibuvCurlCpp::request_options options;
        std::unordered_map<std::string, std::string> headers;
        headers["Content-Type"] = "application/octet-stream";
        headers["Date"] = date;
        headers["Host"] = host;
        headers["Authorization"] = authorization;

        options["method"] = "PUT";
        options["url"] = url;
        options["headers"] = headers;
        options["file_upload"] = filename;

        LibuvCurlCpp::LibuvCurlCpp<T>::request(options, cb_async, callback_payload);
    };
};
#endif //LIBAV_TEST_S3CURLCLIENT_H
