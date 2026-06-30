include(FetchContent)
set(FETCHCONTENT_QUIET OFF)
set(CMAKE_FIND_PACKAGE_PREFER_CONFIG TRUE)

# Need static linking of Boost to avoid end-users having to enable Universe on Ubuntu
set(Boost_USE_STATIC_LIBS ON)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/modules)

set(FC_DEPS)

FetchContent_Declare(
  cppcodec
  URL https://github.com/tplgy/cppcodec/archive/8019b8b580f8573c33c50372baec7039dfe5a8ce.tar.gz
  URL_HASH SHA256=2547d492dfc32bdd3e12674114d52f054efa0b6acbf3bbff679f94ef1cbcf844
  EXCLUDE_FROM_ALL
  SYSTEM
  OVERRIDE_FIND_PACKAGE
)
list(APPEND FC_DEPS cppcodec)

FetchContent_MakeAvailable(${FC_DEPS})
