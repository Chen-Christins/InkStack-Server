#include "error_code_manager.h"

#include <fstream>

#include <chen/config/config.h>
#include <chen/log/log.h>
#include <json/json.h>

namespace blog {

static chen::Logger::ptr logger = LOG_ROOT();

ErrorCodeManager::ErrorCodeManager()
{
}

bool ErrorCodeManager::load(const std::string& configPath)
{
    m_configPath = configPath;

    std::ifstream ifs(configPath);
    if (!ifs.is_open()) {
        ERROR(logger) << "Failed to open error codes config: " << configPath;
        return false;
    }

    Json::Value root;
    Json::CharReaderBuilder builder;
    std::string errs;
    if (!Json::parseFromStream(builder, ifs, &root, &errs)) {
        ERROR(logger) << "Failed to parse error codes JSON: " << errs;
        return false;
    }
    ifs.close();

    m_version = root.get("version", "unknown").asString();

    m_zhMessages.clear();
    m_enMessages.clear();

    const Json::Value& errors = root["errors"];
    if (!errors.isArray()) {
        ERROR(logger) << "errors.json: 'errors' field is not an array";
        return false;
    }

    for (const auto& item : errors) {
        int32_t code = item["errno"].asInt();
        std::string zh = item.get("zh", "").asString();
        std::string en = item.get("en", "").asString();

        m_zhMessages[code] = zh;
        m_enMessages[code] = en;
    }

    INFO(logger) << "ErrorCodeManager loaded " << m_zhMessages.size() << " error codes from " << configPath << " (version: " << m_version << ")";
    return true;
}

bool ErrorCodeManager::reload()
{
    if (m_configPath.empty()) {
        ERROR(logger) << "ErrorCodeManager: no config path, cannot reload";
        return false;
    }
    return load(m_configPath);
}

std::string ErrorCodeManager::getMessage(int32_t errCode) const
{
    // 默认使用中文
    return getMessage(errCode, "zh");
}

std::string ErrorCodeManager::getMessage(int32_t errCode, const std::string& lang) const
{
    const auto& messages = (lang == "en" || lang == "en-US" || lang == "en_US") ? m_enMessages : m_zhMessages;
    auto it = messages.find(errCode);
    if (it != messages.end()) {
        return it->second;
    }
    return lang == "en" ? "Unknown error" : "未知错误";
}

std::map<int32_t, std::string> ErrorCodeManager::getAllMessages(const std::string& lang) const
{
    return (lang == "en" || lang == "en-US" || lang == "en_US") ? m_enMessages : m_zhMessages;
}

std::string ErrorCodeManager::toJson(const std::string& lang) const
{
    Json::Value result;
    const auto& messages = getAllMessages(lang);
    for (const auto& pair : messages) {
        result[std::to_string(pair.first)] = pair.second;
    }
    return chen::JsonUtil::ToString(result);
}

} // namespace blog
