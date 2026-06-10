/**
 * @file DllInjector.h
 * @brief DLL远程线程注入器接口定义
 * @details 本模块实现基于CreateRemoteThread的DLL注入技术，核心流程：
 *
 *          1. 在目标进程中分配内存（VirtualAllocEx）
 *          2. 将DLL路径写入远程内存（WriteProcessMemory）
 *          3. 获取LoadLibraryW地址（GetProcAddress + GetModuleHandleW）
 *          4. 创建远程线程执行LoadLibraryW（CreateRemoteThread）
 *          5. 等待线程完成（WaitForSingleObject）
 *          6. 清理远程内存（VirtualFreeEx）
 *
 *          安全设计:
 *          - 所有资源使用RAII包装，确保异常安全
 *          - 失败时自动清理已分配资源
 *          - 详细的错误码返回，便于问题诊断
 *
 * @note 本注入器仅支持32位进程注入32位DLL（同架构）
 * @warning DLL注入属于敏感操作，仅应在合法授权场景下使用
 */

#pragma once

#ifndef DLL_INJECTOR_H
#define DLL_INJECTOR_H

#include "LauncherUtils.h"
#include <Windows.h>
#include <string>

// ---------------------------------------------------------------------------
// 命名空间定义
// ---------------------------------------------------------------------------

namespace GameLauncher {
namespace Launcher {

// ---------------------------------------------------------------------------
// 注入结果枚举
// ---------------------------------------------------------------------------

/**
 * @enum InjectResult
 * @brief DLL注入操作结果码
 * @details 详细的错误分类，便于调用方进行针对性处理
 */
enum class InjectResult : int32_t {
    Success = 0,                        ///< 注入成功

    // 参数/前置条件错误 (1 - 19)
    InvalidParameter = 1,               ///< 参数无效（空路径、空句柄等）
    DllPathTooLong = 2,                 ///< DLL路径超出系统限制(MAX_PATH)
    DllFileNotFound = 3,                ///< DLL文件在磁盘上不存在

    // 内存操作错误 (20 - 39)
    RemoteAllocFailed = 20,             ///< VirtualAllocEx分配远程内存失败
    RemoteWriteFailed = 21,             ///< WriteProcessMemory写入远程内存失败
    RemoteFreeFailed = 22,              ///< VirtualFreeEx释放远程内存失败（非致命）

    // 地址解析错误 (40 - 59)
    Kernel32NotFound = 40,              ///< 无法获取kernel32.dll模块句柄
    LoadLibraryWNotFound = 41,          ///< 无法在kernel32.dll中找到LoadLibraryW导出函数

    // 线程操作错误 (60 - 79)
    CreateRemoteThreadFailed = 60,      ///< 创建远程线程失败
    WaitForThreadFailed = 61,           ///< 等待远程线程完成失败
    ThreadExitCodeError = 62,           ///< 远程线程执行LoadLibraryW返回错误码

    // 系统错误 (80 - 99)
    SystemError = 80,                   ///< 其他Windows系统错误
};

/**
 * @brief 将注入结果码转换为可读字符串
 * @param[in] result 注入结果码
 * @return 对应的错误描述（UTF-8编码）
 */
const char* InjectResultToString(InjectResult result) noexcept;

// ---------------------------------------------------------------------------
// DLL注入器类
// ---------------------------------------------------------------------------

/**
 * @class DllInjector
 * @brief DLL远程线程注入器
 * @details 封装完整的DLL注入流程，提供线程安全的注入接口。
 *
 *          使用示例:
 *          @code
 *          DllInjector injector;
 *          auto result = injector.Inject(hProcess, L"C:\\path\\to\\hook.dll");
 *          if (result != InjectResult::Success) {
 *              // 处理错误
 *          }
 *          @endcode
 *
 * @note 本类无状态，所有成员函数均为const，可多线程安全使用
 * @warning 注入操作需要目标进程句柄具有PROCESS_CREATE_THREAD、
 *          PROCESS_QUERY_INFORMATION、PROCESS_VM_OPERATION、
 *          PROCESS_VM_WRITE、PROCESS_VM_READ权限
 */
class DllInjector {
public:
    /**
     * @brief 默认构造函数
     */
    DllInjector() noexcept = default;

