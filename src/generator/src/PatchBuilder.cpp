/**
 * @file PatchBuilder.cpp
 * @brief 游戏启动器生成器 - 补丁打包实现
 * @details 实现补丁包的创建、压缩、加密和验证功能
 */

#include "PatchBuilder.h"
#include <fstream>
#include <iostream>
#include <filesystem>
#include <algorithm>
#include <cstring>
#include <zlib.h>

namespace fs = std::filesystem;

// CRC32查找表（标准IEEE CRC32）
static const uint32_t CRC32_TABLE[256] = {
    0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
    0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
    0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
    0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
    0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
    0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
    0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
    0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
    0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
    0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
    0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190, 0x01db7106,
    0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
    0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
    0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
    0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
    0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
    0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
    0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
    0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
    0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
    0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
    0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
    0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
    0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
    0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
    0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
    0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
    0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
    0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
    0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
    0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
    0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
    0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
    0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
    0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
    0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
    0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
    0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
    0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
    0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
    0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
    0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
    0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};

// ============================================================================
// 基础方法
// ============================================================================

void CPatchBuilder::SetProgressCallback(PackProgressCallback callback) {
    m_progressCallback = std::move(callback);
}

void CPatchBuilder::ReportProgress(const PackProgressInfo& info) {
    if (m_progressCallback) {
        m_progressCallback(info);
    }
}

void CPatchBuilder::SetError(const std::wstring& message) {
    m_lastError = message;
}

std::wstring CPatchBuilder::ResultToString(PackResult result) {
    switch (result) {
        case PackResult::Success:           return L"成功";
        case PackResult::InvalidInput:      return L"输入参数无效";
        case PackResult::SourceDirNotFound: return L"源目录不存在";
        case PackResult::OutputCreateError: return L"创建输出文件失败";
        case PackResult::FileReadError:     return L"读取文件失败";
        case PackResult::CompressionError:  return L"压缩失败";
        case PackResult::EncryptionError:   return L"加密失败";
        case PackResult::ChecksumError:     return L"校验和计算失败";
        case PackResult::Cancelled:         return L"用户取消";
        case PackResult::UnknownError:      return L"未知错误";
        default:                            return L"未定义错误";
    }
}

