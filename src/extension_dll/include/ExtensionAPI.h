/**
 * @file ExtensionAPI.h
 * @brief 游戏启动器扩展DLL公共API头文件
 * @details 定义了扩展DLL与主程序之间的标准接口契约。
 *          所有导出函数均采用 __stdcall 调用约定，并使用 extern "C" 防止C++名称修饰，
 *          确保不同编译器之间的二进制兼容性。
 *
 * @note 本文件为扩展DLL与主程序共享的公共头文件，不得包含任何实现细节或平台相关代码。
 * @warning 所有字符串参数和返回值均使用宽字符（UTF-16LE），以支持完整的Unicode字符集。
 *
 * @version 1.0.0
 * @date 2026-06-10
 */

#pragma once

#ifndef EXTENSION_API_H
#define EXTENSION_API_H

/* ========================== 平台检测与宏定义 ========================== */

/**
 * @def EXTENSION_API
 * @brief 导出/导入声明宏
 * @details 当编译扩展DLL本身时，定义 EXTENSION_DLL_BUILD 宏以导出符号；
 *          当主程序包含此头文件时，不定义该宏，从而导入符号。
 */
#ifdef EXTENSION_DLL_BUILD
    #define EXTENSION_API extern "C" __declspec(dllexport)
#else
    #define EXTENSION_API extern "C" __declspec(dllimport)
#endif

/**
 * @def EXTENSION_CALL
 * @brief 调用约定宏
 * @details 强制使用 __stdcall 调用约定，这是Win32 API的标准约定，
 *          确保参数从右向左压栈，由被调用方清理栈空间。
 */
#define EXTENSION_CALL __stdcall

/* ========================== 标准头文件包含 ========================== */

#include <windows.h>
#include <cstdint>
#include <cwchar>

/* ========================== 版本与兼容性定义 ========================== */

/**
 * @brief 扩展DLL接口主版本号
 * @details 当接口发生不兼容变更时递增此版本号。
 */
constexpr uint32_t EXTENSION_API_VERSION_MAJOR = 1;

/**
 * @brief 扩展DLL接口次版本号
 * @details 当接口向后兼容地添加新功能时递增此版本号。
 */
constexpr uint32_t EXTENSION_API_VERSION_MINOR = 0;

/**
 * @brief 扩展DLL接口修订版本号
 * @details 当进行缺陷修复或内部优化时递增此版本号。
 */
constexpr uint32_t EXTENSION_API_VERSION_PATCH = 0;

/**
 * @brief 扩展DLL接口完整版本号（打包为单个32位值）
 * @details 高16位为主版本号，次高8位为次版本号，低8位为修订版本号。
 */
constexpr uint32_t EXTENSION_API_VERSION =
    (EXTENSION_API_VERSION_MAJOR << 16) |
    (EXTENSION_API_VERSION_MINOR << 8)  |
    (EXTENSION_API_VERSION_PATCH);

/* ========================== 结果码枚举 ========================== */

/**
 * @enum EXTENSION_RESULT
 * @brief 扩展DLL操作结果码
 * @details 所有导出函数均返回此枚举类型以指示操作成功或失败的具体原因。
 */
enum EXTENSION_RESULT : uint32_t
{
    EXT_SUCCESS             = 0,    /**< 操作成功完成 */
    EXT_ERROR_GENERIC       = 1,    /**< 通用/未知错误 */
    EXT_ERROR_INVALID_PARAM = 2,    /**< 传入的参数无效（空指针、越界等） */
    EXT_ERROR_OUT_OF_MEMORY = 3,    /**< 内存分配失败 */
    EXT_ERROR_NOT_INITIALIZED = 4,  /**< DLL尚未初始化，无法执行请求的操作 */
    EXT_ERROR_ALREADY_INITIALIZED = 5, /**< DLL已经初始化，重复调用初始化函数 */
    EXT_ERROR_NOT_SUPPORTED = 6,    /**< 请求的功能在当前版本中不受支持 */
    EXT_ERROR_HOOK_FAILED   = 7,    /**< 钩子安装或卸载失败 */
    EXT_ERROR_VERSION_MISMATCH = 8, /**< 接口版本不匹配，主程序与扩展DLL版本不兼容 */
};

