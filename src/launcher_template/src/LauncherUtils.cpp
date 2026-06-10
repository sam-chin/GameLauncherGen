/**
 * @file LauncherUtils.cpp
 * @brief 启动器通用工具函数实现
 * @details 实现LauncherUtils.h中声明的所有工具函数。
 *          所有函数均为无状态实现，线程安全。
 */

#include "LauncherUtils.h"
#include <vector>
#include <algorithm>

// ---------------------------------------------------------------------------
// 命名空间定义
// ---------------------------------------------------------------------------

namespace GameLauncher {
namespace Launcher {
namespace Utils {

// ---------------------------------------------------------------------------
// 系统错误处理实现
// ---------------------------------------------------------------------------

/**
 * @brief 获取当前线程的Last-Error值并格式化为可读字符串
 *
 * 实现细节:
 * 1. 首先调用GetLastError()保存错误码（必须在调用其他Windows API前保存，
 *    因为FormatMessageW可能覆盖Last-Error值）
 * 2. 使用FormatMessageW获取系统预定义的错误描述
 * 3. 清理描述字符串中的换行符
 * 4. 组合成 [错误码] 描述 的格式返回
 */
std::wstring GetLastErrorString() noexcept {
    // 第一步：立即保存Last-Error值
    // 原因：后续任何Windows API调用都可能覆盖此值
    DWORD errorCode = ::GetLastError();
    return FormatSystemError(errorCode);
}

/**
 * @brief 将Windows错误码格式化为可读字符串
 *
 * 实现细节:
 * 使用FormatMessageW的FORMAT_MESSAGE_FROM_SYSTEM标志从系统消息表
 * 获取错误描述。分配足够大的缓冲区（4KB）以容纳长描述。
 *
 * 安全考虑:
 * - 使用LocalFree释放FormatMessageW分配的内存
 * - 如果FormatMessageW失败，返回原始错误码的数字表示
 */
std::wstring FormatSystemError(DWORD errorCode) noexcept {
    // 使用wchar_t缓冲区接收系统错误消息
    // FormatMessageW会自动分配内存（使用FORMAT_MESSAGE_ALLOCATE_BUFFER），
    // 调用方需要使用LocalFree释放
    LPWSTR pBuffer = nullptr;

    DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER
                | FORMAT_MESSAGE_FROM_SYSTEM
                | FORMAT_MESSAGE_IGNORE_INSERTS;

    // 尝试从系统消息表获取错误描述
    DWORD result = ::FormatMessageW(
        flags,              // 格式化选项：分配缓冲区、从系统获取、忽略插入参数
        nullptr,            // 消息定义来源：使用系统默认
        errorCode,          // 要格式化的错误码
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),  // 使用系统默认语言
        reinterpret_cast<LPWSTR>(&pBuffer),  // 输出缓冲区地址（接收分配的内存指针）
        0,                  // 最小缓冲区大小（使用ALLOCATE_BUFFER时忽略）
        nullptr             // 无插入参数
    );

    std::wstring errorString;

    if (result > 0 && pBuffer != nullptr) {
        // 成功获取错误描述，去除末尾的换行符和回车符
        // FormatMessageW通常会在描述末尾添加\r\n
        errorString = pBuffer;

        // 清理末尾空白字符
        while (!errorString.empty() &&
               (errorString.back() == L'\r' ||
                errorString.back() == L'\n' ||
                errorString.back() == L' ')) {
            errorString.pop_back();
        }

        // 释放FormatMessageW分配的内存
        // 必须使用LocalFree，这是Windows API的约定
        ::LocalFree(pBuffer);
        pBuffer = nullptr;
    } else {
        // FormatMessageW失败，构造简单的数字错误码字符串
        // 这种情况很少见，通常表示错误码不在系统消息表中
        errorString = L"未知错误 (代码: " + std::to_wstring(errorCode) + L")";

        // 确保释放可能已分配的内存（虽然失败时通常不会分配）
        if (pBuffer != nullptr) {
            ::LocalFree(pBuffer);
            pBuffer = nullptr;
        }
    }

