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
- **用本地归档**：把 `test_data/20230506-4-1-OD/` 拷到机器上。
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
# (4) 触发考题：服务器 GUI 或手动 nc 127.0.0.1 6001 发送 START,<考题路径>
```
验证：裁判日志出现 START→token→答案→SCORE 流转；`Report.csv` 每行有分数。

---

## 路径二：从源码构建可复现的组件

### 参赛端 Mock 客户端（C++/Qt）
```bash
cd mock-client/od
qmake MockClient.pro      # 需在 .pro 里配置 OPENCV/LIBS 指向本机 OpenCV 3.4 / Qt5
make -j$(nproc)
./MockClient              # 作为 TCP 客户端连裁判 7001/7002，上传答案到 7003
```
SU 赛道同理：`cd ../su && qmake && make`。

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
| 7000 | 参赛端 → 裁判客户端 | 就绪信号 |
| 7001 | 裁判客户端 → 参赛端 | 指令（考题路径 / start / stop）|
| 7002 | 裁判客户端 → 参赛端 | 按时间戳下发 token |
| 7003 | 参赛端 → 裁判客户端 | 答案上传 |

---

## 复现自检清单

- [ ] 加密工具能对新数据产出加密包 + `token.csv`
- [ ] `mock-client` qmake 构建通过，能连裁判并收发 token/答案
- [ ] 三端（或 A/B 用归档二进制 + C 用源码）按序启动，端口无冲突
- [ ] 一次完整答题：裁判日志 START→token→答案→SCORE 闭环
- [ ] 最终分数落在 0–500（OD）或 0–1000（OD+SU）
- [ ] （可选）server-visualizer 在 RViz 显示车辆与点云

## 相关文档
- 架构：[architecture.md](architecture.md)
- 协议：[communication-protocol.md](communication-protocol.md)
- 加密：[encryption-scheme.md](encryption-scheme.md)
- 评分：[scoring-algorithm.md](scoring-algorithm.md)
