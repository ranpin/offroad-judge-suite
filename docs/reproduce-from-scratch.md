# 从零复现指南 · Reproduce From Scratch

目标：在**一台新机器**上，凭借本仓库 + 本地归档的说明，**完整地把评测系统跑起来**。

系统由 4 个组件构成，源码公开程度不同：

| 组件 | 本仓库是否含源码 | 复现方式 |
|------|------------------|----------|
| 数据加密工具（Python） | ✅ `tools/data-encryptor/` | 直接运行 |
| 参赛端 Mock 客户端（C++/Qt） | ✅ `mock-client/{od,su}/` | qmake 构建 |
| 服务器端可视化（ROS/C++） | ✅ `server-visualizer/` | catkin 构建（需外部 ROS 依赖） |
| 裁判客户端 A（Java） | ❌ 仅协议规格 | 依协议自实现 或 用本地归档 jar |
| 评判应用 B（C++/Qt） | ❌ 仅配置模板 | 依算法自实现 或 用本地归档 LocalClient |

> **为什么 A / B 不含源码？** 二者为现场交付二进制（`client.jar` / `LocalClient`），Java 包名与内部实现含原始机构标识，不公开。其**对外行为已完整规格化**（端口、消息格式见 `communication-protocol.md`；评分公式见 `scoring-algorithm.md`），可据此实现功能等价替代。若你拥有本地归档，则直接用二进制即可（步骤见下）。

---

## 路径一：只用本地归档二进制，跑通全流程（最快）

适合：你手上已有完整本地归档（现场二进制 + 考题数据）。目标是验证联调，不重新编译 A/B。

### 1. 环境
- Ubuntu 20.04 / WSL2 Ubuntu（推荐），或 Windows（仅裁判客户端 + 参赛端）
- JRE/JDK 8（运行 client.jar）
- Qt5 + OpenCV 运行库（见仓库根 README「环境依赖」）

```bash
sudo apt install -y openjdk-8-jre libgl1 libxkbcommon-x11-0 libusb-1.0-0 \
  libxcb-icccm4 libxcb-image0 libxcb-keysyms1 libxcb-render-util0 \
  libxcb-shape0 libxcb-xinerama0
```

### 2. 准备一套考题数据
真实考题数据不在本仓库（涉密）。两种替代：
- **用本地归档**：把本地归档的考题目录（形如 `test_data/EXAM-<date>-OD/`）拷到机器上。
- **自造示例**：按数据格式（见 `encryption-scheme.md`）准备一个 `Raw-*` 目录，用 `tools/data-encryptor/` 加密，产出加密包 + `token.csv`。

### 3. 配置三端（保持端口/IP 一致）
- 裁判客户端 `config.properties`：`local.exam.path` 指向考题根；`*.port` 用默认；`soft.ip/server.ip/local.ip=127.0.0.1`；`token.send.strategy` 必须与所用 jar 的包名匹配。
- 评判应用 `config.yml`：`filepath` 指向 `Raw-*-Groundtruth`；`examidx` 与目录前缀一致；端口 6001/6002/6003。
- 参赛端 `config.yml`：`picture_path` / `timeStamp_path` / `path_of_gt` 指向考题对应文件。

配置模板见 `configs/*.example`。

### 4. 按序启动
```bash
# (1) 评判应用 B
cd judge_app && export LD_LIBRARY_PATH=$PWD/lib:$LD_LIBRARY_PATH && export QT_PLUGIN_PATH=$PWD/plugins
./LocalClient &
# (2) 裁判客户端 A（本地归档目录，示例名 judge-client/）
cd judge-client && bash start.sh
# (3) 参赛端 C（本地归档 simulate/OD_Linux，或直接运行下方源码构建产物）
cd simulate/OD_Linux && export LD_LIBRARY_PATH=$PWD/lib:$LD_LIBRARY_PATH && ./MockClient &
# (4) 触发考题：向裁判客户端 A 的 6001 文本端口按行发送（竖线分隔、UTF-8、\n 结尾）：
#       readyExam|{}\n        -> A 在 local.exam.path 解压 examLib.zip 后向 C 的 7001 发 02 30 03
#       beginExam|{"examLib":1,"port":6000}\n  -> A 向 C 的 7001 发 02 31 03，并连 server:6000 收答案
#   （6001 协议细节见 communication-protocol.md；此路径依赖专有 examLib.zip 考题包）
```
验证：裁判日志出现 START→token→答案→SCORE 流转；`Report.csv` 每行有分数。

> **无专有考题包时**：A 的 `readyExam` 会先在 `local.exam.path` 解压 `examLib.zip`，
> 缺失则不会向 C 下发 `02 30 03`。若没有该包，改用**路径二·最小闭环**（`judge_server.py`
> 直接扮演下发端驱动源码构建的 C），无需任何归档二进制。

---

## 路径二：从源码构建可复现的组件

### 参赛端 Mock 客户端（C++/Qt）
```bash
sudo apt install -y qtbase5-dev build-essential libopencv-dev   # 标准 Ubuntu 即可
cd mock-client/od
qmake MockClient.pro        # .pro 自动探测 pkg-config(opencv4)，否则回退 opencv_world
make -j$(nproc)
./MockClient                # 交互输入 y / 0；C 是 7001/7002 服务端，收到开始帧后
                            # 反向连裁判的 7000(就绪)/7003(答案) 上传
```
SU 赛道同理：`cd ../su && qmake && make`。

### 路径二·最小闭环（无专有组件，已实测复现）

**一键（推荐，零外部数据）**：本目录自带合成样例与脚本，clone 后直接：

