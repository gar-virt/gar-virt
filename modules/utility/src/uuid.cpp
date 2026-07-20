#include <utility/uuid.hpp>

#ifdef _WIN32
    #include <rpc.h>
    #include <strsafe.h>

    #include <stdexcept>
#else
    #include <uuid/uuid.h>
#endif

namespace ls_gitea_runner::utility {
namespace {
constexpr int uuid_length{36};
}

std::string uuid() {
    std::string uuid_str(uuid_length, '\0');

#ifdef _WIN32
    UUID uuid{};
    bool ok{};
    RPC_CSTR uuid_ucstr{};

    do {
        if (ok = ::UuidCreate(&uuid) == RPC_S_OK; !ok) {
            break;
        }

        if (ok = ::UuidToStringA(&uuid, &uuid_ucstr) == RPC_S_OK; !ok) {
            break;
        }
    } while (false);

    if (uuid_ucstr) {
        ok = SUCCEEDED(
            ::StringCchCopyA(uuid_str.data(), uuid_str.size() + 1, reinterpret_cast<const char*>(uuid_ucstr)));
        ::RpcStringFreeA(&uuid_ucstr);
    }

    if (!ok) {
        throw std::runtime_error{"UUID creation failed"};
    }
#else
    uuid_t uuid{};
    ::uuid_generate(uuid);                       // NOLINT(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
    ::uuid_unparse_lower(uuid, uuid_str.data()); // NOLINT(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
#endif

    return uuid_str;
}

} // namespace ls_gitea_runner::utility
