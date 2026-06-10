/**
 * @file CommonTypes.h
 * @brief 通用类型定义与错误码枚举
 * @details 本文件定义了32位游戏启动器生成器公共库中使用的通用数据类型、
 *          错误码枚举以及跨平台宏定义。所有模块共享这些基础定义，
 *          确保接口一致性和可维护性。
 *
 * @note 本项目针对MSVC x86平台编译，使用C++17标准
 * @warning 所有路径均使用wchar_t宽字符，确保完整的Unicode支持
 */

#pragma once

#ifndef COMMON_TYPES_H
#define COMMON_TYPES_H

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// 平台检测与宏定义
// ---------------------------------------------------------------------------

#ifdef _WIN32
    #define COMMON_PLATFORM_WINDOWS 1
    // Windows平台下使用宽字符路径
    #define COMMON_PATH_CHAR wchar_t
    #define COMMON_STRING std::wstring
#else
    #error "本库仅支持Windows平台 (MSVC x86)"
#endif

// 导出/导入宏定义（静态库无需特殊处理，但保留扩展性）
#ifdef COMMON_BUILD_DLL
    #ifdef COMMON_EXPORTS
        #define COMMON_API __declspec(dllexport)
    #else
        #define COMMON_API __declspec(dllimport)
    #endif
#else
    #define COMMON_API  // 静态库不使用导出/导入声明
#endif

// ---------------------------------------------------------------------------
// 命名空间定义
// ---------------------------------------------------------------------------

namespace GameLauncher {
namespace Common {

// ---------------------------------------------------------------------------
// 基础类型别名
// ---------------------------------------------------------------------------

using Byte = uint8_t;           ///< 单字节类型，用于原始二进制数据
using ByteArray = std::vector<Byte>;  ///< 字节数组，用于内存中的数据缓冲

// ---------------------------------------------------------------------------
// 错误码枚举
// ---------------------------------------------------------------------------

/**
 * @enum ErrorCode
 * @brief 公共库操作结果错误码
 * @details 所有模块的API均返回此错误码，调用方应始终检查返回值。
 *          错误码设计遵循以下原则：
 *          - 0 表示成功，非0表示失败
 *          - 错误码按功能模块分段，便于定位问题来源
 *          - 每个错误码均有明确的中文说明
 */
enum class ErrorCode : int32_t {
    // 通用成功码
    Success = 0,                        ///< 操作成功完成

    // 通用错误 (1 - 99)
    UnknownError = 1,                   ///< 未知错误
    InvalidParameter = 2,               ///< 参数无效（空指针、越界等）
    OutOfMemory = 3,                    ///< 内存分配失败
    BufferTooSmall = 4,                 ///< 输出缓冲区不足
    NotImplemented = 5,                 ///< 功能未实现

    // 文件/IO错误 (100 - 199)
    FileNotFound = 100,                 ///< 文件不存在
    FileOpenFailed = 101,               ///< 文件打开失败
    FileReadFailed = 102,               ///< 文件读取失败
    FileWriteFailed = 103,              ///< 文件写入失败
    FileSeekFailed = 104,               ///< 文件定位失败
    FileCloseFailed = 105,              ///< 文件关闭失败（通常伴随句柄泄漏风险）
    DirectoryNotFound = 106,            ///< 目录不存在
    PathTooLong = 107,                  ///< 路径超出系统限制
    InvalidFileHandle = 108,            ///< 无效的文件句柄

    // 加密/解密错误 (200 - 299)
    CryptoInvalidKey = 200,             ///< 加密密钥无效
    CryptoInvalidData = 201,            ///< 待加密/解密数据无效
    CryptoStreamError = 202,            ///< 流加密/解密过程中发生错误

