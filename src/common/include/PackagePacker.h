/**
 * @file PackagePacker.h
 * @brief 多文件流打包/解包器接口
 * @details 本模块提供自定义包格式的打包和解包功能，支持：
 *          - 将多个文件打包为单个二进制包
 *          - 从包中解包恢复原始文件结构
 *          - 可选的XOR加密保护
 *          - CRC32完整性校验
 * 
 *          包文件格式（小端序）：
 *          
 *          [文件头 - 32字节]
 *          + 偏移0:  魔数      (4字节)  0x504B4731 "PKG1"
 *          + 偏移4:  版本号    (2字节)  当前为1
 *          + 偏移6:  保留      (2字节)  对齐填充
 *          + 偏移8:  文件数量  (4字节)  包内包含的文件数
 *          + 偏移12: 原始大小  (8字节)  所有文件原始大小之和
 *          + 偏移20: 加密大小  (8字节)  加密后的数据总大小
 *          + 偏移28: 头CRC32  (4字节)  文件头本身的校验码
 *          
 *          [文件条目表 - 每个条目变长]
 *          每个条目：
 *          + 路径长度 (4字节)  包括终止符的wchar_t字符数
 *          + 相对路径 (变长)   UTF-16LE编码的宽字符串
 *          + 原始大小 (8字节)  该文件未加密时的大小
 *          + 加密大小 (8字节)  该文件加密后的大小（与原始大小相同或更大）
 *          + 数据偏移 (8字节)  该文件数据在包中的起始位置
 *          + 数据CRC32(4字节)  该文件数据的CRC32校验码
 *          + 条目CRC32(4字节)  本条目除此项外的CRC32校验码
 *          
 *          [数据区 - 顺序存储]
 *          每个文件的数据顺序存放，可选择加密
 *
 * @note 所有路径使用wchar_t宽字符，支持完整Unicode
 * @warning 打包大文件时应注意内存使用，建议使用流式接口
 */

#pragma once

#ifndef PACKAGE_PACKER_H
#define PACKAGE_PACKER_H

