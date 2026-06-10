/**
 * @file MainDlg.cpp
 * @brief 游戏启动器生成器 - 主对话框实现
 * @details 实现所有UI交互、配置收集、生成控制和日志输出
 */

#include "MainDlg.h"
#include "resource.h"
#include <gdiplus.h>
#include <thread>
#include <future>
#include <iomanip>
#include <sstream>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

IMPLEMENT_DYNAMIC(CMainDlg, CDialogEx)

BEGIN_MESSAGE_MAP(CMainDlg, CDialogEx)
    ON_WM_DESTROY()
    ON_CBN_SELCHANGE(IDC_COMBO_UI_STYLE, &CMainDlg::OnCbnSelchangeUiStyle)
    ON_EN_CHANGE(IDC_EDIT_BG_IMAGE, &CMainDlg::OnEnChangeImagePath)
    ON_EN_CHANGE(IDC_EDIT_BTN_NORMAL, &CMainDlg::OnEnChangeImagePath)
    ON_EN_CHANGE(IDC_EDIT_BTN_HOVER, &CMainDlg::OnEnChangeImagePath)
    ON_EN_CHANGE(IDC_EDIT_BTN_PRESSED, &CMainDlg::OnEnChangeImagePath)
    ON_EN_CHANGE(IDC_EDIT_LOGO, &CMainDlg::OnEnChangeImagePath)
    ON_BN_CLICKED(IDC_BTN_BROWSE_BG, &CMainDlg::OnBnClickedBrowseBg)
    ON_BN_CLICKED(IDC_BTN_BROWSE_BTN_NORMAL, &CMainDlg::OnBnClickedBrowseBtnNormal)
    ON_BN_CLICKED(IDC_BTN_BROWSE_BTN_HOVER, &CMainDlg::OnBnClickedBrowseBtnHover)
    ON_BN_CLICKED(IDC_BTN_BROWSE_BTN_PRESSED, &CMainDlg::OnBnClickedBrowseBtnPressed)
    ON_BN_CLICKED(IDC_BTN_BROWSE_LOGO, &CMainDlg::OnBnClickedBrowseLogo)
    ON_BN_CLICKED(IDC_BTN_BROWSE_PATCH_SOURCE, &CMainDlg::OnBnClickedBrowsePatchSource)
    ON_BN_CLICKED(IDC_BTN_BROWSE_PATCH_OUTPUT, &CMainDlg::OnBnClickedBrowsePatchOutput)
    ON_BN_CLICKED(IDC_BTN_BUILD_PATCH, &CMainDlg::OnBnClickedBuildPatch)
    ON_BN_CLICKED(IDC_BTN_BROWSE_EXTENSION_DLL, &CMainDlg::OnBnClickedBrowseExtensionDll)
    ON_BN_CLICKED(IDC_BTN_BROWSE_TEMPLATE, &CMainDlg::OnBnClickedBrowseTemplate)
    ON_BN_CLICKED(IDC_BTN_BROWSE_OUTPUT, &CMainDlg::OnBnClickedBrowseOutput)
    ON_BN_CLICKED(IDC_BTN_GENERATE, &CMainDlg::OnBnClickedGenerate)
    ON_BN_CLICKED(IDC_BTN_CLEAR_LOG, &CMainDlg::OnBnClickedClearLog)
    ON_MESSAGE(WM_PROGRESS_UPDATE, &CMainDlg::OnProgressUpdate)
    ON_MESSAGE(WM_LOG_MESSAGE, &CMainDlg::OnLogMessage)
    ON_COMMAND(ID_FILE_EXIT, &CMainDlg::OnFileExit)
    ON_COMMAND(ID_HELP_ABOUT, &CMainDlg::OnHelpAbout)
END_MESSAGE_MAP()

// ============================================================================
// 构造函数/析构函数
// ============================================================================

CMainDlg::CMainDlg(CWnd* pParent /*=nullptr*/)
    : CDialogEx(IDD_MAIN_DIALOG, pParent) {
}

CMainDlg::~CMainDlg() {
}

// ============================================================================
// 对话框初始化
// ============================================================================

