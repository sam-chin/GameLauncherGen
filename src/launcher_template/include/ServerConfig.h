/**
 * @file ServerConfig.h
 * @brief 服务器配置数据结构定义
 * @details 本文件定义了游戏启动器所需的服务器连接参数结构体，
 *          以及参数格式化工具。所有字符串均使用宽字符(wchar_t)以确保
 *          完整的Unicode支持，兼容中文服务器名称等场景。
 *
 * @note 参数格式: ur;name=server_name;ip=IP;port=port;ra=163.com
 * @warning 本模块仅包含纯数据结构，不涉及任何系统调用或资源管理
 */

#pragma once

#ifndef SERVER_CONFIG_H
#define SERVER_CONFIG_H

#include <string>
#include <cstdint>

// ---------------------------------------------------------------------------
// 命名空间定义
// ---------------------------------------------------------------------------

namespace GameLauncher {
namespace Launcher {

// ---------------------------------------------------------------------------
// 服务器配置结构体
// ---------------------------------------------------------------------------

/**
 * @struct ServerConfig
 * @brief 服务器连接配置参数
 * @details 存储启动游戏客户端所需的所有服务器连接信息。
 *          所有字符串字段使用std::wstring以支持Unicode字符（如中文服务器名）。
 *
 * 参数格式化输出示例:
 *   ur;name=艾欧尼亚;ip=192.168.1.100;port=7777;ra=163.com
 *
 * 字段说明:
 *   - prefix:   固定前缀 "ur"
 *   - name:     服务器显示名称，支持中文
 *   - ip:       服务器IP地址或域名
 *   - port:     服务器端口号（1-65535）
 *   - ra:       固定后缀 "163.com"
 */
struct ServerConfig {
    std::wstring name;      ///< 服务器名称（支持中文等Unicode字符）
    std::wstring ip;        ///< 服务器IP地址或主机名
    uint16_t     port;      ///< 服务器端口号

    /**
     * @brief 默认构造函数
     * @details 将端口初始化为0，表示未配置状态
     */
    ServerConfig() noexcept
        : name()
        , ip()
        , port(0)
    {}

    /**
     * @brief 完整参数构造函数
     * @param[in] serverName 服务器名称
     * @param[in] serverIp   服务器IP地址或主机名
     * @param[in] serverPort 服务器端口号
     */
    ServerConfig(const std::wstring& serverName,
                 const std::wstring& serverIp,
                 uint16_t serverPort)
        : name(serverName)
        , ip(serverIp)
        , port(serverPort)
    {}

    /**
     * @brief 移动构造函数
     * @details 支持高效传递临时对象，避免不必要的字符串拷贝
     */
    ServerConfig(ServerConfig&& other) noexcept
        : name(std::move(other.name))
        , ip(std::move(other.ip))
        , port(other.port)
    {
        other.port = 0;
    }

    /**
     * @brief 移动赋值运算符
     */
    ServerConfig& operator=(ServerConfig&& other) noexcept {
        if (this != &other) {
            name = std::move(other.name);
            ip = std::move(other.ip);
            port = other.port;
            other.port = 0;
        }
        return *this;
    }

    // 禁用拷贝构造和拷贝赋值，强制使用移动语义减少字符串拷贝
    // 若业务需要拷贝，可显式调用Clone()方法
    ServerConfig(const ServerConfig&) = default;
    ServerConfig& operator=(const ServerConfig&) = default;

    /**
     * @brief 验证配置参数的有效性
     * @return true  所有字段均有效
     * @return false 至少一个字段无效（如空名称、空IP、端口为0）
     *
     * @note 本方法仅做基础格式校验，不做网络连通性测试
     */
    bool IsValid() const noexcept {
        return !name.empty()
            && !ip.empty()
            && (port > 0);
    }

    /**
     * @brief 将配置格式化为启动参数字符串
     * @return 格式化后的宽字符串，格式: ur;name=xxx;ip=xxx;port=xxx;ra=163.com
     *
     * @warning 调用前应先使用 IsValid() 确认配置有效，否则可能产生不完整参数
     *
     * 示例输出:
     *   L"ur;name=艾欧尼亚;ip=192.168.1.100;port=7777;ra=163.com"
     */
    std::wstring ToParameterString() const;
};

} // namespace Launcher
} // namespace GameLauncher

#endif // SERVER_CONFIG_H
