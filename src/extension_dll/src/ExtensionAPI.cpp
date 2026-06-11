/**
 * @file ExtensionAPI.cpp
 * @brief 扩展DLL导出接口实现
 * @details 实现了 ExtensionAPI.h 中声明的所有导出函数，包括：
 *          - DllInit / DllUnInit：模块生命周期管理
 *          - DllGetVersion / DllGetLastErrorMessage：辅助查询接口
 *          - DllInstallHook / DllUninstallHook：钩子框架占位接口
 *
 *          本文件是扩展DLL的核心业务逻辑层，完全独立于主程序，仅通过标准C接口与外部通信。
 *          所有内部状态均封装在匿名命名空间中，避免符号污染。
 *
 * @version 1.0.0
 * @date 2026-06-10
 */

// EXTENSION_DLL_BUILD 已在 CMakeLists.txt 中通过 target_compile_definitions 定义
// 此处不再重复定义，避免 C4005 宏重复定义警告

/* ========================== 头文件包含 ========================== */

#include "ExtensionAPI.h"
#include <windows.h>
#include <string>
#include <mutex>
#include <atomic>
#include <cwchar>
#include <cstdlib>

/* ========================== 内部状态与数据（匿名命名空间封装） ========================== */

namespace
{
    /**
     * @brief 模块全局状态枚举
     * @details 使用原子变量记录DLL的当前生命周期状态，确保线程安全的状态查询。
     */
    enum class DllState : uint32_t
    {
        Uninitialized = 0,  /**< 尚未初始化，DllInit未被调用或调用失败 */
        Initializing  = 1,  /**< 正在初始化中（防止重入竞争） */
        Initialized   = 2,  /**< 初始化成功完成，所有接口可用 */
        UninitInProgress = 3, /**< 正在反初始化中 */
        UninitializedDone = 4 /**< 反初始化完成，所有资源已释放 */
    };

    /**
     * @brief DLL全局状态原子变量
     * @details 使用std::atomic保证多线程环境下的状态转换安全。
     *          初始状态为 Uninitialized。
     */
    std::atomic<DllState> g_dllState(DllState::Uninitialized);

    /**
     * @brief 初始化配置副本
     * @details DllInit成功后，将传入的配置结构体中的关键信息保存于此，供后续接口使用。
     *          使用宽字符字符串以完整支持Unicode路径。
     */
    struct StoredConfig
    {
        std::wstring workingDir;      /**< 工作目录绝对路径 */
        std::wstring configFilePath;  /**< 配置文件路径（可能为空） */
        LOG_CALLBACK logCallback;     /**< 日志回调函数指针（可能为nullptr） */
        uint32_t     apiVersion;      /**< 主程序请求的API版本号 */
    };

    /**
     * @brief 配置数据实例
     * @details 在DllInit成功后分配，DllUnInit时释放。
     *          使用堆分配而非静态存储期，以支持延迟初始化和确定性清理。
     */
    StoredConfig* g_pStoredConfig = nullptr;

    /**
     * @brief 状态互斥锁
     * @details 保护配置数据的读写操作。与原子状态变量配合使用：
     *          - 原子变量用于快速无锁的状态检查
     *          - 互斥锁用于保护配置数据的完整读写
     */
    std::mutex g_stateMutex;

    /**
     * @brief 最后错误消息缓冲区
     * @details 存储最近一次操作失败的描述信息。
     *          使用线程局部存储（TLS）避免多线程竞争，每个线程拥有独立的错误消息副本。
     * @warning 返回的指针指向线程局部缓冲区，不需要释放，但仅在同一线程的后续调用前有效。
     */
    thread_local wchar_t g_lastErrorMessage[512] = { L'\0' };

    /**
     * @brief 钩子框架状态（占位）
     * @details 当前版本为占位实现，仅记录钩子是否"已安装"（模拟状态）。
     *          未来版本将扩展为完整的钩子描述符表。
     */
    std::atomic<bool> g_hookInstalled(false);

    /* ========================== 内部辅助函数 ========================== */

