# 三端通信协议规范

## 1. 网络拓扑

```
┌──────────────┐                    ┌──────────────┐                ┌──────────────┐
│              │                    │              │                │              │
│  比赛服务器   │◄─────── 6001 ────►│  裁判客户端   │◄──7000/7003───►│  参赛软件     │
│  (云端)      │      TCP          │  (Java)      │      TCP       │  (C++/Python)│
│              │                    │              │                │              │
└──────────────┘                    │              │                └──────────────┘
                                    │              │
                                    └──6002/6003───┘
                                         TCP
                                          │
                                          ▼
                                    ┌──────────────┐
                                    │  评判应用     │
                                    │ (C++/Qt)     │
                                    └──────────────┘
```

## 2. 端口清单

| 端口 | 归属方 | 方向 | 用途 |
|------|--------|------|------|
| **6001** | 裁判客户端 → 服务器 | Server-Client | 双向：服务器下发考题、上传分数 |
| **6002** | 裁判客户端 ↔ 评判应用 | 内部 IPC | 评判指令下发 |
| **6003** | 评判应用 → 裁判客户端 | 内部 IPC | 上传实时分数 |
| **7000** | 参赛软件 → 裁判客户端 | Client-Judge | 参赛软件就绪通知 |
| **7001** | 裁判客户端 → 参赛软件 | Judge-Client | 指令转发（考题路径、开始/结束）|
| **7002** | 裁判客户端 → 参赛软件 | Judge-Client | **Token 按时间戳下发** |
| **7003** | 参赛软件 → 裁判客户端 | Client-Judge | **答案上传** |

## 3. 消息格式

### 3.1 服务器指令（端口 6001）

**Server → Judge Client**：
```
<command>[,<param>]\n
```

命令枚举：
| 命令 | 参数 | 说明 |
|------|------|------|
| `START` | 考题路径 | 开始答题 |
| `STOP` | — | 立即结束 |
| `PAUSE` | — | 暂停计时 |
| `RESUME` | — | 继续计时 |
| `LOAD` | 题目 ID | 预加载题目 |

**Judge Client → Server**：
```
SCORE,<exam_id>,<current_score>,<max_score>\n
```

### 3.2 Token 下发（端口 7002）

**Judge Client → Competitor**：
```
<timestamp>,<sensor_type>,<token>\n
```

示例：
```
1683356400123,hdl32,7
1683356400123,gps,3
1683356400123,camera_ts,5
```

**下发频率**：默认 100ms 一个批次（覆盖上一时间窗的所有 token）

**sensor_type 枚举**：
- `camera_ts`：相机时间戳 CSV
- `gps` / `imu`：GPS/IMU CSV
- `gear` / `odom` / `steer`：CAN 总线 CSV
- `hdl32`：多线激光 PCAP
- `urg`：单线激光 LMS
- `gt_od` / `gt_global` / `gt_mp`：真值（不同任务）

### 3.3 答案上传（端口 7003）

**Competitor → Judge Client**：

**OD 赛道格式**：
```
<timestamp>|<json_detections>\n
```

其中 `json_detections`：
```json
[
  {"x1":100, "y1":200, "x2":180, "y2":260, "class":"car", "conf":0.92},
  {"x1":220, "y1":150, "x2":260, "y2":240, "class":"pedestrian", "conf":0.85}
]
```

**SU 赛道格式**（本地定位）：
```
<timestamp>|<x>,<y>,<z>,<qx>,<qy>,<qz>,<qw>\n
```

**SU 赛道格式**（场景理解）：
```
<timestamp>|<semantic_grid_id>\n
```

### 3.4 心跳消息（可选，端口 7000）

```
HEARTBEAT,<client_id>,<seq>\n
```

配置：
```properties
client.soft.heartbeat.enable=true
client.soft.heartbeat.interval=10000  # 10s 一次
client.soft.heartbeat.death.count=3   # 连续 3 次失败判定离线
```

## 4. 连接管理

