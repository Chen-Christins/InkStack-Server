/**
 * @file cose.h
 * @brief WebAuthn COSE_Key 解析
 * @author Christins
 * @date 2026-10-23
 * @copyright Apache 2.0
 */
#pragma once

#include <memory>
#include <string>

#include <openssl/evp.h>

namespace blog {
namespace webauthn {

/**
 * @brief 解析后的 COSE 公钥
 */
struct CoseKey {
    int32_t kty = 0;
    int32_t alg = 0;
    int32_t crv = 0;
    std::shared_ptr<EVP_PKEY> pkey;
};

/**
 * @brief 解析 COSE_Key（CBOR 字节串）
 * @param cbor 原始 CBOR
 * @param out 输出公钥
 * @return 是否成功
 */
bool ParseCoseKey(const std::string& cbor, CoseKey& out);

} // namespace webauthn
} // namespace blog