    /**
     * @brief 设置最后错误消息
     * @details 将指定的宽字符字符串复制到线程局部错误消息缓冲区。
     *          如果消息过长，将自动截断以适应缓冲区大小。
     *
     * @param[in] pMessage 错误消息宽字符串。如果为nullptr，清空错误消息。
     */
    void SetLastErrorMessage(const wchar_t* pMessage)
    {
        if (pMessage == nullptr)
        {
            g_lastErrorMessage[0] = L'\0';
            return;
        }

        // 使用wcsncpy_s安全复制，防止缓冲区溢出
        // 保留最后一个字符位置给终止符
        ::wcsncpy_s(g_lastErrorMessage, _countof(g_lastErrorMessage), pMessage, _TRUNCATE);
    }

    /**
     * @brief 设置最后错误消息（带格式化）
     * @details 使用格式化字符串和可变参数列表生成错误消息。
     *          内部使用_vsnwprintf_s进行安全的宽字符格式化。
     *
     * @param[in] pFormat 格式化字符串（宽字符）
     * @param[in] ...     可变参数列表
     */
    void SetLastErrorMessageFormatted(const wchar_t* pFormat, ...)
    {
        if (pFormat == nullptr)
        {
            g_lastErrorMessage[0] = L'\0';
            return;
        }

        va_list args;
        va_start(args, pFormat);
        // _vsnwprintf_s 返回写入的字符数（不含终止符），负数表示错误
        ::_vsnwprintf_s(g_lastErrorMessage, _countof(g_lastErrorMessage), _TRUNCATE, pFormat, args);
        va_end(args);
    }

    /**
     * @brief 输出日志（内部使用）
     * @details 如果初始化配置中提供了日志回调，则通过回调将日志转发到主程序；
     *          否则，日志被静默丢弃（不输出到控制台或文件，避免污染主程序环境）。
     *
     * @param[in] level   日志级别：0=调试, 1=信息, 2=警告, 3=错误, 4=严重错误
     * @param[in] pMessage 日志消息内容（宽字符）
     */
    void InternalLog(uint32_t level, const wchar_t* pMessage)
    {
        if (pMessage == nullptr)
            return;

        std::lock_guard<std::mutex> lock(g_stateMutex);
        if (g_pStoredConfig != nullptr && g_pStoredConfig->logCallback != nullptr)
        {
            // 通过主程序提供的回调输出日志，来源标记为当前模块名
            g_pStoredConfig->logCallback(level, pMessage, L"ExtensionDLL");
        }
    }

    /**
     * @brief 验证初始化配置结构体的有效性
     * @details 检查结构体大小、版本号、必要字段等是否符合要求。
     *
     * @param[in] pConfig 指向初始化配置结构体的指针
     *
     * @return 如果配置有效，返回 EXT_SUCCESS；否则返回相应的错误码。
     */
    EXTENSION_RESULT ValidateInitConfig(const EXTENSION_INIT_CONFIG* pConfig)
    {
        if (pConfig == nullptr)
        {
            SetLastErrorMessage(L"初始化失败：配置结构体指针为nullptr。");
            return EXT_ERROR_INVALID_PARAM;
        }

        // 检查结构体大小：必须至少包含基础字段的大小
        constexpr uint32_t MIN_STRUCT_SIZE = sizeof(uint32_t) * 2 + sizeof(void*) * 3;
        if (pConfig->structSize < MIN_STRUCT_SIZE)
        {
            SetLastErrorMessageFormatted(
                L"初始化失败：配置结构体大小不兼容。期望至少 %u 字节，实际 %u 字节。",
                MIN_STRUCT_SIZE, pConfig->structSize
            );
            return EXT_ERROR_INVALID_PARAM;
        }

        // 检查API版本兼容性：主版本号必须严格匹配，次版本号可向后兼容
        const uint32_t expectedMajor = (pConfig->apiVersion >> 16) & 0xFFFF;
        const uint32_t dllMajor      = (EXTENSION_API_VERSION >> 16) & 0xFFFF;
        if (expectedMajor != dllMajor)
        {
            SetLastErrorMessageFormatted(
                L"初始化失败：API主版本号不匹配。主程序期望 v%u.x，DLL提供 v%u.x。",
                expectedMajor, dllMajor
            );
            return EXT_ERROR_VERSION_MISMATCH;
        }

        // 工作目录为可选参数，但建议提供
        // 如果提供了工作目录，验证其不为空字符串
        if (pConfig->pWorkingDir != nullptr && pConfig->pWorkingDir[0] == L'\0')
        {
            SetLastErrorMessage(L"初始化失败：工作目录参数为空字符串。");
            return EXT_ERROR_INVALID_PARAM;
        }

        // 保留字段必须为nullptr（向前兼容性检查）
        if (pConfig->pReserved != nullptr)
        {
            SetLastErrorMessage(L"初始化失败：保留字段必须为nullptr。");
            return EXT_ERROR_INVALID_PARAM;
        }

        return EXT_SUCCESS;
    }

