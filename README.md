# IntentClip · 拾意

> 拾取文本，洞悉心意。

IntentClip 是一款面向 Windows 的本地 AI 文本处理助手。用户通过快捷键主动拾取当前选中文本，面板立即展示可配置功能并自动执行默认项，例如总结、改写、解释、提取或生成回复。

项目当前处于产品与技术设计阶段。开发前的决策基线、范围、架构、隐私、商业化及 Microsoft Store 上架要求统一维护在 [docs/README.md](docs/README.md) 中。

## 当前技术基线

- C++20、CMake、vcpkg manifest mode
- Qt 6 Widgets，界面与业务均使用可直接维护的 C++ 类组织
- llama.cpp 作为可替换的本地推理后端
- Windows 10 22H2 / Windows 11，首发 x64
- MSIX 作为 Microsoft Store 首选分发格式

具体版本不在本文写死，由 vcpkg baseline、CMake preset 与发布清单共同锁定。

## 当前状态

- [x] 产品立项与 MVP 边界
- [x] 架构、隐私和商店合规基线
- [x] 商业化与发布策略
- [ ] 交互原型与可用性验证
- [ ] 模型基准测试与权重许可确认
- [ ] 工程骨架与首个端到端闭环
