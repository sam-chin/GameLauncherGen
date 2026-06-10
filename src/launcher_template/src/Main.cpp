/**
 * @file Main.cpp
 * @brief 启动器入口点
 * @details 游戏启动器的主入口函数，负责：
 *          1. 解析命令行参数或配置文件
 *          2. 构建服务器配置
 *          3. 配置启动选项
 *          4. 调用ProcessLauncher执行启动和注入
 *          5. 处理结果并输出状态信息
 *
 *          命令行用法:
 *          Launcher.exe <client.exe路径> <hook.dll路径> <服务器名称> <IP> <端口>
 *
 *          示例:
 *          Launcher.exe "C:\\Game\\client.exe" "C:\\Game\\hook.dll" "艾欧尼亚" "192.168.1.100" 7777
 *
 * @note 本程序为控制台应用程序，输出UTF-8编码的日志信息
 * @warning 必须以管理员权限运行（某些注入场景需要）
 */

#include "ProcessLauncher.h"
#include "ServerConfig.h"
#include "LauncherUtils.h"
#include <Windows.h>
#include <shellapi.h>
#include <iostream>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// 控制台输出辅助函数
// ---------------------------------------------------------------------------

/**
 * @brief 输出UTF-8编码的错误信息到控制台
 * @param[in] message 要输出的消息（UTF-8编码）
 *
 * @details Windows控制台默认使用ANSI代码页，直接输出UTF-8中文会乱码。
 *          本函数先将UTF-8转换为宽字符，再使用wcout输出。
 */
void PrintError(const std::string& message) {
    std::wstring wideMsg = GameLauncher::Launcher::Utils::Utf8ToWide(message);
    std::wcerr << L"[错误] " << wideMsg << std::endl;
}

/**
 * @brief 输出UTF-8编码的信息到控制台
 * @param[in] message 要输出的消息（UTF-8编码）
 */
void PrintInfo(const std::string& message) {
    std::wstring wideMsg = GameLauncher::Launcher::Utils::Utf8ToWide(message);
    std::wcout << L"[信息] " << wideMsg << std::endl;
}

/**
 * @brief 输出UTF-8编码的成功信息到控制台
 * @param[in] message 要输出的消息（UTF-8编码）
 */
void PrintSuccess(const std::string& message) {
    std::wstring wideMsg = GameLauncher::Launcher::Utils::Utf8ToWide(message);
    std::wcout << L"[成功] " << wideMsg << std::endl;
}

// ---------------------------------------------------------------------------
// 命令行参数解析
// ---------------------------------------------------------------------------

/**
 * @brief 解析命令行参数
 * @param[in] argc 参数个数
 * @param[in] argv 参数数组（窄字符，可能是ANSI或UTF-8编码）
 * @param[out] options 解析后的启动选项
 * @return true  解析成功
 * @return false 解析失败
 *
 * @details 参数顺序:
 *          argv[1]: 客户端程序路径 (client.exe)
 *          argv[2]: 注入DLL路径 (hook.dll)
 *          argv[3]: 服务器名称
 *          argv[4]: 服务器IP地址
 *          argv[5]: 服务器端口号
 *
 *          编码处理:
 *          - Windows命令行参数通常是ANSI编码（中文Windows为GBK）
 *          - 使用MultiByteToWideChar将ANSI转换为宽字符
 *          - 如果参数已经是UTF-8，转换结果可能不正确
 *            （这种情况需要使用GetCommandLineW直接获取宽字符参数）
 */
