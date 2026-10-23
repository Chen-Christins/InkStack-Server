#include "base64url.h"

#include <chen/util/string_util.h>

namespace blog {
namespace webauthn {

std::string Base64UrlEncode(const std::string& data)
{
    if (data.empty()) {
        return "";
    }

    std::string out = chen::StringUtil::Base64Encode(data);
    for (char& c : out) {
        if (c == '+') {
            c = '-';
        } else if (c == '/') {
            c = '_';
        }
    }
    while (!out.empty() && out.back() == '=') {
        out.pop_back();
    }
    return out;
}

bool Base64UrlDecode(const std::string& input, std::string& output)
{
    output.clear();
    if (input.empty()) {
        return true;
    }

    std::string data = input;
    for (char& c : data) {
        if (c == '-') {
            c = '+';
        } else if (c == '_') {
            c = '/';
        }
    }
    while (data.size() % 4 != 0) {
        data.push_back('=');
    }

    output = chen::StringUtil::Base64Decode(data);
    return !output.empty() || input.empty();
}

} // namespace webauthn
} // namespace blog
