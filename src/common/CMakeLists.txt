# =============================================================================
# Common Library 静态库构建配置
# =============================================================================
# 构建要求：
#   - CMake 3.20+
#   - Visual Studio 2019+ (C++17)
#   - 目标平台：x86（32位），使用 cmake -A Win32
# =============================================================================

# CMake 最低版本要求（子目录继承根配置，此处不重复调用）
cmake_minimum_required(VERSION 3.20)

# 项目定义
project(CommonLibrary
    VERSION 1.0.0
    DESCRIPTION "32位游戏启动器生成器 - 公共库"
    LANGUAGES CXX
)

# C++ 标准配置
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# MSVC 特定配置
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    add_compile_options(/W4 /MP /wd4251)
endif()

# 源文件定义
set(COMMON_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/CommonTypes.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/CRC32.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/XorCrypto.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/PackagePacker.h
)

set(COMMON_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/CRC32.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/XorCrypto.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/PackagePacker.cpp
)

# 静态库目标
add_library(CommonLibrary STATIC
    ${COMMON_HEADERS}
    ${COMMON_SOURCES}
)

# 包含目录配置
target_include_directories(CommonLibrary
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# 预处理器定义
target_compile_definitions(CommonLibrary
    PRIVATE COMMON_EXPORTS
    PUBLIC UNICODE _UNICODE
)

# Windows 特定链接库
if(WIN32)
    target_link_libraries(CommonLibrary PRIVATE advapi32)
endif()

# 编译选项
target_compile_options(CommonLibrary
    PRIVATE
        $<$<CONFIG:Debug>:/Od /Zi>
        $<$<CONFIG:Release>:/O2 /Oi /GL>
)

# 链接选项
target_link_options(CommonLibrary
    PRIVATE
        $<$<CONFIG:Release>:/LTCG>
)

# 输出目录配置
set_target_properties(CommonLibrary PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# 安装配置
install(TARGETS CommonLibrary
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(FILES ${COMMON_HEADERS}
    DESTINATION include
)

message(STATUS "=== Common Library 构建配置 ===")
message(STATUS "C++ 标准: C++${CMAKE_CXX_STANDARD}")
message(STATUS "目标平台: ${CMAKE_GENERATOR_PLATFORM}")
message(STATUS "================================")
