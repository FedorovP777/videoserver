
#include <uv.h>
#include <EventService.h>
#include <iostream>
#include <functional>
using namespace std;

auto fn_work = [](int * handle) { cout << "do work, value:" << (*handle)++ << endl; };
auto fn_end = []() { cout << "end" << endl; };
using namespace std::placeholders;
int value = 0;
auto i = std::bind(EventService::timer<int *>, 0, 10 * 1000, 100, fn_work, _1, &value);
#define L(a) i([]() { a })

int main(int /*argc*/, char ** /*argv*/)
{
    L(L(L(L(L(fn_end;););););); // replace then coroutines will be implements in std
    return uv_run(uv_default_loop(), UV_RUN_DEFAULT);
}