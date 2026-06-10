/**
 * @file ResourceEmbedder.cpp
 * @brief 游戏启动器生成器 - 资源嵌入核心实现
 * @details 这是生成器的核心模块，实现了三种配置嵌入策略：
 *          1. UpdateResource API - 直接修改PE资源节
 *          2. 自定义PE节 - 创建新的代码/数据节
 *          3. 覆盖区追加 - 追加到PE文件末尾
 *
 * 生产级实现，包含完整的错误处理、进度报告和PE结构验证。
 */

#include "ResourceEmbedder.h"
#include <fstream>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cstring>

// PE结构定义（避免依赖Windows头文件的特定版本）
#ifndef IMAGE_SIZEOF_SHORT_NAME
#define IMAGE_SIZEOF_SHORT_NAME 8
#endif

// 自定义资源类型ID
#define RT_LAUNCHER_CONFIG  MAKEINTRESOURCE(256)
#define ID_LAUNCHER_CONFIG  1

// ============================================================================
// 构造函数和基础方法
// ============================================================================

void CResourceEmbedder::SetProgressCallback(ProgressCallback callback) {
    m_progressCallback = std::move(callback);
}

void CResourceEmbedder::ReportProgress(int percentage, const std::wstring& status) {
    if (m_progressCallback) {
        // 限制百分比范围
        percentage = std::max(0, std::min(100, percentage));
        m_progressCallback(percentage, status);
    }
}

void CResourceEmbedder::SetError(const std::wstring& message) {
    m_lastError = message;
}

std::wstring CResourceEmbedder::ResultToString(EmbedResult result) {
    switch (result) {
        case EmbedResult::Success:           return L"成功";
        case EmbedResult::InvalidInput:      return L"输入参数无效";
        case EmbedResult::TemplateNotFound:  return L"模板文件不存在";
        case EmbedResult::TemplateReadError: return L"读取模板文件失败";
        case EmbedResult::OutputCreateError: return L"创建输出文件失败";
        case EmbedResult::ResourceUpdateError: return L"更新资源失败";
        case EmbedResult::SectionCreateError: return L"创建PE节失败";
        case EmbedResult::InsufficientSpace: return L"空间不足";
        case EmbedResult::PermissionDenied:  return L"权限不足";
        case EmbedResult::UnknownError:      return L"未知错误";
        default:                             return L"未定义错误";
    }
}

std::wstring CResourceEmbedder::GetSystemErrorMessage(DWORD errorCode) {
    LPWSTR buffer = nullptr;
    DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
    DWORD size = FormatMessageW(flags, nullptr, errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                                 reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);
    if (size > 0 && buffer) {
        std::wstring message(buffer, size);
        LocalFree(buffer);
        // 移除尾部换行符
        while (!message.empty() && (message.back() == L'\n' || message.back() == L'\r')) {
            message.pop_back();
        }
        return message;
    }
    if (buffer) LocalFree(buffer);
    return L"未知系统错误 (代码: " + std::to_wstring(errorCode) + L")";
}

// ============================================================================
// 主嵌入逻辑
// ============================================================================

