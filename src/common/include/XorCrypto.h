/**
 * @file XorCrypto.h
 * @brief 基于动态密钥的XOR流加密/解密接口
 * @details 本模块提供一种轻量级的流式XOR加密方案，适用于游戏资源包的简单混淆保护。
 *          
 *          设计特点：
 *          - 流式处理：支持大文件分块处理，无需一次性加载全部数据
 *          - 动态密钥：密钥在加密过程中根据位置动态变化，增强安全性
 *          - 对称性：加密和解密使用完全相同的算法，简化实现
 *          - 无外部依赖：纯标准C++实现，便于移植
 * 
 *          安全说明：
 *          - XOR加密属于轻量级混淆，不适合高安全需求场景
 *          - 动态密钥通过密钥派生函数生成，增加破解难度
 *          - 密钥应妥善保管，不应硬编码在可执行文件中
 *
 * @note 加密和解密是同一个操作（XOR的自反性）
 * @warning 密钥长度至少为4字节，过短的密钥会显著降低安全性
 */

#pragma once

#ifndef XOR_CRYPTO_H
#define XOR_CRYPTO_H

#include "CommonTypes.h"
#include <cstdint>
#include <cstddef>
#include <memory>
#include <vector>

namespace GameLauncher {
namespace Common {

// ---------------------------------------------------------------------------
// 前向声明
// ---------------------------------------------------------------------------

class XorCryptoImpl;  // PIMPL模式，隐藏实现细节

// ---------------------------------------------------------------------------
// XOR流加密器类
// ---------------------------------------------------------------------------

/**
 * @class XorCrypto
 * @brief XOR流加密/解密器
 * @details 使用动态密钥的流式XOR加密方案。
 * 
 *          密钥派生机制：
 *          - 原始密钥通过多次哈希/混合生成密钥流
 *          - 每个字节位置使用不同的密钥值
 *          - 密钥流周期足够大，避免简单重复模式
 * 
 *          使用示例：
 *          @code
 *          // 加密
 *          XorCrypto crypto(key, keyLen);
 *          crypto.Process(input, output, dataLen);
 *          
 *          // 解密（使用相同的密钥）
 *          XorCrypto crypto2(key, keyLen);
 *          crypto2.Process(encrypted, decrypted, dataLen); // 得到原始数据
 *          @endcode
 */
class COMMON_API XorCrypto {
public:
    /**
     * @brief 构造函数
     * @param key 加密密钥指针，不能为空
     * @param keyLength 密钥长度（字节），必须 >= 4
     * @details 根据提供的密钥初始化内部状态，包括：
     *          - 验证密钥有效性
     *          - 初始化密钥流生成器状态
     *          - 预计算部分密钥流（可选优化）
     * 
     * @throw 不抛出异常，错误通过后续Process返回值报告
     */
    XorCrypto(const void* key, size_t keyLength);

    /**
     * @brief 析构函数
     * @details 清理内部密钥状态，防止敏感信息残留在内存中。
     *          使用SecureZeroMemory或等效方法清零密钥缓冲区。
     */
    ~XorCrypto();

    // 禁用拷贝，防止密钥意外共享
    XorCrypto(const XorCrypto&) = delete;
    XorCrypto& operator=(const XorCrypto&) = delete;

    // 允许移动（转移所有权）
    XorCrypto(XorCrypto&& other) noexcept;
    XorCrypto& operator=(XorCrypto&& other) noexcept;

    /**
     * @brief 处理（加密或解密）数据块
     * @param input 输入数据指针
     * @param output 输出数据指针，可与input相同（原地处理）
     * @param length 数据长度（字节）
     * @return ErrorCode 操作结果
     * 
     * @details 核心处理函数，对输入数据的每个字节应用动态XOR密钥。
     *          支持原地处理（input == output），节省内存。
     *          
     *          处理流程：
     *          1. 验证参数有效性
     *          2. 逐字节生成动态密钥
     *          3. 执行XOR运算
     *          4. 更新内部流位置
     * 
     *          健壮性处理：
     *          - input/output为nullptr时返回InvalidParameter
     *          - length为0时返回Success（空操作合法）
     *          - 自动处理内存重叠（原地模式安全）
     */
    ErrorCode Process(const void* input, void* output, size_t length);

    /**
     * @brief 处理内存中的字节数组
     * @param data 输入/输出数据（原地处理）
     * @return ErrorCode 操作结果
     */
    ErrorCode Process(ByteArray& data);

    /**
     * @brief 处理内存中的字节数组（返回新副本）
     * @param input 输入数据
     * @param output 输出数据
     * @return ErrorCode 操作结果
     */
    ErrorCode Process(const ByteArray& input, ByteArray& output);

    /**
     * @brief 重置加密流状态
     * @details 将内部流位置重置为0，使下一次Process从头开始使用密钥流。
     *          适用于需要独立处理多个数据块的场景。
     */
    void Reset();

    /**
     * @brief 获取当前流位置
     * @return 当前已处理的字节数
     */
    uint64_t GetPosition() const;

    /**
     * @brief 设置流位置
     * @param position 新的流位置
     * @details 用于随机访问加密数据，如跳过文件头后解密特定偏移。
     *          设置后会从该位置重新生成对应的密钥流。
     */
    void SetPosition(uint64_t position);

private:
    /**
     * @brief PIMPL实现指针
     * @details 隐藏实现细节，减少头文件依赖，提高编译效率。
     *          同时避免在头文件中暴露敏感的数据结构。
     */
    std::unique_ptr<XorCryptoImpl> m_impl;
};

} // namespace Common
} // namespace GameLauncher

#endif // XOR_CRYPTO_H
