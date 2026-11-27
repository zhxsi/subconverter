#include <iostream>
#include <cstdio>
#include <unistd.h>
#include <sys/stat.h>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <atomic>

#include <curl/curl.h>

#include "handler/settings.h"
#include "utils/base64/base64.h"
#include "utils/defer.h"
#include "utils/file_extra.h"
#include "utils/lock.h"
#include "utils/logger.h"
#include "utils/urlencode.h"
#include "version.h"
#include "webget.h"

#ifdef _WIN32
#ifndef _stat
#define _stat stat
#endif // _stat
#endif // _WIN32

/*
using guarded_mutex = std::lock_guard<std::mutex>;
std::mutex cache_rw_lock;
*/

RWLock cache_rw_lock;

//std::string user_agent_str = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/74.0.3729.169 Safari/537.36";
static auto user_agent_str = "subconverter/" VERSION " cURL/" LIBCURL_VERSION;

struct curl_progress_data
{
    long size_limit = 0L;
};

static inline void curl_init()
{
    static std::once_flag init_flag;
    std::call_once(init_flag, []() {
        curl_global_init(CURL_GLOBAL_ALL);
    });
}

namespace
{
struct ThreadLocalCurlHandle
{
    CURL *handle = nullptr;

    CURL *acquire()
    {
        if(!handle)
            handle = curl_easy_init();
        else
            curl_easy_reset(handle);
        return handle;
    }

    ~ThreadLocalCurlHandle()
    {
        if(handle)
            curl_easy_cleanup(handle);
    }
};

CURL *getThreadLocalCurlHandle()
{
    thread_local ThreadLocalCurlHandle tl_curl;
    return tl_curl.acquire();
}
} // namespace

static int writer(char *data, size_t size, size_t nmemb, std::string *writerData)
{
    if(writerData == nullptr)
        return 0;

    writerData->append(data, size*nmemb);

    return static_cast<int>(size * nmemb);
}

static int dummy_writer(char *, size_t size, size_t nmemb, void *)
{
    /// dummy writer, do not save anything
    return static_cast<int>(size * nmemb);
}

//static int size_checker(void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow)
static int size_checker(void *clientp, curl_off_t, curl_off_t dlnow, curl_off_t, curl_off_t)
{
    if(clientp)
    {
        auto *data = reinterpret_cast<curl_progress_data*>(clientp);
        if(data->size_limit)
        {
            if(dlnow > data->size_limit)
                return 1;
        }
    }
    return 0;
}

static int logger(CURL *handle, curl_infotype type, char *data, size_t size, void *userptr)
{
    (void)handle;
    (void)userptr;
    std::string prefix;
    switch(type)
    {
    case CURLINFO_TEXT:
        prefix = "CURL_INFO: ";
        break;
    case CURLINFO_HEADER_IN:
        prefix = "CURL_HEADER: < ";
        break;
    case CURLINFO_HEADER_OUT:
        prefix = "CURL_HEADER: > ";
        break;
    case CURLINFO_DATA_IN:
    case CURLINFO_DATA_OUT:
    default:
        return 0;
    }
    std::string content(data, size);
    if(content.find("\r\n") != std::string::npos)
    {
        string_array lines = split(content, "\r\n");
        for(auto &x : lines)
        {
            std::string log_content = prefix;
            log_content += x;
            writeLog(0, log_content, LOG_LEVEL_VERBOSE);
        }
    }
    else
    {
        std::string log_content = prefix;
        log_content += trimWhitespace(content);
        writeLog(0, log_content, LOG_LEVEL_VERBOSE);
    }
    return 0;
}