EmbedResult CResourceEmbedder::EmbedResources(
    const std::wstring& templatePath,
    const LauncherConfig& config,
    const std::wstring& outputPath
) {
    ReportProgress(0, L"开始生成启动器...");

    // 1. 验证输入参数
    if (templatePath.empty() || outputPath.empty()) {
        SetError(L"模板路径或输出路径为空");
        return EmbedResult::InvalidInput;
    }

    // 2. 检查模板文件是否存在
    DWORD fileAttr = GetFileAttributesW(templatePath.c_str());
    if (fileAttr == INVALID_FILE_ATTRIBUTES || (fileAttr & FILE_ATTRIBUTE_DIRECTORY)) {
        SetError(L"模板文件不存在: " + templatePath);
        return EmbedResult::TemplateNotFound;
    }

    ReportProgress(5, L"验证模板文件...");

    // 3. 验证模板是有效的PE文件
    if (!ValidateOutputPE(templatePath)) {
        SetError(L"模板文件不是有效的PE文件");
        return EmbedResult::TemplateReadError;
    }

    ReportProgress(10, L"复制模板文件...");

    // 4. 复制模板到输出路径
    if (!CopyTemplateFile(templatePath, outputPath)) {
        return EmbedResult::OutputCreateError;
    }

    ReportProgress(20, L"序列化配置数据...");

    // 5. 验证配置数据
    std::wstring validateError;
    if (!config.Validate(validateError)) {
        SetError(L"配置验证失败: " + validateError);
        // 删除已创建的输出文件
        DeleteFileW(outputPath.c_str());
        return EmbedResult::InvalidInput;
    }

    ReportProgress(30, L"嵌入配置数据...");

    // 6. 根据策略执行嵌入
    bool embedSuccess = false;
    switch (m_strategy) {
        case EmbedStrategy::UpdateResource:
            embedSuccess = EmbedUsingUpdateResource(outputPath, config);
            break;
        case EmbedStrategy::CustomSection:
            embedSuccess = EmbedUsingCustomSection(outputPath, config);
            break;
        case EmbedStrategy::AppendToOverlay:
            embedSuccess = EmbedUsingOverlay(outputPath, config);
            break;
        default:
            SetError(L"未知的嵌入策略");
            break;
    }

    if (!embedSuccess) {
        // 清理失败的输出文件
        DeleteFileW(outputPath.c_str());
        return EmbedResult::ResourceUpdateError;
    }

    ReportProgress(90, L"验证输出文件...");

    // 7. 验证输出文件
    if (!ValidateOutputPE(outputPath)) {
        SetError(L"输出文件PE结构验证失败");
        DeleteFileW(outputPath.c_str());
        return EmbedResult::UnknownError;
    }

    ReportProgress(100, L"生成完成!");
    return EmbedResult::Success;
}

// ============================================================================
// UpdateResource 策略实现
// ============================================================================

bool CResourceEmbedder::EmbedUsingUpdateResource(
    const std::wstring& outputPath,
    const LauncherConfig& config
) {
    ReportProgress(35, L"使用UpdateResource API嵌入数据...");

    // 1. 序列化配置数据
    std::vector<uint8_t> configData;
    if (!SerializeConfigToResource(config, configData)) {
        SetError(L"序列化配置数据失败");
        return false;
    }

    ReportProgress(40, L"打开资源更新句柄...");

    // 2. 打开文件进行资源更新
    // 注意：UpdateResource需要文件未被其他进程占用
    HANDLE hUpdate = BeginUpdateResourceW(outputPath.c_str(), FALSE);
    if (!hUpdate) {
        DWORD error = GetLastError();
        SetError(L"BeginUpdateResource失败: " + GetSystemErrorMessage(error));
        return false;
    }

    ReportProgress(50, L"写入配置资源...");

    // 3. 写入配置数据作为自定义资源
    // 使用自定义资源类型 256，资源ID 1
    if (!UpdateResourceW(hUpdate, RT_LAUNCHER_CONFIG, MAKEINTRESOURCEW(ID_LAUNCHER_CONFIG),
                         MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                         configData.data(), static_cast<DWORD>(configData.size()))) {
        DWORD error = GetLastError();
        SetError(L"UpdateResource(配置数据)失败: " + GetSystemErrorMessage(error));
        EndUpdateResource(hUpdate, TRUE); // 放弃更改
        return false;
    }

    ReportProgress(60, L"写入图片资源...");

    // 4. 写入图片资源
    for (size_t i = 0; i < static_cast<size_t>(ImageType::Count); ++i) {
        if (config.images[i].loaded && !config.images[i].data.empty()) {
            // 图片资源ID从101开始（100 + ImageType）
            WORD imageResId = static_cast<WORD>(101 + i);

            // 根据格式选择资源类型
            LPCWSTR resType = (config.images[i].data.size() >= 2 &&
                               config.images[i].data[0] == 0x42 && config.images[i].data[1] == 0x4D)
                              ? RT_BITMAP : RT_RCDATA;

            if (!UpdateResourceW(hUpdate, resType, MAKEINTRESOURCEW(imageResId),
                                 MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                                 const_cast<uint8_t*>(config.images[i].data.data()),
                                 static_cast<DWORD>(config.images[i].data.size()))) {
                DWORD error = GetLastError();
                SetError(L"UpdateResource(图片)失败: " + GetSystemErrorMessage(error));
                EndUpdateResource(hUpdate, TRUE);
                return false;
            }
        }

        int progress = 60 + static_cast<int>((i + 1) * 20 / static_cast<size_t>(ImageType::Count));
        ReportProgress(progress, L"写入图片资源 (" + std::to_wstring(i + 1) + L"/" +
                       std::to_wstring(static_cast<size_t>(ImageType::Count)) + L")...");
    }

    ReportProgress(85, L"提交资源更改...");

    // 5. 提交所有更改
    if (!EndUpdateResource(hUpdate, FALSE)) {
        DWORD error = GetLastError();
        SetError(L"EndUpdateResource失败: " + GetSystemErrorMessage(error));
        return false;
    }

    ReportProgress(88, L"资源嵌入完成");
    return true;
}

