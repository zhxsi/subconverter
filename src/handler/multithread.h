#ifndef MULTITHREAD_H_INCLUDED
#define MULTITHREAD_H_INCLUDED

#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <type_traits>
#include <vector>

#include <yaml-cpp/yaml.h>

#include "config/regmatch.h"
#include "generator/config/ruleconvert.h"
#include "utils/ini_reader/ini_reader.h"
#include "utils/string.h"

using guarded_mutex = std::lock_guard<std::mutex>;

RegexMatchConfigs safe_get_emojis();
RegexMatchConfigs safe_get_renames();
RegexMatchConfigs safe_get_streams();
RegexMatchConfigs safe_get_times();
std::vector<RulesetContent> safe_get_rulesets();
YAML::Node safe_get_clash_base();
INIReader safe_get_mellow_base();
void safe_set_emojis(RegexMatchConfigs data);
void safe_set_renames(RegexMatchConfigs data);
void safe_set_streams(RegexMatchConfigs data);
void safe_set_times(RegexMatchConfigs data);
void safe_set_rulesets(std::vector<RulesetContent> data);

void enqueueFetchTask(std::function<void()> task);

template <typename Fn>
auto submitFetchTask(Fn &&fn) -> std::future<std::invoke_result_t<std::decay_t<Fn>>>
{
    using R = std::invoke_result_t<std::decay_t<Fn>>;
    auto pkg = std::make_shared<std::packaged_task<R()>>(std::forward<Fn>(fn));
    std::future<R> fut = pkg->get_future();
    enqueueFetchTask([pkg = std::move(pkg)]() mutable { (*pkg)(); });
    return fut;
}

inline std::shared_future<std::string> makeReadyFuture(std::string value)
{
    std::promise<std::string> p;
    p.set_value(std::move(value));
    return p.get_future().share();
}

std::shared_future<std::string> fetchFileAsync(const std::string &path, const std::string &proxy, int cache_ttl, bool find_local = true, bool async = false);
std::string fetchFile(const std::string &path, const std::string &proxy, int cache_ttl, bool find_local = true);

#endif // MULTITHREAD_H_INCLUDED
