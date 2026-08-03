# 技术架构

## 1. 架构目标

- 本地推理、默认离线，不建立对云服务的运行时依赖。
- UI 与推理解耦，模型失败或显存/内存异常不能拖垮主界面。
- 依赖、模型和提示模板可版本化、可复现、可审计。
- 首发保持单机、单用户、单模型，避免为未来功能提前引入服务器复杂度。

## 2. 技术基线

| 领域 | 选择 | 约束 |
|---|---|---|
| 语言 | C++20 | 禁止依赖未标准化语言扩展作为核心逻辑 |
| 构建 | CMake + Presets | Debug/Release、开发/商店打包必须可复现 |
| 依赖 | vcpkg manifest mode | 锁定 baseline；CI 不跟随浮动 latest |
| UI | Qt 6 Widgets | 界面使用 C++ Widget 类；视图与业务逻辑保持分层 |
| 推理 | llama.cpp C API 适配层 | 上层不直接依赖 llama.cpp 类型 |
| 模型 | GGUF，小型指令模型 | 具体权重经质量、性能、许可三道门禁后决定 |
| 分发 | MSIX | Store identity、签名、升级和干净卸载 |
| 平台 | Windows 10 22H2 / 11 x64 | ARM64 和旧版本在后续里程碑评估 |

Qt 采用动态链接，并随应用提供相应开源许可、归属信息和用户替换库的权利说明；若所用模块不在 LGPL 可用范围内，则购买商业许可或更换模块。llama.cpp 当前仓库采用 MIT，但模型权重、词表、提示模板和测试数据仍需分别审计。

## 3. 运行时结构

```text
IntentClip.exe（主进程）
├─ Application / UI
├─ Copy-Twice Gesture & Tray
├─ Clipboard Gateway
├─ Intent Orchestrator
├─ Settings & Local Data
├─ Model Manager
└─ IPC Client
        │ 仅本机命名管道/QLocalSocket
        ▼
IntentClipInference.exe（工作进程）
├─ IPC Server
├─ Prompt & Grammar Engine
├─ llama.cpp Adapter
└─ CPU / 可选硬件后端
```

工作进程按当前 Windows 用户启动，不安装 NT Service，不要求管理员权限。主进程负责所有用户可见决定；工作进程只接收受限请求并返回结构化分类或文本 token。

## 4. 模块边界

- `app-shell`：生命周期、单实例、托盘、路由和更新提示。
- `platform-win`：双复制手势、备选全局快捷键、剪贴板、DPAPI、系统能力探测。
- `intent-core`：输入规范化、候选意图 schema、策略与降级。
- `inference-client`：IPC、取消、超时、工作进程重启。
- `model-manager`：清单、磁盘空间、下载、哈希、原子安装与删除。
- `settings`：版本化配置迁移；敏感值不进普通配置文件。
- `licensing`：权益抽象；核心业务不直接调用 Store API。
- `diagnostics`：本地结构化事件；禁止记录原文和完整输出。

每个模块通过接口或 DTO 交流。Widget 不直接持有模型上下文、推理线程或系统钩子句柄；窗口只订阅应用状态并发送用户意图。

## 5. `Ctrl+C+C` 手势实现

`Ctrl+C+C` 不是标准的单一 Windows 热键，不能依赖 `RegisterHotKey` 直接表达。MVP 使用文档化的低级键盘钩子观察 Ctrl 与 C 的时序，并保留普通组合键作为兼容备选。

- 只识别“每次 C 按下时 Ctrl 均处于按下状态，且两次 C key-down 之间存在 key-up”的序列。
- 默认双击窗口为 500 ms，允许在设置中调整 300–800 ms。
- 不阻止、不修改、不重新注入任何按键；两个 `Ctrl+C` 都继续交给前台应用。
- 忽略按键自动重复与标记为 injected 的事件，避免长按和自动化工具造成循环触发。
- 观察器只保留 Ctrl/C 状态和单调时钟，不记录其他按键、应用文本或完整键盘序列。
- 第二次 C 后读取 `GetClipboardSequenceNumber`，等待剪贴板更新稳定，再以短间隔有限重试读取文本。
- 手势被暂停、应用退出或会话锁定时立即卸载钩子。

