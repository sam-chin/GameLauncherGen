/**
 * @file XorCrypto.cpp
 * @brief 动态密钥XOR流加密/解密实现
 * @details 本文件实现了基于动态密钥流的XOR加密算法。
 *          
 *          密钥派生机制：
 *          - 使用原始密钥作为种子，通过线性同余生成器(LCG)产生伪随机序列
 *          - 结合密钥字节的位置相关混合，增强密钥流的不可预测性
 *          - 密钥流周期约为2^32，足够覆盖大多数游戏资源文件
 * 
 *          安全性说明：
 *          - 本算法提供的是"混淆"级别的保护，而非密码学安全
 *          - 适合防止普通用户的随意查看和修改
 *          - 不适合保护高价值商业机密
 * 
 *          内存安全：
 *          - 使用PIMPL模式隔离实现
 *          - 密钥缓冲区使用SecureZeroMemory清零
 *          - 所有指针参数严格校验
 */

#include "XorCrypto.h"
#include <algorithm>
#include <cstring>

// Windows头文件用于SecureZeroMemory
#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
#endif

namespace GameLauncher {
namespace Common {

// ---------------------------------------------------------------------------
// PIMPL实现类
// ---------------------------------------------------------------------------

/**
 * @class XorCryptoImpl
 * @brief XorCrypto的私有实现
 * @details 隐藏所有内部状态，避免在头文件中暴露敏感信息。
 *          包含密钥缓冲区、密钥流生成器状态和位置跟踪。
 */
class XorCryptoImpl {
public:
    /**
     * @brief 密钥缓冲区
     * @details 存储原始密钥的副本，使用vector便于动态大小和自动清理。
     *          析构时通过SecureZeroMemory显式清零，防止密钥残留在内存中。
     */
    std::vector<uint8_t> keyBuffer;

    /**
     * @brief 当前流位置
     * @details 记录已处理的字节数，用于生成位置相关的动态密钥。
     *          使用64位整数支持超大文件（>4GB）。
     */
    uint64_t position;

    /**
     * @brief 密钥流生成器状态A（LCG状态）
     */
    uint32_t lcgStateA;

    /**
     * @brief 密钥流生成器状态B（辅助混合状态）
     */
    uint32_t lcgStateB;

    /**
     * @brief 密钥长度
     */
    size_t keyLength;

    /**
     * @brief 构造函数
     * @param key 原始密钥指针
     * @param keyLen 密钥长度
     */
    XorCryptoImpl(const void* key, size_t keyLen)
        : position(0)
        , lcgStateA(0)
        , lcgStateB(0)
        , keyLength(keyLen) {
        
        if (key != nullptr && keyLen > 0) {
            // 复制密钥到内部缓冲区
            const uint8_t* keyBytes = static_cast<const uint8_t*>(key);
            keyBuffer.resize(keyLen);
            std::memcpy(keyBuffer.data(), keyBytes, keyLen);

            // 初始化LCG状态
            // 使用密钥内容作为种子，确保不同密钥产生完全不同的密钥流
            InitializeLCGStates();
        }
    }

    /**
     * @brief 析构函数
     * @details 使用SecureZeroMemory清零密钥缓冲区，
     *          防止密钥信息在释放后仍残留在物理内存中。
     *          这是处理敏感数据的标准安全实践。
     */
    ~XorCryptoImpl() {
        SecureClearKey();
    }

    // 禁用拷贝
    XorCryptoImpl(const XorCryptoImpl&) = delete;
    XorCryptoImpl& operator=(const XorCryptoImpl&) = delete;

    // 允许移动
    XorCryptoImpl(XorCryptoImpl&& other) noexcept
        : keyBuffer(std::move(other.keyBuffer))
        , position(other.position)
        , lcgStateA(other.lcgStateA)
        , lcgStateB(other.lcgStateB)
        , keyLength(other.keyLength) {
        // 移动后清空源对象状态
        other.position = 0;
        other.lcgStateA = 0;
        other.lcgStateB = 0;
        other.keyLength = 0;
    }

