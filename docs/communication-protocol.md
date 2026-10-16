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

## 2. 端口清单（经真实运行时核验）

> 说明：以下方向为**实测确认**。裁判客户端在 `6001/7000/7003` 上作为**服务端**监听；参赛软件在 `7001/7002` 上作为**服务端**监听；两端互为客户端主动连接对方。

| 端口 | 监听方(Server) | 连接方(Client) | 用途 |
|------|----------------|----------------|------|
| **6001** | 裁判客户端 | 比赛服务器 | 服务器下发考题控制指令（**文本、按行、竖线分隔**，见 3.1）|
| **7000** | 裁判客户端 | 参赛软件 | 参赛软件就绪/结束通知（二进制帧，见 3.2）|
| **7003** | 裁判客户端 | 参赛软件 | 参赛软件上传答案（二进制帧，见 3.4）|
| **7001** | 参赛软件 | 裁判客户端 | 下发考试指令：开始准备 / 开始 / 结束（二进制帧，见 3.2）|
| **7002** | 参赛软件 | 裁判客户端 | 按时间戳下发 token（二进制帧，见 3.3）|
| **6000** | 比赛服务器 | 裁判客户端 | 裁判客户端回传答案/分数（`beginExam` 指令中的 `port` 指定）|

> 评判应用（独立评分器）与裁判客户端之间另有一组本机 IPC 端口，属可选的分离部署；单机自测模式下裁判客户端可依据预载真值自行计分，评判应用不在此闭环内。

## 3. 消息格式

### 3.1 服务器控制指令（端口 6001，文本）

**Server → Judge Client**：**按行**读取（UTF-8，`\n` 结尾），每行以竖线 `|` 分隔命令名与参数（参数为 JSON 对象，宽松语法）：
```
<command>|<jsonArgs>\n
```
命令枚举（实测确认）：
| 命令 | 参数示例 | 说明 |
|------|----------|------|
| `dispathExam` | `{examLib:1, exam:.., examGroup:.., gameOrg:.., port:.., examType:..}` | 下发考题元数据（题型等）|
| `downfnsh` | `{gameOrg:18, ip:"..", exam:, port:5001}` | 考题下载完成通知 |
| `readyExam` | `{}` | 准备考试 → 裁判端在 `7001` 下发**开始准备帧** `0x02 0x30 0x03` |
| `beginExam` | `{examLib:1, port:6000}` | 开始考试 → 连接 `server.ip:port` 上传答案；在 `7001` 下发**开始帧** `0x02 0x31 0x03`；启动 token 下发 |

> 参赛软件对 `7001` 上的指令采用**定长 3 字节**读取，`0x02`=STX、`0x03`=ETX，中间字节标识动作（`0x30` 准备/开始、`0x31` 开始、`0x35` 结束）。

### 3.2 就绪 / 结束通知（端口 7000，二进制）

**Competitor → Judge**（收到开始准备帧后）：`0x02 0x32 0x03`（就绪）；考试收尾时：`0x02 0x35 0x03`（结束）。

### 3.3 Token 下发（端口 7002，二进制定长 12 字节）

**Judge → Competitor**，每个 token 恰好 12 字节（大端）：
```
0x02 | timestamp(int64, 8B, 大端) | type(char,1B) | key(uint8,1B) | 0x03
```
数据来源为 `token.csv`：`timestamp,type,token`。裁判端按时间戳顺序、以固定节奏（`FixTimeSendStrategy`）下发；参赛软件仅当 `token.timestamp ≥ 当前真值时间戳` 时触发一次检测应答。

### 3.4 答案上传（端口 7003，二进制变长）

**Competitor → Judge**，单帧布局（大端；`imgLen` 为裁剪目标图字节数）：
```
0x02 | len(uint32,4B) | type(char,1B='O') | timestamp(int64,8B) | boxType(int8,1B)
     | box(4×2 double,64B) | attribute(int8,1B) | longitude(double,8B) | latitude(double,8B)
     | cols(uint16,2B) | rows(uint16,2B) | image(imgLen) | 0x03
```
其中 `len = 95 + imgLen`，整帧字节数 `= 101 + imgLen`。`SU` 赛道答案复用同类头部（框/位姿字段不同）。裁判端以 `IoU≥阈值`（默认 0.8）与预载真值匹配计分，再经 `6000` 回传服务器。

心跳（可选，端口 7000）：裁判端周期性探测参赛软件在线状态；单机联调建议关闭以减少假离线。配置：
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

## 7. 时序示例（实测流程）

```
t=0     Server ─── "dispathExam|{...examType...}"\n ──► Judge (6001)  // 设定考题/题型
t=10ms  Server ─── "readyExam|{}"\n ──────────────────► Judge (6001)
        Judge  ─── 02 30 03 ──────────────────────────► Competitor (7001)  // 开始准备
        Competitor ─── 02 32 03 ─────────────────────► Judge (7000)       // 就绪
t=1s    Server ─── "beginExam|{examLib:1,port:6000}"\n ► Judge (6001)
        Judge  ─── connect ──────────────────────────► Server (6000)       // 回传通道
        Judge  ─── 02 31 03 ─────────────────────────► Competitor (7001)  // 开始
        Judge  开始计时；启动 token 下发
t=1.1s  Judge  ─── 02|ts(8B)|type|key|03 ─────────────► Competitor (7002)  // 定长12B token
        ... 按时间戳批量 ...

t=2.0s  Competitor ─── 02|len|'O'|ts|box...|img|03 ───► Judge (7003)       // 答案帧
        Judge: IoU(≥0.8) 与预载真值匹配 → 计分
t=2.1s  Judge  ─── 答案/分数 ─────────────────────────► Server (6000)
        ... 循环 ...

t=30min Server ─── 结束指令 ───────────────────────────► Judge (6001)
        Judge  ─── 02 35 03 ──────────────────────────► Competitor (7001)
        Competitor ─── 02 35 03 ─────────────────────► Judge (7000)        // 结束
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
