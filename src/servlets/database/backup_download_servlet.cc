#include "backup_download_servlet.h"

#include <chen/config/config.h>
#include <chen/log/log.h>
#include <chen/util/util.h>

#include "../../manager/backup_record_manager.h"
#include "../../manager/user_manager.h"

#include <fstream>

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();
static chen::ConfigVar<std::string>::ptr server_work_path = chen::Config::Lookup<std::string>("server.work_path");

BackupDownloadServlet::BackupDownloadServlet()
    : BlogLoginedServlet("BackupDownloadServlet")
{
}

int32_t BackupDownloadServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    bool success = false;
    do {
        int64_t uid = getUserId(request);
        if (!uid) {
            result->setErrno(errcode::NOT_LOGIN);
            break;
        }
        auto current_user = UserMgr::GetInstance()->get(uid);
        if (!current_user) {
            result->setErrno(errcode::USER_NOT_FOUND);
            break;
        }
        if (current_user->getRole() != UserManager::Role::ADMIN) {
            result->setErrno(errcode::ACCESS_DENIED);
            break;
        }

        std::string id_str = request->getParam("id");
        if (id_str.empty()) {
            result->setErrno(errcode::PARAM_MISSING, "param id is required");
            break;
        }
        int64_t id = chen::TypeUtil::Atoi(id_str);
        if (!id) {
            result->setErrno(errcode::PARAM_INVALID, "invalid id");
            break;
        }

        auto info = BackupRecordMgr::GetInstance()->get(id);
        if (!info) {
            result->setErrno(errcode::DB_OPERATION_FAILED, "backup record not found");
            break;
        }

        std::string filepath = info->getFilePath();
        if (filepath.empty()) {
            result->setErrno(errcode::FILE_NOT_FOUND);
            break;
        }

        if (filepath[0] != '/') {
            filepath = server_work_path->getValue() + "/" + filepath;
        }

        std::ifstream ifs(filepath, std::ios::binary);
        if (!ifs) {
            result->setErrno(errcode::FILE_NOT_FOUND);
            break;
        }

        ifs.seekg(0, std::ios::end);
        int64_t fileSize = ifs.tellg();
        ifs.seekg(0, std::ios::beg);

        response->setHeader("Content-Disposition", "attachment; filename=\"" + info->getName() + ".tar.gz\"");
        response->setHeader("Content-Type", "application/octet-stream");
        response->setHeader("Content-Length", std::to_string(fileSize));
        response->setHeader("Cache-Control", "no-cache, no-store, must-revalidate");
        response->setHeader("Pragma", "no-cache");
        response->setHeader("Expires", "0");

        std::string file_content;
        file_content.reserve(fileSize);
        const std::streamsize BUFFER_SIZE = 64 * 1024;
        std::vector<char> buffer(BUFFER_SIZE);
        while (ifs.read(buffer.data(), BUFFER_SIZE) || ifs.gcount() > 0) {
            std::streamsize bytesRead = ifs.gcount();
            file_content.append(buffer.data(), bytesRead);
        }
        if (ifs.bad()) {
            result->setErrno(errcode::FILE_READ_ERROR);
            break;
        }
        response->setBody(file_content);
        success = true;
    } while (0);
    if (!success) {
        response->delHeader("Content-Disposition");
        response->delHeader("Content-Length");
        response->setHeader("Content-Type", "application/json");
        response->setBody(result->toJsonString());
    }
    return 0;
}

} // namespace servlet
} // namespace blog