static inline void curl_set_common_options(CURL *curl_handle, const char *url, curl_progress_data *data)
{
    curl_easy_setopt(curl_handle, CURLOPT_URL, url);
    curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, global.logLevel == LOG_LEVEL_VERBOSE ? 1L : 0L);
    curl_easy_setopt(curl_handle, CURLOPT_DEBUGFUNCTION, logger);
    curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl_handle, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl_handle, CURLOPT_MAXREDIRS, 20L);
    curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl_handle, CURLOPT_CONNECTTIMEOUT, 5L);
    curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, 15L);
    curl_easy_setopt(curl_handle, CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(curl_handle, CURLOPT_COOKIEFILE, "");
    if(data)
    {
        if(data->size_limit)
            curl_easy_setopt(curl_handle, CURLOPT_MAXFILESIZE, data->size_limit);
        curl_easy_setopt(curl_handle, CURLOPT_XFERINFOFUNCTION, size_checker);
        curl_easy_setopt(curl_handle, CURLOPT_XFERINFODATA, data);
    }
}

//static std::string curlGet(const std::string &url, const std::string &proxy, std::string &response_headers, CURLcode &return_code, const string_map &request_headers)
static int curlGet(const FetchArgument &argument, FetchResult &result)
{
    CURL *curl_handle;
    std::string *data = result.content, new_url = argument.url;
    curl_slist *header_list = nullptr;
    defer(curl_slist_free_all(header_list);)
    long retVal;

    curl_init();

    curl_handle = getThreadLocalCurlHandle();
    if(!curl_handle)
        return 0;
    // Clear any cookies from prior requests on this pooled handle while keeping
    // libcurl's persistent TCP/TLS connection pool and DNS cache intact.
    curl_easy_setopt(curl_handle, CURLOPT_COOKIELIST, "ALL");
    if(!argument.proxy.empty())
    {
        if(startsWith(argument.proxy, "cors:"))
        {
            header_list = curl_slist_append(header_list, "X-Requested-With: subconverter " VERSION);
            new_url = argument.proxy.substr(5) + argument.url;
        }
        else
            curl_easy_setopt(curl_handle, CURLOPT_PROXY, argument.proxy.data());
    }
    curl_progress_data limit;
    limit.size_limit = global.maxAllowedDownloadSize;
    curl_set_common_options(curl_handle, new_url.data(), &limit);
    bool has_content_type = argument.request_headers && argument.request_headers->contains("Content-Type");
    if((argument.method == HTTP_POST || argument.method == HTTP_PATCH) && !has_content_type)
        header_list = curl_slist_append(header_list, "Content-Type: application/json;charset=utf-8");
    if(argument.request_headers)
    {
        for(auto &x : *argument.request_headers)
        {
            auto header = x.first + ": " + x.second;
            header_list = curl_slist_append(header_list, header.data());
        }
        if(!argument.request_headers->contains("User-Agent"))
            curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, user_agent_str);
    }
    header_list = curl_slist_append(header_list, "SubConverter-Request: 1");
    header_list = curl_slist_append(header_list, "SubConverter-Version: " VERSION);
    if(header_list)
        curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, header_list);

    if(result.content)
    {
        curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, writer);
        curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, result.content);
    }
    else
        curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, dummy_writer);
    if(result.response_headers)
    {
        curl_easy_setopt(curl_handle, CURLOPT_HEADERFUNCTION, writer);
        curl_easy_setopt(curl_handle, CURLOPT_HEADERDATA, result.response_headers);
    }
    else
        curl_easy_setopt(curl_handle, CURLOPT_HEADERFUNCTION, dummy_writer);

    if(argument.cookies)
    {
        string_array cookies = split(*argument.cookies, "\r\n");
        for(auto &x : cookies)
            curl_easy_setopt(curl_handle, CURLOPT_COOKIELIST, x.c_str());
    }

    switch(argument.method)
    {
    case HTTP_POST:
        curl_easy_setopt(curl_handle, CURLOPT_POST, 1L);
        if(argument.post_data)
        {
            curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, argument.post_data->data());
            curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDSIZE, argument.post_data->size());
        }
        break;
    case HTTP_PATCH:
        curl_easy_setopt(curl_handle, CURLOPT_CUSTOMREQUEST, "PATCH");
        if(argument.post_data)
        {
            curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, argument.post_data->data());
            curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDSIZE, argument.post_data->size());
        }
        break;
    case HTTP_HEAD:
        curl_easy_setopt(curl_handle, CURLOPT_NOBODY, 1L);
        break;
    case HTTP_GET:
        break;
    }

    unsigned int fail_count = 0, max_fails = 1;
    while(true)
    {
        retVal = curl_easy_perform(curl_handle);
        if(retVal == CURLE_OK || max_fails <= fail_count || global.APIMode)
            break;
        if(result.content)
            result.content->clear();
        if(result.response_headers)
            result.response_headers->clear();
        fail_count++;
    }

    long code = 0;
    curl_easy_getinfo(curl_handle, CURLINFO_HTTP_CODE, &code);
    *result.status_code = code;

    if(result.cookies)
    {
        curl_slist *cookies = nullptr;
        curl_easy_getinfo(curl_handle, CURLINFO_COOKIELIST, &cookies);
        if(cookies)
        {
            auto each = cookies;
            while(each)
            {
                result.cookies->append(each->data);
                *result.cookies += "\r\n";
                each = each->next;
            }
        }
        curl_slist_free_all(cookies);
    }

    // Keep pooled curl_handle alive for connection/TLS reuse; clear cookies & per-call pointers.
    curl_easy_setopt(curl_handle, CURLOPT_COOKIELIST, "ALL");
    curl_easy_reset(curl_handle);

    if(data && !argument.keep_resp_on_fail)
    {
        if(retVal != CURLE_OK || *result.status_code != 200)
            data->clear();
        data->shrink_to_fit();
    }

    return *result.status_code;
}

