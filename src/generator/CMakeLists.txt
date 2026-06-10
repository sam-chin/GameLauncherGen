# ============================================================================
# CMakeLists.txt - 游戏启动器生成器
# ============================================================================
# 项目: GameLauncherGenerator
# 类型: MFC对话框应用程序 (32位, 静态链接)
# 标准: C++17
# 编码: Unicode
# ============================================================================

cmake_minimum_required(VERSION 3.20)

# 项目名称和版本
project(GameLauncherGenerator
    VERSION 1.0.0.0
    DESCRIPTION "游戏启动器生成器 - MFC对话框应用程序"
    LANGUAGES CXX
)

# ============================================================================
# 编译器设置
# ============================================================================

# 强制32位构建
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /arch:IA32")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /arch:IA32")

# C++17标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 静态运行时库 (/MT)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# MFC静态链接
set(CMAKE_MFC_FLAG 1)
add_definitions(-D_AFXDLL)

# Unicode编码
add_definitions(-DUNICODE -D_UNICODE)

# Windows平台定义
add_definitions(-DWIN32 -D_WINDOWS)
add_definitions(-DVC_EXTRALEAN -DWIN32_LEAN_AND_MEAN)

# ============================================================================
# 源文件
# ============================================================================

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

# ============================================================================
# 可执行文件目标
# ============================================================================

add_executable(${PROJECT_NAME} WIN32
    ${GENERATOR_HEADERS}
    ${GENERATOR_SOURCES}
    ${GENERATOR_RESOURCES}
)

# 设置目标属性
target_compile_definitions(${PROJECT_NAME} PRIVATE
    _AFX_NO_MFC_CONTROLS_IN_DIALOGS
    _CRT_SECURE_NO_WARNINGS
)

# 包含目录
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/res
)

# ============================================================================
# 编译选项
# ============================================================================

# 生产级编译选项
target_compile_options(${PROJECT_NAME} PRIVATE
    /W4           # 警告级别4
    /WX-          # 不将警告视为错误
    /MP           # 多处理器编译
    /EHsc         # C++异常处理
    /GR           # 启用运行时类型信息
    /Zc:wchar_t   # 将wchar_t视为内置类型
    /Zc:forScope  # 强制for循环作用域一致性
    /Zc:inline    # 移除未使用的内联函数
    /nologo       # 取消显示版权信息
    /permissive-  # 严格标准一致性模式
)

# 发布模式优化
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2 /Ob2 /Oi /Ot /GL")
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG")

# 调试模式
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Zi /Od /RTC1")

# ============================================================================
# 链接库
# ============================================================================

target_link_libraries(${PROJECT_NAME} PRIVATE
    # Windows核心库
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
    
    # GDI+
    gdiplus
    
    # zlib (用于压缩)
    zlibstatic
)

# ============================================================================
# 资源编译设置
# ============================================================================

# 设置资源编译器包含路径
set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} /I\"${CMAKE_CURRENT_SOURCE_DIR}/include\" /I\"${CMAKE_CURRENT_SOURCE_DIR}/res\"")

# ============================================================================
# 安装和输出
# ============================================================================

# 设置输出目录
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin/Debug
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin/Release
)

# 安装目标
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)

# ============================================================================
# 预编译头 (可选)
# ============================================================================

# 如果使用预编译头，取消下面的注释
# target_precompile_headers(${PROJECT_NAME} PRIVATE
#     include/stdafx.h
# )

# ============================================================================
# 版本信息
# ============================================================================

set_target_properties(${PROJECT_NAME} PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

# ============================================================================
# 消息输出
# ============================================================================

message(STATUS "========================================")
message(STATUS "项目: ${PROJECT_NAME}")
message(STATUS "版本: ${PROJECT_VERSION}")
message(STATUS "平台: ${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "编译器: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "C++标准: C++${CMAKE_CXX_STANDARD}")
message(STATUS "MFC模式: 静态链接")
message(STATUS "输出目录: ${CMAKE_BINARY_DIR}/bin")
message(STATUS "========================================")
