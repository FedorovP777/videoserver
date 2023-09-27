
#include <uv.h>
#include <fmt/core.h>
#include <LibuvCurlCpp.h>
#include <EventService.h>
#include <iostream>
#include <functional>
using namespace std;

void done(uv_async_t * handle)
{
    auto result = reinterpret_cast<LibuvCurlCpp::response<int> *>(handle->data);
    std::cout << result->http_code << std::endl;
    std::cout << result->body << std::endl;

    uv_close(reinterpret_cast<uv_handle_t *>(handle), [](uv_handle_t * t) { delete t; });
}

auto fn_work = [](int handle) {
    cout << "do work, value:" << handle << endl;

    LibuvCurlCpp::request_options options;
    std::unordered_map<std::string, std::string> headers;
    headers["Accept"] = "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8,application/"
                        "signed-exchange;v=b3;q=0.9";
    headers["Host"] = "raw.githubusercontent.com";

    options["method"] = "GET";
    options["url"] = "https://1.1.1.1:9000/test";
    options["headers"] = headers;
    auto i = new int(123);
    LibuvCurlCpp::LibuvCurlCpp<int*>::request(options, done, i);
};
auto fn_end = []() { cout << "end" << endl; };
using namespace std::placeholders;
#define L(a) i([]() { a })

int main(int /*argc*/, char ** /*argv*/)
{
    int value = 0;


    EventService::timer<int>(0, 10 * 1000, 1000, fn_work, fn_end, 1);

    return uv_run(uv_default_loop(), UV_RUN_DEFAULT);
}