    /**
     * @brief 保存初始化配置到内部存储
     * @details 将传入的配置结构体中的动态数据（字符串等）深拷贝到堆分配的StoredConfig中。
     *          此操作在状态锁保护下进行，确保线程安全。
     *
     * @param[in] pConfig 指向初始化配置结构体的指针（已通过验证）
     *
     * @return 如果保存成功，返回 EXT_SUCCESS；否则返回 EXT_ERROR_OUT_OF_MEMORY。
     */
    EXTENSION_RESULT SaveConfiguration(const EXTENSION_INIT_CONFIG* pConfig)
    {
        try
        {
            g_pStoredConfig = new StoredConfig();

            if (pConfig->pWorkingDir != nullptr)
            {
                g_pStoredConfig->workingDir = pConfig->pWorkingDir;
            }

            if (pConfig->pConfigFilePath != nullptr)
            {
                g_pStoredConfig->configFilePath = pConfig->pConfigFilePath;
            }

            g_pStoredConfig->logCallback = pConfig->pLogCallback;
            g_pStoredConfig->apiVersion  = pConfig->apiVersion;

            return EXT_SUCCESS;
        }
        catch (const std::bad_alloc&)
        {
            delete g_pStoredConfig;
            g_pStoredConfig = nullptr;
            SetLastErrorMessage(L"初始化失败：内存分配失败（配置结构体）。");
            return EXT_ERROR_OUT_OF_MEMORY;
        }
    }

    /**
     * @brief 清理所有已分配的资源
     * @details 释放配置数据、重置钩子状态、清空错误消息。
     *          此函数设计为幂等操作：多次调用不会产生副作用。
     */
    void CleanupResources(void)
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);

        // 释放配置数据
        if (g_pStoredConfig != nullptr)
        {
            delete g_pStoredConfig;
            g_pStoredConfig = nullptr;
        }

        // 重置钩子状态
        g_hookInstalled.store(false, std::memory_order_release);

        // 清空线程局部错误消息
        SetLastErrorMessage(nullptr);
    }

} // anonymous namespace

/* ========================== 外部声明（来自DllMain.cpp） ========================== */

/**
 * @brief 设置DLL初始化状态标志（DllMain.cpp中定义）
 */
extern "C" void SetDllInitializedState(BOOL bInitialized);

/**
 * @brief 查询DLL是否已完成初始化（DllMain.cpp中定义）
 */
extern "C" BOOL IsDllInitialized(void);

/* ========================== 导出函数实现 ========================== */

/**
 * @fn DllInit
 * @brief 初始化扩展DLL模块
 * @details 执行完整的初始化流程：
 *          1. 状态检查：确保未重复初始化
 *          2. 参数验证：校验配置结构体的完整性和版本兼容性
 *          3. 配置保存：深拷贝配置数据到内部存储
 *          4. 资源分配：初始化钩子框架、加载配置等（当前版本为占位）
 *          5. 状态更新：标记初始化成功
 *
 *          整个流程具有原子性：如果任何步骤失败，已分配的资源将被回滚，
 *          DLL状态恢复到 Uninitialized。
 */
