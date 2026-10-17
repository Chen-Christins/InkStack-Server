#pragma once

#include "blog/data/article_category_rel_info.h"

#include <chen/db/query_builder.h>
#include <chen/ds/lru_cache.h>
#include <chen/util/singleton.h>

namespace blog {

class ArticleCategoryRelManager {
public:
    ArticleCategoryRelManager();

    void add(data::ArticleCategoryRelInfo::ptr info);

    data::ArticleCategoryRelInfo::ptr get(int64_t id);

    bool listByArticleId(std::vector<data::ArticleCategoryRelInfo::ptr>& infos, int64_t id, bool valid);

    bool listByCategoryId(std::vector<data::ArticleCategoryRelInfo::ptr>& infos, int64_t category_id, bool valid);

    data::ArticleCategoryRelInfo::ptr getByArticleIdCategoryId(int64_t article_id, int64_t category_id);

private:
    static data::ArticleCategoryRelInfo::ptr parseRow(chen::ISQLData::ptr rt);

    chen::ds::HashLruCache<int64_t, data::ArticleCategoryRelInfo::ptr> m_cache;
};

typedef chen::Singleton<ArticleCategoryRelManager> ArticleCategoryRelMgr;

} // namespace blog