### 4.1 生命周期

```
[TCP 连接建立]
     ↓
[Client 发送 HELLO]
     ↓
[Judge 校验并回 ACK]
     ↓
[数据/指令/Token 消息交换]
     ↓
[STOP 指令]
     ↓
[TCP 关闭（FIN 四次挥手）]
```

### 4.2 错误处理

| 情况 | 行为 |
|------|------|
| TCP 断开 | 立即尝试重连（最多 3 次，间隔 5s） |
| 消息格式错 | 记录异常，丢弃该条，不影响后续 |
| 未知命令 | 返回 `ERR,UNKNOWN_CMD` 应答 |
| 超时未响应 | 判定对方离线，触发降级 |

## 5. Java 端 Socket 模型

```java
// 简化示例
public class JudgeClient {
    // 端口 6001：接收服务器指令
    private ServerSocket serverCmdSocket;
    // 端口 7000：接收参赛软件就绪
    private ServerSocket softCmdSocket;
    // 端口 7003：接收答案
    private ServerSocket answerSocket;

    // 转发到参赛软件（客户端角色）
    private Socket softCmdForward;   // 7001
    private Socket softTokenForward; // 7002

    public void start() {
        ExecutorService pool = Executors.newFixedThreadPool(5);
        pool.submit(this::listenServerCommands);
        pool.submit(this::listenSoftReady);
        pool.submit(this::listenAnswers);
        pool.submit(this::tokenSender);
        pool.submit(this::logFlusher);
    }
}
```

**每个端口独立线程监听**：
- 端口 6001 → pool-1-thread-1
- 端口 7000 → pool-2-thread-1
- 端口 7003 → pool-3-thread-1（答案高频，独立线程避免阻塞）
- Token 下发 → 独立线程（按 FixTimeSendStrategy 定时批量）
- 日志异步刷盘 → 独立线程

## 6. C++ 端 Qt TCP 模型

```cpp
// mock-client/tcpserver.h
class TCPServer : public QObject {
    Q_OBJECT
public:
    TCPServer(quint16 port, int queue_size);
    void start();
signals:
    void newMessage(const QByteArray&);
private slots:
    void onNewConnection();
    void onReadyRead();
};

// 使用信号槽，异步非阻塞
connect(server, &TCPServer::newMessage, this, &Handler::onMessage);
```

## 7. 时序示例

```
t=0     Server ─── START,EXAM-2023-001 ───► Judge
t=10ms  Judge  ─── LOAD:EXAM-2023-001 ────► Competitor (7001)
t=500ms Competitor ─── READY ─────────────► Judge (7000)
t=1s    Judge  ─── START ─────────────────► Competitor (7001)
        Judge 开始计时（30 分钟）
t=1.1s  Judge  ─── 1683356400123,hdl32,7 ─► Competitor (7002)
        Judge  ─── 1683356400123,gps,3 ───► Competitor (7002)
        ... (每 100ms 一批)

t=2.0s  Competitor ─── 1683356400123|[{...}] ──► Judge (7003)  [答案 1]
t=2.05s Judge → Judge App: 计算 IoU，得分 +1.2
t=2.1s  Judge ─── SCORE,EXAM-2023-001,1.2,500 ──► Server (6001)

... 循环 ...

t=30min Judge  ─── STOP ─────────────────► Competitor (7001)
        Judge  ─── STOP ─────────────────► Server (6001)
```

## 8. 已知限制

1. **无消息序号**：Judge 重启会丢失未上传的答案
2. **无 TLS**：明文传输，局域网嗅探可见
3. **无幂等**：同一 timestamp 的答案可能被重复计分
4. **无背压**：高吞吐时答案队列可能堆积

## 9. 改进方向

- **协议层**：改用 gRPC + Protobuf（跨语言 + 强类型）
- **可靠性**：加入消息 ACK + 序号 + 去重
- **安全性**：TLS 双向认证
- **可观测**：每条消息带 trace_id，接入 OpenTelemetry