BOOL CMainDlg::OnInitDialog() {
    CDialogEx::OnInitDialog();

    // 设置窗口标题
    SetWindowText(_T("游戏启动器生成器 v1.0"));

    // 初始化控件
    InitializeControls();
    InitializeComboBoxes();
    InitializeDefaults();

    // 初始化日志
    LogMessage(_T("游戏启动器生成器已启动"));
    LogMessage(_T("请选择启动器模板EXE并配置各项参数，然后点击\"生成启动器\""));

    // 设置生成按钮状态
    SetGenerateButtonState(true);
    SetPatchButtonState(true);

    return TRUE;
}

void CMainDlg::DoDataExchange(CDataExchange* pDX) {
    CDialogEx::DoDataExchange(pDX);

    DDX_Control(pDX, IDC_COMBO_UI_STYLE, m_comboUiStyle);
    DDX_Control(pDX, IDC_EDIT_BG_IMAGE, m_editBgImage);
    DDX_Control(pDX, IDC_EDIT_BTN_NORMAL, m_editBtnNormal);
    DDX_Control(pDX, IDC_EDIT_BTN_HOVER, m_editBtnHover);
    DDX_Control(pDX, IDC_EDIT_BTN_PRESSED, m_editBtnPressed);
    DDX_Control(pDX, IDC_EDIT_LOGO, m_editLogo);
    DDX_Control(pDX, IDC_EDIT_PATCH_SOURCE, m_editPatchSource);
    DDX_Control(pDX, IDC_EDIT_PATCH_KEY, m_editPatchKey);
    DDX_Control(pDX, IDC_EDIT_PATCH_OUTPUT, m_editPatchOutput);
    DDX_Control(pDX, IDC_PROGRESS_PATCH, m_progressPatch);
    DDX_Control(pDX, IDC_STATIC_PATCH_STATUS, m_staticPatchStatus);
    DDX_Control(pDX, IDC_EDIT_PRIMARY_SERVER, m_editPrimaryServer);
    DDX_Control(pDX, IDC_EDIT_BACKUP_SERVER, m_editBackupServer);
    DDX_Control(pDX, IDC_EDIT_PATCH_BASE_URL, m_editPatchBaseUrl);
    DDX_Control(pDX, IDC_EDIT_TIMEOUT, m_editTimeout);
    DDX_Control(pDX, IDC_EDIT_RETRY_COUNT, m_editRetryCount);
    DDX_Control(pDX, IDC_EDIT_OFFICIAL_SITE, m_editOfficialSite);
    DDX_Control(pDX, IDC_EDIT_RECHARGE_URL, m_editRechargeUrl);
    DDX_Control(pDX, IDC_EDIT_CUSTOMER_SERVICE, m_editCustomerService);
    DDX_Control(pDX, IDC_EDIT_REGISTER_URL, m_editRegisterUrl);
    DDX_Control(pDX, IDC_COMBO_UPDATE_STRATEGY, m_comboUpdateStrategy);
    DDX_Control(pDX, IDC_CHECK_FORCE_CRC, m_checkForceCrc);
    DDX_Control(pDX, IDC_CHECK_AUTO_SUBDIR, m_checkAutoSubdir);
    DDX_Control(pDX, IDC_COMBO_CONFLICT_POLICY, m_comboConflictPolicy);
    DDX_Control(pDX, IDC_EDIT_CLIENT_PATH, m_editClientPath);
    DDX_Control(pDX, IDC_EDIT_EXTENSION_DLL, m_editExtensionDll);
    DDX_Control(pDX, IDC_EDIT_TEMPLATE_PATH, m_editTemplatePath);
    DDX_Control(pDX, IDC_EDIT_OUTPUT_PATH, m_editOutputPath);
    DDX_Control(pDX, IDC_COMBO_EMBED_STRATEGY, m_comboEmbedStrategy);
    DDX_Control(pDX, IDC_PROGRESS_GENERATE, m_progressGenerate);
    DDX_Control(pDX, IDC_STATIC_GENERATE_STATUS, m_staticGenerateStatus);
    DDX_Control(pDX, IDC_EDIT_LOG, m_editLog);
    DDX_Control(pDX, IDC_STATIC_IMAGE_PREVIEW, m_staticImagePreview);
}

