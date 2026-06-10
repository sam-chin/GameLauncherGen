# =============================================================================
# Extension DLL 模块构建配置
# =============================================================================
# 32位 Win32 动态链接库，使用 .def 文件控制导出符号
# 完全独立，不依赖主程序的任何源代码或头文件
# =============================================================================

cmake_minimum_required(VERSION 3.20)

project(ExtensionDLL
    VERSION 1.0.0
    DESCRIPTION "游戏启动器扩展DLL模块 - 32位Win32动态链接库"
    LANGUAGES CXX
)

# C++ 标准配置
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 源文件定义
set(EXTENSION_DLL_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ExtensionAPI.h
)

set(EXTENSION_DLL_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/DllMain.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ExtensionAPI.cpp
)

# 模块定义文件（控制导出符号）
set(EXTENSION_DLL_DEF
    ${CMAKE_CURRENT_SOURCE_DIR}/ExtensionDLL.def
)

# 创建共享库（DLL）目标
add_library(${PROJECT_NAME} SHARED
    ${EXTENSION_DLL_SOURCES}
    ${EXTENSION_DLL_HEADERS}
    ${EXTENSION_DLL_DEF}
)

# 包含目录
target_include_directories(${PROJECT_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# 编译定义
target_compile_definitions(${PROJECT_NAME}
    PRIVATE
        EXTENSION_DLL_BUILD
        UNICODE
        _UNICODE
        WINVER=0x0601
        _WIN32_WINNT=0x0601
        _CRT_SECURE_NO_WARNINGS
    PUBLIC
        EXTENSION_DLL_UNICODE
)

# MSVC 专用编译选项
if(MSVC)
    target_compile_options(${PROJECT_NAME}
        PRIVATE
            /MP /W4
            /EHsc /GR
            /wd4100 /wd4505
    )

    # 运行时库设置：静态多线程（/MT）
    set_property(TARGET ${PROJECT_NAME} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")

    # 链接器选项
    target_link_options(${PROJECT_NAME}
        PRIVATE
            /DEBUG
            /INCREMENTAL
    )
endif()

# 输出属性
set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_NAME "ExtensionDLL"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)

# 安装规则
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(FILES ${EXTENSION_DLL_HEADERS}
    DESTINATION include
)

message(STATUS "=== Extension DLL 构建配置 ===")
message(STATUS "C++ 标准: C++${CMAKE_CXX_STANDARD}")
message(STATUS "目标平台: ${CMAKE_GENERATOR_PLATFORM}")
message(STATUS "=================================")
