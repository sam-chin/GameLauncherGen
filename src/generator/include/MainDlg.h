#pragma once

/**
 * @file MainDlg.h
 * @brief 游戏启动器生成器 - 主对话框
 * @details 包含所有UI控件和配置区域的MFC对话框类。
 *          使用标签页或分组框组织各个功能区域。
 */

#include "ConfigData.h"
#include "ResourceEmbedder.h"
#include "PatchBuilder.h"
#include <afxcmn.h>
#include <afxwin.h>
#include <afxdialogex.h>

// 控件ID定义（与resource.h保持一致）
enum MainDlgControls {
    // UI风格
    IDC_COMBO_UI_STYLE = 1001,
    IDC_STATIC_UI_PREVIEW,

    // 图片替换
    IDC_EDIT_BG_IMAGE = 1101,
    IDC_BTN_BROWSE_BG,
    IDC_EDIT_BTN_NORMAL,
    IDC_BTN_BROWSE_BTN_NORMAL,
    IDC_EDIT_BTN_HOVER,
    IDC_BTN_BROWSE_BTN_HOVER,
    IDC_EDIT_BTN_PRESSED,
    IDC_BTN_BROWSE_BTN_PRESSED,
    IDC_EDIT_LOGO,
    IDC_BTN_BROWSE_LOGO,
    IDC_STATIC_IMAGE_PREVIEW,

    // 补丁打包
    IDC_EDIT_PATCH_SOURCE = 1201,
    IDC_BTN_BROWSE_PATCH_SOURCE,
    IDC_EDIT_PATCH_KEY,
    IDC_EDIT_PATCH_OUTPUT,
    IDC_BTN_BROWSE_PATCH_OUTPUT,
    IDC_BTN_BUILD_PATCH,
    IDC_PROGRESS_PATCH,
    IDC_STATIC_PATCH_STATUS,

    // 网络配置
    IDC_EDIT_PRIMARY_SERVER = 1301,
    IDC_EDIT_BACKUP_SERVER,
    IDC_EDIT_PATCH_BASE_URL,
    IDC_EDIT_TIMEOUT,
    IDC_EDIT_RETRY_COUNT,

    // 业务URL
    IDC_EDIT_OFFICIAL_SITE = 1401,
    IDC_EDIT_RECHARGE_URL,
    IDC_EDIT_CUSTOMER_SERVICE,
    IDC_EDIT_REGISTER_URL,

    // 更新策略
    IDC_COMBO_UPDATE_STRATEGY = 1501,
    IDC_CHECK_FORCE_CRC,
    IDC_CHECK_AUTO_SUBDIR,
    IDC_COMBO_CONFLICT_POLICY,

    // 路径配置
    IDC_EDIT_CLIENT_PATH = 1601,
    IDC_EDIT_EXTENSION_DLL,
    IDC_BTN_BROWSE_EXTENSION_DLL,

    // 生成配置
    IDC_EDIT_TEMPLATE_PATH = 1701,
    IDC_BTN_BROWSE_TEMPLATE,
    IDC_EDIT_OUTPUT_PATH,
    IDC_BTN_BROWSE_OUTPUT,
    IDC_COMBO_EMBED_STRATEGY,
    IDC_PROGRESS_GENERATE,
    IDC_STATIC_GENERATE_STATUS,
    IDC_BTN_GENERATE,

    // 日志
    IDC_EDIT_LOG = 1801,
    IDC_BTN_CLEAR_LOG,

    // 菜单项
    ID_FILE_EXIT = 1901,
    ID_HELP_ABOUT,
};

// 主对话框类
class CMainDlg : public CDialogEx {
    DECLARE_DYNAMIC(CMainDlg)

public:
    CMainDlg(CWnd* pParent = nullptr);
    virtual ~CMainDlg();

    // 对话框数据
#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_MAIN_DIALOG };
#endif

protected:
    // MFC消息映射
    DECLARE_MESSAGE_MAP()

    // 对话框初始化
    virtual BOOL OnInitDialog() override;
    virtual void DoDataExchange(CDataExchange* pDX) override;

    // 命令处理
    afx_msg void OnBnClickedBrowseBg();
    afx_msg void OnBnClickedBrowseBtnNormal();
    afx_msg void OnBnClickedBrowseBtnHover();
    afx_msg void OnBnClickedBrowseBtnPressed();
    afx_msg void OnBnClickedBrowseLogo();
    afx_msg void OnBnClickedBrowsePatchSource();
    afx_msg void OnBnClickedBrowsePatchOutput();
    afx_msg void OnBnClickedBuildPatch();
    afx_msg void OnBnClickedBrowseExtensionDll();
    afx_msg void OnBnClickedBrowseTemplate();
    afx_msg void OnBnClickedBrowseOutput();
    afx_msg void OnBnClickedGenerate();
    afx_msg void OnBnClickedClearLog();
    afx_msg void OnCbnSelchangeUiStyle();
    afx_msg void OnEnChangeImagePath(UINT nID);
    afx_msg void OnDestroy();