EXTENSION_API EXTENSION_RESULT EXTENSION_CALL DllInit(
    const EXTENSION_INIT_CONFIG* pConfig
)
{
    /* ---------- 步骤1：原子状态检查，防止重复初始化和并发竞争 ---------- */
    DllState expected = DllState::Uninitialized;
    if (!g_dllState.compare_exchange_strong(
            expected,
            DllState::Initializing,
            std::memory_order_acq_rel,
            std::memory_order_acquire))
    {
        // 状态不是Uninitialized，说明已经初始化或正在初始化
        if (expected == DllState::Initialized)
        {
            SetLastErrorMessage(L"初始化失败：DLL已经初始化完成，请勿重复调用DllInit。");
            return EXT_ERROR_ALREADY_INITIALIZED;
        }
        else if (expected == DllState::Initializing)
        {
            SetLastErrorMessage(L"初始化失败：DLL正在初始化中，请等待完成。");
            return EXT_ERROR_ALREADY_INITIALIZED; // 使用相同错误码表示"初始化已在进行"
        }
        else
        {
            SetLastErrorMessage(L"初始化失败：DLL处于无效状态，无法重新初始化。");
            return EXT_ERROR_GENERIC;
        }
    }

    /**
     * @note 至此，当前线程已成功将状态从Uninitialized转换为Initializing，
     *       获得了独占的初始化权。后续如果失败，必须将状态恢复。
     */

    /* ---------- 步骤2：验证传入的配置参数 ---------- */
    EXTENSION_RESULT result = ValidateInitConfig(pConfig);
    if (result != EXT_SUCCESS)
    {
        // 验证失败，回滚状态
        g_dllState.store(DllState::Uninitialized, std::memory_order_release);
        return result;
    }

    /* ---------- 步骤3：保存配置数据 ---------- */
    result = SaveConfiguration(pConfig);
    if (result != EXT_SUCCESS)
    {
        CleanupResources();
        g_dllState.store(DllState::Uninitialized, std::memory_order_release);
        return result;
    }

    /* ---------- 步骤4：初始化钩子框架（占位） ---------- */
    /**
     * @details 当前版本钩子框架为占位实现，不执行实际操作。
     * 未来版本将在此进行：
     * - 加载钩子配置文件（JSON/INI格式）
     * - 分配钩子描述符表和跳转缓冲区
     * - 解析目标进程模块基址和导出表
     * - 准备API钩子所需的trampoline代码
     *
     * 以下为模拟的"成功初始化"日志输出：
     */
    InternalLog(1, L"扩展DLL初始化中：钩子框架占位已就绪。");

    /* ---------- 步骤5：标记初始化成功 ---------- */
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        // 同步状态到DllMain可见的变量（用于DLL_PROCESS_DETACH的安全网逻辑）
        SetDllInitializedState(TRUE);
    }

    g_dllState.store(DllState::Initialized, std::memory_order_release);

    // 输出初始化成功日志
    InternalLog(1, L"扩展DLL初始化成功完成。");

    return EXT_SUCCESS;
}

/**
 * @fn DllUnInit
 * @brief 反初始化扩展DLL模块
 * @details 执行完整的资源释放流程：
 *          1. 状态检查：确保DLL已初始化
 *          2. 卸载钩子：移除所有已安装的钩子（当前版本为占位）
 *          3. 释放资源：删除配置数据、关闭句柄等
 *          4. 状态重置：恢复到未初始化状态
 *
 *          此函数设计为幂等操作：多次调用安全，不会产生副作用。
 *          即使DLL未初始化，调用此函数也安全返回。
 */
EXTENSION_API void EXTENSION_CALL DllUnInit(void)
{
    /* ---------- 步骤1：原子状态检查 ---------- */
    DllState expected = DllState::Initialized;
    if (!g_dllState.compare_exchange_strong(
            expected,
            DllState::UninitInProgress,
            std::memory_order_acq_rel,
            std::memory_order_acquire))
    {
        // 状态不是Initialized，无需执行清理
        if (expected == DllState::Uninitialized ||
            expected == DllState::UninitializedDone)
        {
            // 已经清理或未初始化，直接返回（幂等性）
            return;
        }
        else if (expected == DllState::UninitInProgress)
        {
            // 正在清理中，避免重入
            return;
        }
        else
        {
            // Initializing状态：初始化尚未完成，无法清理
            return;
        }
    }

    /* ---------- 步骤2：卸载所有钩子（占位） ---------- */
    if (g_hookInstalled.load(std::memory_order_acquire))
    {
        // 当前版本无实际钩子需要卸载，仅重置标志
        g_hookInstalled.store(false, std::memory_order_release);
        InternalLog(1, L"扩展DLL反初始化：钩子框架占位已清理。");
    }

    /* ---------- 步骤3：释放所有资源 ---------- */
    CleanupResources();

    /* ---------- 步骤4：同步状态并标记反初始化完成 ---------- */
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        SetDllInitializedState(FALSE);
    }

    g_dllState.store(DllState::UninitializedDone, std::memory_order_release);

    InternalLog(1, L"扩展DLL反初始化成功完成。");
}

