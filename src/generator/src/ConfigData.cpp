/**
 * @file ConfigData.cpp
 * @brief 配置数据结构实现
 */

#include "ConfigData.h"
#include <cstring>
#include <fstream>
#include <algorithm>
#include <codecvt>
#include <locale>


// ============================================================================
// 二进制结构构造函数
// ============================================================================

ImageFileInfo::ImageFileInfo() noexcept
    : type(0), format(0), dataOffset(0), dataSize(0)
    , width(0), height(0), reserved{0} {}

NetworkConfig::NetworkConfig() noexcept
    : primaryServerlistOffset(0), primaryServerlistLength(0)
    , backupServerlistOffset(0), backupServerlistLength(0)
    , patchBaseUrlOffset(0), patchBaseUrlLength(0)
    , timeoutSeconds(30), retryCount(3), reserved{0} {}

BusinessUrls::BusinessUrls() noexcept
    : officialSiteOffset(0), officialSiteLength(0)
    , rechargeUrlOffset(0), rechargeUrlLength(0)
    , customerServiceOffset(0), customerServiceLength(0)
    , registerUrlOffset(0), registerUrlLength(0)
    , reserved{0} {}

UpdatePolicy::UpdatePolicy() noexcept
    : strategy(0), forceCrcCheck(1)
    , autoCreateSubdirs(1), conflictPolicy(0)
    , reserved{0} {}

ConfigHeader::ConfigHeader() noexcept
    : magic(GeneratorConstants::CONFIG_SECTION_MAGIC)
    , version(GeneratorConstants::CONFIG_VERSION)
    , totalSize(0), imageCount(0), imageTableOffset(0)
    , networkConfigOffset(0), businessUrlsOffset(0)
    , updatePolicyOffset(0), clientPathOffset(0)
    , clientPathLength(0), extensionDllOffset(0)
    , extensionDllLength(0), uiStylePreset(0)
    , encryptionKeyOffset(0), encryptionKeyLength(0)
    , reserved{0} {}

PatchPackageHeader::PatchPackageHeader() noexcept
    : magic(0x50544348), version(0x00010000)
    , fileCount(0), fileTableOffset(0)
    , dataOffset(0), flags(0), reserved{0} {}

// ============================================================================
// LauncherConfig 实现
// ============================================================================

