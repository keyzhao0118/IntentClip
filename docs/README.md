# IntentClip 文档中心

本文档集是开发、测试、发布和商业决策的共同依据。文档中的“已决定”可直接进入实现；“待验证”必须通过原型、基准测试或政策复核后才能锁定。

## 阅读顺序

1. [产品章程](00-product-charter.md)：为什么做、为谁做、产品承诺与成功标准。
2. [产品需求文档](01-prd.md)：MVP 功能、用户故事、验收标准和非目标。
3. [体验设计规格](02-ux-spec.md)：核心流程、状态、权限提示和可访问性。
4. [技术架构](03-architecture.md)：模块、进程、数据流、推理与打包方案。
5. [隐私与安全](04-security-privacy.md)：数据分类、威胁模型与控制措施。
6. [商业化策略](05-commercialization.md)：免费边界、付费路径与指标。
7. [Microsoft Store 发布手册](06-store-release.md)：审核约束、素材、测试与提交流程。
8. [路线图与质量门禁](07-roadmap.md)：里程碑、Definition of Done 和风险台账。
9. [意图功能矩阵](08-intent-matrix.md)：候选功能、排序输入、执行模板与质量标准。
10. [功能执行测试文本](09-function-execution-test-texts.md)：可直接复制的五项功能与右键管理测试用例。
11. [开发记录](development/README.md)：逐里程碑记录实际代码、构建、调试与验证过程。

## 决策记录

- [ADR-0001：显式触发而非后台监听](decisions/ADR-0001-explicit-activation.md)
- [ADR-0002：Qt Quick 前端与动态链接 Qt](decisions/ADR-0002-qt-quick-and-linking.md)
- [ADR-0003：独立推理工作进程](decisions/ADR-0003-inference-worker.md)
- [ADR-0004：MSIX 与模型交付分离](decisions/ADR-0004-packaging-and-model-delivery.md)
- [ADR-0005：Ctrl+C+C 双复制触发手势](decisions/ADR-0005-copy-twice-gesture.md)
- [ADR-0006：使用 Qt Widgets 实现 UI](decisions/ADR-0006-qt-widgets-ui.md)

## 决策状态约定

- **已决定**：除非出现新证据，通过 ADR 变更，不在实现阶段临时改向。
- **待验证**：必须给出负责人、验证方法和截止里程碑。
- **开放问题**：会显著影响范围、成本或合规，未关闭前不能发布。

## 文档维护规则

- 产品行为变化必须同步 PRD 和体验规格。
- 模块边界、依赖许可或数据流变化必须同步架构及隐私文档。
- 每个候选模型都必须单独记录来源、版本、哈希、许可证和再分发结论。
- 每次商店提交前重新核验政策版本；本文首次核验基于 Microsoft Store Policies 7.19（2025-09-10 发布，2025-10-14 生效）。
- 法律与税务内容是工程风险控制基线，不替代针对发布主体所在司法辖区的专业意见。
