#include "passkey_manager.h"

#include "../util.h"

#include <chen/log/log.h>

#include <ctime>

namespace blog {

static chen::Logger::ptr logger = LOG_ROOT();

data::UserPasskeyInfo::ptr PasskeyManager::getByCredentialId(const std::string& credential_id)
{
    if (credential_id.empty()) {
        return nullptr;
    }

    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }

    auto info = data::UserPasskeyInfoDao::QueryByCredentialId(credential_id, db);
    if (!info || info->getIsDeleted()) {
        return nullptr;
    }
    return info;
}

data::UserPasskeyInfo::ptr PasskeyManager::get(int64_t id)
{
    if (!id) {
        return nullptr;
    }

    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }

    auto info = data::UserPasskeyInfoDao::Query(id, db);
    if (!info || info->getIsDeleted()) {
        return nullptr;
    }
    return info;
}

int PasskeyManager::listByUser(std::vector<data::UserPasskeyInfo::ptr>& results, int64_t uid)
{
    results.clear();
    if (!uid) {
        return 0;
    }

    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return -1;
    }

    std::vector<data::UserPasskeyInfo::ptr> all;
    if (data::UserPasskeyInfoDao::QueryByUserId(all, uid, db)) {
        ERROR(logger) << "query passkeys failed uid=" << uid;
        return -1;
    }

    for (auto& info : all) {
        if (info && !info->getIsDeleted()) {
            results.push_back(info);
        }
    }
    return static_cast<int>(results.size());
}

int PasskeyManager::add(data::UserPasskeyInfo::ptr info)
{
    if (!info) {
        return -1;
    }

    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return -1;
    }

    if (data::UserPasskeyInfoDao::Insert(info, db)) {
        ERROR(logger) << "insert passkey failed user=" << info->getUserId();
        return -1;
    }
    return 0;
}

int PasskeyManager::update(data::UserPasskeyInfo::ptr info)
{
    if (!info) {
        return -1;
    }

    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return -1;
    }

    if (data::UserPasskeyInfoDao::Update(info, db)) {
        ERROR(logger) << "update passkey failed id=" << info->getId();
        return -1;
    }
    return 0;
}

int PasskeyManager::remove(int64_t id)
{
    auto info = get(id);
    if (!info) {
        return -1;
    }

    info->setIsDeleted(1);
    info->setLastUsedAt(time(0));
    return update(info);
}

} // namespace blog
