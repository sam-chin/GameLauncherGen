# =============================================================================
# Launcher Template 可执行程序构建配置
# =============================================================================
# 依赖：CommonLibrary（CRC32、加密、打包等）
# =============================================================================

cmake_minimum_required(VERSION 3.20)

project(LauncherTemplate
    VERSION 1.0.0
    DESCRIPTION "32位游戏启动器生成器 - 启动器模板"
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

# 可执行程序目标（控制台应用，移除 WIN32 标志）
add_executable(LauncherTemplate
    ${LAUNCHER_HEADERS}
    ${LAUNCHER_SOURCES}
)

# 包含目录配置
target_include_directories(LauncherTemplate
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../common/include
)

# 预处理器定义
target_compile_definitions(LauncherTemplate
    PRIVATE
        UNICODE _UNICODE
        _CRT_SECURE_NO_WARNINGS
        NOMINMAX
        WIN32_LEAN_AND_MEAN
)

# 链接配置
target_link_libraries(LauncherTemplate
    PRIVATE
        CommonLibrary
        kernel32
        user32
        advapi32
)

# 编译选项
target_compile_options(LauncherTemplate
    PRIVATE
        $<$<CONFIG:Debug>:/Od /Zi>
        $<$<CONFIG:Release>:/O2 /Oi /GL>
)

# 链接选项
target_link_options(LauncherTemplate
    PRIVATE
        $<$<CONFIG:Release>:/LTCG>
)

# 输出目录配置
set_target_properties(LauncherTemplate PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# 安装配置
install(TARGETS LauncherTemplate
    RUNTIME DESTINATION bin
)

message(STATUS "=== Launcher Template 构建配置 ===")
message(STATUS "C++ 标准: C++${CMAKE_CXX_STANDARD}")
message(STATUS "目标平台: ${CMAKE_GENERATOR_PLATFORM}")
message(STATUS "=====================================")
