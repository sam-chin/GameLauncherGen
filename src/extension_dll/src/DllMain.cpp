/**
 * @file DllMain.cpp
 * @brief 扩展DLL入口点实现
 * @details 实现标准的Win32 DLL入口函数DllMain。遵循Windows DLL最佳实践：
 *          - 在DLL_PROCESS_ATTACH中仅执行最小化、确定性的初始化工作
 *          - 避免在DllMain中调用LoadLibrary、CreateThread、EnterCriticalSection等可能引发死锁的API
 *          - 复杂的初始化逻辑延迟到显式的DllInit导出函数中执行
 *          - 使用DisableThreadLibraryCalls消除不必要的DLL_THREAD_ATTACH/DETACH通知，提升性能
 *
 * @note 本文件仅负责DLL的生命周期入口管理，不包含任何业务逻辑。
 *       所有业务初始化均在 ExtensionAPI.cpp 的 DllInit 中完成。
 *
 * @version 1.0.0
 * @date 2026-06-10
 */

/* ========================== 头文件包含 ========================== */

#include <windows.h>

/* ========================== 内部常量定义 ========================== */

/**
 * @brief 模块实例句柄全局变量
 * @details 保存当前DLL模块的HINSTANCE，供模块内其他代码获取模块路径、加载资源等操作使用。
 *          此变量在DLL_PROCESS_ATTACH时写入，之后为只读状态，无需同步保护。
 */
static HINSTANCE g_hDllModule = nullptr;

/**
 * @brief DLL初始化完成标志
 * @details 指示DllInit是否已成功执行。用于DllMain的DLL_PROCESS_DETACH阶段判断
 *          是否需要执行对应的清理逻辑。使用原子类型避免多线程竞争（虽然DllUnInit应在卸载前调用）。
 */
static volatile LONG g_bDllInitialized = FALSE;

/* ========================== 内部辅助函数 ========================== */

/**
 * @brief 获取DLL模块实例句柄
 * @details 提供模块内其他编译单元访问当前DLL模块句柄的能力。
 *          此函数在DllMain返回后即可安全调用。
 *
 * @return 当前DLL模块的HINSTANCE句柄。如果DllMain尚未执行，返回nullptr。
 */
extern "C" HINSTANCE GetDllModuleHandle(void)
{
    return g_hDllModule;
}

/**
 * @brief 设置DLL初始化状态标志
 * @details 由ExtensionAPI.cpp中的DllInit/DllUnInit调用，同步初始化状态到DllMain可见的变量。
 *
 * @param[in] bInitialized TRUE表示初始化完成，FALSE表示已反初始化。
 */
extern "C" void SetDllInitializedState(BOOL bInitialized)
{
    ::InterlockedExchange(&g_bDllInitialized, static_cast<LONG>(bInitialized));
}

/**
 * @brief 查询DLL是否已完成初始化
 * @details 用于内部状态校验，防止在初始化前调用需要资源的接口。
 *
 * @return 如果DllInit已成功完成且未调用DllUnInit，返回TRUE；否则返回FALSE。
 */
extern "C" BOOL IsDllInitialized(void)
{
    return (g_bDllInitialized != FALSE);
}

/* ========================== DLL入口函数 ========================== */

/**
 * @fn DllMain
 * @brief Windows动态链接库标准入口点
 * @details 操作系统在以下四种情况下调用此函数：
 *          1. DLL_PROCESS_ATTACH - 进程首次加载此DLL时
 *          2. DLL_THREAD_ATTACH  - 进程创建新线程时（已禁用）
 *          3. DLL_THREAD_DETACH  - 线程终止时（已禁用）
 *          4. DLL_PROCESS_DETACH - 进程卸载此DLL时
 *
 *          【关键设计原则】
 *          - 最小化原则：DllMain中只做绝对必要的工作
 *          - 无阻塞原则：不调用任何可能导致阻塞、死锁或递归加载DLL的API
 *          - 延迟初始化原则：资源分配、配置读取、网络连接等重操作全部推迟到DllInit
 *
 * @param[in] hModule       DLL模块的实例句柄（基地址）
 * @param[in] ulReasonForCall 调用原因，枚举值见DLL_PROCESS_*常量
 * @param[in] lpReserved    保留参数：
 *                          - DLL_PROCESS_ATTACH时：非nullptr表示静态加载，nullptr表示动态加载（LoadLibrary）
 *                          - DLL_PROCESS_DETACH时：非nullptr表示进程终止，nullptr表示FreeLibrary调用
 *
 * @return 对于DLL_PROCESS_ATTACH，返回TRUE表示初始化成功，FALSE表示失败（系统将卸载DLL）；
 *         对于其他原因，返回值被忽略。
 */
