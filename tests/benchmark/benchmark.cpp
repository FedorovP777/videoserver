#include <benchmark/benchmark.h>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/common.h>
#include <libavutil/timestamp.h>
#include <uv.h>
}
#include "../../src/SharedQueue.h"
#include <memory>
#include <benchmark/benchmark.h>
class CustomMemoryManager : public benchmark::MemoryManager
{
public:
    int64_t num_allocs;
    int64_t max_bytes_used;


    void Start() BENCHMARK_OVERRIDE
    {
        num_allocs = 0;
        max_bytes_used = 0;
    }

    void Stop(Result& result) BENCHMARK_OVERRIDE
    {
        result.num_allocs = num_allocs;
        result.max_bytes_used = max_bytes_used;
    }
};

std::unique_ptr<CustomMemoryManager> mm(new CustomMemoryManager());

#ifdef MEMORY_PROFILER
void * custom_malloc(size_t size)
{
    void * p = malloc(size);
    mm.get()->num_allocs += 1;
    mm.get()->max_bytes_used += size;
    return p;
}
#    define malloc(size) custom_malloc(size)
#endif


static void BM_QueuePush(benchmark::State & state)
{
    SharedQueue<AVPacket *> queue_write;
    for (auto _ : state)
    {
        auto * p = new AVPacket;
        queue_write.push(p);
    }
}
BENCHMARK(BM_QueuePush)->Threads(8);

static void BM_QueuePushNoVariable(benchmark::State & state)
{
    SharedQueue<AVPacket *> queue_write;
    for (auto _ : state)
    {
        queue_write.push(new AVPacket);
    }
}
BENCHMARK(BM_QueuePushNoVariable);

static void BM_QueuePushNoVariableNoMutex(benchmark::State & state)
{
    SharedQueue<AVPacket *> queue_write;
    queue_write.use_mutex = false;
    for (auto _ : state)
    {
        queue_write.push(new AVPacket);
    }
}
BENCHMARK(BM_QueuePushNoVariableNoMutex);

static void BM_QueuePushNoNewObject(benchmark::State & state)
{
    SharedQueue<AVPacket *> queue_write;
    AVPacket p;
    for (auto _ : state)
    {
        queue_write.push(&p);
    }
}
BENCHMARK(BM_QueuePushNoNewObject)->Threads(8);

static void BM_LibUV_Queue_Work(benchmark::State & state)
{
    uv_loop_t * loop = uv_default_loop();
    for (auto _ : state)
    {
        auto * work_task = new uv_work_t;
        work_task->data = (void *)work_task;
        uv_queue_work(
            loop, work_task, [](uv_work_t * req) {}, [](uv_work_t * req, int status) { delete req; });
    }
    uv_run(loop, UV_RUN_DEFAULT);
}
BENCHMARK(BM_LibUV_Queue_Work);

BENCHMARK_MAIN();