#include "CommonTypes.h"
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace GameLauncher {
namespace Common {

// ---------------------------------------------------------------------------
// 前向声明
// ---------------------------------------------------------------------------

class XorCrypto;  // 可选的加密器

// ---------------------------------------------------------------------------
// 包文件条目结构
// ---------------------------------------------------------------------------

/**
 * @struct PackageEntry
 * @brief 包内单个文件的元数据
 * @details 描述包中一个文件的所有必要信息，包括路径、大小、位置和校验码。
 *          此结构在打包时由调用方提供，在解包时从包文件中读取。
 */
struct COMMON_API PackageEntry {
    std::wstring relativePath;   ///< 相对路径（如 L"data\textures\hero.png"）
    uint64_t originalSize;       ///< 原始文件大小（字节）
    uint64_t encryptedSize;      ///< 加密后大小（通常等于原始大小）
    uint64_t dataOffset;         ///< 数据在包文件中的偏移量
    uint32_t dataCrc32;          ///< 文件数据的CRC32校验码
    uint32_t entryCrc32;         ///< 条目本身的CRC32校验码（除本字段）

    /**
     * @brief 默认构造函数
     * @details 初始化所有数值成员为0
     */
    PackageEntry();
};

/**
 * @brief 包条目列表类型
 */
using PackageEntryList = std::vector<PackageEntry>;

// ---------------------------------------------------------------------------
// 打包器类
// ---------------------------------------------------------------------------

/**
 * @class PackagePacker
 * @brief 多文件打包器
 * @details 将多个文件打包为单个二进制包文件。
 *          支持可选的XOR加密和CRC32校验。
 * 
 *          使用示例：
 *          @code
 *          PackagePacker packer;
 *          packer.AddFile(L"data\textures\hero.png", L"textures\hero.png");
 *          packer.AddFile(L"data\sounds\bgm.mp3", L"sounds\bgm.mp3");
 *          ErrorCode err = packer.Pack(L"output.pkg", crypto);
 *          @endcode
 */
class COMMON_API PackagePacker {
public:
    /**
     * @brief 默认构造函数
     */
    PackagePacker();

    /**
     * @brief 析构函数
     */
    ~PackagePacker();

    // 禁用拷贝，允许移动
    PackagePacker(const PackagePacker&) = delete;
    PackagePacker& operator=(const PackagePacker&) = delete;
    PackagePacker(PackagePacker&&) noexcept;
    PackagePacker& operator=(PackagePacker&&) noexcept;

    /**
     * @brief 添加文件到打包列表
     * @param sourcePath 源文件的完整路径（磁盘上的位置）
     * @param relativePath 在包内的相对路径（解包时的目标路径）
     * @return ErrorCode 操作结果
     * 
     * @details 将文件加入待打包列表，实际读取和打包在Pack()时执行。
     *          允许添加同一源文件到不同的相对路径（复制行为）。
     *          
     *          健壮性处理：
     *          - 检查源文件是否存在且可读
     *          - 验证相对路径不为空
     *          - 检查路径长度是否超出限制
     */
    ErrorCode AddFile(const std::wstring& sourcePath, const std::wstring& relativePath);

    /**
     * @brief 添加目录到打包列表（递归）
     * @param sourceDir 源目录路径
     * @param baseRelativePath 在包内的基础相对路径
     * @return ErrorCode 操作结果
     * 
     * @details 递归遍历目录，将所有文件添加到打包列表。
     *          保持目录结构，子目录的相对路径自动拼接。
     */
    ErrorCode AddDirectory(const std::wstring& sourceDir, const std::wstring& baseRelativePath);

    /**
     * @brief 清空打包列表
     * @details 移除所有已添加的文件，重置为初始状态
     */
    void Clear();

    /**
     * @brief 获取当前待打包文件数量
     * @return 文件数量
     */
    size_t GetFileCount() const;

    /**
     * @brief 执行打包操作
     * @param outputPath 输出包文件路径
     * @param crypto 可选的加密器，为nullptr时不加密
     * @return ErrorCode 操作结果
     * 
     * @details 核心打包函数，执行以下步骤：
     *          1. 验证所有源文件可访问
     *          2. 计算每个文件的CRC32校验码
     *          3. 构建文件条目表
     *          4. 写入文件头
     *          5. 写入文件条目表
     *          6. 顺序写入文件数据（可选加密）
     *          7. 计算并写入头部CRC32
     * 
     *          错误处理：
     *          - 任何步骤失败时，删除不完整的输出文件
     *          - 使用临时文件名，成功后再重命名
     *          - 所有文件句柄在异常路径下正确关闭
     */
    ErrorCode Pack(const std::wstring& outputPath, XorCrypto* crypto = nullptr);

    /**
     * @brief 设置进度回调
     * @param callback 回调函数，参数为(当前文件索引, 总文件数, 当前文件名)
     * @details 用于显示打包进度，回调频率为每个文件处理时一次
     */
    void SetProgressCallback(std::function<void(size_t, size_t, const std::wstring&)> callback);

private:
    /**
     * @brief 内部实现指针（PIMPL）
     */
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

// ---------------------------------------------------------------------------
// 解包器类
// ---------------------------------------------------------------------------

/**
 * @class PackageUnpacker
 * @brief 包文件解包器
 * @details 从包文件中读取并恢复原始文件。
 *          支持完整性校验和可选的解密。
 * 
 *          使用示例：
 *          @code
 *          PackageUnpacker unpacker;
 *          ErrorCode err = unpacker.Open(L"output.pkg");
 *          if (err == ErrorCode::Success) {
 *              err = unpacker.ExtractAll(L"output_dir", crypto);
 *          }
 *          @endcode
 */
class COMMON_API PackageUnpacker {
public:
    /**
     * @brief 默认构造函数
     */
    PackageUnpacker();

    /**
     * @brief 析构函数
     * @details 确保打开的包文件被正确关闭
     */
    ~PackageUnpacker();

    // 禁用拷贝，允许移动
    PackageUnpacker(const PackageUnpacker&) = delete;
    PackageUnpacker& operator=(const PackageUnpacker&) = delete;
    PackageUnpacker(PackageUnpacker&&) noexcept;
    PackageUnpacker& operator=(PackageUnpacker&&) noexcept;

    /**
     * @brief 打开包文件
     * @param packagePath 包文件路径
     * @return ErrorCode 操作结果
     * 
     * @details 打开包文件并解析文件头，验证：
     *          - 魔数是否正确
     *          - 版本号是否支持
     *          - 头部CRC32是否匹配
     *          - 文件数量是否合理
     * 
     *          打开后，可通过GetEntries()获取文件列表，
     *          或通过ExtractFile()/ExtractAll()提取文件。
     */
    ErrorCode Open(const std::wstring& packagePath);

    /**
     * @brief 关闭包文件
     * @details 关闭文件句柄，释放资源，重置内部状态。
     *          析构时会自动调用，但显式调用可提前释放资源。
     */
    void Close();

    /**
     * @brief 检查是否已打开包文件
     * @return true表示已打开
     */
    bool IsOpen() const;

    /**
     * @brief 获取包内文件条目列表
     * @return 文件条目列表的引用
     * @warning 仅在Open成功后有效
     */
    const PackageEntryList& GetEntries() const;

    /**
     * @brief 获取包内文件数量
     * @return 文件数量
     */
    size_t GetFileCount() const;

    /**
     * @brief 提取单个文件
     * @param entryIndex 文件条目索引
     * @param outputPath 输出路径
     * @param crypto 可选的解密器，为nullptr时不解密
     * @return ErrorCode 操作结果
     * 
     * @details 从包中提取指定文件到磁盘。
     *          自动创建必要的目录结构。
     *          验证数据CRC32，不匹配时返回错误。
     */
    ErrorCode ExtractFile(size_t entryIndex, const std::wstring& outputPath, XorCrypto* crypto = nullptr);

    /**
     * @brief 提取文件到内存
     * @param entryIndex 文件条目索引
     * @param outputData 输出数据缓冲区
     * @param crypto 可选的解密器
     * @return ErrorCode 操作结果
     */
    ErrorCode ExtractFileToMemory(size_t entryIndex, ByteArray& outputData, XorCrypto* crypto = nullptr);

    /**
     * @brief 提取所有文件
     * @param outputDir 输出目录
     * @param crypto 可选的解密器
     * @return ErrorCode 操作结果
     * 
     * @details 提取包内所有文件到指定目录，保持相对路径结构。
     *          自动创建所有必要的子目录。
     *          任一文件提取失败时返回错误，但继续处理其他文件。
     */
    ErrorCode ExtractAll(const std::wstring& outputDir, XorCrypto* crypto = nullptr);

    /**
     * @brief 验证包完整性
     * @return ErrorCode 操作结果
     * 
     * @details 验证所有文件条目的CRC32和数据CRC32。
     *          不提取文件，仅做校验。
     */
    ErrorCode VerifyIntegrity();

    /**
     * @brief 设置进度回调
     * @param callback 回调函数，参数为(当前文件索引, 总文件数, 当前文件名)
     */
    void SetProgressCallback(std::function<void(size_t, size_t, const std::wstring&)> callback);

private:
    /**
     * @brief 内部实现指针（PIMPL）
     */
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace Common
} // namespace GameLauncher

#endif // PACKAGE_PACKER_H
