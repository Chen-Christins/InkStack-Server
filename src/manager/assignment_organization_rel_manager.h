#pragma once

#include "blog/data/assignment_organization_rel_info.h"

#include <chen/db/query_builder.h>
#include <chen/ds/lru_cache.h>
#include <chen/util/singleton.h>

namespace blog {

class AssignmentOrganizationRelManager {
public:
    AssignmentOrganizationRelManager();

    void add(blog::data::AssignmentOrganizationRelInfo::ptr info);

    blog::data::AssignmentOrganizationRelInfo::ptr get(int64_t id);

    blog::data::AssignmentOrganizationRelInfo::ptr getByOrgAndAssign(int64_t org_id, int64_t assign_id);

    int64_t getByAssignmentId(std::vector<data::AssignmentOrganizationRelInfo::ptr>& results, int64_t assign_id);

    int64_t getByPages(
        std::vector<data::AssignmentOrganizationRelInfo::ptr>& results,
        int64_t o_id,
        uint64_t offset,
        uint64_t size,
        int32_t status,
        bool isValid);

private:
    static data::AssignmentOrganizationRelInfo::ptr parseRow(chen::ISQLData::ptr rt);

    chen::ds::HashLruCache<int64_t, data::AssignmentOrganizationRelInfo::ptr> m_cache;
};

typedef chen::Singleton<AssignmentOrganizationRelManager> AssignmentOrganizationRelMgr;

} // namespace blog
