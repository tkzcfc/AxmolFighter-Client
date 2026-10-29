# 收集 Source/3rd/vfs 运行时源码，不单独打库。
# 客户端 GLOB 已包含这些文件，include 本文件只为标 LANGUAGE / char8_t。
# 战斗服与 TestsHeadless 把 ${VFS_SOURCES} 加进目标。
set(VFS_ROOT "${CMAKE_CURRENT_LIST_DIR}/../Source/3rd/vfs")
file(TO_CMAKE_PATH "${VFS_ROOT}" VFS_ROOT)

if(NOT EXISTS "${VFS_ROOT}/VirtualFileSystem.h")
  message(FATAL_ERROR "Vendored VFS runtime missing at '${VFS_ROOT}'.")
endif()

file(GLOB_RECURSE VFS_SOURCES CONFIGURE_DEPENDS
  "${VFS_ROOT}/*.cpp"
  "${VFS_ROOT}/*.c"
)
file(GLOB_RECURSE VFS_HEADERS CONFIGURE_DEPENDS
  "${VFS_ROOT}/*.h"
)

file(GLOB VFS_LZ4_C_SOURCES CONFIGURE_DEPENDS "${VFS_ROOT}/3rd/lz4/*.c")
set_source_files_properties(${VFS_LZ4_C_SOURCES} PROPERTIES LANGUAGE C)

file(GLOB_RECURSE VFS_CPP_SOURCES CONFIGURE_DEPENDS "${VFS_ROOT}/*.cpp")
# axmol 全局关了 char8_t；VFS native 路径转换需要 std::u8string。第三方源码关掉告警。
if(MSVC)
  set_property(SOURCE ${VFS_CPP_SOURCES} APPEND PROPERTY COMPILE_OPTIONS /Zc:char8_t /W0)
  set_property(SOURCE ${VFS_LZ4_C_SOURCES} APPEND PROPERTY COMPILE_OPTIONS /W0)
else()
  set_property(SOURCE ${VFS_CPP_SOURCES} APPEND PROPERTY COMPILE_OPTIONS -fchar8_t -w)
  set_property(SOURCE ${VFS_LZ4_C_SOURCES} APPEND PROPERTY COMPILE_OPTIONS -w)
endif()
if(UNIX)
  set_property(SOURCE ${VFS_SOURCES} APPEND PROPERTY COMPILE_DEFINITIONS _FILE_OFFSET_BITS=64)
endif()
