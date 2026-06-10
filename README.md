# GameLauncherGen - 32位游戏登录器生成器

[![Build Status](https://github.com/yourusername/GameLauncherGen/actions/workflows/build.yml/badge.svg)](https://github.com/yourusername/GameLauncherGen/actions/workflows/build.yml)

一套完整的 32位 (x86) 游戏登录器生成器解决方案，采用 **Generator + Launcher Template + Extension DLL + Common Library** 四层架构设计。

## 项目架构

```
GameLauncherGen/
├── src/
│   ├── common/              # 公共静态库 (.lib)
│   │   ├── XOR加解密引擎
│   │   ├── 多文件流打包器
│   │   └── CRC32校验算法
│   ├── launcher_template/   # 登录器模板 (.exe)
│   │   ├── 挂起创建进程 (CREATE_SUSPENDED)
│   │   ├── 远程线程DLL注入
│   │   └── 参数格式化启动
│   ├── extension_dll/       # 扩展DLL (.dll)
│   │   ├── 标准导出接口
│   │   └── Hook逻辑框架
│   └── generator/           # 生成器主程序 (MFC对话框)
│       ├── UI配置与预览
│       ├── 补丁一键打包
│       └── 资源固化与输出
└── .github/workflows/       # GitHub Actions CI/CD
```

## 技术栈

- **语言**: C++17
- **平台**: Windows x86 (32位)
- **框架**: Win32 API + MFC (静态链接)
- **构建**: CMake 3.20+ / MSVC 2019+
- **字符集**: Unicode (完美支持中文路径与空格)

## 核心特性

### 公共库 (Common)
- **XOR流式加解密**: 基于双LCG动态密钥流，支持大文件分块处理
- **多文件打包器**: 自定义PKG格式，含文件头/条目表/数据区三级结构
- **CRC32校验**: 标准IEEE 802.3查表法，支持流式增量计算

### 登录器模板 (Launcher Template)
- **安全启动**: CreateProcess + CREATE_SUSPENDED 挂起目标进程
- **远程注入**: VirtualAllocEx → WriteProcessMemory → CreateRemoteThread → ResumeThread
- **资源清理**: RAII句柄管理器，任何退出路径均自动释放资源
- **参数格式**: `ur;name=区服名;ip=IP;port=端口;ra=163.com`

### 扩展DLL (Extension DLL)
- **标准导出**: `DllInit` / `DllUnInit` / `DllGetVersion` 等6个接口
- **名字防粉碎**: 使用 .def 文件显式导出，确保跨编译器兼容
- **线程安全**: std::atomic 状态管理，CAS原子初始化

### 生成器 (Generator)
- **UI配置**: 预设样式、贴图替换、网络策略、更新选项
- **补丁打包**: 递归目录扫描 → XOR加密 → CRC校验 → .pkg输出
- **资源固化**: UpdateResource API / 自定义PE节 / 追加覆盖区 三种策略
- **一键输出**: 独立的32位成品登录器EXE

## 构建指南

### 环境要求
- Windows 10/11
- Visual Studio 2019 或更高版本 (含C++ MFC工作负载)
- CMake 3.20+

### 构建步骤

```powershell
# 克隆仓库
git clone https://github.com/yourusername/GameLauncherGen.git
cd GameLauncherGen

# 创建构建目录
mkdir build && cd build

# 配置 (强制32位)
cmake .. -A Win32

# 构建
cmake --build . --config Release --parallel

# 输出位置
# src/generator/Release/Generator.exe
# src/launcher_template/Release/LauncherTemplate.exe
# src/extension_dll/Release/ExtensionDLL.dll
```

### GitHub Actions 自动构建

项目已配置 CI/CD，每次推送到 `main` 或 `develop` 分支时自动构建：

```yaml
# .github/workflows/build.yml
# 构建产物将自动上传为 Artifact
```

## 使用说明

### 1. 启动生成器

运行 `Generator.exe`，配置以下参数：

- **UI样式**: 选择预设界面主题
- **贴图资源**: 背景图、按钮三态图 (BMP/PNG)
- **网络配置**: 主/备服务器列表URL、补丁下载地址
- **业务URL**: 官网、充值、客服、注册接口
- **更新策略**: 本地补丁/在线下载、强制CRC校验、覆盖策略
- **路径配置**: 游戏主程序相对路径、扩展DLL相对路径

### 2. 一键生成

点击"生成登录器"按钮：

1. 读取内置的 `LauncherTemplate.exe` 模板
2. 将所有配置数据、图片资源序列化
3. 通过 `UpdateResource` 嵌入到模板中
4. 输出独立的成品 `Launcher.exe`

### 3. 运行登录器

玩家运行生成的 `Launcher.exe`：

1. 解析固化的配置与资源
2. 拉取远程区服列表 (带本地缓存容灾)
3. 选择区服，点击"开始游戏"
4. 挂起启动 `client.exe`
5. 注入 `ExtensionDLL.dll`
6. 恢复主线程，游戏启动

## 安全设计

| 层级 | 措施 |
|------|------|
| **句柄泄漏防护** | SafeHandle/RemoteMemory RAII类，析构自动释放 |
| **内存清理** | SecureZeroMemory清零密钥，防止敏感数据残留 |
| **异常安全** | 移动语义 + 析构保证，任何退出路径均清理资源 |
| **进程防僵尸** | 注入失败时自动 TerminateProcess |
| **PE验证** | 生成器验证模板PE结构合法性 |

## 目录结构

```
GameLauncherGen/
├── CMakeLists.txt              # 根CMake配置
├── README.md                   # 本文件
├── .github/
│   └── workflows/
│       └── build.yml           # CI/CD配置
└── src/
    ├── common/                 # 公共静态库
    │   ├── include/            # 头文件
    │   ├── src/                # 源文件
    │   └── CMakeLists.txt
    ├── launcher_template/      # 登录器模板
    │   ├── include/
    │   ├── src/
    │   └── CMakeLists.txt
    ├── extension_dll/          # 扩展DLL
    │   ├── include/
    │   ├── src/
    │   ├── ExtensionDLL.def
    │   └── CMakeLists.txt
    └── generator/              # 生成器主程序
        ├── include/
        ├── src/
        ├── res/
        └── CMakeLists.txt
```

## 许可证

MIT License - 详见 LICENSE 文件

## 免责声明

本项目仅供学习研究使用，请勿用于非法用途。使用本项目产生的任何后果由使用者自行承担。
