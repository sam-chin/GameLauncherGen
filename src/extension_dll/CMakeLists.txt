/**
 * @file CMakeLists.txt
 * @brief 扩展DLL模块构建配置
 * @details 使用CMake构建系统生成标准的Win32 DLL（x86架构）。
 *          本配置遵循以下设计原则：
 *          - 严格限定为32位构建（强制使用x86工具链）
 *          - 使用模块定义文件(.def)控制导出符号
 *          - 启用Unicode字符集（UNICODE / _UNICODE宏）
 *          - 使用静态运行时库（/MT）避免对VC++运行时的依赖
 *          - 完全独立，不依赖主程序的任何源代码或头文件
 *
 * @note 要求CMake版本 >= 3.16，以支持现代目标属性设置和预编译头等功能。
 * @warning 本DLL必须与主程序使用相同的运行时库类型（/MT或/MD），否则可能导致堆损坏。
 *
 * @version 1.0.0
 * @date 2026-06-10
 */

# ========================== CMake最低版本要求 ==========================
cmake_minimum_required(VERSION 3.16)

# ========================== 项目名称与版本 ==========================
project(ExtensionDLL
    VERSION 1.0.0
    DESCRIPTION "游戏启动器扩展DLL模块 - 32位Win32动态链接库"
    LANGUAGES CXX
)

# ========================== 全局编译设置 ==========================

# 强制使用C++17标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)  # 禁用编译器扩展，确保代码可移植性

# 强制32位构建（x86架构）
# 即使主机为64位系统，也生成32位DLL
if(CMAKE_GENERATOR_PLATFORM)
    set(CMAKE_GENERATOR_PLATFORM Win32 CACHE STRING "目标平台" FORCE)
endif()

# 确保使用x86工具链（适用于Visual Studio生成器）
if(MSVC)
    set(CMAKE_VS_PLATFORM_NAME "Win32" CACHE STRING "Visual Studio目标平台" FORCE)
endif()

# ========================== 源文件定义 ==========================

# 头文件（用于IDE项目组织和安装规则）
set(EXTENSION_DLL_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ExtensionAPI.h
)

# 实现源文件
set(EXTENSION_DLL_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/DllMain.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ExtensionAPI.cpp
)

# 模块定义文件（控制导出符号）
set(EXTENSION_DLL_DEF
    ${CMAKE_CURRENT_SOURCE_DIR}/ExtensionDLL.def
)

# ========================== 目标定义 ==========================

# 创建共享库（DLL）目标
add_library(${PROJECT_NAME} SHARED
    ${EXTENSION_DLL_SOURCES}
    ${EXTENSION_DLL_HEADERS}
    ${EXTENSION_DLL_DEF}
)

# ========================== 包含目录 ==========================

target_include_directories(${PROJECT_NAME}
    PUBLIC
        # 公共头文件目录：主程序包含此头文件时需要的路径
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        # 私有包含目录：仅编译本DLL时使用
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# ========================== 编译定义 ==========================

target_compile_definitions(${PROJECT_NAME}
    PRIVATE
        # 标记正在编译DLL本身，使ExtensionAPI.h导出符号
        EXTENSION_DLL_BUILD
        # 启用Unicode字符集（Windows API使用宽字符版本）
        UNICODE
        _UNICODE
        # 严格Windows版本兼容性：目标Windows 7及以上
        WINVER=0x0601
        _WIN32_WINNT=0x0601
        # 禁用不安全的CRT函数警告（使用安全版本如wcsncpy_s）
        _CRT_SECURE_NO_WARNINGS
    PUBLIC
        # 公共定义：告知使用者此DLL支持Unicode
        EXTENSION_DLL_UNICODE
)

# ========================== MSVC专用编译选项 ==========================

if(MSVC)
    target_compile_options(${PROJECT_NAME}
        PRIVATE
            # 启用多处理器编译，加速构建
            /MP
            # 警告级别：级别4（最高级别，除/Wall外）
            /W4
            # 将警告视为错误（生产代码要求零警告）
            /WX
            # 启用异常处理（C++标准异常）
            /EHsc
            # 启用RTTI（运行时类型识别），用于dynamic_cast和typeid
            /GR
            # 禁用特定的过度敏感警告：
            # C4100: 未引用的形参（已通过UNREFERENCED_PARAMETER处理）
            /wd4100
            # C4505: 未使用的本地函数（某些回调可能暂时未使用）
            /wd4505
    )

    # 运行时库设置：使用静态多线程运行时（/MT）
    # 这样生成的DLL不依赖msvcp140.dll等VC++运行时库
    set_property(TARGET ${PROJECT_NAME} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")

    # 链接器选项
    target_link_options(${PROJECT_NAME}
        PRIVATE
            # 生成调试信息（PDB文件）
            /DEBUG
            # 启用增量链接（加速开发构建）
            /INCREMENTAL
            # 固定基址（可选）：为DLL指定首选加载地址，减少重定位开销
            /BASE:0x10000000
            # 生成映射文件（用于调试和分析）
            /MAP
    )
endif()

# ========================== 模块定义文件 ==========================

# 显式指定.def文件用于控制导出符号
# 这确保即使编译器不同，导出函数名也保持一致（无C++名称修饰）
target_sources(${PROJECT_NAME} PRIVATE ${EXTENSION_DLL_DEF})
set_target_properties(${PROJECT_NAME} PROPERTIES
    LINK_FLAGS "${CMAKE_CURRENT_SOURCE_DIR}/ExtensionDLL.def"
)

# ========================== 输出属性 ==========================

set_target_properties(${PROJECT_NAME} PROPERTIES
    # 输出文件名（不含扩展名）
    OUTPUT_NAME "ExtensionDLL"
    # 运行时输出目录（DLL文件）
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    # 库输出目录（导入库.lib文件）
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    # 调试符号输出目录（PDB文件）
    PDB_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/pdb
)

# ========================== 安装规则 ==========================

# 安装DLL文件到指定目录
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin          # .dll 文件
    LIBRARY DESTINATION lib          # .lib 导入库（Windows上等同于ARCHIVE）
    ARCHIVE DESTINATION lib          # .lib 导入库
)

# 安装公共头文件
install(FILES ${EXTENSION_DLL_HEADERS}
    DESTINATION include
)

# ========================== 构建后验证 ==========================

# 可选：构建后运行dumpbin验证导出符号（需要Visual Studio环境）
if(MSVC)
    find_program(DUMPBIN_EXE dumpbin PATHS "$ENV{VSINSTALLDIR}/VC/Tools/MSVC/*/bin/Hostx86/x86")
    if(DUMPBIN_EXE)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${DUMPBIN_EXE} /EXPORTS $<TARGET_FILE:${PROJECT_NAME}>
            COMMENT "验证DLL导出符号..."
            VERBATIM
        )
    endif()
endif()

# ========================== 使用说明 ==========================
# 构建命令示例（从项目根目录执行）：
#
#   mkdir build && cd build
#   cmake .. -A Win32 -T v142
#   cmake --build . --config Release
#
# 输出文件：
#   build/bin/ExtensionDLL.dll    - 动态链接库
#   build/lib/ExtensionDLL.lib    - 导入库（供主程序链接使用）
#   build/pdb/ExtensionDLL.pdb    - 调试符号文件（Release构建可能不生成）
