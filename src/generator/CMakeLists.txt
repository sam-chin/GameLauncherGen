# =============================================================================
# Generator - 游戏启动器生成器
# =============================================================================
# 类型: MFC 对话框应用程序 (32位)
# 标准: C++17
# 编码: Unicode
# 注意: 使用 CMAKE_MFC_FLAG=2 配合 /MD 共享运行时
#       VS2022 中 CMAKE_MFC_FLAG 与 /MT 不兼容，使用 /MD 是已知可行方案
# =============================================================================

cmake_minimum_required(VERSION 3.20)

project(GameLauncherGenerator
    VERSION 1.0.0.0
    DESCRIPTION "游戏启动器生成器 - MFC对话框应用程序"
    LANGUAGES CXX
)

# C++17 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# MFC 静态链接配置（必须在 add_executable 之前）
set(CMAKE_MFC_FLAG 2)

# 共享运行时库 (/MD) - VS2022 中 CMAKE_MFC_FLAG 与 /MT 不兼容
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL$<$<CONFIG:Debug>:Debug>")

# 禁用 Windows.h 的 min/max 宏
add_definitions(-DNOMINMAX)

# 源文件
set(GENERATOR_HEADERS
    include/GeneratorApp.h
    include/MainDlg.h
    include/ResourceEmbedder.h
    include/PatchBuilder.h
    include/ConfigData.h
)

set(GENERATOR_SOURCES
    src/GeneratorApp.cpp
    src/MainDlg.cpp
    src/ResourceEmbedder.cpp
    src/PatchBuilder.cpp
    src/ConfigData.cpp
)

set(GENERATOR_RESOURCES
    res/Generator.rc
)

# 可执行文件目标
add_executable(${PROJECT_NAME} WIN32
    ${GENERATOR_HEADERS}
    ${GENERATOR_SOURCES}
    ${GENERATOR_RESOURCES}
)

# 包含目录
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/res
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/include
)

# 编译选项
target_compile_options(${PROJECT_NAME} PRIVATE
    /W4
    /MP
    /EHsc
    /GR
    /Zc:wchar_t
    /permissive-
)

# Release 优化
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2 /Ob2 /Oi /Ot /GL")
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG")

# Debug 模式
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Zi /Od /RTC1")

# 链接库
target_link_libraries(${PROJECT_NAME} PRIVATE
    CommonLibrary
    kernel32
    user32
    gdi32
    comdlg32
    advapi32
    shell32
    ole32
    oleaut32
    uuid
    comctl32
    ws2_32
    wininet
    version
    gdiplus
    shlwapi
    htmlhelp
)

# 资源编译器包含路径
set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} /I\"${CMAKE_CURRENT_SOURCE_DIR}/include\" /I\"${CMAKE_CURRENT_SOURCE_DIR}/res\"")

# 输出目录
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin/Debug
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin/Release
)

# 安装目标
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)

message(STATUS "========================================")
message(STATUS "项目: ${PROJECT_NAME}")
message(STATUS "版本: ${PROJECT_VERSION}")
message(STATUS "MFC模式: 静态链接 (CMAKE_MFC_FLAG=2)")
message(STATUS "运行时: /MD (共享)")
message(STATUS "========================================")