    // 自定义消息
    afx_msg LRESULT OnProgressUpdate(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnLogMessage(WPARAM wParam, LPARAM lParam);

    // 菜单命令
    afx_msg void OnFileExit();
    afx_msg void OnHelpAbout();

private:
    // 配置数据
    LauncherConfig m_config;

    // 核心模块
    CResourceEmbedder m_embedder;
    CPatchBuilder m_patchBuilder;

    // 控件变量
    CComboBox   m_comboUiStyle;
    CComboBox   m_comboUpdateStrategy;
    CComboBox   m_comboConflictPolicy;
    CComboBox   m_comboEmbedStrategy;

    CEdit       m_editBgImage;
    CEdit       m_editBtnNormal;
    CEdit       m_editBtnHover;
    CEdit       m_editBtnPressed;
    CEdit       m_editLogo;

    CEdit       m_editPatchSource;
    CEdit       m_editPatchKey;
    CEdit       m_editPatchOutput;
    CProgressCtrl m_progressPatch;
    CStatic     m_staticPatchStatus;

    CEdit       m_editPrimaryServer;
    CEdit       m_editBackupServer;
    CEdit       m_editPatchBaseUrl;
    CEdit       m_editTimeout;
    CEdit       m_editRetryCount;

    CEdit       m_editOfficialSite;
    CEdit       m_editRechargeUrl;
    CEdit       m_editCustomerService;
    CEdit       m_editRegisterUrl;

    CButton     m_checkForceCrc;
    CButton     m_checkAutoSubdir;

    CEdit       m_editClientPath;
    CEdit       m_editExtensionDll;

    CEdit       m_editTemplatePath;
    CEdit       m_editOutputPath;
    CProgressCtrl m_progressGenerate;
    CStatic     m_staticGenerateStatus;

    CEdit       m_editLog;

    // 图片预览
    CStatic     m_staticImagePreview;
    CBitmap     m_previewBitmap;

    // 生成线程相关
    volatile bool m_generating = false;
    volatile bool m_patching = false;

    // 初始化UI
    void InitializeControls();
    void InitializeComboBoxes();
    void InitializeDefaults();

    // 从UI收集配置
    void CollectConfigFromUI();

    // 更新UI从配置
    void UpdateUIFromConfig();

    // 浏览文件/目录辅助函数
    [[nodiscard]] bool BrowseForFile(CString& outPath, const wchar_t* filter, const wchar_t* title);
    [[nodiscard]] bool BrowseForDirectory(CString& outPath, const wchar_t* title);
    [[nodiscard]] bool BrowseForSaveFile(CString& outPath, const wchar_t* filter, const wchar_t* title, const wchar_t* defaultExt);

    // 图片预览
    void UpdateImagePreview(ImageType type);
    void ClearImagePreview();

    // 日志输出
    void LogMessage(const CString& message);
    void LogError(const CString& message);
    void LogSuccess(const CString& message);
    void ClearLog();

    // 生成进度回调
    void OnGenerateProgress(int percentage, const std::wstring& status);

    // 打包进度回调
    void OnPatchProgress(const PackProgressInfo& info);

    // 验证图片文件
    [[nodiscard]] bool ValidateImageFile(const CString& path, CString& error);

    // 工作线程函数（静态）
    static UINT GenerateThreadProc(LPVOID pParam);
    static UINT PatchThreadProc(LPVOID pParam);

    // 生成和打包的实际工作
    void DoGenerate();
    void DoBuildPatch();

    // 启用/禁用生成按钮
    void SetGenerateButtonState(bool enabled);
    void SetPatchButtonState(bool enabled);
};

// 自定义窗口消息
#define WM_PROGRESS_UPDATE  (WM_USER + 100)
#define WM_LOG_MESSAGE      (WM_USER + 101)

// 线程参数结构
struct GenerateThreadParam {
    CMainDlg* pDlg;
    LauncherConfig config;
    std::wstring templatePath;
    std::wstring outputPath;
    EmbedStrategy strategy;
};

struct PatchThreadParam {
    CMainDlg* pDlg;
    std::wstring sourceDir;
    std::wstring outputPath;
    std::wstring encryptionKey;
};