std::vector<uint8_t> LauncherConfig::Serialize() const {
    // 计算需要的总大小
    size_t totalSize = sizeof(ConfigHeader);
    totalSize += sizeof(ImageFileInfo) * static_cast<size_t>(ImageType::Count);
    totalSize += sizeof(NetworkConfig);
    totalSize += sizeof(BusinessUrls);
    totalSize += sizeof(UpdatePolicy);

    // 字符串数据区（所有字符串转换为UTF-8后存储）
    std::vector<uint8_t> stringData;
    auto appendString = [&stringData](const std::wstring& str) -> std::pair<uint32_t, uint32_t> {
        uint32_t offset = static_cast<uint32_t>(stringData.size());
        // 转换为UTF-8
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
        std::string utf8 = converter.to_bytes(str);
        uint32_t length = static_cast<uint32_t>(utf8.length());
        // 包含null终止符
        stringData.insert(stringData.end(), utf8.begin(), utf8.end());
        stringData.push_back(0);
        return { offset, length };
    };

    // 预计算所有字符串偏移
    auto [primaryServerOffset, primaryServerLen] = appendString(primaryServerlistUrl);
    auto [backupServerOffset, backupServerLen] = appendString(backupServerlistUrl);
    auto [patchBaseOffset, patchBaseLen] = appendString(patchBaseUrl);
    auto [officialOffset, officialLen] = appendString(officialSiteUrl);
    auto [rechargeOffset, rechargeLen] = appendString(rechargeUrl);
    auto [customerOffset, customerLen] = appendString(customerServiceUrl);
    auto [registerOffset, registerLen] = appendString(registerUrl);
    auto [clientPathOffset, clientPathLen] = appendString(clientPath);
    auto [extDllOffset, extDllLen] = appendString(extensionDllPath);
    auto [encKeyOffset, encKeyLen] = appendString(encryptionKey);

    // 图片数据区
    std::vector<uint8_t> imageData;
    std::array<ImageFileInfo, static_cast<size_t>(ImageType::Count)> imageInfos;
    for (size_t i = 0; i < static_cast<size_t>(ImageType::Count); ++i) {
        imageInfos[i].type = static_cast<uint32_t>(i);
        if (images[i].loaded && !images[i].data.empty()) {
            imageInfos[i].dataOffset = static_cast<uint32_t>(imageData.size());
            imageInfos[i].dataSize = static_cast<uint32_t>(images[i].data.size());
            imageInfos[i].width = images[i].width;
            imageInfos[i].height = images[i].height;
            // 根据文件头判断格式
            if (images[i].data.size() >= 2 && images[i].data[0] == 0x42 && images[i].data[1] == 0x4D) {
                imageInfos[i].format = 0; // BMP
            } else if (images[i].data.size() >= 8 &&
                       images[i].data[0] == 0x89 && images[i].data[1] == 0x50 &&
                       images[i].data[2] == 0x4E && images[i].data[3] == 0x47) {
                imageInfos[i].format = 1; // PNG
            } else {
                imageInfos[i].format = 0xFFFFFFFF; // 未知
            }
            imageData.insert(imageData.end(), images[i].data.begin(), images[i].data.end());
        } else {
            imageInfos[i].dataOffset = 0;
            imageInfos[i].dataSize = 0;
            imageInfos[i].format = 0xFFFFFFFF;
        }
    }

    // 构建最终缓冲区
    std::vector<uint8_t> result;
    result.reserve(totalSize + stringData.size() + imageData.size());

    // 1. 写入配置头
    ConfigHeader header;
    header.totalSize = static_cast<uint32_t>(totalSize + stringData.size() + imageData.size());
    header.imageCount = static_cast<uint32_t>(ImageType::Count);
    header.imageTableOffset = static_cast<uint32_t>(sizeof(ConfigHeader));
    header.networkConfigOffset = static_cast<uint32_t>(sizeof(ConfigHeader) + sizeof(ImageFileInfo) * static_cast<size_t>(ImageType::Count));
    header.businessUrlsOffset = header.networkConfigOffset + static_cast<uint32_t>(sizeof(NetworkConfig));
    header.updatePolicyOffset = header.businessUrlsOffset + static_cast<uint32_t>(sizeof(BusinessUrls));
    header.clientPathOffset = clientPathOffset;
    header.clientPathLength = clientPathLen;
    header.extensionDllOffset = extDllOffset;
    header.extensionDllLength = extDllLen;
    header.uiStylePreset = static_cast<uint32_t>(uiStyle);
    header.encryptionKeyOffset = encKeyOffset;
    header.encryptionKeyLength = encKeyLen;

    result.insert(result.end(), reinterpret_cast<uint8_t*>(&header), reinterpret_cast<uint8_t*>(&header) + sizeof(header));

    // 2. 写入图片信息表
    for (const auto& info : imageInfos) {
        result.insert(result.end(), reinterpret_cast<const uint8_t*>(&info), reinterpret_cast<const uint8_t*>(&info) + sizeof(info));
    }

    // 3. 写入网络配置
    NetworkConfig netConfig;
    netConfig.primaryServerlistOffset = primaryServerOffset;
    netConfig.primaryServerlistLength = primaryServerLen;
    netConfig.backupServerlistOffset = backupServerOffset;
    netConfig.backupServerlistLength = backupServerLen;
    netConfig.patchBaseUrlOffset = patchBaseOffset;
    netConfig.patchBaseUrlLength = patchBaseLen;
    netConfig.timeoutSeconds = timeoutSeconds;
    netConfig.retryCount = retryCount;
    result.insert(result.end(), reinterpret_cast<uint8_t*>(&netConfig), reinterpret_cast<uint8_t*>(&netConfig) + sizeof(netConfig));

    // 4. 写入业务URL
    BusinessUrls bizUrls;
    bizUrls.officialSiteOffset = officialOffset;
    bizUrls.officialSiteLength = officialLen;
    bizUrls.rechargeUrlOffset = rechargeOffset;
    bizUrls.rechargeUrlLength = rechargeLen;
    bizUrls.customerServiceOffset = customerOffset;
    bizUrls.customerServiceLength = customerLen;
    bizUrls.registerUrlOffset = registerOffset;
    bizUrls.registerUrlLength = registerLen;
    result.insert(result.end(), reinterpret_cast<uint8_t*>(&bizUrls), reinterpret_cast<uint8_t*>(&bizUrls) + sizeof(bizUrls));

    // 5. 写入更新策略
    UpdatePolicy policy;
    policy.strategy = static_cast<uint32_t>(updateStrategy);
    policy.forceCrcCheck = forceCrcCheck ? 1u : 0u;
    policy.autoCreateSubdirs = autoCreateSubdirs ? 1u : 0u;
    policy.conflictPolicy = static_cast<uint32_t>(conflictPolicy);
    result.insert(result.end(), reinterpret_cast<uint8_t*>(&policy), reinterpret_cast<uint8_t*>(&policy) + sizeof(policy));

    // 6. 写入字符串数据区
    result.insert(result.end(), stringData.begin(), stringData.end());

    // 7. 写入图片数据区
    result.insert(result.end(), imageData.begin(), imageData.end());

    return result;
}

