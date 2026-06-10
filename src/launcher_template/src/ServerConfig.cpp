/**
 * @file ServerConfig.cpp
 * @brief 服务器配置实现
 * @details 实现ServerConfig结构体的ToParameterString方法，
 *          将服务器配置格式化为启动参数字符串。
 */

#include "ServerConfig.h"
#include <sstream>

// ---------------------------------------------------------------------------
// 命名空间定义
// ---------------------------------------------------------------------------

namespace GameLauncher {
namespace Launcher {

// ---------------------------------------------------------------------------
// ServerConfig 方法实现
// ---------------------------------------------------------------------------

/**
 * @brief 将配置格式化为启动参数字符串
 *
 * 格式规范:
 *   ur;name=<服务器名称>;ip=<IP地址>;port=<端口号>;ra=163.com
 *
 * 字段说明:
 *   - ur:   固定前缀，标识参数类型（User Request）
 *   - name: 服务器显示名称，支持中文等Unicode字符
 *   - ip:   服务器IP地址或主机名
 *   - port: 服务器端口号（十进制数字）
 *   - ra:   固定后缀（Return Address / 参考地址）
 *
 * 示例输出:
 *   L"ur;name=艾欧尼亚;ip=192.168.1.100;port=7777;ra=163.com"
 *
 * 实现细节:
 * - 使用std::wstringstream进行字符串拼接
 * - 端口号通过std::to_wstring转换为宽字符串
 * - 所有字段使用分号(;)分隔
 * - 不包含首尾空格
 */
std::wstring ServerConfig::ToParameterString() const {
    std::wstringstream paramStream;

    // 固定前缀
    paramStream << L"ur";

    // 服务器名称
    paramStream << L";name=" << name;

    // IP地址
    paramStream << L";ip=" << ip;

    // 端口号（转换为宽字符串）
    paramStream << L";port=" << std::to_wstring(port);

    // 固定后缀
    paramStream << L";ra=163.com";

    return paramStream.str();
}

} // namespace Launcher
} // namespace GameLauncher