```bash
bash tools/soft-judge-reference/run_e2e_demo.sh
# 末行输出：[demo] PASS: repo-only end-to-end closed loop reproduced
```

它从源码构建 `mock-client/od`（out-of-tree，不污染仓库）、用
`tools/soft-judge-reference/sample-data/`（合成极小样例，非真实考题/真值）暂存运行目录，
启动源码构建的参赛端并驱动 `START→token→答案`，最后断言收到可解码答案帧。

<details><summary>展开：手动分步（用真实本地考题数据时）</summary>

仅凭本仓库即可跑通一次端到端协议闭环——用 `tools/soft-judge-reference/judge_server.py`
扮演“服务器+裁判下发端”，直接驱动**源码构建**的 C，不依赖 `client.jar`/`LocalClient`/`examLib.zip`：

```bash
# 1) 构建并启动参赛端 C（config.yml 指向本地考题数据；gt 放在源码约定的相对路径）
cd mock-client/od && qmake && make
#   config.yml: picture_path=<...>/Raw-*-Camera/  timeStamp_path=<...>/Raw-*-Camera/timestamp.csv
#   gt 需位于 ./data/exams/EXAM-OD-001/Raw-001-Groundtruth/gt_detection.csv
printf 'y\n0\n' | ./MockClient &        # 打印 "Waiting for start signal......"

# 2) 运行参考下发端
python3 ../../tools/soft-judge-reference/judge_server.py --token-csv <...>/token.csv
```

实测结果（Ubuntu 24.04 / Qt 5.15 / OpenCV 4.6，单机 127.0.0.1）：
- C 打印 `Receive start signal.` 与 `Send start single to server.`；
- 下发端收到就绪帧 `023203`，回放全部 token 后 C 上传 **111 条答案帧**，
  timestamp 单调递增、可解码出 box / lon-lat / 图像尺寸等全字段；
- 判定输出：`闭环贯通（START→token→答案 全链路）`。

</details>

### 数据加密工具（Python）
```bash
cd tools/data-encryptor
# 编辑 config.ini 指定 raw/encrypt/gt 路径
python3 encrypt_data.py
```
详见 `tools/data-encryptor/README.md`。

### 服务器端可视化（ROS）
```bash
mkdir -p ~/catkin_ws/src && cd ~/catkin_ws/src
# 检出官方 velodyne 与 rviz_car_model 到 src/
git clone https://github.com/ros-drivers/velodyne.git
git clone https://github.com/kkrol3/rviz_car_model.git   # 或等价提供车辆 marker 的包
cp -r <repo>/server-visualizer/src/server_node ./
cd ~/catkin_ws && catkin_make && source devel/setup.bash
rosrun server_node server_node    # 或按需 roslaunch
```
端口/数据路径在 `server-visualizer/config.yml` 配置。

### 裁判客户端 A / 评判应用 B（自实现）
本仓库不提供源码。若要在无归档的机器上完整复现，需按规格实现：
- **A**：监听 6001（服务器指令）/7000（就绪）/7003（答案），并向参赛端 7001（指令）/7002（token 定时批量）转发。消息格式与下发策略见 `communication-protocol.md`。
- **B**：读取加密 GT + 答案，按 `scoring-algorithm.md` 的 IoU/定位误差公式计分，经 6002/6003 回传。
两者都可用任意语言实现，只要遵守上述端口与报文协议即可与源码构建的 C 组件互操作。

---

## 端口矩阵（速查）

| 端口 | 参与方 | 用途 |
|---|---|---|
| 6001 | 服务器 ↔ 裁判客户端 | 考题指令 / 分数上传 |
| 6002 | 裁判客户端 ↔ 评判应用 | 评判指令下发 |
| 6003 | 评判应用 → 裁判客户端 | 实时分数 |
| 7000 | 参赛端 → 裁判客户端 | 就绪/结束帧（`02 32 03` / `02 35 03`）|
| 7001 | 裁判客户端 → 参赛端 | 指令帧（定长 3B：`02 30 03` 准备 / `02 31 03` 开始 / `02 35 03` 结束）|
| 7002 | 裁判客户端 → 参赛端 | token 帧（定长 12B，大端 timestamp+type+key）|
| 7003 | 参赛端 → 裁判客户端 | 答案上传（变长，含裁剪图）|

> 端口**监听方**：C(参赛端) 监听 7001/7002 作为服务端；裁判客户端/下发端监听 7000/7003。
> 建立连接的方向与消息逻辑方向相反，详见 `communication-protocol.md`。

---

## 复现自检清单

- [ ] 加密工具能对新数据产出加密包 + `token.csv`
- [x] `mock-client` 在标准 Ubuntu(qtbase5-dev+libopencv-dev) `qmake && make` 构建通过
- [x] **最小闭环**：源码构建的 C + `judge_server.py` 跑通 START→token→答案（实测 111 答案帧）
- [x] **一键自验**：`bash tools/soft-judge-reference/run_e2e_demo.sh` 零外部数据即 PASS（自带合成样例）
- [ ] 三端（A/B 用归档二进制 + C 用源码）按序启动，端口无冲突（需专有归档）
- [ ] 一次完整答题：裁判日志 START→token→答案→SCORE 闭环（需 examLib.zip 考题包）
- [ ] 最终分数落在 0–500（OD）或 0–1000（OD+SU）
- [ ] （可选）server-visualizer 在 RViz 显示车辆与点云

## 相关文档
- 架构：[architecture.md](architecture.md)
- 协议：[communication-protocol.md](communication-protocol.md)
- 加密：[encryption-scheme.md](encryption-scheme.md)
- 评分：[scoring-algorithm.md](scoring-algorithm.md)