// ============================================================================
// 自定义PE节策略实现
// ============================================================================

bool CResourceEmbedder::EmbedUsingCustomSection(
    const std::wstring& outputPath,
    const LauncherConfig& config
) {
    ReportProgress(35, L"使用自定义PE节策略嵌入数据...");

    // 1. 序列化配置数据
    std::vector<uint8_t> configData;
    if (!SerializeConfigToResource(config, configData)) {
        SetError(L"序列化配置数据失败");
        return false;
    }

    ReportProgress(40, L"读取PE文件到内存...");

    // 2. 读取整个PE文件到内存
    std::vector<uint8_t> peData;
    if (!ReadFileToMemory(outputPath, peData)) {
        SetError(L"读取输出文件失败");
        return false;
    }

    ReportProgress(50, L"创建自定义PE节...");

    // 3. 创建自定义节 ".launchcfg"
    // 节属性: 可读、可写、初始化数据
    uint32_t sectionCharacteristics = IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE;

    if (!CreatePESection(peData, ".launchcfg", configData, sectionCharacteristics)) {
        SetError(L"创建自定义PE节失败: " + m_lastError);
        return false;
    }

    ReportProgress(80, L"写入修改后的PE文件...");

    // 4. 写回文件
    if (!WriteMemoryToFile(outputPath, peData)) {
        SetError(L"写入修改后的PE文件失败");
        return false;
    }

    ReportProgress(88, L"自定义节嵌入完成");
    return true;
}

// ============================================================================
// 覆盖区追加策略实现
// ============================================================================

bool CResourceEmbedder::EmbedUsingOverlay(
    const std::wstring& outputPath,
    const LauncherConfig& config
) {
    ReportProgress(35, L"使用覆盖区追加策略嵌入数据...");

    // 1. 序列化配置数据
    std::vector<uint8_t> configData;
    if (!SerializeConfigToResource(config, configData)) {
        SetError(L"序列化配置数据失败");
        return false;
    }

    ReportProgress(40, L"读取PE文件...");

    // 2. 读取PE文件
    std::vector<uint8_t> peData;
    if (!ReadFileToMemory(outputPath, peData)) {
        SetError(L"读取输出文件失败");
        return false;
    }

    ReportProgress(50, L"计算覆盖区位置...");

    // 3. 计算覆盖区偏移（PE文件实际数据的末尾）
    size_t overlayOffset = 0;
    if (!GetOverlayOffset(peData, overlayOffset)) {
        SetError(L"计算PE覆盖区偏移失败");
        return false;
    }

    ReportProgress(55, L"追加配置数据到覆盖区...");

    // 4. 如果覆盖区已有数据，截断到覆盖区开始位置
    // 否则直接追加
    if (overlayOffset < peData.size()) {
        peData.resize(overlayOffset);
    }

    // 5. 追加配置数据
    // 格式: [魔数4字节][数据大小4字节][配置数据...]
    std::vector<uint8_t> overlayData;
    overlayData.reserve(8 + configData.size());

    // 写入魔数
    uint32_t magic = GeneratorConstants::CONFIG_SECTION_MAGIC;
    overlayData.insert(overlayData.end(), reinterpret_cast<uint8_t*>(&magic), reinterpret_cast<uint8_t*>(&magic) + sizeof(magic));

    // 写入数据大小
    uint32_t dataSize = static_cast<uint32_t>(configData.size());
    overlayData.insert(overlayData.end(), reinterpret_cast<uint8_t*>(&dataSize), reinterpret_cast<uint8_t*>(&dataSize) + sizeof(dataSize));

    // 写入配置数据
    overlayData.insert(overlayData.end(), configData.begin(), configData.end());

    // 追加到PE数据
    peData.insert(peData.end(), overlayData.begin(), overlayData.end());

    ReportProgress(80, L"写入修改后的文件...");

    // 6. 写回文件
    if (!WriteMemoryToFile(outputPath, peData)) {
        SetError(L"写入修改后的文件失败");
        return false;
    }

    ReportProgress(88, L"覆盖区追加完成");
    return true;
}

