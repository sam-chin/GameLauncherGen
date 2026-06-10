/**
 * @file CMakeLists.txt
 * @brief Common Library 静态库构建配置
 * @details 本CMake配置文件用于构建32位游戏启动器生成器的公共库静态库。
 *          配置针对MSVC编译器优化，支持x86架构，使用C++17标准。
 * 
 *          构建要求：
 *          - CMake 3.14 或更高版本
 *          - Visual Studio 2019 或更高版本（支持C++17）
 *          - Windows SDK
 *          - 目标平台：x86（32位）
 * 
 *          使用方法：
 *          mkdir build && cd build
 *          cmake .. -A Win32
 *          cmake --build . --config Release
 */

# ---------------------------------------------------------------------------
# CMake 最低版本要求
# ---------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.14)

# ---------------------------------------------------------------------------
# 项目定义
# ---------------------------------------------------------------------------
project(CommonLibrary
    VERSION 1.0.0
    DESCRIPTION "32位游戏启动器生成器 - 公共库"
    LANGUAGES CXX
)

# ---------------------------------------------------------------------------
# C++ 标准配置
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)  # 不使用编译器扩展，保证可移植性

# ---------------------------------------------------------------------------
# MSVC 特定配置
# ---------------------------------------------------------------------------
if(MSVC)
    # 使用多线程静态运行时库（/MT 或 /MTd）
    # 静态库通常使用静态运行时，避免与使用者的运行时冲突
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

    # 编译器警告级别
    add_compile_options(/W4)  # 最高警告级别

    # 将警告视为错误（可选，生产环境建议开启）
    # add_compile_options(/WX)

    # 禁用特定警告（根据需要）
    # C4251: 类需要使用DLL接口导出（使用PIMPL时常见，静态库可忽略）
    add_compile_options(/wd4251)

    # 启用并行编译
    add_compile_options(/MP)

    # 启用增强指令集（SSE2是x86的标准配置）
    add_compile_options(/arch:SSE2)
endif()

# ---------------------------------------------------------------------------
# 源文件定义
# ---------------------------------------------------------------------------
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

# ---------------------------------------------------------------------------
# 静态库目标
# ---------------------------------------------------------------------------
add_library(CommonLibrary STATIC
    ${COMMON_HEADERS}
    ${COMMON_SOURCES}
)

# ---------------------------------------------------------------------------
# 目标属性配置
# ---------------------------------------------------------------------------
# 设置目标平台为32位（如果未通过-A Win32指定）
if(CMAKE_GENERATOR_PLATFORM STREQUAL "x64" OR CMAKE_GENERATOR_PLATFORM STREQUAL "Win64")
    message(WARNING "当前配置为64位平台，建议添加 -A Win32 参数指定32位平台")
endif()

# 包含目录配置
target_include_directories(CommonLibrary
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# 预处理器定义
target_compile_definitions(CommonLibrary
    PRIVATE
        COMMON_EXPORTS  # 虽然构建静态库，但保留导出宏兼容性
    PUBLIC
        UNICODE
        _UNICODE
)

# ---------------------------------------------------------------------------
# Windows 特定链接库
# ---------------------------------------------------------------------------
if(WIN32)
    target_link_libraries(CommonLibrary
        PRIVATE
            advapi32  # SecureZeroMemory等安全函数
    )
endif()

# ---------------------------------------------------------------------------
# 编译选项
# ---------------------------------------------------------------------------
target_compile_options(CommonLibrary
    PRIVATE
        $<$<CONFIG:Debug>:/Od /Zi>           # Debug: 禁用优化，生成调试信息
        $<$<CONFIG:Release>:/O2 /Oi /GL>     # Release: 最大优化，启用全程序优化
)

# ---------------------------------------------------------------------------
# 链接选项
# ---------------------------------------------------------------------------
target_link_options(CommonLibrary
    PRIVATE
        $<$<CONFIG:Release>:/LTCG>           # Release: 链接时代码生成
)

# ---------------------------------------------------------------------------
# 输出目录配置
# ---------------------------------------------------------------------------
set_target_properties(CommonLibrary PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# ---------------------------------------------------------------------------
# 安装配置（可选）
# ---------------------------------------------------------------------------
install(TARGETS CommonLibrary
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(FILES ${COMMON_HEADERS}
    DESTINATION include
)

# ---------------------------------------------------------------------------
# 测试目标（可选）
# ---------------------------------------------------------------------------
option(BUILD_TESTS "构建单元测试" OFF)

if(BUILD_TESTS)
    enable_testing()
    # 测试目标将在单独文件中定义
    # add_subdirectory(tests)
endif()

# ---------------------------------------------------------------------------
# 构建信息输出
# ---------------------------------------------------------------------------
message(STATUS "")
message(STATUS "=== Common Library 构建配置 ===")
message(STATUS "项目版本: ${PROJECT_VERSION}")
message(STATUS "C++ 标准: ${CMAKE_CXX_STANDARD}")
message(STATUS "编译器: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "目标平台: ${CMAKE_GENERATOR_PLATFORM}")
message(STATUS "构建类型: ${CMAKE_BUILD_TYPE}")
message(STATUS "MSVC 运行时: ${CMAKE_MSVC_RUNTIME_LIBRARY}")
message(STATUS "================================")
message(STATUS "")
