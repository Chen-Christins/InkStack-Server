#pragma once

#include "blog/data/article_label_rel_info.h"

#include <chen/db/query_builder.h>
#include <chen/ds/lru_cache.h>
#include <chen/util/singleton.h>

namespace blog {

class ArticleLabelRelManager {
public:
    ArticleLabelRelManager();

    void add(data::ArticleLabelRelInfo::ptr info);

    data::ArticleLabelRelInfo::ptr get(int64_t id);

    bool listByArticleId(std::vector<data::ArticleLabelRelInfo::ptr>& infos, int64_t id, bool valid);

    bool listByLabelId(std::vector<data::ArticleLabelRelInfo::ptr>& infos, int64_t label_id, bool valid);

    data::ArticleLabelRelInfo::ptr getByArticleIdLabelId(int64_t article_id, int64_t label_id);

private:
    static data::ArticleLabelRelInfo::ptr parseRow(chen::ISQLData::ptr rt);

    chen::ds::HashLruCache<int64_t, data::ArticleLabelRelInfo::ptr> m_cache;
};

typedef chen::Singleton<ArticleLabelRelManager> ArticleLabelRelMgr;

} // namespace blog