// data:[<mediatype>][;base64],<data>
static std::string dataGet(const std::string &url)
{
    if (!startsWith(url, "data:"))
        return "";
    std::string::size_type comma = url.find(',');
    if (comma == std::string::npos || comma == url.size() - 1)
        return "";

    std::string data = urlDecode(url.substr(comma + 1));
    if (endsWith(url.substr(0, comma), ";base64")) {
        return urlSafeBase64Decode(data);
    } else {
        return data;
    }
}

std::string buildSocks5ProxyString(const std::string &addr, int port, const std::string &username, const std::string &password)
{
    std::string authstr = username.size() && password.size() ? username + ":" + password + "@" : "";
    std::string proxystr = "socks5://" + authstr + addr + ":" + std::to_string(port);
    return proxystr;
}

namespace
{
struct InFlightResult
{
    int return_code = 0;
    std::string content;
    std::string headers;
};

struct InFlightEntry
{
    std::mutex mtx;
    std::condition_variable cv;
    bool done = false;
    InFlightResult result;
};

std::mutex inflight_map_mutex;
std::map<std::string, std::shared_ptr<InFlightEntry>> inflight_map;
std::atomic<uint64_t> cache_tmp_seq{0};
std::atomic<time_t> last_cache_cleanup{0};

void maybeCleanupStaleCache()
{
    constexpr time_t kCleanupIntervalSec = 3600;
    time_t now = time(nullptr);
    time_t prev = last_cache_cleanup.load(std::memory_order_relaxed);
    if(prev != 0 && (now - prev) < kCleanupIntervalSec)
        return;
    if(!last_cache_cleanup.compare_exchange_strong(prev, now, std::memory_order_acq_rel))
        return;

    int max_ttl = std::max(global.cacheSubscription, std::max(global.cacheConfig, global.cacheRuleset));
    double stale_age = std::max(7.0 * 86400.0, static_cast<double>(max_ttl) * 2.0);
    cache_rw_lock.writeLock();
    defer(cache_rw_lock.writeUnlock();)
    operateFiles("cache", [now, stale_age](const std::string &file) {
        std::string full = "cache/" + file;
        struct stat st {};
        if(stat(full.c_str(), &st) != 0)
            return 0;
        double age = difftime(now, st.st_mtime);
        bool is_orphan_tmp = file.find(".tmp.") != std::string::npos && age > 300.0;
        if(is_orphan_tmp || age > stale_age)
            std::remove(full.c_str());
        return 0;
    });
}

void atomicFileWrite(const std::string &path, const std::string &data)
{
    std::string tmp_path = path + ".tmp." + std::to_string(cache_tmp_seq.fetch_add(1, std::memory_order_relaxed));
    if(fileWrite(tmp_path, data, true) != 0)
    {
        fileWrite(path, data, true);
        return;
    }
#ifdef _WIN32
    std::remove(path.c_str());
#endif
    if(std::rename(tmp_path.c_str(), path.c_str()) != 0)
    {
        std::remove(tmp_path.c_str());
        fileWrite(path, data, true);
    }
}

std::string buildSingleflightKey(const std::string &url, const std::string &proxy, const string_icase_map *request_headers)
{
    std::string key = url;
    key.push_back('\n');
    key += proxy;
    if(request_headers)
    {
        if(auto it = request_headers->find("User-Agent"); it != request_headers->end())
        {
            key += "\nUA:";
            key += it->second;
        }
        if(auto it = request_headers->find("Referer"); it != request_headers->end())
        {
            key += "\nRef:";
            key += it->second;
        }
    }
    return key;
}
} // namespace

