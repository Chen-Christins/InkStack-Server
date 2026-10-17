#pragma once

#include "blog/data/category_info.h"

#include <chen/db/query_builder.h>
#include <chen/ds/lru_cache.h>
#include <chen/util/singleton.h>

namespace blog {

class CategoryManager {
public:
    CategoryManager();

    void listAll(std::vector<blog::data::CategoryInfo::ptr>& infos, bool isValid = false);

    void add(blog::data::CategoryInfo::ptr info);

    blog::data::CategoryInfo::ptr get(int64_t id);

    blog::data::CategoryInfo::ptr getByName(const std::string& name);

private:
    static data::CategoryInfo::ptr parseRow(chen::ISQLData::ptr rt);

    chen::ds::HashLruCache<int64_t, data::CategoryInfo::ptr> m_cache;
};

typedef chen::Singleton<CategoryManager> CategoryMgr;

} // namespace blog