/**
 * @fn DllGetVersion
 * @brief 获取扩展DLL的版本信息
 * @details 线程安全函数，可在任何时刻调用（包括DllInit之前）。
 *          将版本号分解写入输出参数，同时返回打包的32位版本值。
 */
EXTENSION_API uint32_t EXTENSION_CALL DllGetVersion(
    uint32_t* pMajor,
    uint32_t* pMinor,
    uint32_t* pPatch
)
{
    // 写入各分量（如果输出参数非空）
    if (pMajor != nullptr)
    {
        *pMajor = EXTENSION_API_VERSION_MAJOR;
    }
    if (pMinor != nullptr)
    {
        *pMinor = EXTENSION_API_VERSION_MINOR;
    }
    if (pPatch != nullptr)
    {
        *pPatch = EXTENSION_API_VERSION_PATCH;
    }

    // 返回打包的32位版本号
    return EXTENSION_API_VERSION;
}

/**
 * @fn DllGetLastErrorMessage
 * @brief 获取最后一次错误的描述信息
 * @details 返回指向线程局部缓冲区的指针，不需要调用者释放。
 *          如果没有记录过错误，返回空字符串（非nullptr）。
 *
 * @warning 此函数非线程安全（相对于错误发生线程而言是安全的，
 *          但不同线程的错误消息相互独立）。
 */
EXTENSION_API const wchar_t* EXTENSION_CALL DllGetLastErrorMessage(void)
{
    // 确保至少返回空字符串，永远不会返回nullptr
    if (g_lastErrorMessage[0] == L'\0')
    {
        return L"";
    }
    return g_lastErrorMessage;
}

/* ========================== 钩子接口占位实现 ========================== */

/**
 * @fn DllInstallHook
 * @brief 安装指定类型的钩子（占位实现）
 * @details 当前版本仅记录错误并返回 EXT_ERROR_NOT_SUPPORTED。
 *          未来版本将支持：
 *          - 消息钩子（WH_CALLWNDPROC等）
 *          - API钩子（通过Inline Hook或IAT Hook）
 *          - 内存钩子（监控特定内存区域的读写）
 */
EXTENSION_API EXTENSION_RESULT EXTENSION_CALL DllInstallHook(
    uint32_t hookType,
    HOOK_CALLBACK pCallback,
    void* pUserData
)
{
    // 消除未使用参数的编译器警告
    UNREFERENCED_PARAMETER(hookType);
    UNREFERENCED_PARAMETER(pCallback);
    UNREFERENCED_PARAMETER(pUserData);

    // 检查DLL是否已初始化
    if (g_dllState.load(std::memory_order_acquire) != DllState::Initialized)
    {
        SetLastErrorMessage(L"安装钩子失败：DLL尚未初始化，请先调用DllInit。");
        return EXT_ERROR_NOT_INITIALIZED;
    }

    SetLastErrorMessage(L"安装钩子失败：钩子功能在当前版本中尚未实现（占位接口）。");
    InternalLog(2, L"DllInstallHook被调用，但钩子功能尚未实现。");

    return EXT_ERROR_NOT_SUPPORTED;
}

/**
 * @fn DllUninstallHook
 * @brief 卸载指定类型的钩子（占位实现）
 * @details 当前版本仅记录错误并返回 EXT_ERROR_NOT_SUPPORTED。
 */
EXTENSION_API EXTENSION_RESULT EXTENSION_CALL DllUninstallHook(
    uint32_t hookType
)
{
    UNREFERENCED_PARAMETER(hookType);

    // 检查DLL是否已初始化
    if (g_dllState.load(std::memory_order_acquire) != DllState::Initialized)
    {
        SetLastErrorMessage(L"卸载钩子失败：DLL尚未初始化，请先调用DllInit。");
        return EXT_ERROR_NOT_INITIALIZED;
    }

    SetLastErrorMessage(L"卸载钩子失败：钩子功能在当前版本中尚未实现（占位接口）。");
    InternalLog(2, L"DllUninstallHook被调用，但钩子功能尚未实现。");

    return EXT_ERROR_NOT_SUPPORTED;
}