bool LauncherConfig::Deserialize(const uint8_t* data, size_t size) {
    if (!data || size < sizeof(ConfigHeader)) {
        return false;
    }

    const ConfigHeader* header = reinterpret_cast<const ConfigHeader*>(data);
    if (header->magic != GeneratorConstants::CONFIG_SECTION_MAGIC) {
        return false;
    }
    if (header->version > GeneratorConstants::CONFIG_VERSION) {
        return false;
    }
    if (header->totalSize > size) {
        return false;
    }

    // 反序列化图片信息
    if (header->imageTableOffset + sizeof(ImageFileInfo) * header->imageCount <= size) {
        const ImageFileInfo* imgTable = reinterpret_cast<const ImageFileInfo*>(data + header->imageTableOffset);
        for (uint32_t i = 0; i < header->imageCount && i < static_cast<uint32_t>(ImageType::Count); ++i) {
            size_t idx = static_cast<size_t>(imgTable[i].type);
            if (idx < static_cast<size_t>(ImageType::Count)) {
                images[idx].width = imgTable[i].width;
                images[idx].height = imgTable[i].height;
                if (imgTable[i].dataOffset > 0 && imgTable[i].dataSize > 0 &&
                    imgTable[i].dataOffset + imgTable[i].dataSize <= size) {
                    images[idx].data.assign(data + imgTable[i].dataOffset, data + imgTable[i].dataOffset + imgTable[i].dataSize);
                    images[idx].loaded = true;
                }
            }
        }
    }

    // 反序列化网络配置
    if (header->networkConfigOffset + sizeof(NetworkConfig) <= size) {
        const NetworkConfig* net = reinterpret_cast<const NetworkConfig*>(data + header->networkConfigOffset);
        primaryServerlistUrl = DeserializeString(data, net->primaryServerlistOffset, net->primaryServerlistLength);
        backupServerlistUrl = DeserializeString(data, net->backupServerlistOffset, net->backupServerlistLength);
        patchBaseUrl = DeserializeString(data, net->patchBaseUrlOffset, net->patchBaseUrlLength);
        timeoutSeconds = net->timeoutSeconds;
        retryCount = net->retryCount;
    }

    // 反序列化业务URL
    if (header->businessUrlsOffset + sizeof(BusinessUrls) <= size) {
        const BusinessUrls* biz = reinterpret_cast<const BusinessUrls*>(data + header->businessUrlsOffset);
        officialSiteUrl = DeserializeString(data, biz->officialSiteOffset, biz->officialSiteLength);
        rechargeUrl = DeserializeString(data, biz->rechargeUrlOffset, biz->rechargeUrlLength);
        customerServiceUrl = DeserializeString(data, biz->customerServiceOffset, biz->customerServiceLength);
        registerUrl = DeserializeString(data, biz->registerUrlOffset, biz->registerUrlLength);
    }

    // 反序列化更新策略
    if (header->updatePolicyOffset + sizeof(UpdatePolicy) <= size) {
        const UpdatePolicy* policy = reinterpret_cast<const UpdatePolicy*>(data + header->updatePolicyOffset);
        updateStrategy = static_cast<UpdateStrategy>(policy->strategy);
        forceCrcCheck = policy->forceCrcCheck != 0;
        autoCreateSubdirs = policy->autoCreateSubdirs != 0;
        conflictPolicy = static_cast<ConflictPolicy>(policy->conflictPolicy);
    }

    // 其他字段
    uiStyle = static_cast<UIStylePreset>(header->uiStylePreset);
    clientPath = DeserializeString(data, header->clientPathOffset, header->clientPathLength);
    extensionDllPath = DeserializeString(data, header->extensionDllOffset, header->extensionDllLength);
    encryptionKey = DeserializeString(data, header->encryptionKeyOffset, header->encryptionKeyLength);

    return true;
}