bool ParseCommandLine(int argc, char* argv[],
                      GameLauncher::Launcher::LaunchOptions& options) {
    // 检查参数数量
    if (argc < 6) {
        PrintError("参数不足。用法: Launcher.exe <client.exe> <hook.dll> <服务器名> <IP> <端口>");
        return false;
    }

    // 将窄字符参数转换为宽字符
    // 假设输入为ANSI编码（中文Windows为GBK，代码页936）
    // 使用当前系统默认代码页进行转换
    int ansiCodePage = ::GetACP();  // 获取当前系统ANSI代码页

    auto ConvertAnsiToWide = [ansiCodePage](const char* ansiStr) -> std::wstring {
        if (ansiStr == nullptr || ansiStr[0] == '\0') {
            return std::wstring();
        }

        int requiredSize = ::MultiByteToWideChar(
            ansiCodePage,   // 使用系统默认ANSI代码页
            0,              // 默认标志
            ansiStr,        // 输入ANSI字符串
            -1,             // 自动计算长度
            nullptr,        // 首次调用，仅获取大小
            0               // 缓冲区大小为0
        );

        if (requiredSize <= 0) {
            return std::wstring();
        }

        std::vector<wchar_t> buffer(static_cast<size_t>(requiredSize));
        int converted = ::MultiByteToWideChar(
            ansiCodePage,
            0,
            ansiStr,
            -1,
            buffer.data(),
            requiredSize
        );

        if (converted <= 0) {
            return std::wstring();
        }

        return std::wstring(buffer.data(), static_cast<size_t>(converted - 1));
    };

    // 解析各个参数
    options.clientPath = ConvertAnsiToWide(argv[1]);
    options.dllPath = ConvertAnsiToWide(argv[2]);
    std::wstring serverName = ConvertAnsiToWide(argv[3]);
    std::wstring serverIp = ConvertAnsiToWide(argv[4]);

    // 解析端口号
    int port = std::atoi(argv[5]);
    if (port <= 0 || port > 65535) {
        PrintError("端口号无效，必须是1-65535之间的整数");
        return false;
    }

    // 构建服务器配置
    options.serverConfig = GameLauncher::Launcher::ServerConfig(
        serverName,
        serverIp,
        static_cast<uint16_t>(port)
    );

    return true;
}

/**
 * @brief 使用宽字符命令行参数解析（更可靠的Unicode支持）
 * @param[out] options 解析后的启动选项
 * @return true  解析成功
 * @return false 解析失败
 *
 * @details 直接使用GetCommandLineW获取宽字符命令行，
 *          避免ANSI到宽字符的转换问题。
 *          这是处理中文参数的最佳方式。
 */
bool ParseWideCommandLine(GameLauncher::Launcher::LaunchOptions& options) {
    // 获取宽字符命令行
    LPWSTR pCmdLine = ::GetCommandLineW();
    if (pCmdLine == nullptr) {
        PrintError("无法获取命令行参数");
        return false;
    }

    // 使用CommandLineToArgvW将命令行拆分为参数数组
    int argc = 0;
    LPWSTR* argv = ::CommandLineToArgvW(pCmdLine, &argc);

    if (argv == nullptr) {
        PrintError("解析命令行参数失败");
        return false;
    }

    // 使用RAII确保释放argv内存
    struct ArgvGuard {
        LPWSTR* argv;
        explicit ArgvGuard(LPWSTR* a) : argv(a) {}
        ~ArgvGuard() { if (argv) ::LocalFree(argv); }
        ArgvGuard(const ArgvGuard&) = delete;
        ArgvGuard& operator=(const ArgvGuard&) = delete;
    };
    ArgvGuard guard(argv);

    // 检查参数数量
    if (argc < 6) {
        PrintError("参数不足。用法: Launcher.exe <client.exe> <hook.dll> <服务器名> <IP> <端口>");
        return false;
    }

    // 直接复制宽字符参数（无需编码转换）
    options.clientPath = argv[1];
    options.dllPath = argv[2];
    std::wstring serverName = argv[3];
    std::wstring serverIp = argv[4];

    // 解析端口号（宽字符转整数）
    int port = ::_wtoi(argv[5]);
    if (port <= 0 || port > 65535) {
        PrintError("端口号无效，必须是1-65535之间的整数");
        return false;
    }

    // 构建服务器配置
    options.serverConfig = GameLauncher::Launcher::ServerConfig(
        serverName,
        serverIp,
        static_cast<uint16_t>(port)
    );

    return true;
}

// ---------------------------------------------------------------------------
// 主函数
// ---------------------------------------------------------------------------

/**
 * @brief 程序入口点
 * @param[in] argc 命令行参数个数
 * @param[in] argv 命令行参数数组
 * @return 0  成功
 * @return 1  参数错误
 * @return 2  启动失败
 *
 * @details 主函数执行流程:
 *          1. 设置控制台代码页为UTF-8（如果可能）
 *          2. 解析命令行参数
 *          3. 构建启动选项
 *          4. 调用ProcessLauncher启动并注入
 *          5. 输出结果信息
 *          6. 返回退出码
 */
