/**
 * @file LauncherUtils.h
 * @brief 启动器通用工具函数
 * @details 提供启动器模块所需的辅助功能，包括：
 *          - 宽字符串与多字节字符串转换
 *          - 系统错误码捕获与格式化
 *          - 进程/线程句柄的安全包装
 *          - 路径处理工具
 *
 *          所有工具函数均为纯函数或轻量级包装，不持有长期资源。
 *
 * @note 本模块设计为无状态(stateless)，所有函数线程安全（仅依赖局部变量）
 * @warning 所有Windows API调用均使用宽字符版本（W后缀）
 */

#pragma once

#ifndef LAUNCHER_UTILS_H
#define LAUNCHER_UTILS_H

#include <Windows.h>
#include <string>
#include <system_error>
#include <utility>

// ---------------------------------------------------------------------------
// 命名空间定义
// ---------------------------------------------------------------------------

namespace GameLauncher {
namespace Launcher {
namespace Utils {

// ---------------------------------------------------------------------------
// 系统错误处理
// ---------------------------------------------------------------------------

/**
 * @brief 获取当前线程的Last-Error值并格式化为可读字符串
 * @return 包含错误码和描述信息的宽字符串
 *
 * @details 内部调用GetLastError()获取错误码，使用FormatMessageW获取系统错误描述。
 *          返回的字符串格式: [错误码] 错误描述
 *
 * @note 应在Windows API调用失败后立即调用，避免其他API覆盖Last-Error值
 * @warning 本函数会重置Last-Error值为0（通过FormatMessageW内部实现），
 *          如需保留原始错误码，应先手动调用GetLastError()保存
 */
std::wstring GetLastErrorString() noexcept;

/**
 * @brief 将Windows错误码格式化为可读字符串
 * @param[in] errorCode Windows错误码（如ERROR_FILE_NOT_FOUND）
 * @return 错误描述字符串（宽字符）
 */
std::wstring FormatSystemError(DWORD errorCode) noexcept;

// ---------------------------------------------------------------------------
// 字符串转换工具
// ---------------------------------------------------------------------------

/**
 * @brief 将UTF-8编码的窄字符串转换为宽字符串
 * @param[in] utf8Str UTF-8编码的输入字符串
 * @return 转换后的宽字符串，转换失败返回空字符串
 *
 * @note 使用Windows API MultiByteToWideChar进行转换，支持BOM标记
 * @warning 输入必须是有效的UTF-8序列，否则可能导致转换失败
 */
std::wstring Utf8ToWide(const std::string& utf8Str) noexcept;

/**
 * @brief 将宽字符串转换为UTF-8编码的窄字符串
 * @param[in] wideStr 宽字符输入字符串
 * @return 转换后的UTF-8字符串，转换失败返回空字符串
 */
std::string WideToUtf8(const std::wstring& wideStr) noexcept;

// ---------------------------------------------------------------------------
// 句柄安全包装类 (RAII)
// ---------------------------------------------------------------------------

/**
 * @class SafeHandle
 * @brief Windows句柄的RAII包装类
 * @details 自动管理Windows内核对象句柄的生命周期，确保句柄在任何代码路径下
 *          都能被正确关闭，彻底杜绝句柄泄漏(Handle Leak)。
 *
 *          适用对象类型:
 *          - 进程句柄 (HANDLE from OpenProcess/CreateProcess)
 *          - 线程句柄 (HANDLE from CreateRemoteThread)
 *          - 文件句柄、事件句柄等任何需要CloseHandle的对象
 *
 *          安全特性:
 *          1. 析构时自动调用CloseHandle
 *          2. 移动语义支持，禁止拷贝（避免重复关闭）
 *          3. 显式释放接口 Release()，转移所有权
 *          4. 布尔转换操作符，便于检查句柄有效性
 *
 * @note 使用INVALID_HANDLE_VALUE作为无效句柄标记（与Windows API一致）
 * @warning 不要对同一个句柄创建多个SafeHandle实例，否则会导致重复关闭
 *
 * 使用示例:
 * @code
 *   SafeHandle hProcess(OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid));
 *   if (!hProcess) {
 *       // 打开失败，无需手动关闭
 *       return;
 *   }
 *   // 使用hProcess.Get()获取原始句柄进行API调用
 *   // 函数返回时自动关闭句柄
 * @endcode
 */
class SafeHandle {
public:
    /**
     * @brief 默认构造函数，创建空句柄
     */
    SafeHandle() noexcept
        : m_handle(INVALID_HANDLE_VALUE)
    {}

