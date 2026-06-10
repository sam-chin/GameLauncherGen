/**
 * @file ProcessLauncher.cpp
 * @brief 进程启动器实现
 * @details 实现以CREATE_SUSPENDED方式启动客户端、注入DLL、恢复线程的完整流程。
 *
 *          执行时序:
 *          1. 验证选项 -> 2. 构建命令行 -> 3. 创建挂起进程 -> 4. 注入DLL -> 5. 恢复线程 -> 6. 清理
 *
 *          关键安全设计:
 *          - 进程在注入完成前始终保持挂起状态，避免竞争条件
 *          - 使用SafeHandle RAII管理进程和线程句柄
 *          - 任何步骤失败都会自动终止已创建的进程（防止僵尸进程）
 *          - 详细的错误信息记录，便于问题诊断
 */

#include "ProcessLauncher.h"
#include <sstream>
#include <fstream>

// ---------------------------------------------------------------------------
// 命名空间定义
// ---------------------------------------------------------------------------

namespace GameLauncher {
namespace Launcher {

// ---------------------------------------------------------------------------
// 辅助函数：将启动结果码转换为字符串
// ---------------------------------------------------------------------------

const char* LaunchResultToString(LaunchResult result) noexcept {
    switch (result) {
        case LaunchResult::Success:              return "启动成功";
        case LaunchResult::InvalidParameter:     return "参数无效";
        case LaunchResult::InvalidServerConfig:  return "服务器配置无效";
        case LaunchResult::ClientPathNotFound:   return "客户端程序不存在";
        case LaunchResult::DllPathNotFound:      return "注入DLL不存在";
        case LaunchResult::CreateProcessFailed:  return "创建进程失败";
        case LaunchResult::ProcessInfoInvalid:   return "进程信息无效";
        case LaunchResult::InjectFailed:         return "DLL注入失败";
        case LaunchResult::ResumeThreadFailed:   return "恢复线程失败";
        case LaunchResult::SystemError:          return "系统错误";
        default:                                 return "未知启动结果";
    }
}

// ---------------------------------------------------------------------------
// ProcessLauncher 实现
// ---------------------------------------------------------------------------

/**
 * @brief 启动客户端进程并注入DLL的完整流程
 *
 * 详细执行步骤:
 * 1. 验证启动选项
 *    - 检查所有必填字段
 *    - 验证客户端程序文件存在
 *    - 验证DLL文件存在
 *    - 验证服务器配置有效
 *
 * 2. 构建命令行参数
 *    - 格式: "client.exe" ur;name=xxx;ip=xxx;port=xxx;ra=163.com
 *    - 客户端路径用引号包裹，处理包含空格的路径
 *
 * 3. 以挂起状态创建进程
 *    - 使用CreateProcessW + CREATE_SUSPENDED标志
 *    - 主线程创建后立即挂起，不会执行任何代码
 *    - 获取进程ID、进程句柄、主线程句柄
 *
 * 4. 注入DLL
 *    - 在挂起的进程中执行DLL注入
 *    - 注入完成后，DLL的DllMain已被执行（DLL_PROCESS_ATTACH）
 *
 * 5. 恢复主线程
 *    - 调用ResumeThread恢复主线程执行
 *    - 客户端开始正常启动流程
 *
 * 6. 清理资源
 *    - 关闭进程和线程句柄（客户端继续运行）
 *    - 更新最后错误信息
 */
LaunchResult ProcessLauncher::Launch(const LaunchOptions& options,
                                      DWORD* pProcessId) noexcept {
    // 重置上次错误信息
    m_lastErrorInfo.clear();

    // ========================================================================
    // 步骤1: 验证启动选项
    // ========================================================================
    LaunchResult validationResult = ValidateOptions(options);
    if (validationResult != LaunchResult::Success) {
        return validationResult;
    }

    // ========================================================================
    // 步骤2: 构建命令行参数
    // ========================================================================
    std::wstring commandLine = BuildCommandLine(options.clientPath, options.serverConfig);

    // ========================================================================
    // 步骤3: 以挂起状态创建进程
    // ========================================================================
    Utils::SafeHandle hProcess;
    Utils::SafeHandle hThread;
    DWORD processId = 0;

    LaunchResult createResult = CreateSuspendedProcess(
        options,
        commandLine,
        hProcess,
        hThread,
        processId
    );

    if (createResult != LaunchResult::Success) {
        // 创建进程失败，SafeHandle会自动清理句柄（虽然它们应该是无效的）
        return createResult;
    }

    // 保存进程ID给调用方
    if (pProcessId != nullptr) {
        *pProcessId = processId;
    }

    // ========================================================================
    // 步骤4: 在挂起的进程中注入DLL
    // ========================================================================
    // 此时进程已创建但主线程挂起，是注入的最佳时机
    // 注入的DLL的DllMain会在CreateRemoteThread创建的线程中执行
    // 执行时机是DLL_PROCESS_ATTACH，此时主线程尚未开始执行
    LaunchResult injectResult = PerformInjection(hProcess.Get(), options.dllPath);
    if (injectResult != LaunchResult::Success) {
        // 注入失败，需要终止已创建的进程
        // 如果不终止，会留下一个挂起的僵尸进程
        // TerminateProcess是强制终止，不执行清理代码
        // 但由于进程尚未开始执行，这是安全的
        ::TerminateProcess(hProcess.Get(), static_cast<UINT>(-1));

        // 记录详细错误信息
        m_lastErrorInfo = L"DLL注入失败: " + Utils::GetLastErrorString();
        return injectResult;
    }

    // ========================================================================
    // 步骤5: 恢复主线程
    // ========================================================================
    // 注入完成后，恢复主线程让客户端正常启动
    // 此时DLL已加载，可以拦截客户端的初始化过程
    LaunchResult resumeResult = ResumeMainThread(hThread.Get());
    if (resumeResult != LaunchResult::Success) {
        // 恢复线程失败，终止进程
        ::TerminateProcess(hProcess.Get(), static_cast<UINT>(-1));
        return resumeResult;
    }

    // ========================================================================
    // 步骤6: 清理资源
    // ========================================================================
    // hProcess和hThread在此处离开作用域
    // SafeHandle的析构函数自动调用CloseHandle
    // 关闭句柄不会终止进程，只是本进程不再持有对目标进程的引用
    // 目标进程和线程继续正常运行

    // 显式关闭使意图更清晰（虽然析构会处理）
    hThread.Close();
    hProcess.Close();

    return LaunchResult::Success;
}

/**
 * @brief 验证启动选项的有效性
 *
 * 验证项:
 * 1. 选项整体有效性（所有必填字段非空）
 * 2. 客户端程序文件存在且不是目录
 * 3. DLL文件存在且不是目录
 * 4. 服务器配置有效
 */
LaunchResult ProcessLauncher::ValidateOptions(const LaunchOptions& options) noexcept {
    // 检查选项整体有效性
    if (!options.IsValid()) {
        m_lastErrorInfo = L"启动选项无效：存在空必填字段";
        return LaunchResult::InvalidParameter;
    }

    // 验证客户端程序文件存在
    DWORD clientAttr = ::GetFileAttributesW(options.clientPath.c_str());
    if (clientAttr == INVALID_FILE_ATTRIBUTES) {
        m_lastErrorInfo = L"客户端程序不存在: " + options.clientPath;
        return LaunchResult::ClientPathNotFound;
    }
    if (clientAttr & FILE_ATTRIBUTE_DIRECTORY) {
        m_lastErrorInfo = L"客户端路径指向目录而非文件: " + options.clientPath;
        return LaunchResult::ClientPathNotFound;
    }

    // 验证DLL文件存在
    DWORD dllAttr = ::GetFileAttributesW(options.dllPath.c_str());
    if (dllAttr == INVALID_FILE_ATTRIBUTES) {
        m_lastErrorInfo = L"注入DLL不存在: " + options.dllPath;
        return LaunchResult::DllPathNotFound;
    }
    if (dllAttr & FILE_ATTRIBUTE_DIRECTORY) {
        m_lastErrorInfo = L"DLL路径指向目录而非文件: " + options.dllPath;
        return LaunchResult::DllPathNotFound;
    }

    // 验证服务器配置
    if (!options.serverConfig.IsValid()) {
        m_lastErrorInfo = L"服务器配置无效";
        return LaunchResult::InvalidServerConfig;
    }

    return LaunchResult::Success;
}

/**
 * @brief 构建完整的命令行字符串
 *
 * 格式说明:
 * 命令行由两部分组成：
 * 1. 可执行文件路径（用引号包裹，处理含空格的路径）
 * 2. 服务器参数字符串（ur;name=xxx;ip=xxx;port=xxx;ra=163.com）
 *
 * 最终格式:
 *   "C:\\Path\\client.exe" ur;name=艾欧尼亚;ip=192.168.1.100;port=7777;ra=163.com
 *
 * 安全考虑:
 * - 路径用引号包裹，防止路径中的空格被解析为参数分隔符
 * - 参数格式固定，避免命令注入风险
 */
std::wstring ProcessLauncher::BuildCommandLine(const std::wstring& clientPath,
                                                const ServerConfig& serverConfig) noexcept {
    std::wstringstream cmdLine;

    // 客户端路径用引号包裹
    // 这是必须的：如果路径包含空格（如"Program Files"），
    // 不用引号会导致CreateProcessW将路径错误解析
    cmdLine << L'"' << clientPath << L'"';

    // 添加空格分隔参数
    cmdLine << L' ';

    // 添加服务器参数字符串
    cmdLine << serverConfig.ToParameterString();

    return cmdLine.str();
}

/**
 * @brief 以挂起状态创建进程
 *
 * CreateProcessW参数详解:
 * - lpApplicationName:  可执行文件路径（可为nullptr，从命令行解析）
 * - lpCommandLine:      完整命令行字符串（可修改的缓冲区）
 * - lpProcessAttributes: 进程安全属性（nullptr使用默认）
 * - lpThreadAttributes:  线程安全属性（nullptr使用默认）
 * - bInheritHandles:    是否继承句柄（FALSE，不继承）
 * - dwCreationFlags:    创建标志（CREATE_SUSPENDED，挂起主线程）
 * - lpEnvironment:      环境变量（nullptr，继承父进程）
 * - lpCurrentDirectory: 工作目录（nullptr，使用父进程工作目录）
 * - lpStartupInfo:      启动信息（必须初始化）
 * - lpProcessInformation: 进程信息输出（接收句柄和ID）
 *
 * CREATE_SUSPENDED标志的作用:
 * 主线程创建后处于挂起状态，不会执行任何代码。
 * 这为我们提供了注入DLL的时间窗口，避免竞争条件。
 *
 * 错误处理:
 * - 创建失败：返回CreateProcessFailed，记录错误信息
 * - 创建成功但信息无效：返回ProcessInfoInvalid，关闭句柄
 */
LaunchResult ProcessLauncher::CreateSuspendedProcess(const LaunchOptions& options,
                                                      const std::wstring& commandLine,
                                                      Utils::SafeHandle& hProcess,
                                                      Utils::SafeHandle& hThread,
                                                      DWORD& processId) noexcept {
    // CreateProcessW需要可修改的命令行缓冲区
    // 因此需要复制到非const缓冲区
    std::vector<wchar_t> cmdLineBuffer(commandLine.begin(), commandLine.end());
    cmdLineBuffer.push_back(L'\0');  // 确保以null结尾

    // 初始化STARTUPINFOW结构体
    // 必须清零，然后设置cb字段为结构体大小
    STARTUPINFOW startupInfo = { 0 };
    startupInfo.cb = sizeof(STARTUPINFOW);

    // 进程信息结构体，接收CreateProcessW的输出
    PROCESS_INFORMATION processInfo = { 0 };

    // 确定工作目录
    const wchar_t* workingDirectory = nullptr;
    std::wstring workDir;
    if (!options.workingDir.empty()) {
        workingDirectory = options.workingDir.c_str();
    }
    // 如果未指定工作目录，使用客户端所在目录
    else {
        size_t lastSlash = options.clientPath.find_last_of(L"\\/");
        if (lastSlash != std::wstring::npos) {
            workDir = options.clientPath.substr(0, lastSlash);
            workingDirectory = workDir.c_str();
        }
    }

    // 以挂起状态创建进程
    BOOL createResult = ::CreateProcessW(
        nullptr,                    // 从命令行解析可执行文件路径
        cmdLineBuffer.data(),       // 命令行（可修改的缓冲区）
        nullptr,                    // 默认进程安全属性
        nullptr,                    // 默认线程安全属性
        FALSE,                      // 不继承句柄
        CREATE_SUSPENDED,           // 创建标志：挂起主线程
        nullptr,                    // 继承父进程环境变量
        workingDirectory,           // 工作目录
        &startupInfo,               // 启动信息
        &processInfo                // 进程信息输出
    );

    if (!createResult) {
        // 创建进程失败，记录详细错误信息
        m_lastErrorInfo = L"CreateProcessW失败: " + Utils::GetLastErrorString();
        return LaunchResult::CreateProcessFailed;
    }

    // 验证进程信息有效性
    if (processInfo.hProcess == nullptr || processInfo.hProcess == INVALID_HANDLE_VALUE) {
        // 进程句柄无效，但进程可能已创建
        // 尝试关闭线程句柄（如果有效）
        if (processInfo.hThread != nullptr && processInfo.hThread != INVALID_HANDLE_VALUE) {
            ::CloseHandle(processInfo.hThread);
        }
        m_lastErrorInfo = L"进程创建后句柄无效";
        return LaunchResult::ProcessInfoInvalid;
    }

    if (processInfo.hThread == nullptr || processInfo.hThread == INVALID_HANDLE_VALUE) {
        // 线程句柄无效，关闭进程句柄
        ::CloseHandle(processInfo.hProcess);
        m_lastErrorInfo = L"进程创建后线程句柄无效";
        return LaunchResult::ProcessInfoInvalid;
    }

    if (processInfo.dwProcessId == 0) {
        // 进程ID无效，关闭句柄
        ::CloseHandle(processInfo.hProcess);
        ::CloseHandle(processInfo.hThread);
        m_lastErrorInfo = L"进程创建后进程ID无效";
        return LaunchResult::ProcessInfoInvalid;
    }

    // 将句柄转移到SafeHandle中（取得所有权）
    // SafeHandle会自动管理句柄生命周期
    // 注意：由于SafeHandle禁用了拷贝构造和拷贝赋值，必须使用移动语义
    // 先创建临时对象，然后通过移动赋值转移所有权
    hProcess = Utils::SafeHandle(processInfo.hProcess);
    hThread = Utils::SafeHandle(processInfo.hThread);
    processId = processInfo.dwProcessId;

    // processInfo中的句柄已被SafeHandle取得所有权
    // 不需要手动关闭

    return LaunchResult::Success;
}

/**
 * @brief 在目标进程中执行DLL注入
 *
 * 委托DllInjector执行实际的注入操作
 * 将注入结果转换为启动结果码
 */
LaunchResult ProcessLauncher::PerformInjection(HANDLE hProcess,
                                                const std::wstring& dllPath) noexcept {
    DllInjector injector;
    DWORD exitCode = 0;

    InjectResult injectResult = injector.Inject(hProcess, dllPath, &exitCode);

    if (injectResult != InjectResult::Success) {
        // 转换注入结果为启动结果
        m_lastErrorInfo = L"注入失败: " + Utils::GetLastErrorString();
        return LaunchResult::InjectFailed;
    }

    // 注入成功，exitCode包含加载的DLL模块句柄
    // 在32位进程中，模块句柄是32位值
    // 这里不需要使用exitCode，但已获取以备调试需要
    (void)exitCode;  // 显式标记为已使用，避免编译器警告

    return LaunchResult::Success;
}

/**
 * @brief 恢复目标进程的主线程
 *
 * ResumeThread的作用:
 * 减少主线程的挂起计数。CREATE_SUSPENDED创建时挂起计数为1，
 * ResumeThread将其减为0，线程开始执行。
 *
 * 返回值:
 * - 成功：返回之前的挂起计数（应该为1）
 * - 失败：返回(DWORD)-1
 *
 * 错误处理:
 * - 恢复失败：记录错误，返回ResumeThreadFailed
 * - 恢复成功：线程开始执行，客户端正常启动
 */
LaunchResult ProcessLauncher::ResumeMainThread(HANDLE hThread) noexcept {
    DWORD suspendCount = ::ResumeThread(hThread);

    if (suspendCount == static_cast<DWORD>(-1)) {
        // ResumeThread失败
        // 可能原因：句柄无效、句柄权限不足、线程已终止
        m_lastErrorInfo = L"ResumeThread失败: " + Utils::GetLastErrorString();
        return LaunchResult::ResumeThreadFailed;
    }

    // 成功恢复，suspendCount是之前的挂起计数
    // 对于CREATE_SUSPENDED创建的线程，应该为1
    // 如果不为1，可能表示线程状态异常（但通常不影响功能）
    if (suspendCount != 1) {
        // 记录警告信息（非致命）
        // 线程可能已被其他操作恢复或挂起
        // 这里仅记录，不视为错误
    }

    return LaunchResult::Success;
}

} // namespace Launcher
} // namespace GameLauncher