std::string webGet(const std::string &url, const std::string &proxy, unsigned int cache_ttl, std::string *response_headers, string_icase_map *request_headers)
{
    int return_code = 0;
    std::string content;

    FetchArgument argument {HTTP_GET, url, proxy, nullptr, request_headers, nullptr, cache_ttl};
    FetchResult fetch_res {&return_code, &content, response_headers, nullptr};

    if (startsWith(url, "data:"))
        return dataGet(url);
    // cache system with cross-request singleflight coalescing & atomic cache writes
    if(cache_ttl > 0)
    {
        md("cache");
        const std::string url_md5 = getMD5(url);
        const std::string path = "cache/" + url_md5, path_header = path + "_header";
        auto try_read_valid_cache = [&](bool &exists, std::string &out_content, std::string &out_headers) -> bool
        {
            cache_rw_lock.readLock();
            defer(cache_rw_lock.readUnlock();)
            struct stat result {};
            if(stat(path.data(), &result) == 0)
            {
                exists = true;
                time_t mtime = result.st_mtime, now = time(nullptr);
                if(difftime(now, mtime) <= cache_ttl)
                {
                    out_content = fileGet(path, true);
                    if(response_headers)
                        out_headers = fileGet(path_header, true);
                    return true;
                }
            }
            return false;
        };

        bool cache_exists = false;
        std::string cached_content, cached_headers;
        if(try_read_valid_cache(cache_exists, cached_content, cached_headers))
        {
            writeLog(0, "CACHE HIT: '" + url + "', using local cache.");
            if(response_headers)
                *response_headers = std::move(cached_headers);
            return cached_content;
        }

        // Cross-request singleflight: coalesce concurrent cache-miss fetches for the same URL.
        const std::string sf_key = buildSingleflightKey(url, proxy, request_headers);
        std::shared_ptr<InFlightEntry> entry;
        bool is_leader = false;
        {
            std::lock_guard<std::mutex> lk(inflight_map_mutex);
            auto it = inflight_map.find(sf_key);
            if(it != inflight_map.end())
            {
                entry = it->second;
            }
            else
            {
                entry = std::make_shared<InFlightEntry>();
                inflight_map.emplace(sf_key, entry);
                is_leader = true;
            }
        }

        if(!is_leader)
        {
            std::unique_lock<std::mutex> lk(entry->mtx);
            entry->cv.wait(lk, [&]() { return entry->done; });
            if(response_headers)
                *response_headers = entry->result.headers;
            return entry->result.content;
        }

        InFlightResult sf_res;
        auto finish_singleflight = [&]()
        {
            {
                std::lock_guard<std::mutex> lk(entry->mtx);
                entry->result = sf_res;
                entry->done = true;
            }
            {
                std::lock_guard<std::mutex> lk(inflight_map_mutex);
                auto it = inflight_map.find(sf_key);
                if(it != inflight_map.end() && it->second == entry)
                    inflight_map.erase(it);
            }
            entry->cv.notify_all();
        };
        defer(finish_singleflight());

        // Re-check cache after becoming leader in case a previous leader just populated it.
        if(try_read_valid_cache(cache_exists, cached_content, cached_headers))
        {
            writeLog(0, "CACHE HIT: '" + url + "', using local cache.");
            sf_res.return_code = 200;
            sf_res.content = cached_content;
            sf_res.headers = cached_headers;
            if(response_headers)
                *response_headers = std::move(cached_headers);
            return cached_content;
        }

        if(cache_exists)
            writeLog(0, "CACHE MISS: '" + url + "', TTL timeout, creating new cache."); // out of TTL
        else
            writeLog(0, "CACHE NOT EXIST: '" + url + "', creating new cache.");

        FetchResult leader_res {&sf_res.return_code, &sf_res.content, &sf_res.headers, nullptr};
        curlGet(argument, leader_res);
        if(sf_res.return_code == 200) // success, save new cache atomically
        {
            {
                cache_rw_lock.writeLock();
                defer(cache_rw_lock.writeUnlock();)
                atomicFileWrite(path, sf_res.content);
                atomicFileWrite(path_header, sf_res.headers);
            }
            maybeCleanupStaleCache();
        }
        else
        {
            bool served_cache = false;
            if(global.serveCacheOnFetchFail)
            {
                cache_rw_lock.readLock();
                defer(cache_rw_lock.readUnlock();)
                if(fileExist(path)) // failed, check if cache exist
                {
                    sf_res.content = fileGet(path, true);
                    sf_res.headers = fileGet(path_header, true);
                    served_cache = true;
                }
            }
            if(served_cache)
                writeLog(0, "Fetch failed. Serving cached content."); // cache exist, serving cache
            else
                writeLog(0, "Fetch failed. No local cache available."); // cache not exist or not allow to serve cache, serving nothing
        }
        if(response_headers)
            *response_headers = sf_res.headers;
        return sf_res.content;
    }
    //return curlGet(url, proxy, response_headers, return_code);
    curlGet(argument, fetch_res);
    return content;
}