/* ========================== 回调函数类型定义 ========================== */

/**
 * @typedef HOOK_CALLBACK
 * @brief 通用钩子回调函数原型
 * @details 用于各类钩子事件的回调通知。扩展DLL可通过此回调与主程序进行事件通信。
 *
 * @param[in] eventCode   事件类型标识码（由具体钩子定义）
 * @param[in] pEventData  指向事件相关数据的指针（具体类型由eventCode决定）
 * @param[in] dataSize    事件数据的大小（字节）
 * @param[in] pUserData   用户自定义上下文数据，注册回调时传入
 *
 * @return 如果回调处理了该事件并期望阻止后续处理，返回TRUE；否则返回FALSE继续传递事件。
 */
typedef BOOL (EXTENSION_CALL *HOOK_CALLBACK)(
    uint32_t eventCode,
    const void* pEventData,
    size_t dataSize,
    void* pUserData
);

/**
 * @typedef LOG_CALLBACK
 * @brief 日志输出回调函数原型
 * @details 扩展DLL可通过此回调将内部日志输出到主程序的日志系统中，实现统一的日志管理。
 *
 * @param[in] level       日志级别：0=调试, 1=信息, 2=警告, 3=错误, 4=严重错误
 * @param[in] pMessage    宽字符日志消息字符串（以L""前缀定义）
 * @param[in] pSource     日志来源模块名称（通常为文件名或功能模块名）
 */
typedef void (EXTENSION_CALL *LOG_CALLBACK)(
    uint32_t level,
    const wchar_t* pMessage,
    const wchar_t* pSource
);

/* ========================== 初始化配置结构体 ========================== */

/**
 * @struct EXTENSION_INIT_CONFIG
 * @brief DLL初始化配置参数结构体
 * @details 主程序在调用 DllInit 时传入此结构体，以配置扩展DLL的运行时行为。
 *          所有字符串字段均以宽字符零结尾字符串形式提供。
 */
struct EXTENSION_INIT_CONFIG
{
    uint32_t structSize;            /**< 结构体大小（字节），用于版本兼容性检查 */
    uint32_t apiVersion;            /**< 主程序期望的API版本号 */
    const wchar_t* pWorkingDir;     /**< 扩展DLL的工作目录路径（绝对路径，以反斜杠结尾） */
    const wchar_t* pConfigFilePath; /**< 扩展DLL专属配置文件路径（可为nullptr表示使用默认配置） */
    LOG_CALLBACK pLogCallback;      /**< 日志回调函数指针（可为nullptr表示禁用日志输出） */
    void* pReserved;                /**< 保留字段，必须为nullptr，供未来扩展使用 */
};

/* ========================== 核心导出函数声明 ========================== */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn DllInit
 * @brief 初始化扩展DLL模块
 * @details 这是扩展DLL的生命周期起点。主程序在加载DLL后必须首先调用此函数完成初始化。
 *          此函数负责分配内部资源、读取配置文件、初始化钩子框架等。
 *          该函数是线程安全的，但不应在DllMain中直接调用（避免Loader Lock死锁）。
 *
 * @param[in] pConfig 指向初始化配置结构体的指针，包含工作目录、日志回调等参数。
 *                    此指针在函数返回前必须保持有效。
 *
 * @return 如果初始化成功，返回 EXT_SUCCESS (0)；
 *         如果失败，返回具体的 EXTENSION_RESULT 错误码。
 *
 * @note 此函数可被多次调用，但仅在首次调用时执行实际初始化；
 *       后续调用将返回 EXT_ERROR_ALREADY_INITIALIZED。
 * @warning 不要在 DllMain 的 DLL_PROCESS_ATTACH 中调用此函数。
 */
EXTENSION_API EXTENSION_RESULT EXTENSION_CALL DllInit(
    const EXTENSION_INIT_CONFIG* pConfig
);

