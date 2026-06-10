/**
 * @file DllInjector.cpp
 * @brief DLL远程线程注入器实现
 * @details 实现基于CreateRemoteThread的经典DLL注入技术。
 *
 *          核心注入原理:
 *          在目标进程中创建一个线程，该线程的执行入口是kernel32.dll中的
 *          LoadLibraryW函数，参数是远程内存中存放的DLL路径字符串。
 *          由于Windows系统DLL（如kernel32.dll）在所有进程中加载到相同
 *          的虚拟地址（在同架构下），因此本进程中获取的LoadLibraryW地址
 *          可以直接用于目标进程。
 *
 *          安全考虑:
 *          1. 所有资源使用RAII管理（RemoteMemory）
 *          2. 失败时自动清理已分配资源
 *          3. 等待线程完成后再清理，确保LoadLibraryW已执行完毕
 *          4. 获取线程退出码验证LoadLibraryW是否成功
 */

#include "DllInjector.h"
#include <cstdio>

// ---------------------------------------------------------------------------
// 命名空间定义
// ---------------------------------------------------------------------------

namespace GameLauncher {
namespace Launcher {

// ---------------------------------------------------------------------------
// 辅助函数：将注入结果码转换为字符串
// ---------------------------------------------------------------------------

const char* InjectResultToString(InjectResult result) noexcept {
    switch (result) {
        case InjectResult::Success:                return "注入成功";
        case InjectResult::InvalidParameter:       return "参数无效";
        case InjectResult::DllPathTooLong:         return "DLL路径过长";
        case InjectResult::DllFileNotFound:        return "DLL文件不存在";
        case InjectResult::RemoteAllocFailed:      return "远程内存分配失败";
        case InjectResult::RemoteWriteFailed:      return "远程内存写入失败";
        case InjectResult::RemoteFreeFailed:       return "远程内存释放失败";
        case InjectResult::Kernel32NotFound:       return "无法获取kernel32.dll模块句柄";
        case InjectResult::LoadLibraryWNotFound:   return "无法找到LoadLibraryW导出函数";
        case InjectResult::CreateRemoteThreadFailed: return "创建远程线程失败";
        case InjectResult::WaitForThreadFailed:    return "等待远程线程失败";
        case InjectResult::ThreadExitCodeError:    return "远程线程执行失败（LoadLibraryW返回错误）";
        case InjectResult::SystemError:            return "系统错误";
        default:                                   return "未知注入结果";
    }
}

// ---------------------------------------------------------------------------
// DllInjector 实现
// ---------------------------------------------------------------------------

/**
 * @brief 执行完整的DLL远程线程注入流程
 *
 * 详细执行步骤:
 * 1. 参数校验
 *    - 检查进程句柄有效性
 *    - 检查DLL路径非空
 *    - 检查路径长度不超过MAX_PATH
 *    - 验证DLL文件在磁盘上存在
 *
 * 2. 远程内存分配与写入
 *    - 计算所需内存大小：(路径长度 + 1) * sizeof(wchar_t)
 *    - 使用VirtualAllocEx在目标进程中分配可读写的内存页
 *    - 使用WriteProcessMemory将DLL路径（含结尾\0）写入远程内存
 *
 * 3. 获取LoadLibraryW地址
 *    - 调用GetModuleHandleW(L"kernel32.dll")获取模块句柄
 *    - 调用GetProcAddress获取LoadLibraryW的函数指针
 *    - 将函数指针转换为LPTHREAD_START_ROUTINE类型
 *
 * 4. 创建远程线程
 *    - 调用CreateRemoteThread创建线程
 *    - 线程入口 = LoadLibraryW地址
 *    - 线程参数 = 远程内存中的DLL路径地址
 *
 * 5. 等待线程完成
 *    - 使用WaitForSingleObject等待线程结束
 *    - 设置30秒超时，防止无限等待
 *
 * 6. 验证结果
 *    - 调用GetExitCodeThread获取线程退出码
 *    - 退出码即LoadLibraryW的返回值（模块句柄）
 *    - 如果退出码为0或STILL_ACTIVE，表示加载失败
 *
 * 7. 清理资源
 *    - 关闭线程句柄
 *    - 释放远程分配的内存
 *    - 无论成功与否，确保所有资源被释放
 */
InjectResult DllInjector::Inject(HANDLE hProcess,
                                  const std::wstring& dllPath,
                                  DWORD* pExitCode) const noexcept {
    // ========================================================================
    // 步骤1: 参数校验
    // ========================================================================
    InjectResult validationResult = ValidateDllPath(dllPath);
    if (validationResult != InjectResult::Success) {
        return validationResult;
    }

    // 验证进程句柄有效性
    if (hProcess == nullptr || hProcess == INVALID_HANDLE_VALUE) {
        return InjectResult::InvalidParameter;
    }

    // ========================================================================
    // 步骤2: 在目标进程中分配内存并写入DLL路径
    // ========================================================================
    LPVOID pRemotePath = nullptr;
    InjectResult allocResult = AllocateAndWritePath(hProcess, dllPath, &pRemotePath);
    if (allocResult != InjectResult::Success) {
        // 分配或写入失败，此时远程内存可能已分配也可能未分配
        // AllocateAndWritePath内部已处理清理
        return allocResult;
    }

    // 使用RAII包装远程内存，确保后续任何退出路径都会释放内存
    // 这是防止内存泄漏的关键：即使后续步骤抛出异常或提前返回，
    // RemoteMemory的析构函数也会调用VirtualFreeEx
    Utils::RemoteMemory remoteMem(hProcess, pRemotePath,
                                   (dllPath.length() + 1) * sizeof(wchar_t));

    // ========================================================================
    // 步骤3: 获取LoadLibraryW函数地址
    // ========================================================================
    LPTHREAD_START_ROUTINE pLoadLibraryW = GetLoadLibraryWAddress();
    if (pLoadLibraryW == nullptr) {
        // 获取地址失败，remoteMem析构时会自动释放远程内存
        return InjectResult::LoadLibraryWNotFound;
    }

    // ========================================================================
    // 步骤4: 创建远程线程并等待其完成
    // ========================================================================
    DWORD threadExitCode = 0;
    InjectResult threadResult = CreateAndWaitForRemoteThread(
        hProcess,
        pLoadLibraryW,
        pRemotePath,
        &threadExitCode,
        30000  // 30秒超时
    );

    // 将退出码返回给调用方（如果请求了）
    if (pExitCode != nullptr) {
        *pExitCode = threadExitCode;
    }

    // ========================================================================
    // 步骤5: 验证注入结果
    // ========================================================================
    if (threadResult != InjectResult::Success) {
        // 线程创建或等待失败，remoteMem析构时自动释放远程内存
        return threadResult;
    }

    // 检查LoadLibraryW的返回值
    // 成功时返回加载的DLL模块句柄（非零值）
    // 失败时返回NULL (0)
    // STILL_ACTIVE (259) 表示线程仍在运行（理论上不应出现，因为已等待完成）
    if (threadExitCode == 0) {
        // LoadLibraryW返回NULL，表示加载DLL失败
        // 可能原因：DLL依赖缺失、DLL入口点DllMain返回FALSE、架构不匹配等
        // remoteMem析构时自动释放远程内存
        return InjectResult::ThreadExitCodeError;
    }

    if (threadExitCode == STILL_ACTIVE) {
        // 线程仍在运行，这是异常情况（等待应该已确保线程结束）
        // 可能是WaitForSingleObject被异常中断
        // remoteMem析构时自动释放远程内存
        return InjectResult::WaitForThreadFailed;
    }

    // ========================================================================
    // 步骤6: 清理资源
    // ========================================================================
    // remoteMem在此处离开作用域，析构函数自动调用VirtualFreeEx
    // 释放目标进程中的远程内存
    // 这是内存清理的关键：无论前面的步骤成功或失败，此处的释放都会执行

    // 显式释放（可选，因为析构会处理，但显式释放使意图更清晰）
    remoteMem.Free();

    return InjectResult::Success;
}

/**
 * @brief 验证DLL路径的有效性
 *
 * 检查项详细说明:
 * 1. 路径非空: 空字符串无法指向有效文件
 * 2. 路径长度: Windows的MAX_PATH为260字符，超长路径需要特殊前缀(\\?\)
 * 3. 文件存在: 使用GetFileAttributesW验证文件在磁盘上真实存在
 *
 * 安全考虑:
 * - 不验证文件内容是否为有效DLL（那是LoadLibraryW的职责）
 * - 不验证调用方是否有权限读取文件（CreateProcessW会检查）
 */
InjectResult DllInjector::ValidateDllPath(const std::wstring& dllPath) const noexcept {
    // 检查1: 路径非空
    if (dllPath.empty()) {
        return InjectResult::InvalidParameter;
    }

    // 检查2: 路径长度限制
    // MAX_PATH在Windows中通常为260，包含结尾的\0
    // 实际可用长度为259字符
    if (dllPath.length() >= MAX_PATH) {
        return InjectResult::DllPathTooLong;
    }

    // 检查3: 文件在磁盘上存在
    // GetFileAttributesW是验证文件存在的最轻量级方法
    // 返回INVALID_FILE_ATTRIBUTES表示文件不存在或无法访问
    DWORD fileAttributes = ::GetFileAttributesW(dllPath.c_str());
    if (fileAttributes == INVALID_FILE_ATTRIBUTES) {
        // 文件不存在或无访问权限
        // 区分这两种情况需要更复杂的检查，这里统一视为"文件不存在"
        return InjectResult::DllFileNotFound;
    }

    // 检查是否为目录（目录不是有效的DLL）
    if (fileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        return InjectResult::DllFileNotFound;
    }

    return InjectResult::Success;
}

/**
 * @brief 在目标进程中分配内存并写入DLL路径
 *
 * 内存分配细节:
 * - 大小计算: (路径字符数 + 结尾\0) * sizeof(wchar_t)
 * - 分配标志: MEM_COMMIT | MEM_RESERVE（提交并保留物理内存）
 * - 保护属性: PAGE_READWRITE（可读写，LoadLibraryW需要读取）
 *
 * 写入细节:
 * - 使用WriteProcessMemory将完整路径（含\0）写入远程地址
 * - 验证实际写入的字节数与预期一致
 *
 * 错误处理:
 * - 分配失败：返回RemoteAllocFailed
 * - 写入失败：释放已分配的内存，返回RemoteWriteFailed
 * - 任何失败：确保不泄漏已分配内存
 */
InjectResult DllInjector::AllocateAndWritePath(HANDLE hProcess,
                                                const std::wstring& dllPath,
                                                LPVOID* ppRemoteAddr) const noexcept {
    // 计算所需内存大小
    // +1 是为了包含结尾的空字符\0
    // sizeof(wchar_t) 是因为使用宽字符版本（Unicode）
    SIZE_T pathSize = (dllPath.length() + 1) * sizeof(wchar_t);

    // 在目标进程中分配可读写的内存
    // MEM_COMMIT:  为已保留的内存页分配物理存储（RAM或分页文件）
    // MEM_RESERVE: 保留进程的虚拟地址空间，不分配物理存储
    // 两者结合使用，一次性完成地址保留和物理分配
    LPVOID pRemoteMemory = ::VirtualAllocEx(
        hProcess,           // 目标进程句柄
        nullptr,            // 让系统自动选择分配地址（推荐）
        pathSize,           // 分配的内存大小（字节）
        MEM_COMMIT | MEM_RESERVE,  // 提交并保留
        PAGE_READWRITE      // 可读写权限（LoadLibraryW需要读取路径字符串）
    );

    if (pRemoteMemory == nullptr) {
        // 内存分配失败，常见原因：
        // - 目标进程内存不足
        // - 进程句柄权限不足（缺少PROCESS_VM_OPERATION）
        // - 目标进程的虚拟地址空间已满（32位进程常见）
        return InjectResult::RemoteAllocFailed;
    }

    // 将DLL路径写入远程内存
    // WriteProcessMemory需要目标进程句柄具有PROCESS_VM_WRITE权限
    SIZE_T bytesWritten = 0;
    BOOL writeResult = ::WriteProcessMemory(
        hProcess,           // 目标进程句柄
        pRemoteMemory,      // 远程内存地址
        dllPath.c_str(),    // 本地缓冲区（DLL路径）
        pathSize,           // 要写入的字节数
        &bytesWritten       // 实际写入的字节数（输出参数）
    );

    if (!writeResult || bytesWritten != pathSize) {
        // 写入失败，必须释放已分配的远程内存
        // 否则会造成目标进程内存泄漏
        ::VirtualFreeEx(hProcess, pRemoteMemory, 0, MEM_RELEASE);

        // 清空输出参数，避免调用方误用无效地址
        if (ppRemoteAddr != nullptr) {
            *ppRemoteAddr = nullptr;
        }
        return InjectResult::RemoteWriteFailed;
    }

    // 写入成功，返回远程内存地址
    if (ppRemoteAddr != nullptr) {
        *ppRemoteAddr = pRemoteMemory;
    }

    return InjectResult::Success;
}

/**
 * @brief 获取LoadLibraryW在目标进程中的地址
 *
 * 核心原理（同架构注入的关键）:
 * Windows系统DLL（kernel32.dll, ntdll.dll等）在所有进程中加载到
 * 相同的基址（在同架构下，即32位对32位，64位对64位）。
 * 因此，本进程中获取的LoadLibraryW地址可以直接用于目标进程。
 *
 * 实现步骤:
 * 1. 获取kernel32.dll模块句柄（本进程中）
 * 2. 获取LoadLibraryW导出地址（本进程中）
 * 3. 返回的地址可直接用于CreateRemoteThread
 *
 * 安全考虑:
 * - kernel32.dll始终已加载（任何Windows进程都依赖它）
 * - LoadLibraryW是kernel32.dll的公开导出函数，始终存在
 * - 但在极端情况下（如系统文件损坏）可能获取失败
 */
LPTHREAD_START_ROUTINE DllInjector::GetLoadLibraryWAddress() const noexcept {
    // 获取kernel32.dll模块句柄
    // GetModuleHandleW不会增加模块的引用计数，因此不需要FreeLibrary
    // 传入nullptr获取本进程的主模块（EXE）句柄，但我们需要kernel32.dll
    HMODULE hKernel32 = ::GetModuleHandleW(L"kernel32.dll");
    if (hKernel32 == nullptr) {
        // 极端异常情况：kernel32.dll未加载
        // 这在正常Windows系统中不可能发生
        return nullptr;
    }

    // 获取LoadLibraryW的导出地址
    // GetProcAddress返回的是函数在模块中的相对虚拟地址(RVA)加上模块基址
    // 由于kernel32.dll在所有同架构进程中基址相同，此地址可直接使用
    FARPROC pLoadLibraryW = ::GetProcAddress(hKernel32, "LoadLibraryW");
    if (pLoadLibraryW == nullptr) {
        // 极端异常情况：LoadLibraryW不存在
        // 这在任何正常Windows系统中都不可能发生
        return nullptr;
    }

    // 将FARPROC转换为LPTHREAD_START_ROUTINE
    // 两者都是函数指针类型，但签名不同
    // 在Windows x86上，所有函数指针大小相同（4字节），可以直接转换
    // 这是CreateRemoteThread要求的线程入口函数类型
    return reinterpret_cast<LPTHREAD_START_ROUTINE>(pLoadLibraryW);
}

/**
 * @brief 创建远程线程并等待其完成
 *
 * 详细流程:
 * 1. 创建远程线程
 *    - 入口函数: LoadLibraryW地址
 *    - 参数: 远程内存中的DLL路径地址
 *    - 创建标志: 0（立即运行）
 *
 * 2. 等待线程完成
 *    - 使用WaitForSingleObject等待线程句柄
 *    - 超时时间: 30秒（防止无限等待）
    *    - 等待结果处理:
 *      - WAIT_OBJECT_0: 线程正常结束
 *      - WAIT_TIMEOUT:  超时，线程可能死锁或DllMain阻塞
 *      - WAIT_FAILED:   等待失败
 *
 * 3. 获取退出码
 *    - 使用GetExitCodeThread获取线程返回值
 *    - 返回值即LoadLibraryW的结果
 *
 * 4. 关闭线程句柄
 *    - 使用SafeHandle确保句柄被关闭
 *
 * 错误处理:
 * - 创建失败：返回CreateRemoteThreadFailed
 * - 等待超时：终止线程（尝试），返回WaitForThreadFailed
 * - 等待失败：返回WaitForThreadFailed
 */
InjectResult DllInjector::CreateAndWaitForRemoteThread(HANDLE hProcess,
                                                        LPTHREAD_START_ROUTINE pThreadProc,
                                                        LPVOID pRemotePath,
                                                        DWORD* pExitCode,
                                                        DWORD timeoutMs) const noexcept {
    // ========================================================================
    // 创建远程线程
    // ========================================================================
    // CreateRemoteThread需要目标进程句柄具有以下权限:
    // - PROCESS_CREATE_THREAD: 创建远程线程
    // - PROCESS_QUERY_INFORMATION: 查询进程信息（某些Windows版本需要）
    // - PROCESS_VM_OPERATION: 操作虚拟内存（已用于VirtualAllocEx）
    // - PROCESS_VM_WRITE: 写入虚拟内存（已用于WriteProcessMemory）
    // - PROCESS_VM_READ: 读取虚拟内存（GetExitCodeThread需要）

    DWORD threadId = 0;
    HANDLE hThread = ::CreateRemoteThread(
        hProcess,           // 目标进程句柄
        nullptr,            // 默认安全属性（继承进程令牌）
        0,                  // 默认堆栈大小（与主线程相同）
        pThreadProc,        // 线程入口函数地址（LoadLibraryW）
        pRemotePath,        // 线程参数（DLL路径的远程地址）
        0,                  // 创建标志：0表示立即运行
        &threadId           // 输出线程ID
    );

    if (hThread == nullptr) {
        // 创建远程线程失败，常见原因：
        // - 进程句柄缺少PROCESS_CREATE_THREAD权限
        // - 目标进程受保护（如某些安全软件、系统进程）
        // - 目标进程的虚拟内存空间已满
        // - 安全软件拦截了远程线程创建
        return InjectResult::CreateRemoteThreadFailed;
    }

    // 使用RAII包装线程句柄，确保任何退出路径都会关闭句柄
    // 这是防止句柄泄漏的关键措施
    Utils::SafeHandle safeThread(hThread);

    // ========================================================================
    // 等待线程完成
    // ========================================================================
    // WaitForSingleObject阻塞等待直到线程结束或超时
    // 超时设置很重要：DllMain可能在DLL_PROCESS_ATTACH时阻塞
    // 如果DllMain死锁，我们不能无限等待
    DWORD waitResult = ::WaitForSingleObject(safeThread.Get(), timeoutMs);

    switch (waitResult) {
        case WAIT_OBJECT_0:
            // 线程正常结束，继续获取退出码
            break;

        case WAIT_TIMEOUT:
            // 等待超时，线程可能仍在运行
            // 这通常意味着DllMain在DLL_PROCESS_ATTACH中阻塞或死锁
            // 尝试终止线程（不一定成功，但值得尝试）
            ::TerminateThread(safeThread.Get(), static_cast<DWORD>(-1));
            return InjectResult::WaitForThreadFailed;

        case WAIT_FAILED:
            // 等待失败，获取详细错误信息
            return InjectResult::WaitForThreadFailed;

        default:
            // 其他未预期的等待结果
            return InjectResult::WaitForThreadFailed;
    }

    // ========================================================================
    // 获取线程退出码
    // ========================================================================
    // 线程退出码即LoadLibraryW的返回值
    // - 非零值：成功，返回值是加载的DLL模块句柄
    // - 0 (NULL)：失败，DLL加载出错
    // - STILL_ACTIVE (259)：线程仍在运行（理论上不应出现）
    DWORD exitCode = 0;
    BOOL getCodeResult = ::GetExitCodeThread(safeThread.Get(), &exitCode);

    if (!getCodeResult) {
        // 获取退出码失败
        return InjectResult::WaitForThreadFailed;
    }

    // 返回退出码给调用方
    if (pExitCode != nullptr) {
        *pExitCode = exitCode;
    }

    // safeThread在此处离开作用域，析构函数自动调用CloseHandle
    // 这是句柄清理的关键：无论成功或失败，线程句柄都会被关闭

    return InjectResult::Success;
}

} // namespace Launcher
} // namespace GameLauncher
