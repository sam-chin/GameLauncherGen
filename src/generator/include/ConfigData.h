#pragma once

/**
 * @file ConfigData.h
 * @brief 游戏启动器生成器 - 配置数据结构定义
 * @details 定义所有需要嵌入到启动器模板中的配置数据结构
 * 采用二进制序列化方式，所有字符串使用UTF-8编码存储
 */

#include <cstdint>
#include <string>
#include <vector>
#include <array>
#include <memory>

// 魔数和版本定义
namespace GeneratorConstants {
    // 配置数据区块魔数 "LNCH" (Launcher Config)
    constexpr uint32_t CONFIG_SECTION_MAGIC = 0x4C4E4348;
    // 当前配置格式版本
    constexpr uint32_t CONFIG_VERSION = 0x00010000; // 1.0.0.0
    // 最大字符串长度
    constexpr size_t MAX_STRING_LENGTH = 2048;
    // 最大路径长度
    constexpr size_t MAX_PATH_LENGTH = 512;
    // 加密密钥最大长度
    constexpr size_t MAX_KEY_LENGTH = 256;
}

// 图片类型枚举
enum class ImageType : uint32_t {
    Background = 0,     // 背景图片
    ButtonNormal,       // 按钮正常状态
    ButtonHover,        // 按钮悬停状态
    ButtonPressed,      // 按钮按下状态
    Logo,               // Logo图片
    Count               // 类型总数
};

// UI风格预设
enum class UIStylePreset : uint32_t {
    Default = 0,        // 默认风格
    Dark,               // 深色风格
    Light,              // 浅色风格
    Custom,             // 自定义风格
    Count               // 预设总数
};

// 更新策略枚举
enum class UpdateStrategy : uint32_t {
    LocalPatch = 0,     // 本地补丁优先
    OnlineDownload,     // 在线下载优先
    ForceOnline         // 强制在线更新
};

// 文件冲突处理策略
enum class ConflictPolicy : uint32_t {
    Overwrite = 0,      // 覆盖现有文件
    Skip,               // 跳过现有文件
    Prompt              // 提示用户（启动器端处理）
};

// 图片文件信息结构
#pragma pack(push, 1)
struct ImageFileInfo {
    uint32_t    type;           // ImageType 值
    uint32_t    format;         // 0=BMP, 1=PNG
    uint32_t    dataOffset;     // 数据在区块中的偏移
    uint32_t    dataSize;       // 数据大小（字节）
    uint32_t    width;          // 图片宽度
    uint32_t    height;         // 图片高度
    uint32_t    reserved[2];    // 保留字段

    ImageFileInfo() noexcept;
};

// 网络配置结构
struct NetworkConfig {
    uint32_t    primaryServerlistOffset;    // 主服务器列表URL偏移
    uint32_t    primaryServerlistLength;    // 主服务器列表URL长度
    uint32_t    backupServerlistOffset;     // 备用服务器列表URL偏移
    uint32_t    backupServerlistLength;     // 备用服务器列表URL长度
    uint32_t    patchBaseUrlOffset;         // 补丁下载基础URL偏移
    uint32_t    patchBaseUrlLength;         // 补丁下载基础URL长度
    uint32_t    timeoutSeconds;             // 超时时间（秒）
    uint32_t    retryCount;                 // 重试次数
    uint32_t    reserved[4];                // 保留字段

    NetworkConfig() noexcept;
};

// 业务URL配置结构
struct BusinessUrls {
    uint32_t    officialSiteOffset;     // 官网URL偏移
    uint32_t    officialSiteLength;     // 官网URL长度
    uint32_t    rechargeUrlOffset;      // 充值URL偏移
    uint32_t    rechargeUrlLength;      // 充值URL长度
    uint32_t    customerServiceOffset;  // 客服URL偏移
    uint32_t    customerServiceLength;  // 客服URL长度
    uint32_t    registerUrlOffset;      // 注册URL偏移
    uint32_t    registerUrlLength;      // 注册URL长度
    uint32_t    reserved[4];            // 保留字段

    BusinessUrls() noexcept;
};

// 更新策略配置结构
struct UpdatePolicy {
    uint32_t    strategy;           // UpdateStrategy 值
    uint32_t    forceCrcCheck;      // 强制CRC校验 (0/1)
    uint32_t    autoCreateSubdirs;  // 自动创建子目录 (0/1)
    uint32_t    conflictPolicy;     // ConflictPolicy 值
    uint32_t    reserved[4];        // 保留字段

    UpdatePolicy() noexcept;
};

