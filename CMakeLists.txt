# =============================================================================
# GameLauncherGen - 游戏登录器生成器
# 根 CMake 配置文件
# =============================================================================
# 支持平台: Windows x86 (32位)
# 编译器: MSVC 2019 或更高版本
# 标准: C++17
# =============================================================================

cmake_minimum_required(VERSION 3.20)

# 项目基本信息
project(GameLauncherGen
    VERSION 1.0.0
    DESCRIPTION "32位游戏登录器生成器"
    LANGUAGES CXX
)

# 强制 32 位平台配置
if(NOT CMAKE_GENERATOR_PLATFORM)
    set(CMAKE_GENERATOR_PLATFORM "Win32" CACHE STRING "构建平台" FORCE)
endif()

if(NOT CMAKE_GENERATOR_PLATFORM STREQUAL "Win32")
    message(FATAL_ERROR "本项目仅支持 Win32 (x86) 平台。请使用: cmake -A Win32")
endif()

# C++ 标准配置
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# MSVC 特定编译选项
if(MSVC)
    add_compile_options(/MP /W4 /permissive- /Zc:__cplusplus)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    add_compile_definitions(UNICODE _UNICODE)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    add_compile_definitions(_SCL_SECURE_NO_WARNINGS)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2 /GL /Gy")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG /OPT:REF /OPT:ICF")
    set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /LTCG /OPT:REF /OPT:ICF")
endif()

# 全局包含目录
set(COMMON_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/common/include)

# 子项目目录（构建顺序由依赖关系自动处理）
add_subdirectory(src/common)
add_subdirectory(src/launcher_template)
add_subdirectory(src/extension_dll)
add_subdirectory(src/generator)

# 安装规则
install(TARGETS GameLauncherGenerator
    RUNTIME DESTINATION bin
)

install(TARGETS LauncherTemplate
    RUNTIME DESTINATION template
)

install(TARGETS ExtensionDLL
    RUNTIME DESTINATION template
)

# 打包配置 (CPack)
set(CPACK_PACKAGE_NAME "GameLauncherGen")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_VENDOR "GameLauncherGen")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "32位游戏登录器生成器")
set(CPACK_GENERATOR "ZIP")
include(CPack)
