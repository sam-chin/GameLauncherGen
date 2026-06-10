/**
 * @file CRC32.h
 * @brief CRC32循环冗余校验算法接口
 * @details 本模块提供基于标准查找表法的CRC32计算实现。
 *          采用IEEE 802.3标准多项式：0xEDB88320
 *          
 *          设计特点：
 *          - 使用预计算查找表，单次查表即可处理一个字节，效率极高
 *          - 支持增量计算，可流式处理大数据
 *          - 线程安全：查找表在编译期初始化，无共享可变状态
 *          - 结果与ZIP、PNG、Ethernet等标准兼容
 *
 * @note 查找表在首次使用时惰性初始化，使用std::call_once保证线程安全
 * @warning 对于超大文件，建议使用流式接口避免一次性加载全部数据到内存
 */

#pragma once

#ifndef CRC32_H
#define CRC32_H

#include "CommonTypes.h"
#include <cstdint>
#include <cstddef>

namespace GameLauncher {
namespace Common {

// ---------------------------------------------------------------------------
// CRC32计算器类
// ---------------------------------------------------------------------------

/**
 * @class CRC32
 * @brief CRC32校验码计算器
 * @details 提供两种使用模式：
 *          1. 静态一次性计算：适合小数据块
 *          2. 流式增量计算：适合大文件或网络流
 * 
 *          使用示例（一次性）：
 *          @code
 *          uint32_t crc = CRC32::Calculate(data, size);
 *          @endcode
 * 
 *          使用示例（流式）：
 *          @code
 *          CRC32 calculator;
 *          calculator.Update(chunk1, size1);
 *          calculator.Update(chunk2, size2);
 *          uint32_t crc = calculator.Finalize();
 *          @endcode
 */
class COMMON_API CRC32 {
public:
    /**
     * @brief 默认构造函数
     * @details 初始化内部状态为CRC32起始值（0xFFFFFFFF）
     */
    CRC32();

    /**
     * @brief 析构函数
     * @details 无特殊资源需要释放，但显式声明以遵循RAII原则
     */
    ~CRC32();

    // 禁用拷贝和移动，确保状态唯一性
    CRC32(const CRC32&) = delete;
    CRC32& operator=(const CRC32&) = delete;
    CRC32(CRC32&&) = delete;
    CRC32& operator=(CRC32&&) = delete;

    /**
     * @brief 向计算器添加数据
     * @param data 指向数据缓冲区的指针，允许为nullptr（此时忽略）
     * @param length 数据长度（字节），允许为0（此时忽略）
     * @return ErrorCode::Success 或 ErrorCode::InvalidParameter
     * 
     * @details 此函数可多次调用，每次处理一块数据。
     *          内部状态持续累积，直到调用Finalize()获取最终结果。
     *          
     *          健壮性处理：
     *          - data为nullptr且length>0时返回错误，防止空指针解引用
     *          - length为0时直接返回成功，视为合法的空操作
     */
    ErrorCode Update(const void* data, size_t length);

    /**
     * @brief 完成计算并返回CRC32结果
     * @return 32位CRC校验码
     * @details 对内部状态取反得到最终结果（CRC32标准要求）。
     *          调用后内部状态重置，可重新开始新的计算。
     */
    uint32_t Finalize();

    /**
     * @brief 重置计算器状态
     * @details 将内部状态恢复为初始值，可复用对象进行新的计算
     */
    void Reset();

    // -----------------------------------------------------------------------
    // 静态便捷接口
    // -----------------------------------------------------------------------

    /**
     * @brief 一次性计算数据块的CRC32值
     * @param data 数据指针
     * @param length 数据长度
     * @return CRC32校验码
     * @details 这是最常见的使用方式，适合内存中的完整数据块。
     *          内部自动处理初始化和最终化步骤。
     */
    static uint32_t Calculate(const void* data, size_t length);

    /**
     * @brief 计算字节数组的CRC32值
     * @param data 字节数组
     * @return CRC32校验码
     */
    static uint32_t Calculate(const ByteArray& data);

    /**
     * @brief 验证数据块的CRC32是否匹配期望值
     * @param data 数据指针
     * @param length 数据长度
     * @param expectedCrc 期望的CRC32值
     * @return true表示匹配，false表示不匹配或参数错误
     */
    static bool Verify(const void* data, size_t length, uint32_t expectedCrc);

private:
    /**
     * @brief 当前CRC计算状态
     * @details 使用0xFFFFFFFF作为初始值（CRC32标准），
     *          每次处理一个字节后通过查找表更新
     */
    uint32_t m_state;

    /**
     * @brief 查找表初始化标志
     * @details 确保查找表只初始化一次，避免重复计算
     */
    static bool s_tableInitialized;

    /**
     * @brief CRC32查找表
     * @details 包含256个预计算值，每个对应一个字节输入。
     *          使用静态存储期，全局共享，减少内存占用。
     */
    static uint32_t s_lookupTable[256];

    /**
     * @brief 初始化查找表
     * @details 使用IEEE 802.3标准多项式生成256个条目。
     *          在首次使用时调用，由std::call_once保证线程安全。
     */
    static void InitializeTable();

    /**
     * @brief 确保查找表已初始化
     * @details 线程安全的惰性初始化封装
     */
    static void EnsureTableInitialized();
};

} // namespace Common
} // namespace GameLauncher

#endif // CRC32_H
