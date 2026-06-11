#pragma once

/**
 * @file PatchBuilder.h
 * @brief 游戏启动器生成器 - 补丁打包模块
 * @details 提供一键补丁打包功能：
 *          1. 扫描源目录中的所有文件
 *          2. 计算CRC32校验值
 *          3. 可选压缩（使用zlib）
 *          4. 使用自定义加密密钥加密
 *          5. 输出.pkg补丁包文件
 */

#include <string>
#include <vector>
#include <functional>
#include <cstdint>
#include "ConfigData.h"

// 打包结果枚举
enum class PackResult {
    Success = 0,            // 成功
    InvalidInput,           // 输入参数无效
    SourceDirNotFound,      // 源目录不存在
    OutputCreateError,      // 创建输出文件失败
    FileReadError,          // 读取文件失败
    CompressionError,       // 压缩失败
    EncryptionError,        // 加密失败
    ChecksumError,          // 校验和计算失败
    Cancelled,              // 用户取消
    UnknownError            // 未知错误
};

// 打包进度信息
struct PackProgressInfo {
    int         overallPercentage;      // 总体进度 0-100
    int         currentFileIndex;       // 当前文件索引
    int         totalFiles;             // 总文件数
    std::wstring currentFileName;       // 当前处理的文件名
    std::wstring statusMessage;         // 状态消息
};

// 进度回调函数类型
using PackProgressCallback = std::function<void(const PackProgressInfo& info)>;

// 补丁打包器类
class CPatchBuilder {
public:
    CPatchBuilder() = default;
    ~CPatchBuilder() = default;

    // 禁止拷贝
    CPatchBuilder(const CPatchBuilder&) = delete;
    CPatchBuilder& operator=(const CPatchBuilder&) = delete;

    /**
     * @brief 设置进度回调
     */
    void SetProgressCallback(PackProgressCallback callback);

    /**
     * @brief 设置是否启用压缩
     */
    void SetCompressionEnabled(bool enabled) noexcept { m_compressionEnabled = enabled; }

    /**
     * @brief 设置压缩级别 (0-9, 默认6)
     */
    void SetCompressionLevel(int level) noexcept {
        m_compressionLevel = (level < 0) ? 0 : (level > 9) ? 9 : level;
    }

    /**
     * @brief 设置加密密钥
     */
    void SetEncryptionKey(const std::wstring& key) { m_encryptionKey = key; }

    /**
     * @brief 取消打包操作
     */
    void Cancel() noexcept { m_cancelled = true; }

    /**
     * @brief 重置取消状态
     */
    void ResetCancel() noexcept { m_cancelled = false; }

    /**
     * @brief 执行补丁打包（主入口函数）
     * @param sourceDir 源目录路径
     * @param outputPath 输出.pkg文件路径
     * @return 打包结果
     *
     * @details 打包流程：
     * 1. 扫描源目录，收集所有文件
     * 2. 对每个文件计算CRC32
     * 3. 可选压缩文件数据
     * 4. 使用XOR+密钥加密
     * 5. 写入.pkg文件头、文件表、数据区
     */
    [[nodiscard]] PackResult BuildPatch(
        const std::wstring& sourceDir,
        const std::wstring& outputPath
    );

    /**
     * @brief 验证补丁包完整性
     * @param patchPath 补丁包路径
     * @return 是否有效
     */
    [[nodiscard]] bool VerifyPatch(const std::wstring& patchPath);

    /**
     * @brief 获取最后一次错误信息
     */
    [[nodiscard]] const std::wstring& GetLastErrorMessage() const noexcept { return m_lastError; }

    /**
     * @brief 将结果码转换为可读字符串
     */
    [[nodiscard]] static std::wstring ResultToString(PackResult result);

    /**
     * @brief 计算文件的CRC32
     */
    [[nodiscard]] static uint32_t CalculateCRC32(const std::vector<uint8_t>& data);

private:
    bool m_compressionEnabled = true;
    int m_compressionLevel = 6;
    std::wstring m_encryptionKey;
    bool m_cancelled = false;
    PackProgressCallback m_progressCallback;
    std::wstring m_lastError;

    // 扫描目录获取所有文件
    [[nodiscard]] bool ScanDirectory(
        const std::wstring& dir,
        std::vector<std::wstring>& outFiles,
        std::wstring& outBasePath
    );

    // 读取文件到内存
    [[nodiscard]] bool ReadFileToMemory(
        const std::wstring& path,
        std::vector<uint8_t>& outData
    );

    // 压缩数据
    [[nodiscard]] bool CompressData(
        const std::vector<uint8_t>& input,
        std::vector<uint8_t>& output
    );

    // 加密数据（简单XOR+密钥混合）
    void EncryptData(
        std::vector<uint8_t>& data,
        const std::wstring& key
    );

    // 报告进度
    void ReportProgress(const PackProgressInfo& info);

    // 设置错误
    void SetError(const std::wstring& message);

    // 获取相对路径
    [[nodiscard]] std::wstring GetRelativePath(
        const std::wstring& basePath,
        const std::wstring& fullPath
    );

    // 写入补丁包文件
    [[nodiscard]] bool WritePatchPackage(
        const std::wstring& outputPath,
        const std::vector<PatchFileEntry>& entries,
        const std::vector<std::vector<uint8_t>>& fileDatas,
        const PatchPackageHeader& header
    );
};