/**
 * @fn DllUnInit
 * @brief 反初始化扩展DLL模块
 * @details 这是扩展DLL的生命周期终点。主程序在卸载DLL前必须调用此函数释放所有资源。
 *          此函数负责卸载所有钩子、释放内存、关闭文件句柄、清理线程局部存储等。
 *          调用此函数后，扩展DLL的所有其他接口均不再可用。
 *
 * @note 此函数可被多次调用，但仅在首次调用时执行实际清理；
 *       后续调用将安全地忽略（幂等操作）。
 * @warning 不要在 DllMain 的 DLL_PROCESS_DETACH 中调用此函数（除非在FreeLibrary的线程上下文中）。
 * @warning 确保在调用此函数前，所有由扩展DLL创建的线程已终止。
 */
EXTENSION_API void EXTENSION_CALL DllUnInit(void);

/**
 * @fn DllGetVersion
 * @brief 获取扩展DLL的版本信息
 * @details 允许主程序在初始化前查询扩展DLL的版本，以进行兼容性检查。
 *          此函数是线程安全的，可在任何时刻调用。
 *
 * @param[out] pMajor 指向接收主版本号的变量的指针（可为nullptr）
 * @param[out] pMinor 指向接收次版本号的变量的指针（可为nullptr）
 * @param[out] pPatch 指向接收修订版本号的变量的指针（可为nullptr）
 *
 * @return 返回打包的32位版本号，格式与 EXTENSION_API_VERSION 相同。
 *         如果所有输出参数均为nullptr，仅返回版本号而不写入任何数据。
 */
EXTENSION_API uint32_t EXTENSION_CALL DllGetVersion(
    uint32_t* pMajor,
    uint32_t* pMinor,
    uint32_t* pPatch
);

/**
 * @fn DllGetLastErrorMessage
 * @brief 获取最后一次错误的描述信息
 * @details 当某个接口函数返回错误码后，可调用此函数获取人类可读的错误描述。
 *          返回的字符串指向内部静态缓冲区，不需要调用者释放，但非线程安全。
 *
 * @return 指向宽字符错误描述字符串的指针。如果没有错误记录，返回空字符串。
 */
EXTENSION_API const wchar_t* EXTENSION_CALL DllGetLastErrorMessage(void);

#ifdef __cplusplus
}
#endif

/* ========================== 钩子接口占位声明（供未来扩展） ========================== */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn DllInstallHook
 * @brief 安装指定类型的钩子（占位接口）
 * @details 【当前版本为占位实现，仅返回 EXT_ERROR_NOT_SUPPORTED】
 *          未来版本将支持安装进程内钩子（如消息钩子、API钩子）以拦截游戏进程行为。
 *          钩子安装后，指定类型的事件将通过回调函数通知扩展DLL。
 *
 * @param[in] hookType    钩子类型标识码（由具体钩子类型枚举定义）
 * @param[in] pCallback   钩子事件回调函数指针
 * @param[in] pUserData   传递给回调函数的用户上下文数据
 *
 * @return 当前版本始终返回 EXT_ERROR_NOT_SUPPORTED。
 */
EXTENSION_API EXTENSION_RESULT EXTENSION_CALL DllInstallHook(
    uint32_t hookType,
    HOOK_CALLBACK pCallback,
    void* pUserData
);

/**
 * @fn DllUninstallHook
 * @brief 卸载指定类型的钩子（占位接口）
 * @details 【当前版本为占位实现，仅返回 EXT_ERROR_NOT_SUPPORTED】
 *          未来版本将支持卸载由 DllInstallHook 安装的钩子。
 *
 * @param[in] hookType    要卸载的钩子类型标识码
 *
 * @return 当前版本始终返回 EXT_ERROR_NOT_SUPPORTED。
 */
EXTENSION_API EXTENSION_RESULT EXTENSION_CALL DllUninstallHook(
    uint32_t hookType
);

#ifdef __cplusplus
}
#endif

#endif // EXTENSION_API_H
