/**
 * @file CMakeLists.txt
 * @brief Launcher Template 可执行程序构建配置
 * @details 本CMake配置文件用于构建32位游戏启动器生成器的核心启动器模板。
 *          配置针对MSVC编译器优化，支持x86架构，使用C++17标准。
 *
 *          构建要求:
 *          - CMake 3.14 或更高版本
 *          - Visual Studio 2019 或更高版本（支持C++17）
 *          - Windows SDK
 *          - 目标平台：x86（32位）
 *
 *          使用方法:
 *          mkdir build && cd build
 *          cmake .. -A Win32
 *          cmake --build . --config Release
 *
 *          依赖关系:
 *          - CommonLibrary: 公共库（CRC32、加密、打包等）
 */

# ---------------------------------------------------------------------------
# CMake 最低版本要求
# ---------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.14)

# ---------------------------------------------------------------------------
# 项目定义
# ---------------------------------------------------------------------------
project(LauncherTemplate
    VERSION 1.0.0
    DESCRIPTION "32位游戏启动器生成器 - 启动器模板"
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
    # 静态运行时避免目标机器缺少VC++运行库的问题
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

    # 编译器警告级别
    add_compile_options(/W4)  # 最高警告级别

    # 将警告视为错误（可选，生产环境建议开启）
    # add_compile_options(/WX)

    # 禁用特定警告
    # C4251: 类需要使用DLL接口导出（使用PIMPL时常见，静态库可忽略）
    add_compile_options(/wd4251)

    # 启用并行编译，加速构建
    add_compile_options(/MP)

    # 启用增强指令集（SSE2是x86的标准配置）
    add_compile_options(/arch:SSE2)
endif()

# ---------------------------------------------------------------------------
# 源文件定义
# ---------------------------------------------------------------------------
set(LAUNCHER_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ServerConfig.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/LauncherUtils.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/DllInjector.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ProcessLauncher.h
)

set(LAUNCHER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ServerConfig.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/LauncherUtils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/DllInjector.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ProcessLauncher.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Main.cpp
)

# ---------------------------------------------------------------------------
# 公共库依赖
# ---------------------------------------------------------------------------
# 添加CommonLibrary子目录（如果尚未添加）
if(NOT TARGET CommonLibrary)
    # 检查CommonLibrary是否存在
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../common/CMakeLists.txt)
        add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../common
                         ${CMAKE_BINARY_DIR}/common)
    else()
        message(FATAL_ERROR "找不到CommonLibrary。请确保common目录存在。")
    endif()
endif()

# ---------------------------------------------------------------------------
# 可执行程序目标
# ---------------------------------------------------------------------------
add_executable(LauncherTemplate
    ${LAUNCHER_HEADERS}
    ${LAUNCHER_SOURCES}
)

# ---------------------------------------------------------------------------
# 目标属性配置
# ---------------------------------------------------------------------------
# 设置目标平台为32位（如果未通过-A Win32指定）
if(CMAKE_GENERATOR_PLATFORM STREQUAL "x64" OR CMAKE_GENERATOR_PLATFORM STREQUAL "Win64")
    message(WARNING "当前配置为64位平台，建议添加 -A Win32 参数指定32位平台")
endif()

# 包含目录配置
# PRIVATE: 仅本目标使用，不传递给依赖本目标的其他目标
target_include_directories(LauncherTemplate
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../common/include
)

# 预处理器定义
target_compile_definitions(LauncherTemplate
    PRIVATE
        UNICODE
        _UNICODE
        _CRT_SECURE_NO_WARNINGS  # 禁用MSVC的"安全"函数警告（如strcpy）
        NOMINMAX                 # 禁用Windows.h中的min/max宏，避免与STL冲突
        WIN32_LEAN_AND_MEAN      # 精简Windows.h，减少编译时间和冲突
)

# ---------------------------------------------------------------------------
# 链接配置
# ---------------------------------------------------------------------------
# 链接CommonLibrary
target_link_libraries(LauncherTemplate
    PRIVATE
        CommonLibrary
)

# Windows特定链接库
target_link_libraries(LauncherTemplate
    PRIVATE
        kernel32    # Windows核心API（进程、线程、内存管理）
        user32      # 用户界面API
        advapi32    # 高级Windows API（安全、注册表）
)

# ---------------------------------------------------------------------------
# 编译选项
# ---------------------------------------------------------------------------
target_compile_options(LauncherTemplate
    PRIVATE
        # Debug配置: 禁用优化，生成完整调试信息
        $<$<CONFIG:Debug>:/Od /Zi>
        # Release配置: 最大优化，启用内部函数，全程序优化
        $<$<CONFIG:Release>:/O2 /Oi /GL>
)

# ---------------------------------------------------------------------------
# 链接选项
# ---------------------------------------------------------------------------
target_link_options(LauncherTemplate
    PRIVATE
        # Release配置: 链接时代码生成，进一步优
        $<$<CONFIG:Release>:/LTCG>
        # 设置子系统为控制台（/SUBSYSTEM:CONSOLE）
        /SUBSYSTEM:CONSOLE
)

# ---------------------------------------------------------------------------
# 输出目录配置
# ---------------------------------------------------------------------------
set_target_properties(LauncherTemplate PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# ---------------------------------------------------------------------------
# 安装配置（可选）
# ---------------------------------------------------------------------------
install(TARGETS LauncherTemplate
    RUNTIME DESTINATION bin
)

# ---------------------------------------------------------------------------
# 构建信息输出
# ---------------------------------------------------------------------------
message(STATUS "")
message(STATUS "=== Launcher Template 构建配置 ===")
message(STATUS "项目版本: ${PROJECT_VERSION}")
message(STATUS "C++ 标准: ${CMAKE_CXX_STANDARD}")
message(STATUS "编译器: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "目标平台: ${CMAKE_GENERATOR_PLATFORM}")
message(STATUS "构建类型: ${CMAKE_BUILD_TYPE}")
message(STATUS "MSVC 运行时: ${CMAKE_MSVC_RUNTIME_LIBRARY}")
message(STATUS "=====================================")
message(STATUS "")
