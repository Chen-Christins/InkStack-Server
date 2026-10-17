#pragma once

#include "blog/data/user_info.h"

#include <chen/db/query_builder.h>
#include <chen/ds/lru_cache.h>
#include <chen/util/singleton.h>

namespace blog {

class UserManager {
public:
    enum Role {
        USER = 1,
        ADMIN = 2,
        EDITOR = 3,
        CHECKER = 4
    };
    enum Status {
        INACTIVE = 0,
        ACTIVE = 1,
        BANNED = 2
    };

    UserManager();

    void add(blog::data::UserInfo::ptr info);

    void update(blog::data::UserInfo::ptr info);

    void getAllIds(std::vector<int64_t>& ids, bool isValid);

    /**
     * @brief 确保存在超级管理员（启动时无管理员则按配置自动创建）
     */
    void ensureSuperAdmin();

    uint64_t listByPages(
        std::vector<blog::data::UserInfo::ptr>& infos,
        uint64_t offset,
        uint64_t size,
        int32_t role,
        int32_t state,
        int32_t days,
        bool isValid);

    blog::data::UserInfo::ptr get(int64_t id);

    blog::data::UserInfo::ptr getByAccount(const std::string& v);

    blog::data::UserInfo::ptr getByEmail(const std::string& v);

    blog::data::UserInfo::ptr getByName(const std::string& v);

    static std::string GetToken(data::UserInfo::ptr info, int64_t us);

    static std::string generateToken();

private:
    static data::UserInfo::ptr parseRow(chen::ISQLData::ptr rt);

    chen::ds::HashLruCache<int64_t, data::UserInfo::ptr> m_cache;
};

typedef chen::Singleton<UserManager> UserMgr;

} // namespace blog
