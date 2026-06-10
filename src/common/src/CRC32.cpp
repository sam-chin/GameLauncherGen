/**
 * @file CRC32.cpp
 * @brief CRC32查找表法实现
 * @details 本文件实现了基于IEEE 802.3标准的CRC32算法。
 *          使用预计算查找表实现O(1)单字节处理，效率极高。
 *          
 *          线程安全策略：
 *          - 查找表使用静态存储期，只读访问
 *          - 初始化使用std::call_once保证单次执行
 *          - 计算器实例状态独立，多实例可并发使用
 * 
 *          内存安全：
 *          - 无动态内存分配
 *          - 所有操作基于栈和静态存储
 *          - 输入参数严格校验
 */

#include "CRC32.h"
#include <mutex>
#include <cstring>

namespace GameLauncher {
namespace Common {

// ---------------------------------------------------------------------------
// 静态成员定义
// ---------------------------------------------------------------------------

/**
 * @brief CRC32查找表
 * @details 256个32位条目，每个对应一个8位输入值。
 *          使用静态存储期，程序生命周期内持续存在。
 */
uint32_t CRC32::s_lookupTable[256] = { 0 };

/**
 * @brief 查找表初始化状态标志
 */
bool CRC32::s_tableInitialized = false;

/**
 * @brief 用于线程安全初始化的互斥锁
 * @details std::call_once需要配合once_flag使用，
 *          但此处使用更简单的双检锁模式（已初始化后无锁开销）
 */
static std::once_flag s_initFlag;

// ---------------------------------------------------------------------------
// 构造函数与析构函数
// ---------------------------------------------------------------------------

CRC32::CRC32() : m_state(0xFFFFFFFFu) {
    // 确保查找表已初始化（线程安全）
    EnsureTableInitialized();
}

CRC32::~CRC32() {
    // 无需特殊清理，但清零状态可减少信息泄露风险
    m_state = 0;
}

// ---------------------------------------------------------------------------
// 核心计算接口
// ---------------------------------------------------------------------------

ErrorCode CRC32::Update(const void* data, size_t length) {
    // 健壮性：参数校验
    // 如果数据指针为空但长度大于0，这是明显的编程错误，必须返回错误
    if (data == nullptr && length > 0) {
        return ErrorCode::InvalidParameter;
    }

    // 长度为0是合法的空操作，直接返回成功
    // 这允许调用方无条件调用Update而无需前置检查
    if (length == 0) {
        return ErrorCode::Success;
    }

    // 确保查找表可用
    EnsureTableInitialized();

    // 将void指针转换为字节指针进行处理
    const uint8_t* bytes = static_cast<const uint8_t*>(data);

    // 核心查表算法：
    // CRC32更新公式：state = table[(state ^ byte) & 0xFF] ^ (state >> 8)
    // 这是标准的按字节查表法，每个字节仅需一次查表和两次异或
    for (size_t i = 0; i < length; ++i) {
        m_state = s_lookupTable[(m_state ^ bytes[i]) & 0xFF] ^ (m_state >> 8);
    }

    return ErrorCode::Success;
}

uint32_t CRC32::Finalize() {
    // CRC32标准要求最终结果对初始值取反
    // 0xFFFFFFFF是标准初始值，最终结果为 state ^ 0xFFFFFFFF = ~state
    uint32_t result = ~m_state;

    // 重置状态，允许对象复用
    Reset();

    return result;
}

void CRC32::Reset() {
    // 恢复为初始状态
    m_state = 0xFFFFFFFFu;
}

// ---------------------------------------------------------------------------
// 静态便捷接口
// ---------------------------------------------------------------------------

uint32_t CRC32::Calculate(const void* data, size_t length) {
    // 参数校验：空数据返回0（与空输入的CRC约定一致）
    if (data == nullptr || length == 0) {
        return 0;
    }

    // 创建临时计算器，处理数据，返回结果
    CRC32 calculator;
    calculator.Update(data, length);
    return calculator.Finalize();
}

uint32_t CRC32::Calculate(const ByteArray& data) {
    if (data.empty()) {
        return 0;
    }

    return Calculate(data.data(), data.size());
}

bool CRC32::Verify(const void* data, size_t length, uint32_t expectedCrc) {
    // 参数校验
    if (data == nullptr && length > 0) {
        return false;
    }

    uint32_t actualCrc = Calculate(data, length);
    return actualCrc == expectedCrc;
}

// ---------------------------------------------------------------------------
// 查找表初始化
// ---------------------------------------------------------------------------

void CRC32::InitializeTable() {
    // IEEE 802.3标准多项式：x^32 + x^26 + x^23 + x^22 + x^16 + x^12 + x^11
    //                      + x^10 + x^8 + x^7 + x^5 + x^4 + x^2 + x + 1
    // 对应十六进制表示：0xEDB88320（反转后的形式，用于LSB-first算法）
    constexpr uint32_t polynomial = 0xEDB88320u;

    // 为每个可能的字节值（0-255）预计算CRC32余数
    for (uint32_t i = 0; i < 256; ++i) {
        uint32_t crc = i;

        // 对当前字节执行8次位运算（模拟多项式除法）
        for (int j = 0; j < 8; ++j) {
            // 如果最低位为1，右移并与多项式异或
            // 如果最低位为0，仅右移
            if (crc & 1u) {
                crc = (crc >> 1) ^ polynomial;
            } else {
                crc >>= 1;
            }
        }

        s_lookupTable[i] = crc;
    }

    s_tableInitialized = true;
}

void CRC32::EnsureTableInitialized() {
    // 使用std::call_once确保线程安全的单次初始化
    // 这是C++11标准提供的机制，比双检锁更简洁可靠
    std::call_once(s_initFlag, &CRC32::InitializeTable);
}

} // namespace Common
} // namespace GameLauncher
