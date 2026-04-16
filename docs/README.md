# Docs Index

本目录用于存放项目设计、计划、验证和关键决策文档。

## 当前文档

- `architecture.md`：系统结构、数据流和模块边界
- `pin-plan.md`：引脚、定时器、串口和回环连接规划
- `roadmap.md`：项目里程碑与阶段退出条件
- `verification-log.md`：编译、烧录、串口和测量验证记录
- `decision-log.md`：关键技术选择和约束变化记录
- `error-analysis.md`：当前误差样例、原因说明和答辩表述
- `demo-script.md`：演示步骤、示例输出和现场回退方案
- `lcd-integration-notes.md`：Apollo 4342 RGBLCD 与 GT9xxx 触摸集成真值和联调结论

## 使用建议

- 确认硬件资源时，先更新 `pin-plan.md`
- 做出路线选择时，先更新 `decision-log.md`
- 每完成一个阶段，更新 `verification-log.md`
- 进入演示收尾阶段后，同步更新 `error-analysis.md` 与 `demo-script.md`
- 如果 README 中的项目描述发生变化，同步检查这里的文档是否仍然一致
