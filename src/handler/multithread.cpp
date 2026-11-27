#include <algorithm>
#include <condition_variable>
#include <future>
#include <queue>
#include <thread>
#include <vector>

#include "handler/settings.h"
#include "utils/network.h"
#include "webget.h"
#include "multithread.h"
//#include "vfs.h"

//safety lock for multi-thread
std::mutex on_emoji, on_rename, on_stream, on_time, on_rulesets;

namespace
{
class FetchThreadPool
{
public:
    explicit FetchThreadPool(size_t worker_count)
    {
        workers_.reserve(worker_count);
        for(size_t i = 0; i < worker_count; ++i)
        {
            workers_.emplace_back([this]() { workerLoop(); });
        }
    }

    ~FetchThreadPool()
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stopping_ = true;
        }
        cv_.notify_all();
        for(std::thread &t : workers_)
        {
            if(t.joinable())
                t.join();
        }
    }

    void enqueue(std::function<void()> task)
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            tasks_.push(std::move(task));
        }
        cv_.notify_one();
    }

private:
    void workerLoop()
    {
        while(true)
        {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                cv_.wait(lock, [this]() { return stopping_ || !tasks_.empty(); });
                if(stopping_ && tasks_.empty())
                    return;
                task = std::move(tasks_.front());
                tasks_.pop();
            }
            task();
        }
    }

    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool stopping_ = false;
};

FetchThreadPool &getFetchPool()
{
    static FetchThreadPool pool([]() -> size_t {
        int desired = std::max(16, global.maxConcurThreads * 2);
        return static_cast<size_t>(std::clamp(desired, 4, 64));
    }());
    return pool;
}
} // namespace

void enqueueFetchTask(std::function<void()> task)
{
    getFetchPool().enqueue(std::move(task));
}

RegexMatchConfigs safe_get_emojis()
{
    guarded_mutex guard(on_emoji);
    return global.emojis;
}

RegexMatchConfigs safe_get_renames()
{
    guarded_mutex guard(on_rename);
    return global.renames;
}

RegexMatchConfigs safe_get_streams()
{
    guarded_mutex guard(on_stream);
    return global.streamNodeRules;
}

RegexMatchConfigs safe_get_times()
{
    guarded_mutex guard(on_time);
    return global.timeNodeRules;
}

std::vector<RulesetContent> safe_get_rulesets()
{
    guarded_mutex guard(on_rulesets);
    return global.rulesetsContent;
}

void safe_set_emojis(RegexMatchConfigs data)
{
    guarded_mutex guard(on_emoji);
    global.emojis.swap(data);
}

void safe_set_renames(RegexMatchConfigs data)
{
    guarded_mutex guard(on_rename);
    global.renames.swap(data);
}

void safe_set_streams(RegexMatchConfigs data)
{
    guarded_mutex guard(on_stream);
    global.streamNodeRules.swap(data);
}

void safe_set_times(RegexMatchConfigs data)
{
    guarded_mutex guard(on_time);
    global.timeNodeRules.swap(data);
}

void safe_set_rulesets(std::vector<RulesetContent> data)
{
    guarded_mutex guard(on_rulesets);
    global.rulesetsContent.swap(data);
}

std::shared_future<std::string> fetchFileAsync(const std::string &path, const std::string &proxy, int cache_ttl, bool find_local, bool async)
{
    if(find_local && fileExist(path, true))
        return makeReadyFuture(fileGet(path, true));
    if(isLink(path))
    {
        if(async)
            return submitFetchTask([path, proxy, cache_ttl]() { return webGet(path, proxy, cache_ttl); }).share();
        return makeReadyFuture(webGet(path, proxy, cache_ttl));
    }
    return makeReadyFuture(std::string());
}

std::string fetchFile(const std::string &path, const std::string &proxy, int cache_ttl, bool find_local)
{
    if(find_local && fileExist(path, true))
        return fileGet(path, true);
    if(isLink(path))
        return webGet(path, proxy, cache_ttl);
    return std::string();
}
