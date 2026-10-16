# soft-judge-reference · 协议级参考实现

在**不使用专有裁判客户端 / 官方考题包（`examLib.zip`）**的前提下，仅凭
[`docs/communication-protocol.md`](../../docs/communication-protocol.md) 描述的线协议，
驱动参赛端（本仓库 `mock-client/` 或任何兼容实现）完成一次**可复现的端到端闭环**：

```
START(02 30 03) → BEGIN(02 31 03) → 按时间戳下发 token → 接收并解码参赛端答案帧
```

## 角色与端口

| 端口 | 监听方 | 连接方 | 内容 |
|---|---|---|---|
| 7001 | 参赛端 | 本脚本 | 指令帧（定长 3 字节）|
| 7002 | 参赛端 | 本脚本 | token 帧（定长 12 字节）|
| 7000 | 本脚本 | 参赛端 | 就绪 `02 32 03` / 结束 `02 35 03` |
| 7003 | 本脚本 | 参赛端 | 答案帧（变长，含裁剪图）|

## 用法

```bash
# 1) 先构建并启动参赛端（其 config.yml 需指向本地考题数据）
cd ../../mock-client/od && qmake MockClient.pro && make
./MockClient            # 交互输入 y / 0，进入 "Waiting for start signal......"

# 2) 运行本参考实现（扮演服务器+裁判下发端）
python3 judge_server.py --token-csv /path/to/token.csv
```

`token.csv` 需含表头 `timestamp,type,token`。运行后应看到：
参赛端打印 `Receive start signal.`；本脚本收到就绪帧 `023203` 并解析出若干答案帧
（timestamp / box / lon-lat / 图像尺寸等全字段），末尾打印 `闭环贯通` 判定。

## 参数

见 `python3 judge_server.py --help`。端口、目标答案数、静默模式均可配置。

> 说明：本脚本只实现线协议与答案帧解码，用于**验证链路连通性与协议正确性**，
> 不做官方评分（官方计分依赖真值坐标系，见 `docs/scoring-algorithm.md`）。
> 官方多端联调（含独立评判应用）流程见仓库根 README 与本地归档 `QUICKSTART.md`。