void CMainDlg::InitializeControls() {
    // 设置进度条范围
    m_progressPatch.SetRange(0, 100);
    m_progressPatch.SetPos(0);
    m_progressGenerate.SetRange(0, 100);
    m_progressGenerate.SetPos(0);

    // 设置状态文本
    m_staticPatchStatus.SetWindowText(_T("就绪"));
    m_staticGenerateStatus.SetWindowText(_T("就绪"));

    // 限制输入长度
    m_editTimeout.SetLimitText(5);
    m_editRetryCount.SetLimitText(3);
    m_editPatchKey.SetLimitText(GeneratorConstants::MAX_KEY_LENGTH);
}

void CMainDlg::InitializeComboBoxes() {
    // UI风格预设
    m_comboUiStyle.AddString(_T("默认风格"));
    m_comboUiStyle.AddString(_T("深色风格"));
    m_comboUiStyle.AddString(_T("浅色风格"));
    m_comboUiStyle.AddString(_T("自定义风格"));
    m_comboUiStyle.SetCurSel(0);

    // 更新策略
    m_comboUpdateStrategy.AddString(_T("本地补丁优先"));
    m_comboUpdateStrategy.AddString(_T("在线下载优先"));
    m_comboUpdateStrategy.AddString(_T("强制在线更新"));
    m_comboUpdateStrategy.SetCurSel(0);

    // 冲突处理策略
    m_comboConflictPolicy.AddString(_T("覆盖现有文件"));
    m_comboConflictPolicy.AddString(_T("跳过现有文件"));
    m_comboConflictPolicy.AddString(_T("提示用户"));
    m_comboConflictPolicy.SetCurSel(0);

    // 嵌入策略
    m_comboEmbedStrategy.AddString(_T("UpdateResource API (推荐)"));
    m_comboEmbedStrategy.AddString(_T("自定义PE节"));
    m_comboEmbedStrategy.AddString(_T("追加到覆盖区"));
    m_comboEmbedStrategy.SetCurSel(0);
}

void CMainDlg::InitializeDefaults() {
    // 设置默认值
    m_editTimeout.SetWindowText(_T("30"));
    m_editRetryCount.SetWindowText(_T("3"));
    m_editClientPath.SetWindowText(_T("client.exe"));

    // 设置默认URL示例
    m_editPrimaryServer.SetWindowText(_T("http://patch.example.com/serverlist.txt"));
    m_editPatchBaseUrl.SetWindowText(_T("http://patch.example.com/patches/"));
}

// ============================================================================
// 配置收集与更新
// ============================================================================

void CMainDlg::CollectConfigFromUI() {
    CString temp;

    // UI风格
    int styleSel = m_comboUiStyle.GetCurSel();
    m_config.uiStyle = static_cast<UIStylePreset>(styleSel);

    // 图片路径
    m_editBgImage.GetWindowText(temp);
    m_config.backgroundPath = temp.GetString();
    m_editBtnNormal.GetWindowText(temp);
    m_config.buttonNormalPath = temp.GetString();
    m_editBtnHover.GetWindowText(temp);
    m_config.buttonHoverPath = temp.GetString();
    m_editBtnPressed.GetWindowText(temp);
    m_config.buttonPressedPath = temp.GetString();
    m_editLogo.GetWindowText(temp);
    m_config.logoPath = temp.GetString();

    // 网络配置
    m_editPrimaryServer.GetWindowText(temp);
    m_config.primaryServerlistUrl = temp.GetString();
    m_editBackupServer.GetWindowText(temp);
    m_config.backupServerlistUrl = temp.GetString();
    m_editPatchBaseUrl.GetWindowText(temp);
    m_config.patchBaseUrl = temp.GetString();

    m_editTimeout.GetWindowText(temp);
    m_config.timeoutSeconds = _ttoi(temp);
    if (m_config.timeoutSeconds == 0) m_config.timeoutSeconds = 30;

    m_editRetryCount.GetWindowText(temp);
    m_config.retryCount = _ttoi(temp);
    if (m_config.retryCount == 0) m_config.retryCount = 3;

    // 业务URL
    m_editOfficialSite.GetWindowText(temp);
    m_config.officialSiteUrl = temp.GetString();
    m_editRechargeUrl.GetWindowText(temp);
    m_config.rechargeUrl = temp.GetString();
    m_editCustomerService.GetWindowText(temp);
    m_config.customerServiceUrl = temp.GetString();
    m_editRegisterUrl.GetWindowText(temp);
    m_config.registerUrl = temp.GetString();

    // 更新策略
    int strategySel = m_comboUpdateStrategy.GetCurSel();
    m_config.updateStrategy = static_cast<UpdateStrategy>(strategySel);
    m_config.forceCrcCheck = (m_checkForceCrc.GetCheck() == BST_CHECKED);
    m_config.autoCreateSubdirs = (m_checkAutoSubdir.GetCheck() == BST_CHECKED);
    int conflictSel = m_comboConflictPolicy.GetCurSel();
    m_config.conflictPolicy = static_cast<ConflictPolicy>(conflictSel);

    // 路径配置
    m_editClientPath.GetWindowText(temp);
    m_config.clientPath = temp.GetString();
    m_editExtensionDll.GetWindowText(temp);
    m_config.extensionDllPath = temp.GetString();

    // 生成配置
    m_editTemplatePath.GetWindowText(temp);
    m_config.templatePath = temp.GetString();
    m_editOutputPath.GetWindowText(temp);
    m_config.outputPath = temp.GetString();

    // 加密密钥
    m_editPatchKey.GetWindowText(temp);
    m_config.encryptionKey = temp.GetString();

    // 补丁配置
    m_editPatchSource.GetWindowText(temp);
    m_config.patchSourceDir = temp.GetString();
    m_editPatchOutput.GetWindowText(temp);
    m_config.patchOutputPath = temp.GetString();
}