    /**
     * @brief 从原始句柄构造，取得所有权
     * @param[in] handle 有效的Windows句柄，或INVALID_HANDLE_VALUE
     *
     * @warning 传入后SafeHandle取得所有权，调用方不应再对该句柄调用CloseHandle
     */
    explicit SafeHandle(HANDLE handle) noexcept
        : m_handle(handle)
    {}

    /**
     * @brief 析构函数，自动关闭句柄
     * @details 这是防止句柄泄漏的核心机制。无论函数是正常返回还是异常抛出，
     *          析构函数都会被调用，确保句柄释放。
     */
    ~SafeHandle() noexcept {
        Close();
    }

    // 禁止拷贝，防止重复关闭同一句柄导致未定义行为
    SafeHandle(const SafeHandle&) = delete;
    SafeHandle& operator=(const SafeHandle&) = delete;

    /**
     * @brief 移动构造函数
     * @details 转移句柄所有权，源对象变为空句柄状态。
     *          这是实现异常安全代码的关键：即使发生异常，
     *          只有一个SafeHandle持有句柄，避免重复释放。
     */
    SafeHandle(SafeHandle&& other) noexcept
        : m_handle(other.m_handle)
    {
        other.m_handle = INVALID_HANDLE_VALUE;
    }

    /**
     * @brief 移动赋值运算符
     */
    SafeHandle& operator=(SafeHandle&& other) noexcept {
        if (this != &other) {
            Close();                         // 先释放当前持有的句柄
            m_handle = other.m_handle;       // 转移所有权
            other.m_handle = INVALID_HANDLE_VALUE;
        }
        return *this;
    }

    /**
     * @brief 显式关闭句柄
     * @details 安全关闭当前持有的句柄，并将内部句柄置为INVALID_HANDLE_VALUE。
     *          多次调用是安全的（第二次及以后无操作）。
     */
    void Close() noexcept {
        if (m_handle != INVALID_HANDLE_VALUE && m_handle != nullptr) {
            ::CloseHandle(m_handle);
            m_handle = INVALID_HANDLE_VALUE;
        }
    }

    /**
     * @brief 释放句柄所有权，返回原始句柄
     * @return 当前持有的原始句柄
     * @details 调用后SafeHandle不再管理该句柄，调用方必须负责关闭。
     *          用于需要将句柄传递给外部API且外部API负责关闭的场景。
     */
    HANDLE Release() noexcept {
        HANDLE temp = m_handle;
        m_handle = INVALID_HANDLE_VALUE;
        return temp;
    }

    /**
     * @brief 获取原始句柄（不转移所有权）
     * @return 当前持有的原始句柄
     * @note 返回的句柄仍由SafeHandle管理，调用方不得关闭
     */
    HANDLE Get() const noexcept {
        return m_handle;
    }

    /**
     * @brief 检查句柄是否有效
     * @return true  句柄有效（不是INVALID_HANDLE_VALUE且不是nullptr）
     * @return false 句柄无效
     */
    bool IsValid() const noexcept {
        return m_handle != INVALID_HANDLE_VALUE && m_handle != nullptr;
    }

    /**
     * @brief 布尔转换操作符
     * @return 句柄是否有效
     * @details 支持if (handle) { ... } 语法
     */
    explicit operator bool() const noexcept {
        return IsValid();
    }

    /**
     * @brief 获取内部句柄变量的地址（用于接收API输出）
     * @return 内部句柄变量的地址
     * @details 用于需要接收输出句柄的Windows API（如CreateProcessW的phProcess参数）
     *
     * @warning 调用前必须确保当前句柄已关闭或无效，否则旧句柄将泄漏
     */
    HANDLE* AddressOf() noexcept {
        return &m_handle;
    }

private:
    HANDLE m_handle;  ///< 内部持有的Windows句柄
};

// ---------------------------------------------------------------------------
// 远程内存安全包装类 (RAII)
// ---------------------------------------------------------------------------

/**
 * @class RemoteMemory
 * @brief 目标进程远程内存的RAII管理类
 * @details 管理通过VirtualAllocEx在远程进程中分配的内存，确保在对象销毁时
 *          自动调用VirtualFreeEx释放远程内存，防止目标进程内存泄漏。
 *
 *          安全特性:
 *          1. 析构时自动调用VirtualFreeEx
 *          2. 移动语义支持，禁止拷贝
 *          3. 支持提前释放和所有权转移
 *
 * @note 远程内存属于目标进程地址空间，必须通过VirtualFreeEx释放，
 *       不能在本进程调用free/delete
 * @warning 目标进程句柄必须在RemoteMemory销毁前保持有效
 */
class RemoteMemory {
public:
    /**
     * @brief 默认构造函数，创建空内存块
     */
    RemoteMemory() noexcept
        : m_processHandle(INVALID_HANDLE_VALUE)
        , m_remoteAddress(nullptr)
        , m_size(0)
    {}