std::wstring LauncherConfig::DeserializeString(const uint8_t* data, uint32_t offset, uint32_t length) const {
    if (!data || length == 0 || length > GeneratorConstants::MAX_STRING_LENGTH) {
        return L"";
    }
    try {
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
        return converter.from_bytes(reinterpret_cast<const char*>(data + offset), reinterpret_cast<const char*>(data + offset + length));
    } catch (...) {
        return L"";
    }
}

void LauncherConfig::SerializeString(std::vector<uint8_t>& buffer, const std::wstring& str,
                                     uint32_t& outOffset, uint32_t& outLength) const {
    outOffset = static_cast<uint32_t>(buffer.size());
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
    std::string utf8 = converter.to_bytes(str);
    outLength = static_cast<uint32_t>(utf8.length());
    buffer.insert(buffer.end(), utf8.begin(), utf8.end());
    buffer.push_back(0);
}

bool LauncherConfig::Validate(std::wstring& errorMessage) const {
    // 验证客户端路径
    if (clientPath.empty()) {
        errorMessage = L"客户端路径不能为空";
        return false;
    }
    if (clientPath.length() > GeneratorConstants::MAX_PATH_LENGTH) {
        errorMessage = L"客户端路径过长";
        return false;
    }

    // 验证网络URL（至少主服务器列表不能为空）
    if (primaryServerlistUrl.empty()) {
        errorMessage = L"主服务器列表URL不能为空";
        return false;
    }

    // 验证超时时间
    if (timeoutSeconds == 0 || timeoutSeconds > 300) {
        errorMessage = L"超时时间必须在1-300秒之间";
        return false;
    }

    // 验证加密密钥长度
    if (!encryptionKey.empty() && encryptionKey.length() > GeneratorConstants::MAX_KEY_LENGTH) {
        errorMessage = L"加密密钥过长";
        return false;
    }

    return true;
}

void LauncherConfig::ClearImages() {
    for (auto& img : images) {
        img.data.clear();
        img.width = 0;
        img.height = 0;
        img.loaded = false;
    }
}

bool LauncherConfig::LoadImage(ImageType type, const std::wstring& path, std::wstring& errorMessage) {
    size_t idx = static_cast<size_t>(type);
    if (idx >= static_cast<size_t>(ImageType::Count)) {
        errorMessage = L"无效的图片类型";
        return false;
    }

    // 打开文件
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        errorMessage = L"无法打开图片文件: " + path;
        return false;
    }

    auto fileSize = file.tellg();
    if (fileSize <= 0 || static_cast<size_t>(fileSize) > 50 * 1024 * 1024) { // 最大50MB
        errorMessage = L"图片文件大小无效或超过50MB限制";
        return false;
    }

    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer(static_cast<size_t>(fileSize));
    if (!file.read(reinterpret_cast<char*>(buffer.data()), fileSize)) {
        errorMessage = L"读取图片文件失败";
        return false;
    }
    file.close();

    // 魔数校验
    bool isBmp = buffer.size() >= 2 && buffer[0] == 0x42 && buffer[1] == 0x4D;
    bool isPng = buffer.size() >= 8 &&
                 buffer[0] == 0x89 && buffer[1] == 0x50 &&
                 buffer[2] == 0x4E && buffer[3] == 0x47 &&
                 buffer[4] == 0x0D && buffer[5] == 0x0A &&
                 buffer[6] == 0x1A && buffer[7] == 0x0A;

    if (!isBmp && !isPng) {
        errorMessage = L"图片文件格式不支持（仅支持BMP/PNG）";
        return false;
    }

    // 解析图片尺寸
    uint32_t width = 0, height = 0;
    if (isBmp && buffer.size() >= 26) {
        // BMP: 宽度在偏移18(4字节)，高度在偏移22(4字节)
        width = *reinterpret_cast<uint32_t*>(buffer.data() + 18);
        height = *reinterpret_cast<uint32_t*>(buffer.data() + 22);
    } else if (isPng && buffer.size() >= 24) {
        // PNG: IHDR块在偏移16开始，宽度在偏移16(4字节大端)，高度在偏移20
        width = (buffer[16] << 24) | (buffer[17] << 16) | (buffer[18] << 8) | buffer[19];
        height = (buffer[20] << 24) | (buffer[21] << 16) | (buffer[22] << 8) | buffer[23];
    }

    images[idx].data = std::move(buffer);
    images[idx].width = width;
    images[idx].height = height;
    images[idx].loaded = true;

    return true;
}