    // 组合最终输出: [错误码] 描述
    return L"[" + std::to_wstring(errorCode) + L"] " + errorString;
}

// ---------------------------------------------------------------------------
// 字符串转换工具实现
// ---------------------------------------------------------------------------

/**
 * @brief UTF-8到宽字符串转换
 *
 * 实现细节:
 * 1. 首先调用MultiByteToWideChar获取所需缓冲区大小（cbMultiByte=-1包含结尾\0）
 * 2. 分配足够大的wchar_t缓冲区
 * 3. 再次调用MultiByteToWideChar执行实际转换
 * 4. 返回转换结果
 *
 * 错误处理:
 * - 任何步骤失败返回空字符串
 * - 使用std::vector管理缓冲区，避免内存泄漏
 */
std::wstring Utf8ToWide(const std::string& utf8Str) noexcept {
    if (utf8Str.empty()) {
        return std::wstring();
    }

    // 第一步：获取所需宽字符数量
    // CP_UTF8: 使用UTF-8代码页
    // -1: 输入以null结尾，自动计算长度
    int requiredSize = ::MultiByteToWideChar(
        CP_UTF8,        // 代码页：UTF-8
        0,              // 标志：默认行为
        utf8Str.c_str(), // 输入字符串
        -1,             // 自动计算长度（包含结尾\0）
        nullptr,        // 首次调用，仅获取大小
        0               // 缓冲区大小为0
    );

    if (requiredSize <= 0) {
        // 转换失败，可能是无效的UTF-8序列
        return std::wstring();
    }

    // 第二步：分配缓冲区并执行转换
    std::vector<wchar_t> buffer(static_cast<size_t>(requiredSize));

    int converted = ::MultiByteToWideChar(
        CP_UTF8,
        0,
        utf8Str.c_str(),
        -1,
        buffer.data(),
        requiredSize
    );

    if (converted <= 0) {
        return std::wstring();
    }

    // 构造wstring（去除结尾的\0，因为std::wstring会自动管理）
    return std::wstring(buffer.data(), static_cast<size_t>(converted - 1));
}

/**
 * @brief 宽字符串到UTF-8转换
 *
 * 实现细节与Utf8ToWide对称，使用WideCharToMultiByte
 */
std::string WideToUtf8(const std::wstring& wideStr) noexcept {
    if (wideStr.empty()) {
        return std::string();
    }

    // 第一步：获取所需UTF-8字节数
    int requiredSize = ::WideCharToMultiByte(
        CP_UTF8,        // 代码页：UTF-8
        0,              // 标志：默认行为
        wideStr.c_str(), // 输入宽字符串
        -1,             // 自动计算长度
        nullptr,        // 首次调用，仅获取大小
        0,              // 缓冲区大小为0
        nullptr,        // 默认字符（UTF-8不需要）
        nullptr         // 使用默认字符标志（UTF-8不需要）
    );

    if (requiredSize <= 0) {
        return std::string();
    }

    // 第二步：分配缓冲区并执行转换
    std::vector<char> buffer(static_cast<size_t>(requiredSize));

    int converted = ::WideCharToMultiByte(
        CP_UTF8,
        0,
        wideStr.c_str(),
        -1,
        buffer.data(),
        requiredSize,
        nullptr,
        nullptr
    );

    if (converted <= 0) {
        return std::string();
    }

    return std::string(buffer.data(), static_cast<size_t>(converted - 1));
}

// ---------------------------------------------------------------------------
// 路径工具实现
// ---------------------------------------------------------------------------

/**
 * @brief 获取当前可执行文件所在目录
 *
 * 实现细节:
 * 1. 调用GetModuleFileNameW获取当前模块完整路径
 * 2. 查找最后一个反斜杠或斜杠的位置
 * 3. 提取目录部分
 * 4. 确保目录以反斜杠结尾
 *
 * 错误处理:
 * - 如果GetModuleFileNameW失败，返回当前工作目录
 * - 如果路径中没有分隔符，返回空字符串
 */
std::wstring GetExecutableDirectory() noexcept {
    // MAX_PATH在Windows中为260，但某些场景可能更长
    // 使用MAX_PATH + 1确保足够
    wchar_t pathBuffer[MAX_PATH + 1] = { 0 };

    // 获取当前可执行文件的完整路径
    // nullptr表示获取当前模块（即本EXE）
    DWORD result = ::GetModuleFileNameW(nullptr, pathBuffer, MAX_PATH);

    if (result == 0 || result >= MAX_PATH) {
        // 获取失败或路径过长，返回当前目录
        // 获取当前工作目录作为后备
        DWORD cwdResult = ::GetCurrentDirectoryW(MAX_PATH, pathBuffer);
        if (cwdResult > 0 && cwdResult < MAX_PATH) {
            std::wstring cwd(pathBuffer);
            // 确保以反斜杠结尾
            if (!cwd.empty() && cwd.back() != L'\\' && cwd.back() != L'/') {
                cwd += L'\\';
            }
            return cwd;
        }
        return std::wstring();
    }

    std::wstring fullPath(pathBuffer);

    // 查找最后一个路径分隔符（支持\和/两种风格）
    size_t lastSlash = fullPath.find_last_of(L"\\/");

    if (lastSlash == std::wstring::npos) {
        // 路径中没有分隔符，异常情况
        return std::wstring();
    }

    // 提取目录部分（包含分隔符）
    std::wstring directory = fullPath.substr(0, lastSlash + 1);

    // 统一使用反斜杠
    std::replace(directory.begin(), directory.end(), L'/', L'\\');

    return directory;
}

/**
 * @brief 拼接目录路径和文件名
 *
 * 实现细节:
 * 1. 去除目录末尾的多余分隔符（保留一个）
 * 2. 去除文件名开头的分隔符
 * 3. 用反斜杠拼接
 */
std::wstring JoinPath(const std::wstring& directory, const std::wstring& fileName) noexcept {
    if (directory.empty()) {
        return fileName;
    }

    if (fileName.empty()) {
        return directory;
    }

    std::wstring result = directory;

    // 确保目录以反斜杠结尾（但不去除已有的）
    if (result.back() != L'\\' && result.back() != L'/') {
        result += L'\\';
    } else {
        // 如果末尾是斜杠，统一为反斜杠
        // 同时去除连续的分隔符
        while (result.size() > 1 &&
               (result.back() == L'\\' || result.back() == L'/')) {
            result.pop_back();
        }
        result += L'\\';
    }

    // 去除文件名开头的分隔符
    std::wstring cleanFileName = fileName;
    size_t startPos = 0;
    while (startPos < cleanFileName.size() &&
           (cleanFileName[startPos] == L'\\' || cleanFileName[startPos] == L'/')) {
        ++startPos;
    }

    if (startPos > 0) {
        cleanFileName = cleanFileName.substr(startPos);
    }

    result += cleanFileName;
    return result;
}

} // namespace Utils
} // namespace Launcher
} // namespace GameLauncher
