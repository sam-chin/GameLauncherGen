/**
 * @file GeneratorApp.cpp
 * @brief 游戏启动器生成器 - 应用程序实现
 */

#include "GeneratorApp.h"
#include "MainDlg.h"
#include <combaseapi.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// 全局应用程序对象
CGeneratorApp theApp;

// ============================================================================
// 构造函数
// ============================================================================

CGeneratorApp::CGeneratorApp() noexcept {
    // 支持重新启动管理器
    m_dwRestartManagerSupportFlags = AFX_RESTART_MANAGER_SUPPORT_RESTART;

    // 设置应用程序信息
    SetAppID(_T("GameLauncher.Generator.1.0"));
}

// ============================================================================
// InitInstance - 应用程序初始化
// ============================================================================

BOOL CGeneratorApp::InitInstance() {
    // 初始化OLE/COM库（用于Shell操作和拖放）
    if (!InitializeCOM()) {
        AfxMessageBox(_T("初始化COM组件失败"), MB_ICONERROR | MB_OK);
        return FALSE;
    }

    // 初始化标准控件和通用控件
    INITCOMMONCONTROLSEX initCtrls;
    initCtrls.dwSize = sizeof(initCtrls);
    initCtrls.dwICC = ICC_WIN95_CLASSES | ICC_INTERNET_CLASSES | ICC_LINK_CLASS;
    if (!InitCommonControlsEx(&initCtrls)) {
        AfxMessageBox(_T("初始化公共控件失败"), MB_ICONERROR | MB_OK);
        return FALSE;
    }

    // 初始化MFC标准功能
    CWinApp::InitInstance();

    // 初始化GDI+
    if (!InitializeGdiPlus()) {
        AfxMessageBox(_T("初始化GDI+失败"), MB_ICONERROR | MB_OK);
        return FALSE;
    }

    // 设置对话框背景颜色
    SetDialogBkColor(RGB(240, 240, 240), RGB(0, 0, 0));

    // 创建并显示主对话框
    CMainDlg mainDlg;
    m_pMainWnd = &mainDlg;

    INT_PTR nResponse = mainDlg.DoModal();
    if (nResponse == IDOK) {
        // 用户点击确定（正常关闭）
    } else if (nResponse == IDCANCEL) {
        // 用户点击取消或关闭
    } else if (nResponse == -1) {
        // 对话框创建失败
        TRACE(traceAppMsg, 0, _T("警告: 对话框创建失败，应用程序将异常终止\n"));
    }

    // 由于对话框已关闭，返回FALSE以退出应用程序
    return FALSE;
}

// ============================================================================
// ExitInstance - 应用程序退出清理
// ============================================================================

int CGeneratorApp::ExitInstance() {
    // 清理GDI+
    ShutdownGdiPlus();

    // 反初始化COM
    CoUninitialize();

    return CWinApp::ExitInstance();
}

// ============================================================================
// 私有辅助函数
// ============================================================================

bool CGeneratorApp::InitializeCOM() {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        return false;
    }
    return true;
}

bool CGeneratorApp::InitializeGdiPlus() {
    Gdiplus::GdiplusStartupInput input;
    Gdiplus::GdiplusStartupOutput output;
    ULONG_PTR token = 0;

    input.GdiplusVersion = 1;
    input.DebugEventCallback = nullptr;
    input.SuppressBackgroundThread = FALSE;
    input.SuppressExternalCodecs = FALSE;

    Gdiplus::Status status = Gdiplus::GdiplusStartup(&token, &input, &output);
    if (status != Gdiplus::Ok) {
        return false;
    }

    m_gdiplusToken = token;
    m_gdiplusInitialized = true;
    return true;
}

void CGeneratorApp::ShutdownGdiPlus() {
    if (m_gdiplusInitialized && m_gdiplusToken != 0) {
        Gdiplus::GdiplusShutdown(m_gdiplusToken);
        m_gdiplusToken = 0;
        m_gdiplusInitialized = false;
    }
}