// ============================================================================
// 序列化和文件操作
// ============================================================================

bool CResourceEmbedder::SerializeConfigToResource(
    const LauncherConfig& config,
    std::vector<uint8_t>& outData
) {
    try {
        outData = config.Serialize();
        return !outData.empty();
    } catch (const std::exception& e) {
        SetError(std::wstring(L"序列化异常: ") +
                 std::wstring(e.what(), e.what() + std::strlen(e.what())));
        return false;
    }
}

bool CResourceEmbedder::CopyTemplateFile(
    const std::wstring& source,
    const std::wstring& destination
) {
    // 先删除目标文件（如果存在）
    DeleteFileW(destination.c_str());

    // 使用Windows API复制，保留所有属性
    if (!CopyFileW(source.c_str(), destination.c_str(), FALSE)) {
        DWORD error = GetLastError();
        SetError(L"复制模板文件失败: " + GetSystemErrorMessage(error));
        return false;
    }
    return true;
}

bool CResourceEmbedder::ReadFileToMemory(
    const std::wstring& path,
    std::vector<uint8_t>& outData
) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        SetError(L"无法打开文件: " + path);
        return false;
    }

    auto fileSize = file.tellg();
    if (fileSize < 0) {
        SetError(L"获取文件大小失败");
        return false;
    }

    file.seekg(0, std::ios::beg);
    outData.resize(static_cast<size_t>(fileSize));

    if (!file.read(reinterpret_cast<char*>(outData.data()), fileSize)) {
        SetError(L"读取文件数据失败");
        return false;
    }

    return true;
}

bool CResourceEmbedder::WriteMemoryToFile(
    const std::wstring& path,
    const std::vector<uint8_t>& data
) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        SetError(L"无法创建文件: " + path);
        return false;
    }

    if (!file.write(reinterpret_cast<const char*>(data.data()), data.size())) {
        SetError(L"写入文件数据失败");
        return false;
    }

    file.flush();
    return true;
}

// ============================================================================
// PE文件操作
// ============================================================================

