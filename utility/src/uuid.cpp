#include "utility/uuid.hpp"

#include <stdexcept>

#ifdef _WIN32
#include <rpc.h>
#include <strsafe.h>
#else
#include <uuid/uuid.h>
#endif

namespace utility {

auto uuid() -> std::string {
  // 36-byte string + terminator
  char uuidCString[36 + 1]{};

#ifdef _WIN32
  UUID uuid{};

  RPC_STATUS status{};
  RPC_CSTR uuidStr{};

  do {
    if ((status = ::UuidCreate(&uuid)) != RPC_S_OK) {
      break;
    }

    if ((status = ::UuidToStringA(&uuid, &uuidStr)) != RPC_S_OK) {
      break;
    }
  } while (false);

  if (uuidStr) {
    ::StringCchCopyA(uuidCString, _countof(uuidCString),
                     reinterpret_cast<const char *>(uuidStr));
    ::RpcStringFreeA(&uuidStr);
  }

  if (status != RPC_S_OK) {
    throw std::runtime_error{"UUID creation failed"};
  }
#else
  uuid_t uuid{};
  ::uuid_generate(uuid);
  ::uuid_unparse_lower(uuid, uuidCString);
#endif

  return {uuidCString, uuidCString + 36};
}

} // namespace utility
