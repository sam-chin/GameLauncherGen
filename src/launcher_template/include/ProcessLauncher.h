/**
 * @file ProcessLauncher.h
 * @brief 进程启动器接口定义
 * @details 本模块负责以CREATE_SUSPENDED方式启动目标客户端进程，
 *          并在注入DLL后恢复主线程执行。核心流程：
 *
 *          1. 构建命令行参数（包含服务器配置）
 *          2. 以挂起状态创建进程（CreateProcessW + CREATE_SUSPENDED）
 *          3. 在挂起的进程中注入DLL（调用DllInjector）
 *          4. 恢复主线程执行（ResumeThread）
 *          5. 清理资源（关闭句柄）
 *
 *          安全设计:
 *          - 使用SafeHandle RAII类管理所有Windows句柄
 *          - 任何步骤失败都会自动清理已创建的资源
 *          - 进程在注入完成前保持挂起，避免竞争条件
 *
 * @note 目标客户端必须是32位Windows可执行程序
 * @warning 启动器需要足够的权限才能对目标进程进行注入操作
 */

#pragma once

#ifndef PROCESS_LAUNCHER_H
#define PROCESS_LAUNCHER_H

#include "DllInjector.h"
#include "ServerConfig.h"
#include "LauncherUtils.h"
#include <Windows.h>
#include <string>
#include <memory>

// ---------------------------------------------------------------------------
// 命名空间定义
// ---------------------------------------------------------------------------

namespace GameLauncher {
namespace Launcher {

// ---------------------------------------------------------------------------
// 启动结果枚举
// ---------------------------------------------------------------------------

/**
 * @enum LaunchResult
 * @brief 进程启动操作结果码
 */
enum class LaunchResult : int32_t {
    Success = 0,                        ///< 启动成功

    // 参数/配置错误 (1 - 19)
    InvalidParameter = 1,               ///< 参数无效
    InvalidServerConfig = 2,            ///< 服务器配置无效
    ClientPathNotFound = 3,             ///< 客户端程序路径不存在
    DllPathNotFound = 4,                ///< 注入DLL路径不存在

    // 进程创建错误 (20 - 39)
    CreateProcessFailed = 20,           ///< CreateProcessW调用失败
    ProcessInfoInvalid = 21,            ///< 进程创建后信息无效

    // 注入错误 (40 - 59)
    InjectFailed = 40,                  ///< DLL注入失败

    // 线程恢复错误 (60 - 79)
    ResumeThreadFailed = 60,            ///< ResumeThread调用失败

    // 系统错误 (80 - 99)
    SystemError = 80,                   ///< 其他系统错误
};

/**
 * @brief 将启动结果码转换为可读字符串
 * @param[in] result 启动结果码
 * @return 对应的错误描述（UTF-8编码）
 */
const char* LaunchResultToString(LaunchResult result) noexcept;

// ---------------------------------------------------------------------------
// 启动选项结构体
// ---------------------------------------------------------------------------

/**
 * @struct LaunchOptions
 * @brief 进程启动选项配置
 * @details 封装启动客户端进程所需的所有参数，便于扩展和维护。
 */
struct LaunchOptions {
    std::wstring clientPath;    ///< 客户端程序(client.exe)的完整路径
    std::wstring dllPath;       ///< 要注入的DLL完整路径
    std::wstring workingDir;    ///< 工作目录（空表示使用客户端所在目录）
    ServerConfig serverConfig;  ///< 服务器连接配置

    /**
     * @brief 验证启动选项的有效性
     * @return true  所有必填字段均有效
     * @return false 至少一个字段无效
     */
    bool IsValid() const noexcept {
        return !clientPath.empty()
            && !dllPath.empty()
            && serverConfig.IsValid();
    }
};

// ---------------------------------------------------------------------------
// 进程启动器类
// ---------------------------------------------------------------------------

/**
 * @class ProcessLauncher
 * @brief 游戏客户端进程启动器
 * @details 负责以挂起状态启动客户端，注入DLL，然后恢复执行。
 *
 *          使用示例:
 *          @code
 *          ProcessLauncher launcher;
 *          LaunchOptions options;
 *          options.clientPath = L"C:\\Game\\client.exe";
 *          options.dllPath = L"C:\\Game\\hook.dll";
 *          options.serverConfig = ServerConfig(L"艾欧尼亚", L"192.168.1.100", 7777);
 *
 *          auto result = launcher.Launch(options);
 *          if (result != LaunchResult::Success) {
 *              // 处理错误
 *          }
 *          @endcode
 */
class ProcessLauncher {
public:
    /**
     * @brief 默认构造函数
     */
    ProcessLauncher() noexcept = default;