// ============================================================================
// 浏览文件/目录辅助函数
// ============================================================================

bool CMainDlg::BrowseForFile(CString& outPath, const wchar_t* filter, const wchar_t* title) {
    CFileDialog dlg(TRUE, nullptr, nullptr,
                    OFN_HIDEREADONLY | OFN_FILEMUSTEXIST,
                    filter, this);
    if (title) {
        dlg.GetOFN().lpstrTitle = title;
    }
    if (dlg.DoModal() == IDOK) {
        outPath = dlg.GetPathName();
        return true;
    }
    return false;
}

bool CMainDlg::BrowseForDirectory(CString& outPath, const wchar_t* title) {
    BROWSEINFO bi;
    memset(&bi, 0, sizeof(bi));
    bi.hwndOwner = m_hWnd;
    bi.lpszTitle = title;
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

    LPITEMIDLIST pidl = SHBrowseForFolder(&bi);
    if (pidl) {
        wchar_t path[MAX_PATH];
        if (SHGetPathFromIDList(pidl, path)) {
            outPath = path;
        }
        CoTaskMemFree(pidl);
        return !outPath.IsEmpty();
    }
    return false;
}

bool CMainDlg::BrowseForSaveFile(CString& outPath, const wchar_t* filter, const wchar_t* title, const wchar_t* defaultExt) {
    CFileDialog dlg(FALSE, defaultExt, nullptr,
                    OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
                    filter, this);
    if (title) {
        dlg.GetOFN().lpstrTitle = title;
    }
    if (dlg.DoModal() == IDOK) {
        outPath = dlg.GetPathName();
        return true;
    }
    return false;
}

// ============================================================================
// 图片浏览按钮事件
// ============================================================================

void CMainDlg::OnBnClickedBrowseBg() {
    CString path;
    if (BrowseForFile(path, _T("图片文件 (*.bmp;*.png)|*.bmp;*.png|所有文件 (*.*)|*.*||"), _T("选择背景图片"))) {
        m_editBgImage.SetWindowText(path);
    }
}

void CMainDlg::OnBnClickedBrowseBtnNormal() {
    CString path;
    if (BrowseForFile(path, _T("图片文件 (*.bmp;*.png)|*.bmp;*.png|所有文件 (*.*)|*.*||"), _T("选择按钮正常状态图片"))) {
        m_editBtnNormal.SetWindowText(path);
    }
}

void CMainDlg::OnBnClickedBrowseBtnHover() {
    CString path;
    if (BrowseForFile(path, _T("图片文件 (*.bmp;*.png)|*.bmp;*.png|所有文件 (*.*)|*.*||"), _T("选择按钮悬停状态图片"))) {
        m_editBtnHover.SetWindowText(path);
    }
}

void CMainDlg::OnBnClickedBrowseBtnPressed() {
    CString path;
    if (BrowseForFile(path, _T("图片文件 (*.bmp;*.png)|*.bmp;*.png|所有文件 (*.*)|*.*||"), _T("选择按钮按下状态图片"))) {
        m_editBtnPressed.SetWindowText(path);
    }
}

