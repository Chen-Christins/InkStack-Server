/**
 * @file base64url.h
 * @brief WebAuthn 使用的 base64url 编解码
 * @author Christins
 * @date 2026-10-23
 * @copyright Apache 2.0
 */
#pragma once

#include <string>

namespace blog {
namespace webauthn {

/**
 * @brief base64url 编码（无 padding，URL 安全字符集）
 */
std::string Base64UrlEncode(const std::string& data);

/**
 * @brief base64url 解码
 * @param input base64url 字符串，可带/不带 padding
 * @param output 解码后的原始字节
 * @return 是否成功
 */
bool Base64UrlDecode(const std::string& input, std::string& output);

} // namespace webauthn
} // namespace blog
