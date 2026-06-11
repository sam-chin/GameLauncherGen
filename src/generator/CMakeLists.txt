# =============================================================================
# Generator - 游戏启动器生成器
# =============================================================================
# 类型: MFC 对话框应用程序 (32位, 静态链接)
# 标准: C++17
# 编码: Unicode
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

# 静态运行时库 (/MT) - 必须在 MFC 配置之前
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# MFC 静态链接配置
# 注意: CMAKE_MFC_FLAG 2 表示静态链接，但 CMake 仍可能添加 _AFXDLL
# 我们使用 /U_AFXDLL 强制取消该定义
set(CMAKE_MFC_FLAG 2)

# Unicode 编码
add_definitions(-DUNICODE -D_UNICODE)

# Windows 平台定义
add_definitions(-DWIN32 -D_WINDOWS)
add_definitions(-D_AFX_NO_MFC_CONTROLS_IN_DIALOGS)
add_definitions(-D_CRT_SECURE_NO_WARNINGS)
add_definitions(-DVC_EXTRALEAN)
add_definitions(-DWIN32_LEAN_AND_MEAN)

# 禁用 Windows.h 的 min/max 宏，避免与 std::min/max 冲突
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

# 编译选项 - 关键: /U_AFXDLL 取消 _AFXDLL 定义，强制使用静态 MFC
target_compile_options(${PROJECT_NAME} PRIVATE
    /W4
    /MP
    /EHsc
    /GR
    /Zc:wchar_t
    /permissive-
    /U_AFXDLL           # 强制取消 _AFXDLL 定义，使用静态 MFC
)

# Release 优化
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2 /Ob2 /Oi /Ot /GL")
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG")

# Debug 模式
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Zi /Od /RTC1")

# MFC 静态链接链接器选项：排除冲突的默认库
# uafxcwd.lib (Unicode MFC Debug Static) 与 afxnmcdd.lib (Unicode MFC Debug Static No Control Containers)
# 可能冲突，使用 /NODEFAULTLIB 排除
target_link_options(${PROJECT_NAME} PRIVATE
    $<$<CONFIG:Debug>:/NODEFAULTLIB:uafxcwd.lib>
    $<$<CONFIG:Debug>:/NODEFAULTLIB:afxnmcdd.lib>
    $<$<CONFIG:Release>:/NODEFAULTLIB:uafxcw.lib>
    $<$<CONFIG:Release>:/NODEFAULTLIB:afxnmcw.lib>
)

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
message(STATUS "MFC模式: 静态链接")
message(STATUS "========================================")