    // 禁止拷贝和移动（本类无状态，但保持语义清晰）
    DllInjector(const DllInjector&) = delete;
    DllInjector& operator=(const DllInjector&) = delete;
    DllInjector(DllInjector&&) = delete;
    DllInjector& operator=(DllInjector&&) = delete;

    /**
     * @brief 执行DLL远程线程注入
     * @param[in] hProcess   目标进程句柄（必须具有足够权限）
     * @param[in] dllPath    要注入的DLL完整路径（宽字符）
     * @param[out] pExitCode 可选输出参数，接收远程线程的退出码
     *                       （即LoadLibraryW的返回值，成功时为模块句柄）
     * @return 注入结果码，Success表示注入成功
     *
     * @details 完整注入流程:
     *          1. 参数校验（路径非空、文件存在、路径长度检查）
     *          2. 在目标进程中分配可读写的内存页
     *          3. 将DLL路径（包含结尾空字符）写入远程内存
     *          4. 获取kernel32.dll中LoadLibraryW的地址
     *          5. 创建远程线程，线程入口为LoadLibraryW，参数为DLL路径地址
     *          6. 等待线程执行完成（最多等待30秒）
     *          7. 获取线程退出码，确认LoadLibraryW是否成功
     *          8. 释放远程分配的内存
     *          9. 无论成功与否，清理所有资源
     *
     * @note 即使注入失败，本函数也会尽力清理已分配的资源
     * @warning 目标进程和本进程必须是相同架构（均为32位）
     */
    InjectResult Inject(HANDLE hProcess,
                        const std::wstring& dllPath,
                        DWORD* pExitCode = nullptr) const noexcept;

private:
    /**
     * @brief 验证DLL路径的有效性
     * @param[in] dllPath 要验证的DLL路径
     * @return 验证通过返回Success，否则返回具体错误码
     *
     * @details 检查项:
     *          - 路径非空
     *          - 路径长度不超过MAX_PATH
     *          - DLL文件在磁盘上存在（使用GetFileAttributesW）
     */
    InjectResult ValidateDllPath(const std::wstring& dllPath) const noexcept;

    /**
     * @brief 在目标进程中分配并写入DLL路径
     * @param[in] hProcess   目标进程句柄
     * @param[in] dllPath    DLL路径字符串
     * @param[out] ppRemoteAddr 输出分配的远程内存地址
     * @return 操作结果
     *
     * @details 分配内存大小为 (dllPath长度 + 1) * sizeof(wchar_t)，
     *          确保包含结尾的空字符。
     */
    InjectResult AllocateAndWritePath(HANDLE hProcess,
                                      const std::wstring& dllPath,
                                      LPVOID* ppRemoteAddr) const noexcept;

    /**
     * @brief 获取LoadLibraryW在目标进程中的地址
     * @return LoadLibraryW函数地址，获取失败返回nullptr
     *
     * @details 利用Windows的DLL基址随机化(ASLR)在同架构进程中
     *          kernel32.dll的加载地址相同这一特性，直接在本进程获取
     *          地址即可用于目标进程。
     *
     * @note 此方法仅在32位进程注入32位DLL时有效
     * @warning 64位进程注入32位DLL（或反之）时，地址空间不兼容，
     *          必须使用其他技术（如WoW64层操作）
     */
    LPTHREAD_START_ROUTINE GetLoadLibraryWAddress() const noexcept;

    /**
     * @brief 创建远程线程并等待其完成
     * @param[in] hProcess      目标进程句柄
     * @param[in] pThreadProc   线程入口函数地址（LoadLibraryW）
     * @param[in] pRemotePath   远程内存中的DLL路径地址
     * @param[out] pExitCode    输出线程退出码
     * @param[in] timeoutMs     等待超时时间（毫秒）
     * @return 操作结果
     */
    InjectResult CreateAndWaitForRemoteThread(HANDLE hProcess,
                                               LPTHREAD_START_ROUTINE pThreadProc,
                                               LPVOID pRemotePath,
                                               DWORD* pExitCode,
                                               DWORD timeoutMs = 30000) const noexcept;
};

} // namespace Launcher
} // namespace GameLauncher

#endif // DLL_INJECTOR_H
