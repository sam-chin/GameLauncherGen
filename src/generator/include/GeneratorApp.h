#pragma once

/**
 * @file GeneratorApp.h
 * @brief 游戏启动器生成器 - 应用程序类
 * @details MFC应用程序入口类，负责初始化COM、GDI+等子系统
 */

#ifndef VC_EXTRALEAN
#define VC_EXTRALEAN
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <afxwin.h>         // MFC核心和标准组件
#include <afxext.h>         // MFC扩展
#include <afxcmn.h>         // MFC公共控件支持
#include <gdiplus.h>        // GDI+支持

#pragma comment(lib, "gdiplus.lib")

// 应用程序类
class CGeneratorApp : public CWinApp {
public:
    CGeneratorApp() noexcept;

    // 重载
    virtual BOOL InitInstance() override;
    virtual int ExitInstance() override;

    // GDI+初始化状态
    [[nodiscard]] bool IsGdiPlusInitialized() const noexcept { return m_gdiplusInitialized; }

private:
    ULONG_PTR m_gdiplusToken = 0;       // GDI+初始化令牌
    bool m_gdiplusInitialized = false;  // GDI+初始化标志

    // 初始化COM组件
    [[nodiscard]] bool InitializeCOM();

    // 初始化GDI+
    [[nodiscard]] bool InitializeGdiPlus();

    // 清理GDI+
    void ShutdownGdiPlus();
};

// 全局应用程序对象
extern CGeneratorApp theApp;
