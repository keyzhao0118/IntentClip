# Microsoft Store 发布手册

## 1. 发布账户与身份

- 尽早在 Partner Center 查询并保留产品名称，确认 `IntentClip`、`拾意`及图形标识不存在冲突。
- 独立开发者可使用个人账户；若发布者名称会让消费者理解为企业实体，或未来主要功能要求财务信息，应使用公司账户并完成验证。
- 商标、图标、字体、截图、模型名称和第三方品牌均保留授权证据。

## 2. 包格式决策

首选 MSIX：获得 Store identity、签名、可靠升级和干净卸载。若未来改用未打包的 MSI/EXE URL 提交，当前政策要求所有 PE 文件具有受信任 CA 代码签名、提交不可变的版本化 URL、完整离线安装包以及静默安装；因此它不是 MVP 默认方案。

MSIX manifest 仅声明实际使用能力。应用不安装驱动或 NT Service，不要求管理员权限，不修改默认应用、任务栏固定或系统设置。

## 3. 与本产品直接相关的政策控制

| 政策主题 | 产品措施 |
|---|---|
| 独特价值与准确描述 | 首屏和商店页明确“主动拾取、本地识别、需本地模型”；不宣称通用智能 |
| 安全与动态代码 | 模型作为经哈希验证的数据；不下载 DLL、脚本或插件改变核心功能 |
| 可测试性 | 审核说明包含首次模型准备步骤、时间、体积、测试文本和断网边界 |
| 可用性 | 低配检测、模型错误恢复、UI 不被推理阻塞、异常可优雅退出 |
| 个人信息 | 提供公开隐私政策；产品内可访问；说明文本、历史、反馈和权益数据 |
| Capabilities | 只声明与快捷键、剪贴板、网络下载等真实功能相符的最小能力 |
| 财务交易 | 首个 Pro 使用 Store 内购；价格、试用与权益范围在页面和应用中一致 |
| 生成式 AI | 商店元数据与 Partner Center 均披露；提供不当内容反馈和处理流程 |
| 年龄分级 | 如实完成 IARC；模型可能生成的动态内容纳入评估和防护 |
| 知识产权 | 应用、模型、测试数据、字体和素材全部进入第三方清单 |

## 4. 商店页素材

- 名称：`IntentClip · 拾意`，不要在标题堆砌关键词。
- 首句：说明“Windows 本地 AI 文本意图助手，需要下载本地模型”。
- 功能：只写已发布能力；明确最低/推荐内存、磁盘和架构。
- AI 披露：说明结果由本地生成式 AI 产生，可能不准确，不用于高风险专业决策。
- 隐私：突出默认不上传文本，并链接完整政策；避免绝对化宣传。
- 截图：至少覆盖拾取浮层、候选动作、结果、模型管理和隐私设置；不得用未实现概念图。
- 支持：公开支持页面、反馈邮箱/表单、隐私政策和版本说明。

## 5. 提交前检查清单

### 包与安装

- [ ] Release 构建，版本号四段一致，Publisher 与 Partner Center identity 一致。
- [ ] Windows App Certification Kit 通过。
- [ ] 全新安装、升级、卸载、重装、标准用户、多显示器测试通过。
- [ ] 所有 Qt DLL、平台插件、VC runtime、QML 模块和许可文件齐全。
- [ ] 安装目录无模型缓存、调试日志、个人路径或测试密钥。

### 产品与 AI

- [ ] 无模型、下载中、校验失败、内存不足、断网和推理崩溃均可恢复。
- [ ] 审核人员无需登录即可验证核心功能。
- [ ] 生成式 AI 已在元数据和提交表单中披露。
- [ ] 不当内容反馈路径可用，且发送前不默认包含用户文本。
- [ ] 高风险类别和不支持内容有合适提示。

### 法务与商业

- [ ] 隐私政策 URL 和支持 URL 可公开访问并使用 HTTPS。
- [ ] SBOM、第三方 notices、Qt 许可履约材料、llama.cpp MIT notice 齐全。
- [ ] 模型权重的商业使用、修改与再分发结论有证据。
- [ ] IARC 问卷与动态 AI 能力相符。
- [ ] 价格、试用、权益、退款和恢复购买流程一致。

## 6. 审核备注模板要点

审核备注应说明：这是 Win32/MSIX 本地 AI 应用；无需账号；首次需下载约多少 MB 的模型；测试快捷键和示例文本；所有推理在本机；网络请求用途；如何进入反馈、隐私和清除页面；最低硬件；若审核环境受限，可使用哪个内置示例完成测试。

## 7. 发布节奏

先用私有/受控分发验证安装包，再小比例公开发布，观察崩溃、模型下载失败和审核反馈。应用更新与模型清单更新分开；模型下架或许可变化时可停止新下载，但不能破坏用户已有合法安装。每次提交前记录政策页面的版本与日期。

## 8. 官方依据（首次核验：2026-08-03）

- [Microsoft Store Policies 7.19](https://learn.microsoft.com/en-us/windows/apps/publish/store-policies)
- [Publish Windows apps and games](https://learn.microsoft.com/en-us/windows/apps/publish/)
- [MSIX documentation](https://learn.microsoft.com/en-us/windows/msix/)
- [Windows App Certification Kit](https://learn.microsoft.com/en-us/windows/uwp/debug-test-perf/windows-app-certification-kit)
- [llama.cpp 官方仓库与 MIT LICENSE](https://github.com/ggml-org/llama.cpp/blob/master/LICENSE)
- [Qt Base 官方仓库许可清单](https://github.com/qt/qtbase/tree/dev/LICENSES)

政策会变化；正式提交时必须重新核验，而不是仅依赖本文。
