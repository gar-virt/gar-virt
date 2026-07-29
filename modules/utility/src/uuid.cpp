#include <utility/uuid.hpp>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/version.hpp>

#if BOOST_VERSION >= 109000 // Boost 1.90+:
    #include <boost/uuid/generators.hpp>
#else
    #include <boost/uuid/uuid_generators.hpp>
#endif

#include <random>

// A notable implementation where std::random_device is deterministic in old versions of MinGW-w64 (bug 338, fixed since
// GCC 9.2). The latest MinGW-w64 versions can be downloaded from GCC with the MCF thread model.
// Source: https://en.cppreference.com/cpp/numeric/random/random_device
// Bug: https://sourceforge.net/p/mingw-w64/bugs/338/
// Git commit: 0e7ffed96cfdde1f9c37fb9305785b507141047b (git://gcc.gnu.org/git/gcc.git)

#if defined(__MINGW32__) && ((__GNUC__ < 9) || (__GNUC__ == 9 && __GNUC_MINOR__ < 2))
    #error "MinGW GCC < 9.2 has deterministic std::random_device (PR libstdc++/85494). Please upgrade your toolchain."
#endif

namespace gv::utility {

std::string uuid() {
#if BOOST_VERSION >= 108600 // Boost 1.86+:
    // random_generator uses a cryptographically strong pseudorandom number generator (ChaCha20/12), seeded with entropy
    // from std::random_device.
    using random_generator = boost::uuids::random_generator;
#else
    // Fallback: Source random bytes from std::random_device. Discouraged both for performance reasons and because it
    // isn't guaranteed to produce non-deterministic numbers when a non-deterministic source is unavailable.
    using random_generator = boost::uuids::basic_random_generator<std::random_device>;
#endif
    thread_local random_generator rng; // NOLINT(misc-use-internal-linkage): False positive
    return boost::uuids::to_string(rng());
}

} // namespace gv::utility