    // 打包/解包错误 (300 - 399)
    PackInvalidHeader = 300,            ///< 包文件头无效或损坏
    PackInvalidFormat = 301,            ///< 包格式不合法
    PackFileCountMismatch = 302,        ///< 文件数量不匹配
    PackSizeMismatch = 303,             ///< 大小信息不匹配（可能数据损坏）
    PackCrcCheckFailed = 304,           ///< CRC32校验失败（数据可能被篡改）
    PackEntryNotFound = 305,            ///< 包内找不到指定文件条目
    PackVersionMismatch = 306,          ///< 包版本不匹配

    // CRC32错误 (400 - 499)
    CrcInvalidData = 400,               ///< CRC计算输入数据无效
};

/**
 * @brief 将错误码转换为可读字符串
 * @param code 错误码
 * @return 对应的错误描述字符串（UTF-8编码）
 */
inline const char* ErrorCodeToString(ErrorCode code) {
    switch (code) {
        case ErrorCode::Success: return "操作成功";
        case ErrorCode::UnknownError: return "未知错误";
        case ErrorCode::InvalidParameter: return "参数无效";
        case ErrorCode::OutOfMemory: return "内存分配失败";
        case ErrorCode::BufferTooSmall: return "缓冲区不足";
        case ErrorCode::NotImplemented: return "功能未实现";
        case ErrorCode::FileNotFound: return "文件不存在";
        case ErrorCode::FileOpenFailed: return "文件打开失败";
        case ErrorCode::FileReadFailed: return "文件读取失败";
        case ErrorCode::FileWriteFailed: return "文件写入失败";
        case ErrorCode::FileSeekFailed: return "文件定位失败";
        case ErrorCode::FileCloseFailed: return "文件关闭失败";
        case ErrorCode::DirectoryNotFound: return "目录不存在";
        case ErrorCode::PathTooLong: return "路径过长";
        case ErrorCode::InvalidFileHandle: return "无效的文件句柄";
        case ErrorCode::CryptoInvalidKey: return "加密密钥无效";
        case ErrorCode::CryptoInvalidData: return "加密数据无效";
        case ErrorCode::CryptoStreamError: return "流加密错误";
        case ErrorCode::PackInvalidHeader: return "包文件头无效";
        case ErrorCode::PackInvalidFormat: return "包格式不合法";
        case ErrorCode::PackFileCountMismatch: return "文件数量不匹配";
        case ErrorCode::PackSizeMismatch: return "大小信息不匹配";
        case ErrorCode::PackCrcCheckFailed: return "CRC32校验失败";
        case ErrorCode::PackEntryNotFound: return "包内条目未找到";
        case ErrorCode::PackVersionMismatch: return "包版本不匹配";
        case ErrorCode::CrcInvalidData: return "CRC输入数据无效";
        default: return "未定义的错误码";
    }
}

// ---------------------------------------------------------------------------
// 包文件格式常量
// ---------------------------------------------------------------------------

/**
 * @brief 包文件魔数，用于快速识别文件类型
 * @details 文件头前4字节为此值，防止将普通文件误识别为包文件
 */
constexpr uint32_t PACKAGE_MAGIC = 0x504B4731;  // "PKG1" 小端序

/**
 * @brief 包文件格式版本号
 * @details 用于后续版本兼容性处理，当前为第1版
 */
constexpr uint16_t PACKAGE_VERSION = 1;

// ---------------------------------------------------------------------------
// 工具宏
// ---------------------------------------------------------------------------

/**
 * @brief 安全释放指针并将指针置空
 * @details 防止重复释放和野指针访问，是内存安全的基础措施
 */
#define SAFE_DELETE(ptr) do { \
    if ((ptr) != nullptr) { \
        delete (ptr); \
        (ptr) = nullptr; \
    } \
} while (0)

/**
 * @brief 安全释放数组指针并将指针置空
 */
#define SAFE_DELETE_ARRAY(ptr) do { \
    if ((ptr) != nullptr) { \
        delete[] (ptr); \
        (ptr) = nullptr; \
    } \
} while (0)

} // namespace Common
} // namespace GameLauncher

#endif // COMMON_TYPES_H
