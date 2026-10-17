#include "resource_manager.h"

#include "../util.h"
#include "cache_util.h"

#include <chen/config/config.h>
#include <chen/log/log.h>
#include <chen/util/util.h>

namespace blog {

static chen::Logger::ptr logger = LOG_ROOT();
static chen::ConfigVar<std::string>::ptr server_work_path = chen::Config::Lookup<std::string>("server.work_path");

static const size_t kCacheMaxSize = 500;

ResourceManager::ResourceManager()
    : m_cache(8, kCacheMaxSize, 0)
{
}

ResourceManager::ResourceType ResourceManager::GetResourceType(const std::string& filename)
{
    auto pos = filename.rfind('.');
    if (pos == std::string::npos) {
        return TYPE_OTHER;
    }
    std::string ext = filename.substr(pos + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    if (ext == "jpg" || ext == "jpeg" || ext == "png" || ext == "bmp" || ext == "gif" || ext == "webp") {
        return TYPE_IMAGE;
    } else if (ext == "mp4" || ext == "avi" || ext == "mov" || ext == "wmv" || ext == "flv" || ext == "mkv") {
        return TYPE_VIDEO;
    } else if (
        ext == "pdf" || ext == "doc" || ext == "docx" || ext == "xls" || ext == "xlsx" || ext == "ppt" || ext == "pptx" || ext == "txt"
        || ext == "md") {
        return TYPE_DOCUMENT;
    } else if (ext == "zip" || ext == "rar" || ext == "7z" || ext == "tar" || ext == "gz") {
        return TYPE_COMPRESSED;
    } else if (ext == "mp3" || ext == "wav" || ext == "aac" || ext == "flac" || ext == "ogg" || ext == "m4a") {
        return TYPE_AUDIO;
    } else {
        return TYPE_OTHER;
    }
}

data::ResourceInfo::ptr ResourceManager::parseRow(chen::ISQLData::ptr rt)
{
    return data::ResourceInfoDao::ParseRow(rt);
}

void ResourceManager::add(blog::data::ResourceInfo::ptr info)
{
    m_cache.set(info->getId(), info);
}

data::ResourceInfo::ptr ResourceManager::get(int64_t id)
{
    auto v = m_cache.get(id);
    if (v) {
        return v;
    }
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }
    v = data::ResourceInfoDao::Query(id, db);
    if (v) {
        m_cache.set(id, v);
    }
    return v;
}

void ResourceManager::getByBizUid(std::vector<data::ResourceInfo::ptr>& results, const std::string& biz_type, int64_t biz_id, int64_t uid)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return;
    }
    auto qb = data::ResourceInfoDao::newQuery();
    qb->where("biz_type", "=", biz_type);
    qb->where("biz_id", "=", biz_id);
    qb->where("owner_id", "=", uid);
    qb->where("is_deleted", "=", (int64_t)0);
    qb->orderBy("id", "DESC");
    if (data::ResourceInfoDao::QueryByBuilder(results, qb, db)) {
        ERROR(logger) << "QueryByBuilder failed";
        return;
    }
    for (auto& info : results) {
        if (!m_cache.exists(info->getId())) {
            m_cache.set(info->getId(), info);
        }
    }
}

data::ResourceInfo::ptr ResourceManager::getByBizUidName(const std::string& biz_type, int64_t biz_id, int64_t uid, const std::string& filename)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }
    auto qb = data::ResourceInfoDao::newQuery();
    qb->where("biz_type", "=", biz_type);
    qb->where("biz_id", "=", biz_id);
    qb->where("owner_id", "=", uid);
    qb->where("name", "=", filename);
    qb->limit(1);
    std::vector<data::ResourceInfo::ptr> results;
    if (data::ResourceInfoDao::QueryByBuilder(results, qb, db) || results.empty()) {
        return nullptr;
    }
    auto info = results[0];
    m_cache.set(info->getId(), info);
    return info;
}

data::ResourceInfo::ptr ResourceManager::getByPath(const std::string& path)
{
    std::string ck = "res:path:" + path;
    int64_t cachedId = getCachedIdMapping(ck);
    if (cachedId > 0) {
        return get(cachedId);
    }
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }
    auto qb = data::ResourceInfoDao::newQuery();
    qb->where("path", "=", path);
    qb->limit(1);
    std::vector<data::ResourceInfo::ptr> results;
    if (data::ResourceInfoDao::QueryByBuilder(results, qb, db) || results.empty()) {
        return nullptr;
    }
    auto info = results[0];
    m_cache.set(info->getId(), info);
    cacheIdMapping(ck, info->getId());
    return info;
}
} // namespace blog