void flushCache()
{
    //guarded_mutex guard(cache_rw_lock);
    cache_rw_lock.writeLock();
    defer(cache_rw_lock.writeUnlock();)
    operateFiles("cache", [](const std::string &file){ remove(("cache/" + file).data()); return 0; });
}

int webPost(const std::string &url, const std::string &data, const std::string &proxy, const string_icase_map &request_headers, std::string *retData)
{
    //return curlPost(url, data, proxy, request_headers, retData);
    int return_code = 0;
    FetchArgument argument {HTTP_POST, url, proxy, &data, &request_headers, nullptr, 0, true};
    FetchResult fetch_res {&return_code, retData, nullptr, nullptr};
    return webGet(argument, fetch_res);
}

int webPatch(const std::string &url, const std::string &data, const std::string &proxy, const string_icase_map &request_headers, std::string *retData)
{
    //return curlPatch(url, data, proxy, request_headers, retData);
    int return_code = 0;
    FetchArgument argument {HTTP_PATCH, url, proxy, &data, &request_headers, nullptr, 0, true};
    FetchResult fetch_res {&return_code, retData, nullptr, nullptr};
    return webGet(argument, fetch_res);
}

int webHead(const std::string &url, const std::string &proxy, const string_icase_map &request_headers, std::string &response_headers)
{
    //return curlHead(url, proxy, request_headers, response_headers);
    int return_code = 0;
    FetchArgument argument {HTTP_HEAD, url, proxy, nullptr, &request_headers, nullptr, 0};
    FetchResult fetch_res {&return_code, nullptr, &response_headers, nullptr};
    return webGet(argument, fetch_res);
}

string_array headers_map_to_array(const string_map &headers)
{
    string_array result;
    for(auto &kv : headers)
        result.push_back(kv.first + ": " + kv.second);
    return result;
}

int webGet(const FetchArgument& argument, FetchResult &result)
{
    return curlGet(argument, result);
}