int main(int argc, char* argv[]) {
    // ========================================================================
    // 设置控制台输出代码页
    // ========================================================================
    // 尝试将控制台输出代码页设置为UTF-8（65001）
    // 这样可以直接输出UTF-8编码的中文
    // 如果失败（旧版Windows不支持），使用默认代码页
    UINT originalOutputCP = ::GetConsoleOutputCP();
    UINT originalCP = ::GetConsoleCP();

    // 设置输入和输出代码页为UTF-8
    ::SetConsoleOutputCP(CP_UTF8);
    ::SetConsoleCP(CP_UTF8);

    // ========================================================================
    // 解析命令行参数
    // ========================================================================
    GameLauncher::Launcher::LaunchOptions options;

    // 优先使用宽字符命令行解析（更好的Unicode支持）
    if (!ParseWideCommandLine(options)) {
        // 宽字符解析失败，回退到窄字符解析
        if (!ParseCommandLine(argc, argv, options)) {
            // 恢复原始代码页
            ::SetConsoleOutputCP(originalOutputCP);
            ::SetConsoleCP(originalCP);
            return 1;  // 参数错误
        }
    }

    // 输出配置信息
    PrintInfo("启动器配置:");
    std::wcout << L"  客户端: " << options.clientPath << std::endl;
    std::wcout << L"  DLL:    " << options.dllPath << std::endl;
    std::wcout << L"  服务器: " << options.serverConfig.name << std::endl;
    std::wcout << L"  IP:     " << options.serverConfig.ip << std::endl;
    std::wcout << L"  端口:   " << options.serverConfig.port << std::endl;

    // ========================================================================
    // 执行启动和注入
    // ========================================================================
    GameLauncher::Launcher::ProcessLauncher launcher;
    DWORD processId = 0;

    PrintInfo("正在启动客户端进程...");
    GameLauncher::Launcher::LaunchResult result = launcher.Launch(options, &processId);

    // ========================================================================
    // 处理结果
    // ========================================================================
    if (result == GameLauncher::Launcher::LaunchResult::Success) {
        PrintSuccess("客户端启动成功！");
        std::wcout << L"进程ID: " << processId << std::endl;

        // 恢复原始代码页
        ::SetConsoleOutputCP(originalOutputCP);
        ::SetConsoleCP(originalCP);
        return 0;
    } else {
        // 启动失败，输出详细错误信息
        std::string errorMsg = GameLauncher::Launcher::LaunchResultToString(result);
        PrintError(errorMsg);

        // 输出Windows详细错误信息
        const std::wstring& detailError = launcher.GetLastErrorInfo();
        if (!detailError.empty()) {
            std::wcerr << L"详细错误: " << detailError << std::endl;
        }

        // 恢复原始代码页
        ::SetConsoleOutputCP(originalOutputCP);
        ::SetConsoleCP(originalCP);
        return 2;  // 启动失败
    }
}

/**
 * @brief Windows GUI入口点（备用）
 * @details 如果编译为GUI应用程序而非控制台应用程序，
 *          使用WinMain作为入口点。
 *          当前实现直接调用main函数。
 */
INT WINAPI WinMain(HINSTANCE /*hInstance*/,
                   HINSTANCE /*hPrevInstance*/,
                   LPSTR /*lpCmdLine*/,
                   INT /*nCmdShow*/) {
    // 获取宽字符命令行参数
    int argc = 0;
    LPWSTR* argvW = ::CommandLineToArgvW(::GetCommandLineW(), &argc);

    // 构建窄字符参数数组（仅用于兼容性，实际使用宽字符解析）
    std::vector<std::string> argStrings;
    std::vector<char*> argPointers;

    if (argvW != nullptr) {
        for (int i = 0; i < argc; ++i) {
            // 将宽字符转换为UTF-8窄字符
            int requiredSize = ::WideCharToMultiByte(
                CP_UTF8, 0, argvW[i], -1, nullptr, 0, nullptr, nullptr
            );
            if (requiredSize > 0) {
                std::string arg(requiredSize - 1, '\0');
                ::WideCharToMultiByte(
                    CP_UTF8, 0, argvW[i], -1, &arg[0], requiredSize, nullptr, nullptr
                );
                argStrings.push_back(std::move(arg));
            }
        }
        ::LocalFree(argvW);
    }

    // 构建argv数组
    for (auto& str : argStrings) {
        argPointers.push_back(&str[0]);
    }

    int result = main(static_cast<int>(argPointers.size()),
                      argPointers.empty() ? nullptr : argPointers.data());

    return result;
}