BOOL APIENTRY DllMain(
    HINSTANCE hModule,
    DWORD     ulReasonForCall,
    LPVOID    lpReserved
)
{
    // 消除未使用参数的编译器警告（在Release构建中尤为重要）
    UNREFERENCED_PARAMETER(lpReserved);

    switch (ulReasonForCall)
    {
        /* ==================== 进程加载DLL ==================== */
        case DLL_PROCESS_ATTACH:
        {
            /**
             * 【阶段一：保存模块句柄】
             * 这是DllMain中唯一必须立即执行的操作。
             * 模块句柄后续用于加载资源、获取模块路径等场景。
             */
            g_hDllModule = hModule;

            /**
             * 【阶段二：禁用线程通知】
             * 调用DisableThreadLibraryCalls告知操作系统：
             * 本DLL不需要接收DLL_THREAD_ATTACH和DLL_THREAD_DETACH通知。
             * 这能显著减少进程创建/销毁线程时的开销，提升整体性能。
             *
             * @note 如果未来版本需要感知线程生命周期（如TLS管理），需移除此调用。
             */
            ::DisableThreadLibraryCalls(hModule);

            /**
             * 【阶段三：占位 - 钩子框架预初始化提示】
             * 以下为未来钩子系统的预留位置。当前版本不执行任何实际操作。
             * 未来可能在此进行：
             * - 验证当前进程是否为预期的游戏进程（通过GetModuleFileName比对映像名称）
             * - 预分配钩子描述符数组（仅分配内存，不安装钩子）
             * - 初始化同步原语（轻量级的spinlock或原子变量）
             *
             * @warning 严禁在此调用以下API：
             * - LoadLibrary / LoadLibraryEx（可能导致Loader Lock死锁）
             * - CreateThread / _beginthreadex（新线程在DllMain返回前无法正常运行）
             * - EnterCriticalSection / WaitForSingleObject（可能阻塞Loader Lock）
             * - 任何可能触发SEH或调用其他DLL入口的复杂操作
             */

            // 当前版本：DllMain成功完成，返回TRUE
            // 实际的资源初始化由导出函数DllInit在Loader Lock释放后执行
            return TRUE;
        }

        /* ==================== 进程卸载DLL ==================== */
        case DLL_PROCESS_DETACH:
        {
            /**
             * 【清理阶段】
             * 理想情况下，DllUnInit应在FreeLibrary之前被显式调用，完成所有资源释放。
             * 但以下情况可能导致DllUnInit未被调用：
             * 1. 主程序异常崩溃，未执行清理逻辑
             * 2. 进程正在终止（lpReserved != nullptr），操作系统将回收所有资源
             *
             * 因此，DllMain的DLL_PROCESS_DETACH仅作为安全网（safety net）：
             * - 如果lpReserved != nullptr（进程终止），操作系统会自动清理资源，无需额外操作
             * - 如果lpReserved == nullptr（FreeLibrary），且g_bDllInitialized仍为TRUE，
             *   说明DllUnInit未被调用，此时应执行最小化的紧急清理
             */

            if (lpReserved == nullptr)
            {
                // FreeLibrary路径：DllUnInit未被调用或调用失败
                // 执行最小化的清理（仅释放关键资源，避免崩溃）
                // 当前版本无需要紧急清理的资源，故为空操作
            }
            else
            {
                // 进程终止路径：操作系统将回收所有资源
                // 无需执行任何清理，避免在进程终止时引发访问冲突
            }

            // 重置模块句柄（虽然进程即将终止或DLL已卸载，但保持代码整洁）
            g_hDllModule = nullptr;
            break;
        }

        /* ==================== 线程通知（已禁用，理论上不会到达） ==================== */
        case DLL_THREAD_ATTACH:
        {
            /**
             * 由于已调用DisableThreadLibraryCalls，此分支理论上不会执行。
             * 保留此case以消除编译器警告，并作为文档说明。
             */
            break;
        }

        case DLL_THREAD_DETACH:
        {
            /**
             * 由于已调用DisableThreadLibraryCalls，此分支理论上不会执行。
             * 保留此case以消除编译器警告，并作为文档说明。
             */
            break;
        }

        /* ==================== 未知/未处理的原因码 ==================== */
        default:
        {
            /**
             * Windows未来可能引入新的通知类型。
             * 对于未知原因码，安全做法是忽略处理，返回TRUE。
             */
            break;
        }
    }

    return TRUE;
}