    // 禁止拷贝和移动
    ProcessLauncher(const ProcessLauncher&) = delete;
    ProcessLauncher& operator=(const ProcessLauncher&) = delete;
    ProcessLauncher(ProcessLauncher&&) = delete;
    ProcessLauncher& operator=(ProcessLauncher&&) = delete;

    /**
     * @brief 启动客户端进程并注入DLL
     * @param[in] options 启动选项配置
     * @param[out] pProcessId 可选输出参数，接收创建的进程ID
     * @return 启动结果码
     *
     * @details 完整启动流程:
     *          1. 验证启动选项（路径存在、配置有效）
     *          2. 构建命令行参数: client.exe ur;name=xxx;ip=xxx;port=xxx;ra=163.com
     *          3. 以CREATE_SUSPENDED创建进程，主线程处于挂起状态
     *          4. 在挂起的进程中注入DLL
     *          5. 恢复主线程，客户端开始正常执行
     *          6. 关闭进程和线程句柄（客户端继续运行）
     *
     * @note 即使启动失败，也会清理所有已分配资源
     * @warning 本函数阻塞执行，直到注入完成或失败。注入操作可能需要数秒。
     */
    LaunchResult Launch(const LaunchOptions& options,
                        DWORD* pProcessId = nullptr) noexcept;

    /**
     * @brief 获取最后一次操作的详细错误信息
     * @return 包含Windows错误码和描述的信息字符串
     *
     * @note 仅在Launch返回非Success时包含有效信息
     */
    const std::wstring& GetLastErrorInfo() const noexcept {
        return m_lastErrorInfo;
    }

private:
    /**
     * @brief 验证启动选项
     * @param[in] options 要验证的选项
     * @return 验证结果
     */
    LaunchResult ValidateOptions(const LaunchOptions& options) noexcept;

    /**
     * @brief 构建完整的命令行字符串
     * @param[in] clientPath  客户端路径
     * @param[in] serverConfig 服务器配置
     * @return 构建好的命令行字符串
     *
     * @details 格式: "client.exe" ur;name=xxx;ip=xxx;port=xxx;ra=163.com
     *          客户端路径用引号包裹以处理包含空格的路径
     */
    std::wstring BuildCommandLine(const std::wstring& clientPath,
                                   const ServerConfig& serverConfig) noexcept;

    /**
     * @brief 以挂起状态创建进程
     * @param[in] options      启动选项
     * @param[in] commandLine  命令行字符串
     * @param[out] hProcess    输出进程句柄（SafeHandle包装）
     * @param[out] hThread     输出主线程句柄（SafeHandle包装）
     * @param[out] processId   输出进程ID
     * @return 操作结果
     */
    LaunchResult CreateSuspendedProcess(const LaunchOptions& options,
                                        const std::wstring& commandLine,
                                        Utils::SafeHandle& hProcess,
                                        Utils::SafeHandle& hThread,
                                        DWORD& processId) noexcept;

    /**
     * @brief 在目标进程中注入DLL
     * @param[in] hProcess 目标进程句柄
     * @param[in] dllPath  DLL路径
     * @return 操作结果
     */
    LaunchResult PerformInjection(HANDLE hProcess,
                                   const std::wstring& dllPath) noexcept;

    /**
     * @brief 恢复目标进程的主线程
     * @param[in] hThread 主线程句柄
     * @return 操作结果
     */
    LaunchResult ResumeMainThread(HANDLE hThread) noexcept;

    std::wstring m_lastErrorInfo;   ///< 最后一次错误的详细信息
};

} // namespace Launcher
} // namespace GameLauncher

#endif // PROCESS_LAUNCHER_H