bool CResourceEmbedder::ValidateOutputPE(const std::wstring& filePath) {
    std::vector<uint8_t> data;
    if (!ReadFileToMemory(filePath, data)) {
        return false;
    }

    if (data.size() < sizeof(IMAGE_DOS_HEADER)) {
        return false;
    }

    // 验证DOS头
    const IMAGE_DOS_HEADER* dosHeader = reinterpret_cast<const IMAGE_DOS_HEADER*>(data.data());
    if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
        return false;
    }

    // 验证NT头偏移
    if (dosHeader->e_lfanew + sizeof(IMAGE_NT_HEADERS) > data.size()) {
        return false;
    }

    // 验证NT头
    const IMAGE_NT_HEADERS* ntHeaders = reinterpret_cast<const IMAGE_NT_HEADERS*>(data.data() + dosHeader->e_lfanew);
    if (ntHeaders->Signature != IMAGE_NT_SIGNATURE) {
        return false;
    }

    // 验证机器类型为i386
    if (ntHeaders->FileHeader.Machine != IMAGE_FILE_MACHINE_I386) {
        // 非32位PE，但仍然可能是有效的PE
        // 这里我们只验证PE结构完整性，不强制要求32位
    }

    // 验证节表
    WORD numSections = ntHeaders->FileHeader.NumberOfSections;
    DWORD sectionTableOffset = dosHeader->e_lfanew + sizeof(IMAGE_NT_HEADERS);
    if (sectionTableOffset + numSections * sizeof(IMAGE_SECTION_HEADER) > data.size()) {
        return false;
    }

    // 验证每个节的RawData范围
    const IMAGE_SECTION_HEADER* sections = reinterpret_cast<const IMAGE_SECTION_HEADER*>(data.data() + sectionTableOffset);
    for (WORD i = 0; i < numSections; ++i) {
        if (sections[i].PointerToRawData + sections[i].SizeOfRawData > data.size()) {
            return false;
        }
    }

    return true;
}

bool CResourceEmbedder::GetOverlayOffset(
    const std::vector<uint8_t>& peData,
    size_t& outOffset
) {
    if (peData.size() < sizeof(IMAGE_DOS_HEADER)) {
        return false;
    }

    const IMAGE_DOS_HEADER* dosHeader = reinterpret_cast<const IMAGE_DOS_HEADER*>(peData.data());
    if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
        return false;
    }

    if (dosHeader->e_lfanew + sizeof(IMAGE_NT_HEADERS) > peData.size()) {
        return false;
    }

    const IMAGE_NT_HEADERS* ntHeaders = reinterpret_cast<const IMAGE_NT_HEADERS*>(peData.data() + dosHeader->e_lfanew);
    if (ntHeaders->Signature != IMAGE_NT_SIGNATURE) {
        return false;
    }

    WORD numSections = ntHeaders->FileHeader.NumberOfSections;
    DWORD sectionTableOffset = dosHeader->e_lfanew + sizeof(IMAGE_NT_HEADERS);

    if (sectionTableOffset + numSections * sizeof(IMAGE_SECTION_HEADER) > peData.size()) {
        return false;
    }

    // 计算最后一个节的末尾
    size_t maxEnd = sectionTableOffset + numSections * sizeof(IMAGE_SECTION_HEADER);

    const IMAGE_SECTION_HEADER* sections = reinterpret_cast<const IMAGE_SECTION_HEADER*>(peData.data() + sectionTableOffset);
    for (WORD i = 0; i < numSections; ++i) {
        size_t sectionEnd = static_cast<size_t>(sections[i].PointerToRawData) + sections[i].SizeOfRawData;
        if (sectionEnd > maxEnd) {
            maxEnd = sectionEnd;
        }
    }

    outOffset = maxEnd;
    return true;
}

