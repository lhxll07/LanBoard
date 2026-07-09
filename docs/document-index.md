# 项目文档索引

本文档说明 `docs/` 下计划、报告和测试记录的关系。阶段性文档保留历史状态，不直接覆盖；新的同步或验证轮次使用新的日期文件。

## 命名规则

- 长期维护的规范文件不加日期，例如 `network-messages.md`。
- 阶段性计划、报告和测试记录统一使用日期后缀：`主题-文档类型-YYYYMMDD.md`。
- `workplan` 表示执行前计划。
- `work-report` 表示实现完成后的工作报告。
- `test-results` 表示验证记录。

## 当前主线

截至 2026-07-09，当前需要审阅的是：

- [main-sync-work-report-20260709.md](./main-sync-work-report-20260709.md)

该报告记录 `refactor/network-internals` 已再次同步最新 `origin/main` 后的冲突处理、自动验证结果和剩余风险。本轮合并前计划为：

- [main-sync-workplan-20260709.md](./main-sync-workplan-20260709.md)

## 阅读顺序

1. [network-refactor-workplan-20260705.md](./network-refactor-workplan-20260705.md)
   - 网络内部拆分的初始方案。
   - 说明为什么不直接合并旧网络重构分支。

2. [network-refactor-work-report-20260705.md](./network-refactor-work-report-20260705.md)
   - 网络拆分完成后的工作报告。
   - 记录 `LineJsonProtocol`、`NetworkAddressUtils`、`RoomDiscoveryService` 的实际改动。

3. [pre-main-merge-workplan-20260705.md](./pre-main-merge-workplan-20260705.md)
   - 第一次并入 `main` 前的验证计划。
   - 重点是构建、局域网发现、三款回合制游戏联机和本地在线服务端。

4. [pre-main-merge-test-results-20260705.md](./pre-main-merge-test-results-20260705.md)
   - 第一次并入前验证记录。
   - 记录桌面构建、自动测试、GUI 多实例和本地服务端测试结果。

5. [main-sync-workplan-20260706.md](./main-sync-workplan-20260706.md)
   - 2026-07-06 同步最新 `main` 的计划。
   - 处理 `unify-game-runtime`、Survivor 入口和网络拆分之间的冲突。

6. [main-sync-test-results-20260706.md](./main-sync-test-results-20260706.md)
   - 2026-07-06 同步 `main` 后的验证记录。
   - 记录冲突处理、构建、E2E 和 GUI 多实例复查。

7. [main-sync-workplan-20260707.md](./main-sync-workplan-20260707.md)
   - 2026-07-07 当前合并前计划。
   - 处理最新 `origin/main` 继续前进后产生的新分叉和冲突。

8. [main-sync-test-results-20260707.md](./main-sync-test-results-20260707.md)
   - 2026-07-07 同步 `main` 后的验证记录。
   - 记录 ENet 主线整合、构建、局域网发现、控制层 E2E 和本地在线服务端 ENet E2E 结果。

9. [main-sync-work-report-20260708.md](./main-sync-work-report-20260708.md)
   - 2026-07-08 再次同步最新 `main` 后的工作报告。
   - 记录吸收 Survivor 主线更新、处理 `NetworkManager` 冲突、修复连接中误判掉线和验证结果。

10. [main-sync-workplan-20260709.md](./main-sync-workplan-20260709.md)
    - 2026-07-09 当前合并前计划。
    - 记录当前分支相对 `origin/main` 再次落后的事实、预计冲突、未跟踪文件处理建议和同步验证步骤。

11. [main-sync-work-report-20260709.md](./main-sync-work-report-20260709.md)
    - 2026-07-09 同步最新 `main` 后的工作报告。
    - 记录处理 `README.md` 和 `AppController` 冲突、保留网络拆分与主线 Survivor 更新、构建和自动验证结果。

## 长期维护文档

- [network-messages.md](./network-messages.md)
  - 当前网络消息表。
  - 新增或修改联网消息时应同步更新。

## 维护约定

- 不覆盖旧的日期文档，除非只是修正错别字、断链或明显事实错误。
- 新一轮同步主线、合并前验证或发布前验证，应新增对应日期文件。
- 新增日期文档后，同步更新本索引和 `README.md` 的相关文档列表。
- `AGENTS.md` 当前是本地未跟踪文件，不作为项目文档索引的一部分。