uint32_t CPatchBuilder::CalculateCRC32(const std::vector<uint8_t>& data) {
    uint32_t crc = 0xFFFFFFFF;
    for (uint8_t byte : data) {
        crc = CRC32_TABLE[(crc ^ byte) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFF;
}

// ============================================================================
// 主打包逻辑
// ============================================================================

PackResult CPatchBuilder::BuildPatch(
    const std::wstring& sourceDir,
    const std::wstring& outputPath
) {
    m_cancelled = false;

    PackProgressInfo progress;
    progress.overallPercentage = 0;
    progress.currentFileIndex = 0;
    progress.totalFiles = 0;
    progress.statusMessage = L"开始扫描源目录...";
    ReportProgress(progress);

    // 1. 验证输入
    if (sourceDir.empty() || outputPath.empty()) {
        SetError(L"源目录或输出路径为空");
        return PackResult::InvalidInput;
    }

    // 2. 检查源目录
    if (!fs::exists(sourceDir) || !fs::is_directory(sourceDir)) {
        SetError(L"源目录不存在: " + sourceDir);
        return PackResult::SourceDirNotFound;
    }

    // 3. 扫描目录获取所有文件
    std::vector<std::wstring> files;
    std::wstring basePath;
    if (!ScanDirectory(sourceDir, files, basePath)) {
        SetError(L"扫描源目录失败");
        return PackResult::UnknownError;
    }

    if (files.empty()) {
        SetError(L"源目录为空");
        return PackResult::InvalidInput;
    }

    progress.totalFiles = static_cast<int>(files.size());
    progress.statusMessage = L"找到 " + std::to_wstring(files.size()) + L" 个文件";
    ReportProgress(progress);

    // 4. 处理每个文件
    std::vector<PatchFileEntry> entries;
    std::vector<std::vector<uint8_t>> fileDatas;
    entries.reserve(files.size());
    fileDatas.reserve(files.size());

    for (size_t i = 0; i < files.size(); ++i) {
        if (m_cancelled) {
            SetError(L"操作已取消");
            return PackResult::Cancelled;
        }

        const auto& filePath = files[i];
        progress.currentFileIndex = static_cast<int>(i);
        progress.currentFileName = fs::path(filePath).filename().wstring();
        progress.statusMessage = L"处理: " + progress.currentFileName;
        progress.overallPercentage = static_cast<int>((i * 60) / files.size());
        ReportProgress(progress);

        // 读取文件
        std::vector<uint8_t> fileData;
        if (!ReadFileToMemory(filePath, fileData)) {
            SetError(L"读取文件失败: " + filePath);
            return PackResult::FileReadError;
        }

        // 计算CRC32
        uint32_t crc = CalculateCRC32(fileData);

        // 压缩（如果启用）
        std::vector<uint8_t> processedData;
        bool isCompressed = false;
        if (m_compressionEnabled) {
            if (!CompressData(fileData, processedData)) {
                SetError(L"压缩文件失败: " + filePath);
                return PackResult::CompressionError;
            }
            // 只有当压缩后更小才使用压缩数据
            if (processedData.size() < fileData.size()) {
                isCompressed = true;
            } else {
                processedData = fileData;
                isCompressed = false;
            }
        } else {
            processedData = fileData;
        }

        // 加密（如果提供了密钥）
        if (!m_encryptionKey.empty()) {
            EncryptData(processedData, m_encryptionKey);
        }

        // 创建条目
        PatchFileEntry entry;
        entry.relativePath = GetRelativePath(basePath, filePath);
        entry.originalSize = fileData.size();
        entry.compressedSize = processedData.size();
        entry.crc32 = crc;
        entry.compressed = isCompressed;
        // offset将在写入时计算

        entries.push_back(entry);
        fileDatas.push_back(std::move(processedData));
    }

    progress.overallPercentage = 60;
    progress.statusMessage = L"写入补丁包文件...";
    ReportProgress(progress);

    // 5. 构建并写入补丁包
    PatchPackageHeader header;
    header.fileCount = static_cast<uint32_t>(entries.size());
    header.flags = m_compressionEnabled ? 1 : 0;

    if (!WritePatchPackage(outputPath, entries, fileDatas, header)) {
        return PackResult::OutputCreateError;
    }

    progress.overallPercentage = 100;
    progress.statusMessage = L"补丁包生成完成!";
    ReportProgress(progress);

    return PackResult::Success;
}

// ============================================================================
// 文件扫描
// ============================================================================

bool CPatchBuilder::ScanDirectory(
    const std::wstring& dir,
    std::vector<std::wstring>& outFiles,
    std::wstring& outBasePath
) {
    try {
        outBasePath = fs::absolute(dir).wstring();
        if (!outBasePath.empty() && outBasePath.back() != L'\\' && outBasePath.back() != L'/') {
            outBasePath += L'\\';
        }

        for (const auto& entry : fs::recursive_directory_iterator(dir, fs::directory_options::skip_permission_denied)) {
            if (m_cancelled) {
                return false;
            }
            if (entry.is_regular_file()) {
                outFiles.push_back(entry.path().wstring());
            }
        }

        // 按路径排序，确保确定性输出
        std::sort(outFiles.begin(), outFiles.end());
        return true;
    } catch (const fs::filesystem_error& e) {
        SetError(std::wstring(L"文件系统错误: ") +
                 std::wstring(e.what(), e.what() + std::strlen(e.what())));
        return false;
    }
}

// ============================================================================
// 文件读写
// ============================================================================

bool CPatchBuilder::ReadFileToMemory(
    const std::wstring& path,
    std::vector<uint8_t>& outData
) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return false;
    }

    auto fileSize = file.tellg();
    if (fileSize < 0) {
        return false;
    }

    // 限制单个文件大小为1GB
    if (fileSize > 1024LL * 1024 * 1024) {
        SetError(L"文件过大: " + path);
        return false;
    }

    file.seekg(0, std::ios::beg);
    outData.resize(static_cast<size_t>(fileSize));

    if (!file.read(reinterpret_cast<char*>(outData.data()), fileSize)) {
        return false;
    }

    return true;
}

// ============================================================================
// 压缩
// ============================================================================

bool CPatchBuilder::CompressData(
    const std::vector<uint8_t>& input,
    std::vector<uint8_t>& output
) {
    if (input.empty()) {
        output.clear();
        return true;
    }

    // 计算最大输出大小
    uLong maxOutputSize = compressBound(static_cast<uLong>(input.size()));
    output.resize(maxOutputSize);

    uLong actualSize = maxOutputSize;
    int result = compress2(output.data(), &actualSize, input.data(), static_cast<uLong>(input.size()), m_compressionLevel);

    if (result != Z_OK) {
        return false;
    }

    output.resize(actualSize);
    return true;
}

// ============================================================================
// 加密
// ============================================================================

void CPatchBuilder::EncryptData(
    std::vector<uint8_t>& data,
    const std::wstring& key
) {
    if (data.empty() || key.empty()) {
        return;
    }

    // 将密钥转换为UTF-8字节序列
    std::string keyBytes;
    for (wchar_t wc : key) {
        keyBytes.push_back(static_cast<char>(wc & 0xFF));
        keyBytes.push_back(static_cast<char>((wc >> 8) & 0xFF));
    }

    if (keyBytes.empty()) {
        return;
    }

    // 生成密钥流（简单的密钥派生）
    std::vector<uint8_t> keyStream(data.size());
    size_t keyLen = keyBytes.size();

    for (size_t i = 0; i < data.size(); ++i) {
        // 使用密钥字节和位置混合生成密钥流
        uint8_t k = static_cast<uint8_t>(keyBytes[i % keyLen]);
        uint8_t pos = static_cast<uint8_t>(i & 0xFF);
        keyStream[i] = (k ^ pos ^ (k >> (i % 7 + 1))) ^ 0xA5;
    }

    // XOR加密
    for (size_t i = 0; i < data.size(); ++i) {
        data[i] ^= keyStream[i];
    }
}

