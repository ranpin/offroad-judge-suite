# Multi-Sensor Autonomous Off-Road Challenge Judging Suite (2023)

> 面向 2023 年自动驾驶越野挑战赛的多传感器评测基础设施个人项目归档
>
> A judging & data-security infrastructure for an autonomous off-road challenge (2023 season).
> Repository contains: multi-sensor data encryption toolchain, communication protocol specifications, scoring configuration templates, and a C++ Qt mock client for competitors.

**⚠️ 免责声明 Disclaimer**
本仓库仅为个人项目归档与技术总结，所有敏感信息（赛事名称、主办方名称、真实 IP、GT 真值、原始考题数据）已做脱敏处理。仓库**不包含**比赛使用的原始数据、答案与二进制发布物。此归档**不代表任何官方主办方**，如需查看完整资料请通过正当渠道向赛事组委会申请。

---

## 📌 Table of Contents

- [项目背景](#项目背景)
- [系统整体架构](#系统整体架构)
- [目录结构](#目录结构)
- [核心组件](#核心组件)
  - [数据加密工具 (Python)](#数据加密工具-python)
  - [裁判客户端 (Java)](#裁判客户端-java)
  - [评判应用 (C++/Qt)](#评判应用-cqt)
  - [参赛端 Mock 客户端 (C++/Qt)](#参赛端-mock-客户端-cqt)
  - [协议级下发端参考实现 (Python)](#协议级下发端参考实现-python)
  - [服务器端可视化 (ROS/C++)](#服务器端可视化-rosc)
- [评测流程时序图](#评测流程时序图)
- [通信协议规范](#通信协议规范)
- [数据格式规范](#数据格式规范)
- [评分算法](#评分算法)
- [配置示例](#配置示例)
- [环境依赖](#环境依赖)
- [常见问题](#常见问题)
- [相关文档](#相关文档)

---

## 项目背景

本项目的定位是**自动驾驶赛事技术支撑系统**，服务于以下两类赛道：

| 赛道 | 英文 | 输入 | 输出 | 关键挑战 |
|------|------|------|------|----------|
| **OD 目标检测** | Object Detection | 单目图像 + 32 线激光 + 单线激光 + GPS/IMU/CAN | 3D 目标包围框 | 户外越野小目标遮挡 |
| **SU 场景理解与定位** | Scene Understanding | 全量多传感器 | 本地/全局定位 + 场景语义 | 非结构化道路 |

### 系统核心能力

```
┌────────────────────────────────────────────────────────────┐
│         多传感器考题加密与在线下发（防作弊）                │
│  ├── CSV 类：随机 ASCII 偏移（每行独立 token）              │
│  ├── PCAP/LMS 二进制：字节级循环移位                        │
│  ├── Ground Truth：独立 token 加密，仅裁判端可解密          │
│  └── Token 按时间戳在线下发，参赛队无法提前解密             │
└────────────────────────────────────────────────────────────┘
┌────────────────────────────────────────────────────────────┐
│    三端 Socket 通信架构（服务器 / 裁判 / 参赛软件）         │
│  ├── 端口分配：6001 (server), 7000-7003 (local)            │
│  ├── 多路并发：Java 每端口独立线程监听                      │
│  └── 心跳/超时：可配置答题时限（默认 30 分钟）              │
└────────────────────────────────────────────────────────────┘
┌────────────────────────────────────────────────────────────┐
│         实时评分与可视化（IoU + 定位误差）                  │
│  ├── OD：IoU ≥ 0.8 判定命中，线性插值计定位分              │
│  ├── SU：本地定位 + 场景理解 + 全局定位（合计 500 分）      │
│  └── 总分 1000 分，实时推送服务器排行榜                     │
└────────────────────────────────────────────────────────────┘
```

---

## 系统整体架构

```
┌─────────────────────────────────────────────────────────────────────────┐
│                            2023 赛事评测系统全景图                       │
└─────────────────────────────────────────────────────────────────────────┘

【赛前离线】

  原始考题数据                        加密工具 (Python)
  ┌───────────────┐                 ┌──────────────────┐
  │ 相机/激光/GPS │ ─── encrypt ──► │ 加密考题数据包   │
  │ IMU/CAN/真值  │                 │ + token.csv      │
  └───────────────┘                 └────────┬─────────┘
                                             │
                            ┌────────────────┴────────────────┐
                            ▼                                 ▼
                    分发给参赛队                        裁判组内部保存
                    （已加密，无 token）              （GT + token.csv）


【比赛当天在线】

   比赛服务器（云端）
        │
        │ 端口 6001（考题指令）
        ▼
   裁判客户端（Java, client.jar）
        │
        ├── 端口 7000 ← 参赛软件就绪信号
        ├── 端口 7001 → 转发指令到参赛软件
        ├── 端口 7002 → 按时间戳下发 token
        └── 端口 7003 ← 收集参赛软件答案
             │
             ▼
   参赛软件（Mock Client / 真实算法）
   ├── 接收 token → 在线解密考题数据
   ├── 运行 OD/SU 算法
   └── 上传预测结果（包围框 / 定位坐标）

             │
             ▼
   评判应用（C++/Qt, judge_app）
   ├── 读取加密的 GT → 实时解密
   ├── 计算 IoU / 定位误差
   └── 打分并推送到服务器
```

---

## 目录结构

```
offroad-judge-suite/
├── README.md                          # 本文件
├── LICENSE                            # MIT
├── .gitignore                         # 忽略编译产物 / GT / 二进制
│
├── docs/                              # 详细技术文档
│   ├── architecture.md                # 系统架构设计说明
│   ├── encryption-scheme.md           # 数据加密方案设计
│   ├── scoring-algorithm.md           # 评分算法详解
│   ├── communication-protocol.md      # 三端通信协议
│   └── reproduce-from-scratch.md      # 从零复现：构建 + 三端联调
│
├── tools/                             # 可复用工具集
│   ├── data-encryptor/                # 多传感器数据加密工具
│   │   ├── encrypt_data.py            # 主程序（267 行，独立运行）
│   │   ├── config.ini                 # 输入路径配置
│   │   └── README.md                  # 使用说明
│   └── soft-judge-reference/          # 协议级下发端参考实现（可复现闭环）
│       ├── judge_server.py            # 纯 Python，驱动 mock-client 跑通 START→token→答案
│       └── README.md                  # 用法与端口说明
│
├── mock-client/                       # C++/Qt 参赛端模拟软件
│   ├── od/                            # OD 赛道模拟客户端源码
│   │   ├── main.cpp                   # 主流程：接收 token → 解密 → 输出答案
│   │   ├── tcpclient.{h,cpp}          # TCP 客户端（连接裁判）
│   │   ├── tcpserver.{h,cpp}          # TCP 服务端（接收下发）
│   │   ├── CropImage.{h,cpp}          # OpenCV 图像裁剪
│   │   ├── groundtruthfile.{h,cpp}    # GT 文件读取
│   │   └── MockClient.pro             # qmake 项目文件
│   └── su/                            # SU 赛道模拟客户端源码
│       └── (结构同 od，处理本地/全局定位)
│
├── server-visualizer/                 # ROS 服务器端可视化（唯一带源码的服务器组件）
│   ├── src/server_node/               # 自定义 ROS 包：Qt GUI + TCP 下发 + RViz 发布
│   ├── config.yml                     # 端口 / 数据路径（脱敏）
│   ├── players.csv                    # 参赛队/IP 列表样例
│   ├── default.rviz                   # RViz 显示配置
│   └── README.md                      # 依赖 velodyne / rviz_car_model 与构建说明
│
├── configs/                           # 配置模板（脱敏）
│   ├── judge-client.properties.example
│   ├── judge-app.yml.example
│   ├── competitor-app-od.yml.example
│   └── competitor-app-su.yml.example
│
└── examples/                          # 参赛端使用示例
    ├── competitor-entry-template.txt  # 参赛软件入口代码模板
    └── competitor-usage-readme.txt    # 参赛端使用说明
```

> **哪些组件只有源码、哪些只有二进制？**
> 本仓库对**可公开的部分提供源码**：数据加密工具（Python）、参赛端 Mock 客户端（C++/Qt）、服务器端可视化（ROS/C++）。
> **比赛主服务器（server.jar）与 Java 裁判客户端（client.jar）** 属现场交付二进制，其 Java 包名内含原始机构标识，**未在本仓库发布源码，也不上传二进制**——其对外行为已在 [`docs/communication-protocol.md`](docs/communication-protocol.md) 完整规格化，可据此自行实现替代。**评判应用（judge_app）** 同理仅有 Linux 二进制。完整复现路径见 [`docs/reproduce-from-scratch.md`](docs/reproduce-from-scratch.md)。

---

## 核心组件

### 数据加密工具 (Python)

**位置**：[`tools/data-encryptor/encrypt_data.py`](tools/data-encryptor/encrypt_data.py)

**功能**：对多传感器考题数据进行差异化加密，生成加密后的考题包与配套的 token 文件。

**支持的数据类型**：

| 文件类型 | 加密算法 | 密钥粒度 |
|----------|----------|----------|
| `*.csv` (GPS/IMU/CAN/时间戳) | ASCII 字符偏移 (César) | **每行独立** token ∈ [1, 9] |
| `*.pcap` (HDL-32 多线激光) | 字节循环移位 `(byte+token) % 256` | **每时间戳** 一个 token |
| `*.lms` (URG 单线激光) | 字节循环移位 | **每时间戳** 一个 token |
| `gt_detection.csv` (真值) | ASCII 偏移，**固定 token** | 全局一个 token |

**核心代码摘要**：

```python
def encrypt_string(s, token_min=1, token_max=9):
    token = random.randint(token_min, token_max)
    encrypted = ''.join(chr(ord(c) + token) for c in s)
    return encrypted, token

def encrypt_shift_byte(byte, token):
    return (byte + token) % 256
```

**安全性说明**：本工具为**离线调试**版本，实际现场使用不同参数（现场 token 空间更大）。

---

### 裁判客户端 (Java)

**运行时**：JDK 1.8+

**角色**：三端通信的中枢（Server ↔ Judge Client ↔ Competitor Software）

**端口分配**（详见 `configs/judge-client.properties.example`）：

| 端口 | 方向 | 用途 |
|------|------|------|
| 6001 | Server → Client | 服务器下发考题指令 |
| 7000 | Client → Judge | 参赛软件就绪通知 |
| 7001 | Judge → Client | 转发指令到参赛软件 |
| 7002 | Judge → Client | 按时间戳下发 token |
| 7003 | Client → Judge | 参赛软件答案上传 |

**关键配置**：

```properties
# 准备考试最大时长（5 分钟）
prepare.timeout=300000
# 答题时限（30 分钟）
answer.timeout=1800000
# Token 下发策略（可插拔）
token.send.strategy=com.example.soft.token.FixTimeSendStrategy
# 心跳检测
client.soft.heartbeat.enable=true
client.soft.heartbeat.interval=10000
client.soft.heartbeat.death.count=3
```

> **本仓库不包含 Java 源代码**（原始交付为 jar 发布物）。文档描述其架构与协议。

---

### 评判应用 (C++/Qt)

**运行时**：Qt 5.x + OpenCV，Linux

**角色**：实时评分核心，读取加密 GT 与答案，计算 IoU / 定位误差。

**评分配置**（`configs/judge-app.yml.example`）：

```yaml
# OD 赛道
IoUThreshold: 0.8           # 命中判定阈值
ODLocThreshLmin: 0.5        # 定位误差下限（满分）
ODLocThreshLmax: 1.0        # 定位误差上限（0 分）

# SU 赛道
SULocThreshLpmin: 0.5       # 本地定位局部阈值
SULocWeightp: 0.7           # 局部定位权重
SULocWeighta: 0.3           # 全局定位权重

# 权重与总分
maxscore: 1000
scores: [500, 500]          # OD 满分 500，SU 满分 500
```

> **本仓库不包含 C++ 评判应用源代码**（原始交付为可执行文件）。仅公开配置。

---

### 参赛端 Mock 客户端 (C++/Qt)

**位置**：[`mock-client/`](mock-client/)

**功能**：模拟参赛队的算法行为，接收 token → 解密 → 输出随机/固定答案，用于**系统联调测试**。

**代码结构**：

```
main.cpp                     # 事件循环 + 主流程编排
├── tcpserver.cpp/h          # 端口 7001 接收指令、7002 接收 token
├── tcpclient.cpp/h          # 端口 7003 上传答案
├── CropImage.cpp/h          # 图像裁剪（调用 OpenCV）
└── groundtruthfile.cpp/h    # GT 文件读取与解析
```

**编译（已在 Ubuntu 24.04 / Qt 5.15 / OpenCV 4.6 实测通过）**：

```bash
sudo apt install -y qtbase5-dev build-essential libopencv-dev
cd mock-client/od
qmake MockClient.pro      # .pro 自动探测 pkg-config(opencv4)，否则回退 opencv_world
make -j$(nproc)
./MockClient              # 交互输入 y / 0，作为服务端等待 7001 开始帧
```

> **无专有组件即可复现闭环**：`tools/soft-judge-reference/judge_server.py` 可扮演下发端，
> 驱动上面源码构建的 `MockClient` 完成 `START→token→答案`（实测解出 111 条答案帧）。
> 步骤见 [`docs/reproduce-from-scratch.md`](docs/reproduce-from-scratch.md) 路径二·最小闭环。

**运行流程（实测核对）**：

```
[启动] 读 config.yml(picture_path/timeStamp_path) + 约定路径的 gt_detection.csv
   ↓   作为服务端打开 7001(指令) / 7002(token)
[等待] 阻塞等待 7001 收到开始帧 0x02 0x30 0x03 → 打印 "Receive start signal."
   ↓
[就绪] 反连裁判 7000，写就绪帧 0x02 0x32 0x03
   ↓
[循环] 反连裁判 7003；每收到一帧 12B token(7002)：
   - 解出 timestamp / type / key
   - 当 timestamp ≥ 当前 GT 时间戳时，裁剪 Camera 图并组一帧答案
   - 通过 7003 上传二进制答案帧（见 communication-protocol.md）
   ↓
[结束] 答题时限到或收到结束帧
```

---

### 协议级下发端参考实现 (Python)

**位置**：[`tools/soft-judge-reference/judge_server.py`](tools/soft-judge-reference/judge_server.py)

**角色**：在没有专有裁判客户端 / 考题包的前提下，用纯标准库 Python 实现 `communication-protocol.md`
的线协议，扮演“服务器 + 裁判下发端”，驱动源码构建的参赛端完成一次**可复现端到端闭环**。

**已实测结果**（Ubuntu 24.04 / Qt 5.15 / OpenCV 4.6，单机）：参赛端打印 `Receive start signal.`；
参考实现收到就绪帧 `023203`、回放全部 token 后解出 **111 条答案帧**，timestamp 单调递增、
box/lon-lat/图像尺寸等字段完整可解码 → 判定 `闭环贯通`。

```bash
python3 tools/soft-judge-reference/judge_server.py --token-csv <...>/token.csv
```

---

### 服务器端可视化 (ROS/C++)

**位置**：[`server-visualizer/`](server-visualizer/)

**功能**：比赛服务器侧的可视化 / 下发节点。基于 **ROS + Qt + RViz**，向裁判链路下发 token 与指令、接收各队分数，并在 RViz 中实时显示车辆位姿、GPS、图像与真值点云。

**这是本评测系统里唯一附带源码的服务器端组件**（主服务器 server.jar 与 Java 裁判客户端 client.jar 均为二进制交付，不公开）。

**代码结构**：

```
server-visualizer/src/server_node/
├── src/cserver.cpp           # TCP 指令/Token 下发（端口 6001/6002）
├── src/cscoreserver.cpp      # 接收各队分数（端口 6003）
├── src/*Publisher.cpp        # gps / image / odGt / suGt / tf 的 RViz 发布器
├── src/coordinateconvertion.cpp / wgs84tocartesian.hpp   # WGS84→笛卡尔坐标转换
└── ui/main_window.ui         # Qt Designer 主界面
```

**依赖（本仓库未内置，需自行检出到同一 catkin 工作区）**：`velodyne_*`（velodyne 官方驱动）、`rviz_car_model`、ROS Melodic。

**构建**：

```bash
mkdir -p ~/catkin_ws/src && cd ~/catkin_ws/src
# 放入 server_node + velodyne + rviz_car_model
cd ~/catkin_ws && catkin_make && source devel/setup.bash
```

> 该目录源码来自项目历史调试材料，**已脱敏**：移除原始机构/赛事标识与个人署名，替换个人绝对路径与示例邮箱。

---

## 评测流程时序图

```
Server              Judge Client          Competitor            Judge App
  │                      │                     │                     │
  │ ── 6001: readyExam/beginExam ─►│            │                     │
  │                      │                     │                     │
  │                      │ ── 7001: 02 30 03 ─►│                     │
  │                      │                     │                     │
  │                      │ ◄── 7000: 02 32 03 ─│                     │
  │                      │                     │                     │
  │                      │   [计时开始，最多 5 分钟准备时间]           │
  │                      │                     │                     │
  │                      │ ── 7002: token(ts) ►│                     │
  │                      │                     │ decrypt(t1)         │
  │                      │                     │ process()           │
  │                      │ ◄── 7003: ANSWER(1)─│                     │
  │                      │                     │                     │
  │                      │                     │                     │ ── 计算 IoU
  │                      │                     │                     │    读取加密 GT
  │                      │ ◄─────────────────────────────────────────│
  │                      │                     │                     │
  │ ◄── 6001: SCORE ─────│                     │                     │
  │                      │                     │                     │
  │    [实时分数推送]     │                     │                     │
```

---

## 通信协议规范

> ⚠️ 本节为**实测核对后**的线协议摘要，与早期草稿不同。完整字节级定义、字段偏移、
> 心跳/超时与真实端口方向以 [`docs/communication-protocol.md`](docs/communication-protocol.md) 为准。

**端口与角色（实测）**：裁判客户端 A 监听 `6001/7000/7003`；参赛端 C 监听 `7001/7002`。
连接建立方向与消息逻辑方向相反——A 作为客户端**反连** C 的 7001/7002，C 收到开始帧后
**反连** A 的 7000/7003。

**6001（服务器→A，文本按行、竖线分隔）**：`<cmd>|<jsonArgs>\n`（UTF-8），如
`readyExam|{}`、`beginExam|{"examLib":1,"port":6000}`。

**7001 指令帧（定长 3 字节）**：`0x02 <act> 0x03`，`0x30`=准备、`0x31`=开始、`0x35`=结束。

**7002 token 帧（定长 12 字节，大端）**：`0x02 + timestamp(int64,8B) + type(1B) + key(1B) + 0x03`。

**7003 答案帧（变长，大端）**：`0x02 | len(uint32,4B) | 'O' | timestamp(int64) | boxType(int8)
| box(4×2 double=64B) | attribute(int8) | lon(double) | lat(double) | cols(u16) | rows(u16) | image | 0x03`。

### 参考实现（可复现闭环）

[`tools/soft-judge-reference/`](tools/soft-judge-reference/) 提供一份**纯 Python、无第三方依赖**的下发端
参考实现，仅凭上述协议即可驱动源码构建的参赛端跑通 `START→token→答案` 全链路，无需任何专有二进制/考题包。

### 通信协议详细文档

参见 [docs/communication-protocol.md](docs/communication-protocol.md)

---

## 数据格式规范

考题数据以目录组织：

```
EXAM-2023-001-OD/
├── Raw-Example-001/
│   ├── Raw-Example-001-Calib/                   # 标定参数（不加密）
│   │   ├── GPS.txt
│   │   ├── HDL32-E.txt / HDL32-I.txt
│   │   ├── Laser.txt / Odom.txt / Steer.txt
│   ├── Raw-Example-001-Camera/                  # 图像（不加密）
│   │   └── *.jpg (每帧一张)
│   ├── Raw-Example-001-Groundtruth/             # 真值（加密）
│   │   ├── gt_detection.csv
│   │   └── gt_global.csv
│   ├── Raw-Example-001-Camera-Timestamp.csv     # 加密
│   ├── Raw-Example-001-GPS.csv                  # 加密
│   ├── Raw-Example-001-IMU.csv                  # 加密
│   ├── Raw-Example-001-Canbus-{Gear,Odom,Steer}.csv  # 加密
│   ├── Raw-Example-001-HDL32.pcap               # 加密
│   ├── Raw-Example-001-URG.lms                  # 加密
│   ├── Raw-Example-001-infor.xlsx               # 不加密
│   └── Raw-Example-001-Task.txt                 # 不加密（题目描述）
└── token-*.csv                                  # 裁判端持有
```

### GT 数据格式示例（脱敏）

```csv
# gt_detection.csv (加密前)
timestamp,frame_idx,class,x,y,w,h,confidence
1683356400000,1234,car,320,240,80,60,1.0
1683356400000,1234,pedestrian,400,300,40,90,1.0
```

---

## 评分算法

### OD 赛道（满分 500 分）

```
每个真值框的评分：
  if IoU(pred_box, gt_box) >= 0.8:
      hit = 1  (命中)
  else:
      hit = 0

总分构成：
  OD_score = w0 * Accuracy + w1 * Time + w2 * Image + w3 * Localization
  其中 w0=w1=w2=w3=1.0，各占 125 分

定位误差计分（线性插值）：
  error ∈ [0, Lmin=0.5m]   → 满分 125
  error ∈ [Lmin, Lmax=1.0m] → 线性递减
  error > Lmax              → 0 分
```

### SU 赛道（满分 500 分）

```
SU_score = 本地定位时间 (50) + 本地定位精度 (100)
         + 场景理解时间 (50) + 场景理解精度 (100)
         + 全局定位时间   (50) + 全局定位精度   (150)

本地定位精度：
  Loc_score = w_p * Local_Partial + w_a * Local_All
  其中 w_p=0.7, w_a=0.3
```

详细算法说明参见 [docs/scoring-algorithm.md](docs/scoring-algorithm.md)

---

## 配置示例

### 裁判客户端配置（脱敏后）

```properties
# configs/judge-client.properties.example
local.cmd.server.port=6001
local.cmd.soft.port=7000
local.answer.soft.port=7003
soft.cmd.socket.port=7001
soft.token.socket.port=7002

server.ip=127.0.0.1
local.ip=127.0.0.1

prepare.timeout=300000
answer.timeout=1800000
```

完整配置见 [`configs/`](configs/) 目录。

---

## 环境依赖

| 组件 | 依赖 |
|------|------|
| **加密工具** | Python 3.7+（无第三方依赖，仅使用标准库） |
| **Mock Client** | Qt 5.12+, OpenCV 4.x, C++11 编译器 |
| **裁判客户端** | JDK 1.8+（本仓库无源码，仅配置示例） |
| **评判应用** | Qt 5.x, OpenCV 4.x（本仓库无源码，仅配置示例） |

---

## 常见问题

**Q1: 为什么不包含 Java/C++ 完整源代码？**

A: 交付形态是发布产物（jar / exe）。项目原始 Java 端源码由组委会维护，C++ 端源码大部分属于主办方软件。本仓库仅归档**个人独立编写**的模块：Python 加密工具（`encrypt_data.py` 267 行）、C++ 参赛端 Mock 客户端。

**Q2: 为什么不包含考题数据？**

A: 考题数据涉及赛事安全与版权，且体积达 100+ GB。仓库仅保留**脱敏后的配置模板**和**协议格式说明**。

**Q3: 加密方案为什么这么简单？**

A: 现场使用的加密实现是黑盒的，本仓库中的加密工具是**离线调试版本**，用于快速生成模拟考题用于系统联调。真正现场版本 token 空间更大，且 GT 数据加密方案对参赛队不透明。

---

## 相关文档

| 主题 | 文件 |
|------|------|
| 系统架构详解 | [docs/architecture.md](docs/architecture.md) |
| 加密方案设计 | [docs/encryption-scheme.md](docs/encryption-scheme.md) |
| 评分算法 | [docs/scoring-algorithm.md](docs/scoring-algorithm.md) |
| 通信协议 | [docs/communication-protocol.md](docs/communication-protocol.md) |
| 参赛端使用示例 | [examples/competitor-entry-template.txt](examples/competitor-entry-template.txt) |

---

## 项目收获与技术反思

本项目让我在以下方面积累了系统工程经验：

| 维度 | 收获 |
|------|------|
| **多传感器数据处理** | 理解 LiDAR PCAP 帧结构、GPS/IMU/CAN 时序对齐 |
| **通信架构设计** | Java Socket 多端并发、消息路由、超时/心跳机制 |
| **评分算法** | IoU 判定、线性插值计分、多子任务权重分配 |
| **数据安全** | 差异化加密策略、在线 token 下发防作弊设计 |
| **工程部署** | Linux/Windows 跨平台、Qt 编译链、OpenCV 集成 |

**已知不足与改进方向**：

- 加密算法本质是凯撒密码（token 空间仅 1-9），应升级为 **AES-256** + 帧独立密钥
- TCP 无应用层 ACK，客户端崩溃可能丢失部分答案，应引入**消息序号 + 重传**
- 无系统性压力测试，若要支持千队并发应改造为 **Netty + Kafka** 架构
- Java Properties 加载路径含中文会报 URL encoding 错误 → 工程规范教训

---

## License

[MIT](LICENSE) © 2023-2026

---

<p align="center"><sub>Repository archived for portfolio purposes. All sensitive competition data has been sanitized.</sub></p>
