#include "utility/uuid.hpp"

#include <stdexcept>

#ifdef _WIN32
    #include <rpc.h>
    #include <strsafe.h>
#else
    #include <uuid/uuid.h>
#endif

namespace ls_gitea_runner::utility {

std::string uuid() {
    // 36-byte string + terminator
    char uuid_cstr[36 + 1]{};

#ifdef _WIN32
    UUID uuid{};
    RPC_STATUS status{};
    RPC_CSTR uuid_str{};

    do {
        if ((status = ::UuidCreate(&uuid)) != RPC_S_OK) {
            break;
        }

        if ((status = ::UuidToStringA(&uuid, &uuid_str)) != RPC_S_OK) {
            break;
        }
    } while (false);

    if (uuid_str) {
        ::StringCchCopyA(uuid_cstr, _countof(uuid_cstr), reinterpret_cast<const char*>(uuid_str));
        ::RpcStringFreeA(&uuid_str);
    }

    if (status != RPC_S_OK) {
        throw std::runtime_error{"UUID creation failed"};
    }
#else
    uuid_t uuid{};
    ::uuid_generate(uuid);
    ::uuid_unparse_lower(uuid, uuid_cstr);
#endif

    return {uuid_cstr, uuid_cstr + 36};
}

} // namespace ls_gitea_runner::utility
