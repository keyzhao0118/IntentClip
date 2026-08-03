# 0001：CMake、经典模式 vcpkg 与 Qt 工程启动

- 日期：2026-08-03
- 状态：已完成
- 目标：使用 CMake 创建最小 Qt Widgets 工程，通过经典模式 vcpkg 提供 Qt，并弹出一个 Qt 窗口。

## 环境基线

| 工具 | 本次环境 |
|---|---|
| CMake | 3.31.12 |
| vcpkg | `D:\\vcpkg`，classic mode |
| triplet | `x64-windows` |
| Qt | 6.11.1，使用 `qtbase[widgets]` |
| 编译器 | Visual Studio 18 Community C++ 工具链 |
| 生成器 | Ninja |

本项目刻意不创建 `vcpkg.json`。CMake 通过 `VCPKG_ROOT` 指向的 `scripts/buildsystems/vcpkg.cmake` 查找经典模式下已经安装的库。

## 本次新增文件

| 文件 | 责任 |
|---|---|
| `CMakeLists.txt` | 定义 C++20、Qt Widgets 可执行程序和链接关系 |
| `CMakePresets.json` | 固定 Debug/Release、Ninja、vcpkg toolchain 和 x64 triplet |
| `app/main.cpp` | 创建 `QApplication`，显示主窗口并进入事件循环 |
| `app/main_window.h/.cpp` | 定义最小 `QMainWindow` 页面、布局和样式 |
| `.gitignore` | 排除构建、IDE 和运行时产物 |
| `docs/decisions/ADR-0006-qt-widgets-ui.md` | 记录从 Qt Quick 改为 Qt Widgets 的正式决策 |

## 关键实现

- `find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets)`：只引入当前页面所需的 Qt Widgets。
- `qt_add_executable`：创建 Windows GUI 可执行程序。
- `QApplication`：管理 Windows GUI 事件循环。
- `MainWindow`：使用 `QVBoxLayout` 和 `QLabel` 构建最小页面；对象的父子关系负责自动释放控件。
- `WIN32_EXECUTABLE TRUE`：Release/Windows GUI 程序不附带控制台窗口。
- `Qt6::QWindowsIntegrationPlugin`：构建后把 `qwindows[d].dll` 放入可执行文件旁的 `platforms` 目录，保证当前经典 vcpkg 环境下 Demo 可独立启动。

## 设计调整记录

最初创建文件时按早期架构文档采用了 Qt Quick/QML。进入构建前，项目维护者明确要求 UI 使用 Qt Widgets，以匹配自身经验并降低后续维护成本。因此本里程碑完成了以下纠正：

- `Qt6::Quick` 改为 `Qt6::Widgets`。
- 删除 QML 模块和 `Main.qml`。
- `QGuiApplication + QQmlApplicationEngine` 改为 `QApplication + MainWindow`。
- 新增 ADR-0006，并将 ADR-0002 标记为已废弃。

这次调整发生在首次构建前，没有遗留 QML 运行时或构建产物。

## 使用方式

在 Visual Studio Developer PowerShell 中设置 `VCPKG_ROOT` 后执行：

```powershell
$env:VCPKG_ROOT = "D:\vcpkg"
cmake --preset windows-debug
cmake --build --preset windows-debug
./out/build/windows-debug/IntentClip.exe
```

若从普通 PowerShell 主动加载 Visual Studio 环境，开发环境脚本会改变当前目录，必须显式切回项目：

```powershell
. "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\Launch-VsDevShell.ps1" -Arch amd64 -HostArch amd64
Set-Location "D:\IntentClip"
$env:VCPKG_ROOT = "D:\vcpkg"
cmake --preset windows-debug
cmake --build --preset windows-debug
```

## 构建与调试记录

### 1. 受限环境不能启动 Ninja

- 现象：首次配置在运行 `ninja.exe --version` 时返回 `operation not permitted`，随后提示未设置 C++ 编译器。
- 原因：执行环境限制阻止了构建工具启动，不是 CMakeLists 或 Qt 查找错误。
- 处理：在允许启动本机编译工具的环境中使用完全相同的 preset 重试。
- 验证：Ninja 与 MSVC 19.51.36248.0 被正确识别。

### 2. Visual Studio 开发脚本改变工作目录

- 现象：CMake 从 `C:/Users/keyzhao/source/repos` 查找 `CMakePresets.json`。
- 原因：`Launch-VsDevShell.ps1` 启动后切换到了 Visual Studio 默认源码目录。
- 处理：加载脚本后执行 `Set-Location "D:\IntentClip"`。
- 验证：CMake 从项目根目录读取 `windows-debug` preset。

### 3. 清理缓存验证 vcpkg toolchain

- 现象：一次受中断缓存的配置产生 `CMAKE_TOOLCHAIN_FILE` 未使用警告。
- 处理：执行 `cmake --fresh --preset windows-debug`，从空缓存重新配置。
- 验证：Qt 从 `D:/vcpkg/installed/x64-windows` 找到，配置和生成无该警告。

### 4. 首次运行缺少 Qt Windows 平台插件

- 现象：程序弹出 Visual C++ Runtime 对话框，错误为 `no Qt platform plugin could be initialized`。
- 原因：vcpkg 的 app-local DLL 部署复制了 Qt6 Widgets/Gui/Core 等 DLL，但没有创建 `platforms/qwindowsd.dll`。
- 初次修复尝试：把 `QWindowsIntegrationPlugin` 作为顶层 Qt component 查找；失败，因为 vcpkg 将插件 config 放在 `share/Qt6Gui`，而非独立的顶层 component 目录。
- 最终修复：先查找 `Qt6::Widgets`，再通过 `${Qt6Gui_DIR}` 查找 `Qt6QWindowsIntegrationPlugin`；使用导入目标和生成器表达式构建后复制插件，未硬编码具体 Debug/Release 文件名。
- 验证：`out/build/windows-debug/platforms/qwindowsd.dll` 存在，程序成功进入主窗口。

### 5. `windeployqt` 评估

- 结论：Qt 上游的 `windeployqt` 比单独复制平台插件更完整，正式部署应优先使用它或 Qt CMake deploy script。
- 当前限制：本机经典 vcpkg 的 `qtbase` 未启用 `windeployqt` feature。尝试添加时，vcpkg 计划重建 30 多个已经安装的 Qt 包并要求 `--recurse`。
- 决策：不强行修改共享的全局 vcpkg 环境。Demo 阶段保留最小平台插件部署；建立项目专用的干净 vcpkg 环境时，从一开始安装 `qtbase[widgets,windeployqt]`，再替换当前部署命令。
- 结果：vcpkg 在执行任何重建前退出，现有全局安装未被修改。

## 验证结果

- `cmake --fresh --preset windows-debug`：通过。
- `cmake --build --preset windows-debug`：通过，共编译/链接 5 个步骤；修复后增量构建通过。
- 可执行文件：`out/build/windows-debug/IntentClip.exe`。
- 运行状态：进程保持运行，主窗口标题为 `IntentClip · 拾意`。
- 页面内容：显示标题、标语和 `Demo ready`；Windows 可访问性树能识别三个文本控件。
- 当前窗口由本次验证保持打开，便于开发者直接查看。
- `windows-release` preset 已定义，但本里程碑未构建验证；正式打包前单独验证 Release 与部署流程。

## 本里程碑未实现

托盘、`Ctrl+C+C`、剪贴板、Content/Intent/Result 面板和本地模型均未开始。本次只证明 CMake、经典 vcpkg、Qt Widgets、编译和窗口启动链路可用。
