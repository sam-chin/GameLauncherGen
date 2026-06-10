#pragma once

/**
 * @file ResourceEmbedder.h
 * @brief 游戏启动器生成器 - 资源嵌入核心模块
 * @details 这是生成器的核心突破点，负责将配置数据、图片、DLL路径等
 *          嵌入到启动器模板EXE中，输出独立的可执行文件。
 *
 * 支持两种嵌入策略：
 * 1. UpdateResource API：直接修改PE文件的资源节
 * 2. 自定义PE节：创建新的PE节存储配置数据
 */

#include "ConfigData.h"
#include <string>
#include <vector>
#include <functional>
#include <Windows.h>

// 嵌入结果枚举
enum class EmbedResult {
    Success = 0,            // 成功
    InvalidInput,           // 输入参数无效
    TemplateNotFound,       // 模板文件不存在
    TemplateReadError,      // 读取模板失败
    OutputCreateError,      // 创建输出文件失败
    ResourceUpdateError,    // UpdateResource失败
    SectionCreateError,     // 创建PE节失败
    InsufficientSpace,      // 空间不足
    PermissionDenied,       // 权限不足
    UnknownError            // 未知错误
};

// 嵌入策略枚举
enum class EmbedStrategy {
    UpdateResource = 0,     // 使用UpdateResource API修改资源
    CustomSection,          // 创建自定义PE节
    AppendToOverlay         // 追加到PE文件尾部（覆盖区）
};

// 进度回调函数类型
using ProgressCallback = std::function<void(int percentage, const std::wstring& status)>;

// 资源嵌入器类
class CResourceEmbedder {
public:
    CResourceEmbedder() = default;
    ~CResourceEmbedder() = default;

    // 禁止拷贝
    CResourceEmbedder(const CResourceEmbedder&) = delete;
    CResourceEmbedder& operator=(const CResourceEmbedder&) = delete;

    /**
     * @brief 设置进度回调函数
     * @param callback 回调函数，percentage范围0-100
     */
    void SetProgressCallback(ProgressCallback callback);

    /**
     * @brief 设置嵌入策略
     * @param strategy 嵌入策略
     */
    void SetEmbedStrategy(EmbedStrategy strategy) noexcept { m_strategy = strategy; }

    /**
     * @brief 获取嵌入策略
     */
    [[nodiscard]] EmbedStrategy GetEmbedStrategy() const noexcept { return m_strategy; }

    /**
     * @brief 执行资源嵌入（主入口函数）
     * @param templatePath 启动器模板EXE路径
     * @param config 启动器配置数据
     * @param outputPath 输出EXE路径
     * @return 嵌入结果
     *
     * @details 核心生成逻辑：
     * 1. 读取模板EXE文件
     * 2. 复制模板到输出路径
     * 3. 根据策略嵌入配置数据
     * 4. 验证输出文件完整性
     */
    [[nodiscard]] EmbedResult EmbedResources(
        const std::wstring& templatePath,
        const LauncherConfig& config,
        const std::wstring& outputPath
    );

    /**
     * @brief 使用UpdateResource策略嵌入
     * @param outputPath 输出文件路径（已复制模板）
     * @param config 配置数据
     * @return 是否成功
     */
    [[nodiscard]] bool EmbedUsingUpdateResource(
        const std::wstring& outputPath,
        const LauncherConfig& config
    );

    /**
     * @brief 使用自定义PE节策略嵌入
     * @param outputPath 输出文件路径（已复制模板）
     * @param config 配置数据
     * @return 是否成功
     */
    [[nodiscard]] bool EmbedUsingCustomSection(
        const std::wstring& outputPath,
        const LauncherConfig& config
    );

    /**
     * @brief 使用追加到覆盖区策略嵌入
     * @param outputPath 输出文件路径（已复制模板）
     * @param config 配置数据
     * @return 是否成功
     */
    [[nodiscard]] bool EmbedUsingOverlay(
        const std::wstring& outputPath,
        const LauncherConfig& config
    );

    /**
     * @brief 将配置数据序列化为资源格式
     * @param config 配置数据
     * @param outData 输出字节数组
     * @return 是否成功
     */
    [[nodiscard]] bool SerializeConfigToResource(
        const LauncherConfig& config,
        std::vector<uint8_t>& outData
    );

    /**
     * @brief 验证输出PE文件完整性
     * @param filePath PE文件路径
     * @return 是否有效
     */
    [[nodiscard]] bool ValidateOutputPE(const std::wstring& filePath);

    /**
     * @brief 获取最后一次错误信息
     */
    [[nodiscard]] const std::wstring& GetLastErrorMessage() const noexcept { return m_lastError; }

    /**
     * @brief 将错误码转换为可读字符串
     */
    [[nodiscard]] static std::wstring ResultToString(EmbedResult result);

private:
    EmbedStrategy m_strategy = EmbedStrategy::UpdateResource;
    ProgressCallback m_progressCallback;
    std::wstring m_lastError;

    // 报告进度
    void ReportProgress(int percentage, const std::wstring& status);

    // 设置错误信息
    void SetError(const std::wstring& message);

    // 复制文件（保留所有属性）
    [[nodiscard]] bool CopyTemplateFile(
        const std::wstring& source,
        const std::wstring& destination
    );

    // 读取整个文件到内存
    [[nodiscard]] bool ReadFileToMemory(
        const std::wstring& path,
        std::vector<uint8_t>& outData
    );

    // 将内存数据写入文件
    [[nodiscard]] bool WriteMemoryToFile(
        const std::wstring& path,
        const std::vector<uint8_t>& data
    );

    // 计算PE文件的覆盖区偏移
    [[nodiscard]] bool GetOverlayOffset(
        const std::vector<uint8_t>& peData,
        size_t& outOffset
    );

    // 创建自定义PE节
    [[nodiscard]] bool CreatePESection(
        std::vector<uint8_t>& peData,
        const char* sectionName,
        const std::vector<uint8_t>& sectionData,
        uint32_t characteristics
    );

    // 对齐函数
    [[nodiscard]] static uint32_t AlignUp(uint32_t value, uint32_t alignment) noexcept {
        return (value + alignment - 1) & ~(alignment - 1);
    }

    // 获取系统错误信息
    [[nodiscard]] static std::wstring GetSystemErrorMessage(DWORD errorCode);
};