bool CResourceEmbedder::CreatePESection(
    std::vector<uint8_t>& peData,
    const char* sectionName,
    const std::vector<uint8_t>& sectionData,
    uint32_t characteristics
) {
    if (peData.size() < sizeof(IMAGE_DOS_HEADER)) {
        SetError(L"PE数据太小");
        return false;
    }

    IMAGE_DOS_HEADER* dosHeader = reinterpret_cast<IMAGE_DOS_HEADER*>(peData.data());
    if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
        SetError(L"无效的DOS头");
        return false;
    }

    if (dosHeader->e_lfanew + sizeof(IMAGE_NT_HEADERS) > peData.size()) {
        SetError(L"NT头超出文件范围");
        return false;
    }

    IMAGE_NT_HEADERS* ntHeaders = reinterpret_cast<IMAGE_NT_HEADERS*>(peData.data() + dosHeader->e_lfanew);
    if (ntHeaders->Signature != IMAGE_NT_SIGNATURE) {
        SetError(L"无效的NT头");
        return false;
    }

    WORD numSections = ntHeaders->FileHeader.NumberOfSections;
    DWORD sectionTableOffset = dosHeader->e_lfanew + sizeof(IMAGE_NT_HEADERS);

    // 检查是否有空间添加新节表项
    // 节表后面通常是数据目录或第一个节的数据
    DWORD firstSectionOffset = 0xFFFFFFFF;
    IMAGE_SECTION_HEADER* sections = reinterpret_cast<IMAGE_SECTION_HEADER*>(peData.data() + sectionTableOffset);

    for (WORD i = 0; i < numSections; ++i) {
        if (sections[i].PointerToRawData > 0 && sections[i].PointerToRawData < firstSectionOffset) {
            firstSectionOffset = sections[i].PointerToRawData;
        }
    }

    // 计算节表后的可用空间
    DWORD sectionTableEnd = sectionTableOffset + (numSections + 1) * sizeof(IMAGE_SECTION_HEADER);
    if (firstSectionOffset != 0xFFFFFFFF && sectionTableEnd > firstSectionOffset) {
        // 没有足够空间添加新节表项，需要移动数据
        // 这是一个复杂操作，这里简化处理：返回错误
        SetError(L"PE文件没有足够空间添加新节表项");
        return false;
    }

    // 对齐数据大小
    DWORD fileAlignment = ntHeaders->OptionalHeader.FileAlignment;
    DWORD sectionAlignment = ntHeaders->OptionalHeader.SectionAlignment;
    DWORD alignedRawSize = AlignUp(static_cast<DWORD>(sectionData.size()), fileAlignment);
    DWORD alignedVirtualSize = AlignUp(static_cast<DWORD>(sectionData.size()), sectionAlignment);

    // 找到最后一个节，计算新节的偏移
    DWORD lastRawEnd = 0;
    DWORD lastVirtualEnd = 0;
    for (WORD i = 0; i < numSections; ++i) {
        DWORD rawEnd = sections[i].PointerToRawData + sections[i].SizeOfRawData;
        DWORD virtualEnd = sections[i].VirtualAddress + sections[i].Misc.VirtualSize;
        if (rawEnd > lastRawEnd) lastRawEnd = rawEnd;
        if (virtualEnd > lastVirtualEnd) lastVirtualEnd = virtualEnd;
    }

    if (lastRawEnd == 0) {
        // 没有现有节，使用NT头后的位置
        lastRawEnd = AlignUp(sectionTableEnd, fileAlignment);
    }

    DWORD newSectionRawOffset = AlignUp(lastRawEnd, fileAlignment);
    DWORD newSectionVirtualAddr = AlignUp(lastVirtualEnd, sectionAlignment);

    // 添加新节表项
    IMAGE_SECTION_HEADER newSection = {};
    std::strncpy(reinterpret_cast<char*>(newSection.Name), sectionName, IMAGE_SIZEOF_SHORT_NAME);
    newSection.Misc.VirtualSize = static_cast<DWORD>(sectionData.size());
    newSection.VirtualAddress = newSectionVirtualAddr;
    newSection.SizeOfRawData = alignedRawSize;
    newSection.PointerToRawData = newSectionRawOffset;
    newSection.Characteristics = characteristics;

    // 在节表末尾添加新项
    std::memcpy(peData.data() + sectionTableOffset + numSections * sizeof(IMAGE_SECTION_HEADER),
                &newSection, sizeof(newSection));

    // 更新节数量
    ntHeaders->FileHeader.NumberOfSections++;

    // 更新镜像大小
    ntHeaders->OptionalHeader.SizeOfImage = AlignUp(newSectionVirtualAddr + alignedVirtualSize, sectionAlignment);

    // 扩展文件数据并写入新节内容
    size_t newFileSize = newSectionRawOffset + alignedRawSize;
    if (newFileSize > peData.size()) {
        peData.resize(newFileSize, 0);
    }

    std::memcpy(peData.data() + newSectionRawOffset, sectionData.data(), sectionData.size());

    return true;
}