void CMainDlg::OnBnClickedBrowseLogo() {
    CString path;
    if (BrowseForFile(path, _T("图片文件 (*.bmp;*.png)|*.bmp;*.png|所有文件 (*.*)|*.*||"), _T("选择Logo图片"))) {
        m_editLogo.SetWindowText(path);
    }
}

// ============================================================================
// 图片路径变更事件 - 自动加载和预览
// ============================================================================

void CMainDlg::OnEnChangeImagePath(UINT nID) {
    ImageType type = ImageType::Count;
    switch (nID) {
        case IDC_EDIT_BG_IMAGE:     type = ImageType::Background; break;
        case IDC_EDIT_BTN_NORMAL:   type = ImageType::ButtonNormal; break;
        case IDC_EDIT_BTN_HOVER:    type = ImageType::ButtonHover; break;
        case IDC_EDIT_BTN_PRESSED:  type = ImageType::ButtonPressed; break;
        case IDC_EDIT_LOGO:         type = ImageType::Logo; break;
        default: return;
    }

    CEdit* pEdit = nullptr;
    switch (nID) {
        case IDC_EDIT_BG_IMAGE:     pEdit = &m_editBgImage; break;
        case IDC_EDIT_BTN_NORMAL:   pEdit = &m_editBtnNormal; break;
        case IDC_EDIT_BTN_HOVER:    pEdit = &m_editBtnHover; break;
        case IDC_EDIT_BTN_PRESSED:  pEdit = &m_editBtnPressed; break;
        case IDC_EDIT_LOGO:         pEdit = &m_editLogo; break;
    }

    if (pEdit) {
        CString path;
        pEdit->GetWindowText(path);
        if (!path.IsEmpty()) {
            std::wstring error;
            if (m_config.LoadImage(type, path.GetString(), error)) {
                UpdateImagePreview(type);
            } else {
                LogError(CString(error.c_str()));
            }
        }
    }
}

// ============================================================================
// 图片预览
// ============================================================================

void CMainDlg::UpdateImagePreview(ImageType type) {
    size_t idx = static_cast<size_t>(type);
    if (idx >= static_cast<size_t>(ImageType::Count) || !m_config.images[idx].loaded) {
        ClearImagePreview();
        return;
    }

    // 使用GDI+加载并显示缩略图
    CRect rect;
    m_staticImagePreview.GetClientRect(&rect);

    // 创建内存DC
    CDC* pDC = m_staticImagePreview.GetDC();
    if (!pDC) return;

    CDC memDC;
    memDC.CreateCompatibleDC(pDC);

    CBitmap bitmap;
    bitmap.CreateCompatibleBitmap(pDC, rect.Width(), rect.Height());
    CBitmap* pOldBitmap = memDC.SelectObject(&bitmap);

    // 填充背景
    memDC.FillSolidRect(&rect, RGB(200, 200, 200));

    // 绘制图片信息
    CString info;
    info.Format(_T("类型: %d\\n尺寸: %dx%d\\n大小: %zu bytes"),
                static_cast<int>(type),
                m_config.images[idx].width,
                m_config.images[idx].height,
                m_config.images[idx].data.size());
    memDC.SetBkMode(TRANSPARENT);
    memDC.SetTextColor(RGB(0, 0, 0));
    memDC.DrawText(info, &rect, DT_CENTER | DT_VCENTER | DT_WORDBREAK);

    memDC.SelectObject(pOldBitmap);
    m_staticImagePreview.ReleaseDC(pDC);

    // 设置预览位图
    m_staticImagePreview.SetBitmap(bitmap);
    m_previewBitmap.Detach();
    m_previewBitmap.Attach(bitmap.Detach());
}

void CMainDlg::ClearImagePreview() {
    m_staticImagePreview.SetBitmap(nullptr);
    if (m_previewBitmap.GetSafeHandle()) {
        m_previewBitmap.DeleteObject();
    }
}

// ============================================================================
// 补丁打包事件
// ============================================================================

void CMainDlg::OnBnClickedBrowsePatchSource() {
    CString path;
    if (BrowseForDirectory(path, _T("选择补丁源目录"))) {
        m_editPatchSource.SetWindowText(path);
    }
}