    XorCryptoImpl& operator=(XorCryptoImpl&& other) noexcept {
        if (this != &other) {
            // 先清空当前密钥
            SecureClearKey();

            // 转移所有权
            keyBuffer = std::move(other.keyBuffer);
            position = other.position;
            lcgStateA = other.lcgStateA;
            lcgStateB = other.lcgStateB;
            keyLength = other.keyLength;

            // 清空源对象
            other.position = 0;
            other.lcgStateA = 0;
            other.lcgStateB = 0;
            other.keyLength = 0;
        }
        return *this;
    }

    /**
     * @brief 初始化LCG状态
     * @details 使用密钥内容派生两个独立的LCG种子。
     *          通过多轮混合确保密钥的每个字节都影响初始状态。
     */
    void InitializeLCGStates() {
        if (keyBuffer.empty()) {
            lcgStateA = 0x12345678u;
            lcgStateB = 0x9ABCDEF0u;
            return;
        }

        // 从密钥内容派生种子
        uint32_t seedA = 0x811C9DC5u;  // FNV-1a偏移基值
        uint32_t seedB = 0x1505u;      // 另一组基值

        // 对密钥的每个字节执行FNV-1a哈希混合
        for (size_t i = 0; i < keyBuffer.size(); ++i) {
            seedA ^= keyBuffer[i];
            seedA *= 0x01000193u;  // FNV-1a质数

            seedB = (seedB << 5) + seedB + keyBuffer[i];
        }

        // 额外混合轮次，增强雪崩效应
        for (int round = 0; round < 4; ++round) {
            seedA = (seedA << 13) | (seedA >> 19);
            seedA ^= seedB;
            seedB = (seedB << 17) | (seedB >> 15);
            seedB += seedA;
        }

        // 确保种子非零（LCG需要非零种子）
        lcgStateA = (seedA == 0) ? 0x12345678u : seedA;
        lcgStateB = (seedB == 0) ? 0x9ABCDEF0u : seedB;
    }

    /**
     * @brief 生成下一个密钥字节
     * @return 动态密钥字节
     * @details 结合两个LCG的输出和原始密钥字节生成动态密钥。
     *          每个位置的密钥都是唯一的，避免简单重复模式。
     */
    uint8_t GenerateKeyByte() {
        // LCG A: 使用经典参数 (a=1664525, c=1013904223, m=2^32)
        // 这些参数来自Numerical Recipes，具有良好的统计特性
        lcgStateA = lcgStateA * 1664525u + 1013904223u;

        // LCG B: 使用不同参数，产生独立序列
        lcgStateB = lcgStateB * 22695477u + 1u;

        // 结合两个LCG输出和原始密钥
        uint32_t combined = lcgStateA ^ (lcgStateB >> 16);

        // 如果存在原始密钥，混合密钥字节
        if (!keyBuffer.empty()) {
            size_t keyIndex = static_cast<size_t>(position % keyBuffer.size());
            combined ^= keyBuffer[keyIndex] * 0x01010101u;
        }

        // 混合位置信息，确保同一密钥在不同位置产生不同输出
        combined ^= static_cast<uint32_t>(position * 0x9E3779B9u);

        // 取最高字节作为密钥字节（经过充分混合）
        return static_cast<uint8_t>((combined >> 24) ^ (combined >> 16) ^ (combined >> 8) ^ combined);
    }