    /**
     * @brief 从已分配的远程内存构造
     * @param[in] hProcess      目标进程句柄（必须有PROCESS_VM_OPERATION权限）
     * @param[in] remoteAddress 远程进程中的内存地址
     * @param[in] size          分配的内存大小（字节）
     *
     * @warning 传入的进程句柄必须在RemoteMemory生命周期内保持有效
     */
    RemoteMemory(HANDLE hProcess, LPVOID remoteAddress, SIZE_T size) noexcept
        : m_processHandle(hProcess)
        , m_remoteAddress(remoteAddress)
        , m_size(size)
    {}

    /**
     * @brief 析构函数，自动释放远程内存
     * @details 这是防止目标进程内存泄漏的核心机制。
     */
    ~RemoteMemory() noexcept {
        Free();
    }

    // 禁止拷贝
    RemoteMemory(const RemoteMemory&) = delete;
    RemoteMemory& operator=(const RemoteMemory&) = delete;

    /**
     * @brief 移动构造函数
     */
    RemoteMemory(RemoteMemory&& other) noexcept
        : m_processHandle(other.m_processHandle)
        , m_remoteAddress(other.m_remoteAddress)
        , m_size(other.m_size)
    {
        other.m_processHandle = INVALID_HANDLE_VALUE;
        other.m_remoteAddress = nullptr;
        other.m_size = 0;
    }

    /**
     * @brief 移动赋值运算符
     */
    RemoteMemory& operator=(RemoteMemory&& other) noexcept {
        if (this != &other) {
            Free();
            m_processHandle = other.m_processHandle;
            m_remoteAddress = other.m_remoteAddress;
            m_size = other.m_size;
            other.m_processHandle = INVALID_HANDLE_VALUE;
            other.m_remoteAddress = nullptr;
            other.m_size = 0;
        }
        return *this;
    }

    /**
     * @brief 显式释放远程内存
     * @details 调用VirtualFreeEx释放远程内存，并将内部状态重置。
     *          多次调用是安全的。
     */
    void Free() noexcept {
        if (m_remoteAddress != nullptr && m_processHandle != INVALID_HANDLE_VALUE) {
            ::VirtualFreeEx(m_processHandle, m_remoteAddress, 0, MEM_RELEASE);
            m_remoteAddress = nullptr;
            m_processHandle = INVALID_HANDLE_VALUE;
            m_size = 0;
        }
    }

    /**
     * @brief 获取远程内存地址
     * @return 远程进程中的内存地址
     */
    LPVOID GetAddress() const noexcept {
        return m_remoteAddress;
    }

    /**
     * @brief 获取分配的内存大小
     * @return 内存大小（字节）
     */
    SIZE_T GetSize() const noexcept {
        return m_size;
    }

    /**
     * @brief 检查内存是否有效
     * @return true  内存地址有效
     * @return false 内存已释放或未分配
     */
    bool IsValid() const noexcept {
        return m_remoteAddress != nullptr;
    }

    /**
     * @brief 布尔转换操作符
     */
    explicit operator bool() const noexcept {
        return IsValid();
    }

private:
    HANDLE m_processHandle;   ///< 目标进程句柄（释放内存时需要）
    LPVOID m_remoteAddress;   ///< 远程进程中的内存地址
    SIZE_T m_size;            ///< 分配的内存大小
};

// ---------------------------------------------------------------------------
// 路径工具
// ---------------------------------------------------------------------------

/**
 * @brief 获取当前可执行文件所在目录
 * @return 当前进程EXE所在目录的完整路径（包含末尾反斜杠）
 *
 * @details 使用GetModuleFileNameW获取当前模块路径，然后提取目录部分。
 *          返回的路径可用于拼接DLL路径等场景。
 *
 * @note 返回路径以反斜杠结尾，方便直接拼接文件名
 * @warning 路径长度受MAX_PATH限制（260字符），超长路径场景需特殊处理
 */
std::wstring GetExecutableDirectory() noexcept;

/**
 * @brief 拼接目录路径和文件名
 * @param[in] directory 目录路径（可以带或不带末尾分隔符）
 * @param[in] fileName  文件名
 * @return 拼接后的完整路径
 */
std::wstring JoinPath(const std::wstring& directory, const std::wstring& fileName) noexcept;

} // namespace Utils
} // namespace Launcher
} // namespace GameLauncher

#endif // LAUNCHER_UTILS_H