void CMainDlg::OnBnClickedBrowsePatchOutput() {
    CString path;
    if (BrowseForSaveFile(path, _T("补丁包文件 (*.pkg)|*.pkg|所有文件 (*.*)|*.*||"),
                          _T("保存补丁包"), _T("pkg"))) {
        m_editPatchOutput.SetWindowText(path);
    }
}

void CMainDlg::OnBnClickedBuildPatch() {
    if (m_patching) {
        m_patchBuilder.Cancel();
        return;
    }

    CString sourceDir, outputPath, key;
    m_editPatchSource.GetWindowText(sourceDir);
    m_editPatchOutput.GetWindowText(outputPath);
    m_editPatchKey.GetWindowText(key);

    if (sourceDir.IsEmpty()) {
        AfxMessageBox(_T("请选择补丁源目录"), MB_ICONWARNING);
        return;
    }
    if (outputPath.IsEmpty()) {
        AfxMessageBox(_T("请选择补丁包输出路径"), MB_ICONWARNING);
        return;
    }

    // 设置打包器参数
    m_patchBuilder.ResetCancel();
    m_patchBuilder.SetCompressionEnabled(true);
    m_patchBuilder.SetCompressionLevel(6);
    m_patchBuilder.SetEncryptionKey(key.GetString());
    m_patchBuilder.SetProgressCallback([this](const PackProgressInfo& info) {
        // 发送消息到UI线程
        PackProgressInfo* pInfo = new PackProgressInfo(info);
        PostMessage(WM_PROGRESS_UPDATE, 1, reinterpret_cast<LPARAM>(pInfo));
    });

    // 启动工作线程
    auto* pParam = new PatchThreadParam{
        this,
        sourceDir.GetString(),
        outputPath.GetString(),
        key.GetString()
    };

    m_patching = true;
    SetPatchButtonState(true);
    LogMessage(_T("开始构建补丁包..."));

    AfxBeginThread(PatchThreadProc, pParam);
}

UINT CMainDlg::PatchThreadProc(LPVOID pParam) {
    auto* pThreadParam = static_cast<PatchThreadParam*>(pParam);
    if (!pThreadParam || !pThreadParam->pDlg) {
        delete pThreadParam;
        return 1;
    }

    pThreadParam->pDlg->DoBuildPatch();

    delete pThreadParam;
    return 0;
}

void CMainDlg::DoBuildPatch() {
    CString sourceDir, outputPath;
    m_editPatchSource.GetWindowText(sourceDir);
    m_editPatchOutput.GetWindowText(outputPath);

    PackResult result = m_patchBuilder.BuildPatch(sourceDir.GetString(), outputPath.GetString());

    // 发送完成消息到UI线程
    CString* pMsg = new CString(CPatchBuilder::ResultToString(result).c_str());
    PostMessage(WM_LOG_MESSAGE,
                (result == PackResult::Success) ? 1 : 2,
                reinterpret_cast<LPARAM>(pMsg));

    m_patching = false;
    PostMessage(WM_PROGRESS_UPDATE, 2, 0); // 重置按钮状态
}

void CMainDlg::OnPatchProgress(const PackProgressInfo& info) {
    m_progressPatch.SetPos(info.overallPercentage);
    m_staticPatchStatus.SetWindowText(info.statusMessage.c_str());
}

// ============================================================================
// 扩展DLL浏览
// ============================================================================

void CMainDlg::OnBnClickedBrowseExtensionDll() {
    CString path;
    if (BrowseForFile(path, _T("DLL文件 (*.dll)|*.dll|所有文件 (*.*)|*.*||"), _T("选择扩展DLL"))) {
        m_editExtensionDll.SetWindowText(path);
    }
}

// ============================================================================
// 生成配置浏览
// ============================================================================

void CMainDlg::OnBnClickedBrowseTemplate() {
    CString path;
    if (BrowseForFile(path, _T("可执行文件 (*.exe)|*.exe|所有文件 (*.*)|*.*||"), _T("选择启动器模板EXE"))) {
        m_editTemplatePath.SetWindowText(path);
    }
}

void CMainDlg::OnBnClickedBrowseOutput() {
    CString path;
    if (BrowseForSaveFile(path, _T("可执行文件 (*.exe)|*.exe|所有文件 (*.*)|*.*||"),
                          _T("保存生成的启动器"), _T("exe"))) {
        m_editOutputPath.SetWindowText(path);
    }
}