// 主配置头结构（位于配置区块起始位置）
struct ConfigHeader {
    uint32_t    magic;                  // 魔数 CONFIG_SECTION_MAGIC
    uint32_t    version;                // 配置版本
    uint32_t    totalSize;              // 整个配置区块大小
    uint32_t    imageCount;             // 图片数量
    uint32_t    imageTableOffset;       // 图片信息表偏移
    uint32_t    networkConfigOffset;    // 网络配置偏移
    uint32_t    businessUrlsOffset;     // 业务URL偏移
    uint32_t    updatePolicyOffset;     // 更新策略偏移
    uint32_t    clientPathOffset;       // 客户端路径偏移
    uint32_t    clientPathLength;       // 客户端路径长度
    uint32_t    extensionDllOffset;     // 扩展DLL路径偏移
    uint32_t    extensionDllLength;     // 扩展DLL路径长度
    uint32_t    uiStylePreset;          // UI风格预设
    uint32_t    encryptionKeyOffset;    // 加密密钥偏移
    uint32_t    encryptionKeyLength;    // 加密密钥长度
    uint32_t    reserved[8];            // 保留字段

    ConfigHeader() noexcept;
};
#pragma pack(pop)

// C++ 高级配置数据结构（用于UI和序列化）
struct LauncherConfig {
    // UI风格
    UIStylePreset       uiStyle = UIStylePreset::Default;

    // 图片路径
    std::wstring        backgroundPath;
    std::wstring        buttonNormalPath;
    std::wstring        buttonHoverPath;
    std::wstring        buttonPressedPath;
    std::wstring        logoPath;

    // 图片二进制数据（加载后缓存）
    struct ImageData {
        std::vector<uint8_t>    data;
        uint32_t                width = 0;
        uint32_t                height = 0;
        bool                    loaded = false;
    };
    std::array<ImageData, static_cast<size_t>(ImageType::Count)> images;

    // 网络配置
    std::wstring        primaryServerlistUrl;
    std::wstring        backupServerlistUrl;
    std::wstring        patchBaseUrl;
    uint32_t            timeoutSeconds = 30;
    uint32_t            retryCount = 3;

    // 业务URL
    std::wstring        officialSiteUrl;
    std::wstring        rechargeUrl;
    std::wstring        customerServiceUrl;
    std::wstring        registerUrl;

    // 更新策略
    UpdateStrategy      updateStrategy = UpdateStrategy::LocalPatch;
    bool                forceCrcCheck = true;
    bool                autoCreateSubdirs = true;
    ConflictPolicy      conflictPolicy = ConflictPolicy::Overwrite;

    // 路径配置
    std::wstring        clientPath = L"client.exe";
    std::wstring        extensionDllPath;

    // 加密配置
    std::wstring        encryptionKey;

    // 补丁打包配置
    std::wstring        patchSourceDir;
    std::wstring        patchOutputPath;

    // 生成配置
    std::wstring        templatePath;       // 启动器模板路径
    std::wstring        outputPath;         // 输出路径

    LauncherConfig() = default;

    // 序列化整个配置为字节数组
    [[nodiscard]] std::vector<uint8_t> Serialize() const;

    // 从字节数组反序列化
    [[nodiscard]] bool Deserialize(const uint8_t* data, size_t size);

    // 验证配置完整性
    [[nodiscard]] bool Validate(std::wstring& errorMessage) const;

    // 清空图片数据
    void ClearImages();

    // 加载图片文件（带BMP/PNG魔数校验）
    [[nodiscard]] bool LoadImageFile(ImageType type, const std::wstring& path, std::wstring& errorMessage);

private:
    // 辅助序列化函数
    void SerializeString(std::vector<uint8_t>& buffer, const std::wstring& str, uint32_t& outOffset, uint32_t& outLength) const;
    std::wstring DeserializeString(const uint8_t* data, uint32_t offset, uint32_t length) const;
};

// 补丁文件条目
struct PatchFileEntry {
    std::wstring        relativePath;       // 相对路径
    uint64_t            originalSize;       // 原始大小
    uint64_t            compressedSize;     // 压缩后大小
    uint32_t            crc32;              // CRC32校验值
    uint64_t            offset;             // 在补丁包中的偏移
    bool                compressed;         // 是否压缩
};

// 补丁包头结构
#pragma pack(push, 1)
struct PatchPackageHeader {
    uint32_t    magic;              // 魔数 "PTCH"
    uint32_t    version;            // 版本
    uint32_t    fileCount;          // 文件数量
    uint32_t    fileTableOffset;    // 文件表偏移
    uint32_t    dataOffset;         // 数据区偏移
    uint32_t    flags;              // 标志位
    uint32_t    reserved[4];        // 保留

    PatchPackageHeader() noexcept;
};
#pragma pack(pop)