// ============================================================================
// 路径处理
// ============================================================================

std::wstring CPatchBuilder::GetRelativePath(
    const std::wstring& basePath,
    const std::wstring& fullPath
) {
    if (basePath.empty()) {
        return fullPath;
    }

    // 简单的相对路径计算
    if (fullPath.find(basePath) == 0) {
        return fullPath.substr(basePath.length());
    }
    return fullPath;
}

// ============================================================================
// 写入补丁包
// ============================================================================

bool CPatchBuilder::WritePatchPackage(
    const std::wstring& outputPath,
    const std::vector<PatchFileEntry>& entries,
    const std::vector<std::vector<uint8_t>>& fileDatas,
    const PatchPackageHeader& header
) {
    // 删除已存在的输出文件
    DeleteFileW(outputPath.c_str());

    std::ofstream file(outputPath, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        SetError(L"无法创建输出文件: " + outputPath);
        return false;
    }

    // 计算偏移
    uint32_t headerSize = sizeof(PatchPackageHeader);
    uint32_t fileTableSize = static_cast<uint32_t>(entries.size()) * (
        sizeof(uint32_t) +    // relativePathLength
        sizeof(uint32_t) +    // originalSize
        sizeof(uint32_t) +    // compressedSize
        sizeof(uint32_t) +    // crc32
        sizeof(uint64_t) +    // offset
        sizeof(uint32_t)      // flags
    );

    // 为路径字符串预留空间（估算）
    uint32_t pathDataSize = 0;
    for (const auto& entry : entries) {
        pathDataSize += static_cast<uint32_t>(entry.relativePath.length() * sizeof(wchar_t)) + sizeof(uint32_t);
    }

    uint32_t dataOffset = headerSize + fileTableSize + pathDataSize;
    uint32_t currentOffset = dataOffset;

    // 1. 写入头
    PatchPackageHeader writeHeader = header;
    writeHeader.fileTableOffset = headerSize;
    writeHeader.dataOffset = dataOffset;

    file.write(reinterpret_cast<const char*>(&writeHeader), sizeof(writeHeader));

    // 2. 计算并写入文件表和数据
    std::vector<PatchFileEntry> writeEntries = entries;
    for (size_t i = 0; i < writeEntries.size(); ++i) {
        writeEntries[i].offset = currentOffset;

        // 写入文件表项
        uint32_t pathLen = static_cast<uint32_t>(writeEntries[i].relativePath.length());
        uint32_t originalSize = static_cast<uint32_t>(writeEntries[i].originalSize);
        uint32_t compressedSize = static_cast<uint32_t>(writeEntries[i].compressedSize);
        uint32_t crc = writeEntries[i].crc32;
        uint64_t offset = writeEntries[i].offset;
        uint32_t flags = writeEntries[i].compressed ? 1 : 0;

        file.write(reinterpret_cast<const char*>(&pathLen), sizeof(pathLen));
        file.write(reinterpret_cast<const char*>(writeEntries[i].relativePath.data()), pathLen * sizeof(wchar_t));
        file.write(reinterpret_cast<const char*>(&originalSize), sizeof(originalSize));
        file.write(reinterpret_cast<const char*>(&compressedSize), sizeof(compressedSize));
        file.write(reinterpret_cast<const char*>(&crc), sizeof(crc));
        file.write(reinterpret_cast<const char*>(&offset), sizeof(offset));
        file.write(reinterpret_cast<const char*>(&flags), sizeof(flags));

        currentOffset += static_cast<uint32_t>(fileDatas[i].size());
    }

    // 3. 写入文件数据
    for (size_t i = 0; i < fileDatas.size(); ++i) {
        if (m_cancelled) {
            file.close();
            DeleteFileW(outputPath.c_str());
            return false;
        }
        file.write(reinterpret_cast<const char*>(fileDatas[i].data()), fileDatas[i].size());
    }

    file.flush();
    file.close();

    return true;
}

// ============================================================================
// 验证补丁包
// ============================================================================

bool CPatchBuilder::VerifyPatch(const std::wstring& patchPath) {
    std::ifstream file(patchPath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        SetError(L"无法打开补丁包");
        return false;
    }

    auto fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    if (fileSize < static_cast<std::streamoff>(sizeof(PatchPackageHeader))) {
        SetError(L"补丁包文件太小");
        return false;
    }

    // 读取头
    PatchPackageHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(header));

    if (header.magic != 0x50544348) {
        SetError(L"补丁包魔数无效");
        return false;
    }

    // 基本验证通过
    file.close();
    return true;
}