// ============================================================================
// 生成启动器事件
// ============================================================================

void CMainDlg::OnBnClickedGenerate() {
    if (m_generating) {
        // 已经在生成中，忽略
        return;
    }

    // 收集配置
    CollectConfigFromUI();

    // 验证配置
    std::wstring validateError;
    if (!m_config.Validate(validateError)) {
        AfxMessageBox(CString(validateError.c_str()), MB_ICONWARNING);
        return;
    }

    // 验证模板文件
    if (m_config.templatePath.empty()) {
        AfxMessageBox(_T("请选择启动器模板EXE文件"), MB_ICONWARNING);
        return;
    }
    if (GetFileAttributesW(m_config.templatePath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        AfxMessageBox(_T("模板文件不存在"), MB_ICONWARNING);
        return;
    }

    // 验证输出路径
    if (m_config.outputPath.empty()) {
        AfxMessageBox(_T("请选择输出路径"), MB_ICONWARNING);
        return;
    }

    // 获取嵌入策略
    int strategySel = m_comboEmbedStrategy.GetCurSel();
    EmbedStrategy strategy = static_cast<EmbedStrategy>(strategySel);

    // 设置进度回调
    m_embedder.SetProgressCallback([this](int percentage, const std::wstring& status) {
        OnGenerateProgress(percentage, status);
    });
    m_embedder.SetEmbedStrategy(strategy);

    // 创建线程参数
    auto* pParam = new GenerateThreadParam{
        this,
        m_config,
        m_config.templatePath,
        m_config.outputPath,
        strategy
    };

    m_generating = true;
    SetGenerateButtonState(false);
    m_progressGenerate.SetPos(0);
    LogMessage(_T("开始生成启动器..."));
    LogMessage(_T("模板: ") + CString(m_config.templatePath.c_str()));
    LogMessage(_T("输出: ") + CString(m_config.outputPath.c_str()));

    // 启动工作线程
    AfxBeginThread(GenerateThreadProc, pParam);
}

UINT CMainDlg::GenerateThreadProc(LPVOID pParam) {
    auto* pThreadParam = static_cast<GenerateThreadParam*>(pParam);
    if (!pThreadParam || !pThreadParam->pDlg) {
        delete pThreadParam;
        return 1;
    }

    pThreadParam->pDlg->DoGenerate();

    delete pThreadParam;
    return 0;
}

void CMainDlg::DoGenerate() {
    // 在工作线程中执行生成
    EmbedResult result = m_embedder.EmbedResources(
        m_config.templatePath,
        m_config,
        m_config.outputPath
    );

    // 发送完成消息到UI线程
    CString* pMsg = new CString(CResourceEmbedder::ResultToString(result).c_str());
    PostMessage(WM_LOG_MESSAGE,
                (result == EmbedResult::Success) ? 1 : 2,
                reinterpret_cast<LPARAM>(pMsg));

    if (result != EmbedResult::Success) {
        CString* pError = new CString(m_embedder.GetLastErrorMessage().c_str());
        PostMessage(WM_LOG_MESSAGE, 2, reinterpret_cast<LPARAM>(pError));
    }

    m_generating = false;
    PostMessage(WM_PROGRESS_UPDATE, 0, 0); // 重置UI状态
}

void CMainDlg::OnGenerateProgress(int percentage, const std::wstring& status) {
    m_progressGenerate.SetPos(percentage);
    m_staticGenerateStatus.SetWindowText(status.c_str());
}

// ============================================================================
// 自定义消息处理
// ============================================================================

LRESULT CMainDlg::OnProgressUpdate(WPARAM wParam, LPARAM lParam) {
    if (wParam == 0) {
        // 生成完成，重置UI
        SetGenerateButtonState(true);
        m_progressGenerate.SetPos(100);
    } else if (wParam == 1) {
        // 补丁进度更新
        auto* pInfo = reinterpret_cast<PackProgressInfo*>(lParam);
        if (pInfo) {
            OnPatchProgress(*pInfo);
            delete pInfo;
        }
    } else if (wParam == 2) {
        // 补丁完成
        SetPatchButtonState(true);
        m_progressPatch.SetPos(100);
        m_staticPatchStatus.SetWindowText(_T("完成"));
    }
    return 0;
}

LRESULT CMainDlg::OnLogMessage(WPARAM wParam, LPARAM lParam) {
    auto* pMsg = reinterpret_cast<CString*>(lParam);
    if (pMsg) {
        if (wParam == 1) {
            LogSuccess(*pMsg);
        } else if (wParam == 2) {
            LogError(*pMsg);
        } else {
            LogMessage(*pMsg);
        }
        delete pMsg;
    }
    return 0;
}

// ============================================================================
// 日志输出
// ============================================================================

void CMainDlg::LogMessage(const CString& message) {
    CString currentText;
    m_editLog.GetWindowText(currentText);

    CTime now = CTime::GetCurrentTime();
    CString timestamp = now.Format(_T("[%H:%M:%S] "));

    CString newLine = timestamp + message + _T("\r\n");
    currentText += newLine;

    m_editLog.SetWindowText(currentText);
    m_editLog.LineScroll(m_editLog.GetLineCount());
}

void CMainDlg::LogError(const CString& message) {
    LogMessage(_T("[错误] ") + message);
}

void CMainDlg::LogSuccess(const CString& message) {
    LogMessage(_T("[成功] ") + message);
}

void CMainDlg::ClearLog() {
    m_editLog.SetWindowText(_T(""));
}

void CMainDlg::OnBnClickedClearLog() {
    ClearLog();
}

// ============================================================================
// 按钮状态控制
// ============================================================================

void CMainDlg::SetGenerateButtonState(bool enabled) {
    CWnd* pBtn = GetDlgItem(IDC_BTN_GENERATE);
    if (pBtn) {
        pBtn->SetWindowText(enabled ? _T("生成启动器") : _T("生成中..."));
        pBtn->EnableWindow(enabled ? TRUE : FALSE);
    }
}

void CMainDlg::SetPatchButtonState(bool enabled) {
    CWnd* pBtn = GetDlgItem(IDC_BTN_BUILD_PATCH);
    if (pBtn) {
        pBtn->SetWindowText(enabled ? _T("构建补丁包") : _T("构建中..."));
        pBtn->EnableWindow(enabled ? TRUE : FALSE);
    }
}

// ============================================================================
// UI风格变更
// ============================================================================

void CMainDlg::OnCbnSelchangeUiStyle() {
    int sel = m_comboUiStyle.GetCurSel();
    // 可以在这里更新UI预览
    LogMessage(_T("UI风格已切换"));
}

// ============================================================================
// 图片文件验证
// ============================================================================

bool CMainDlg::ValidateImageFile(const CString& path, CString& error) {
    if (path.IsEmpty()) {
        error = _T("路径为空");
        return false;
    }

    CFile file;
    if (!file.Open(path, CFile::modeRead | CFile::shareDenyWrite)) {
        error = _T("无法打开文件");
        return false;
    }

    // 读取文件头进行魔数校验
    BYTE header[8] = { 0 };
    UINT read = file.Read(header, 8);
    file.Close();

    if (read < 2) {
        error = _T("文件太小");
        return false;
    }

    bool isBmp = (header[0] == 0x42 && header[1] == 0x4D);
    bool isPng = (read >= 8 && header[0] == 0x89 && header[1] == 0x50 &&
                  header[2] == 0x4E && header[3] == 0x47);

    if (!isBmp && !isPng) {
        error = _T("不支持的图片格式（仅支持BMP/PNG）");
        return false;
    }

    return true;
}

// ============================================================================
// 菜单命令
// ============================================================================

void CMainDlg::OnFileExit() {
    OnOK();
}

void CMainDlg::OnHelpAbout() {
    AfxMessageBox(_T("游戏启动器生成器 v1.0\n\n"
                      "用于生成自定义配置的游戏启动器EXE。\n"
                      "支持配置嵌入、图片替换和补丁打包功能。\n\n"
                      "Copyright (C) 2024"),
                  MB_ICONINFORMATION | MB_OK);
}

// ============================================================================
// 对话框销毁
// ============================================================================

void CMainDlg::OnDestroy() {
    // 取消正在进行的操作
    if (m_generating) {
        // 等待生成完成或超时
    }
    if (m_patching) {
        m_patchBuilder.Cancel();
    }

    CDialogEx::OnDestroy();
}
