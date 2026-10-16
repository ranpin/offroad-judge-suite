# Server Visualizer（ROS 服务器端可视化）

**角色**：比赛服务器侧的可视化/下发节点（对应本仓库 README 三端图里的"服务器"角色）。基于 **ROS + Qt + RViz**，负责向裁判/参赛链路下发数据令牌与指令、接收各队分数，并在 RViz 中实时显示车辆位姿、GPS、图像与真值点云。

> 这是本评测系统中**唯一随附源码的服务器端组件**。核心 Java 裁判客户端与比赛主服务仅以二进制交付，**不发布源码也不上传二进制**；其对外行为规格见 [`docs/communication-protocol.md`](../docs/communication-protocol.md)。

## 目录结构
```
server-visualizer/
├── src/server_node/          # 自定义 ROS 包（Qt GUI + TCP 服务 + 发布/订阅）
│   ├── include/server_node/  # CServer / CScoreServer / gpsPublisher / imagePublisher / odGtPublisher / suGtPublisher / tfPublisher 等
│   ├── src/                  # 对应实现
│   ├── ui/main_window.ui     # Qt Designer 界面
│   ├── resources/            # 图标资源
│   ├── CMakeLists.txt
│   └── package.xml
├── config.yml                # 端口 / 数据路径 / 输出文件（已脱敏为相对路径）
├── players.csv               # 参赛队名与 IP 列表（示例：车队1 / 127.0.0.1）
├── default.rviz              # RViz 显示配置
└── ServerVisualizer.workspace# Qt Creator ROS 工作区文件
```

## 依赖的公开 ROS 包（本目录未内置）
`server_node` 依赖以下标准 ROS 包，请在 catkin 工作区中一并检出后构建：
- `velodyne_driver` / `velodyne_pointcloud` / `velodyne_laserscan` / `velodyne_msgs`（velodyne 官方驱动）
- `rviz_car_model`（车辆模型 RViz 显示插件）
- ROS **Melodic**（工作区默认 `/opt/ros/melodic`），Qt4/5、PCL

## 端口约定（与三端协议一致）
| 键 | 值 | 含义 |
|---|---|---|
| `requestListenPort` | 6000 | 监听请求 |
| `clientCommandPort` | 6001 | 向裁判客户端发指令 |
| `clientTokenPort` | 6002 | 下发 token |
| `scoreListenPort` | 6003 | 接收各队分数 |

详见 [../docs/communication-protocol.md](../docs/communication-protocol.md)。

## 构建
```bash
mkdir -p ~/catkin_ws/src && cd ~/catkin_ws/src
# 将 server_node 与依赖的 velodyne / rviz_car_model 放同层
git clone <velodyne> ; git clone <rviz_car_model>
cp -r /path/to/server-visualizer/src/server_node ./
cd ~/catkin_ws && catkin_make
source devel/setup.bash
roslaunch server_node <your.launch>      # 或直接运行可执行节点
```

## 配置
编辑 `config.yml`：
- `dataPath`：考题数据目录（支持多目录，分号分隔），示例已改为 `./data/exams/...`
- `playerInfoFile`：指向 `players.csv`，逐行 `name, 队伍名` / `ip, 地址`

## 说明
本目录源码来自项目历史调试材料，已做脱敏：移除原始机构/赛事标识与个人署名、替换个人绝对路径与示例邮箱。算法与协议原理见 `docs/`。