    /**
     * @brief 安全清零密钥
     * @details 使用Windows SecureZeroMemory或等效方法确保密钥被真正清零。
     *          普通memset可能被编译器优化掉，SecureZeroMemory保证执行。
     */
    void SecureClearKey() {
        if (!keyBuffer.empty()) {
#ifdef _WIN32
            SecureZeroMemory(keyBuffer.data(), keyBuffer.size());
#else
            // 非Windows平台使用volatile指针防止优化
            volatile uint8_t* p = keyBuffer.data();
            for (size_t i = 0; i < keyBuffer.size(); ++i) {
                p[i] = 0;
            }
#endif
            keyBuffer.clear();
        }
    }
};

// ---------------------------------------------------------------------------
// XorCrypto公共接口实现
// ---------------------------------------------------------------------------

XorCrypto::XorCrypto(const void* key, size_t keyLength)
    : m_impl(nullptr) {
    
    // 健壮性：密钥参数校验
    // 密钥指针不能为空
    if (key == nullptr) {
        // 创建空实现，后续Process会返回错误
        m_impl = std::make_unique<XorCryptoImpl>(nullptr, 0);
        return;
    }

    // 密钥长度至少4字节，过短的密钥安全性极差
    if (keyLength < 4) {
        m_impl = std::make_unique<XorCryptoImpl>(nullptr, 0);
        return;
    }

    // 创建实现对象
    m_impl = std::make_unique<XorCryptoImpl>(key, keyLength);
}

XorCrypto::~XorCrypto() {
    // unique_ptr自动处理Impl的析构
    // Impl析构时会调用SecureClearKey清零密钥
}

XorCrypto::XorCrypto(XorCrypto&& other) noexcept
    : m_impl(std::move(other.m_impl)) {
}

XorCrypto& XorCrypto::operator=(XorCrypto&& other) noexcept {
    if (this != &other) {
        m_impl = std::move(other.m_impl);
    }
    return *this;
}

ErrorCode XorCrypto::Process(const void* input, void* output, size_t length) {
    // 健壮性：实现对象检查
    if (!m_impl) {
        return ErrorCode::CryptoInvalidKey;
    }

    // 健壮性：参数校验
    // input和output至少有一个有效（允许原地处理时两者相同且非空）
    if (input == nullptr || output == nullptr) {
        return ErrorCode::InvalidParameter;
    }

    // 长度为0是合法的空操作
    if (length == 0) {
        return ErrorCode::Success;
    }

    // 密钥有效性检查
    if (m_impl->keyLength < 4) {
        return ErrorCode::CryptoInvalidKey;
    }

    // 获取字节指针
    const uint8_t* inBytes = static_cast<const uint8_t*>(input);
    uint8_t* outBytes = static_cast<uint8_t*>(output);

    // 核心XOR处理循环
    // 逐字节处理，每个字节使用独立的动态密钥
    for (size_t i = 0; i < length; ++i) {
        // 生成当前位置的动态密钥字节
        uint8_t keyByte = m_impl->GenerateKeyByte();

        // 执行XOR运算（加密和解密是同一操作）
        outBytes[i] = inBytes[i] ^ keyByte;

        // 更新位置计数
        ++m_impl->position;
    }

    return ErrorCode::Success;
}

ErrorCode XorCrypto::Process(ByteArray& data) {
    if (data.empty()) {
        return ErrorCode::Success;
    }

    return Process(data.data(), data.data(), data.size());
}

ErrorCode XorCrypto::Process(const ByteArray& input, ByteArray& output) {
    // 分配输出缓冲区
    output.resize(input.size());

    if (input.empty()) {
        return ErrorCode::Success;
    }

    return Process(input.data(), output.data(), input.size());
}

void XorCrypto::Reset() {
    if (m_impl) {
        m_impl->position = 0;
        // 重新初始化LCG状态，确保从头开始生成相同的密钥流
        m_impl->InitializeLCGStates();
    }
}

uint64_t XorCrypto::GetPosition() const {
    if (m_impl) {
        return m_impl->position;
    }
    return 0;
}

void XorCrypto::SetPosition(uint64_t position) {
    if (!m_impl) {
        return;
    }

    // 重置到初始状态
    m_impl->position = 0;
    m_impl->InitializeLCGStates();

    // 快进密钥流到指定位置
    // 对于大位置，直接丢弃中间值比存储整个密钥流更高效
    for (uint64_t i = 0; i < position; ++i) {
        m_impl->GenerateKeyByte();
        ++m_impl->position;
    }
}

} // namespace Common
} // namespace GameLauncher