低级钩子回调只更新轻量状态并向 Qt 事件循环投递消息；不得在回调中读取剪贴板、操作 UI、分配大块内存或调用模型。回调内的 Ctrl 状态由收到的按键事件维护，不依赖 `GetAsyncKeyState`。相关接口依据见 Windows 官方文档：[LowLevelKeyboardProc](https://learn.microsoft.com/en-us/windows/win32/winmsg/lowlevelkeyboardproc)、[KBDLLHOOKSTRUCT](https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-kbdllhookstruct)、[GetClipboardSequenceNumber](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getclipboardsequencenumber) 和备选的 [RegisterHotKey](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-registerhotkey)。

若低级钩子安装失败、安全软件限制或特定远程桌面环境不兼容，产品自动降级到用户配置的普通全局快捷键并显示一次性说明。

## 6. 推理流水线

1. 输入网关只接收纯文本，进行 Unicode 规范化、长度检查和敏感模式提示。
2. 分类提示使用固定版本，并通过 llama.cpp grammar/受约束解码生成 JSON。
3. 校验 JSON schema；失败时只允许一次确定性修复或降级到固定动作列表。
4. 用户选定动作后，生成提示由模板、用户文本和有限选项组成。
5. token 通过 IPC 流式返回；取消信号可中止推理。
6. 完成后立即释放请求文本；只有用户显式启用历史才进入持久层。

分类与生成使用不同的采样配置。分类优先确定性和低延迟，生成允许有限温度。所有默认参数进入版本控制和基准报告，禁止散落在 UI 代码中。

## 7. 模型选择门禁

不在架构文档中凭主观印象指定某个权重。候选模型必须同时通过：

- **许可**：允许商业使用、允许所选交付方式；模型卡和版权声明可随产品展示。
- **质量**：在自建中英意图集上的 Top-1、Top-3、schema 合法率和高风险误判达到阈值。
- **性能**：在最低与推荐硬件上测试首次加载、首 token、tokens/s、峰值内存和安装体积。
- **产品性**：中文能力稳定，能遵守 JSON grammar，输出长度可控。

候选档位建议从 0.5B–1.5B 的 Q4/Q5 GGUF 开始，最终选择以测量结果为准。模型清单至少包含 `id`、版本、来源 URL、许可证、文件大小、SHA-256、最低内存和默认上下文。

## 8. 模型交付

- 应用包不内置主模型；首次运行后由用户选择下载，降低安装包体积和模型换代耦合。
- 下载源必须 HTTPS、版本化、支持续传；临时文件不参与加载。
- 下载结束后校验 SHA-256，再原子移动到正式目录。
- 模型是数据，不作为动态代码执行；禁止下载 DLL、脚本或未声明插件来改变审核后的核心功能。
- 商店审核必须能完成下载并测试；提交说明提供模型大小、等待时间和操作路径。
- 若 CDN 成本或审核可靠性不可接受，发布候选阶段重新评估“商店可选包/内置微型模型”。

## 9. 性能与线程

- UI 主线程不做文件哈希、网络下载、模型加载或推理。
- 每个工作进程同一时间只执行一个生成任务；分类可复用已加载上下文，但不得和生成竞争导致卡顿。
- Content 文本变化会递增请求 generation；主进程和工作进程都丢弃 generation 已过期的分类结果和 token。
- Intent 项看似可勾选，但调度器只维护一个 active intent；新选择先发送取消，再启动新生成。
- 空闲卸载策略可配置，默认优先保持模型热态；进入电池节能模式时降低线程数。
- 启动先展示壳层，再异步检查模型；不得用模型加载阻塞首屏。
- 所有性能指标按冷启动、热启动、短文本、长文本分层记录。

## 10. 构建与目录建议

```text
/
├─ app/                 # 主程序与 Qt Widgets 界面
├─ worker/              # 推理工作进程
├─ libs/                # 领域与平台库
├─ tests/               # 单元、集成、基准数据
├─ packaging/msix/      # manifest、资源、打包脚本
├─ cmake/               # 公共 CMake 模块
├─ docs/                # 决策与产品文档
├─ CMakeLists.txt
├─ CMakePresets.json
└─ vcpkg.json
```

## 11. 测试策略

- 单元：schema、配置迁移、输入边界、模型清单、许可清单解析。
- 契约：主进程与工作进程 IPC 版本兼容、取消和崩溃恢复。
- 黄金集：意图分类、模板回归、危险输出和多语言边界。
- 性能：固定硬件画像与固定模型哈希，防止版本升级悄然退化。
- 安装：全新安装、覆盖升级、降级拒绝、卸载、无网首次运行。
- UI：键盘、DPI、主题、高对比度、屏幕阅读器和多显示器。
- 手势：不同应用复制延迟、长按 C、按键自动重复、远程桌面、会话锁定、剪贴板占用和快速三击。

## 12. 依赖升级原则

依赖升级单独提交，包含变更日志、许可差异、安全影响和回归结果。llama.cpp 更新频繁，发布分支必须固定到已验证 commit，不直接追踪主分支。Qt 与 MSVC runtime 的部署文件由发布产物清单校验，不靠开发机环境偶然补齐。
