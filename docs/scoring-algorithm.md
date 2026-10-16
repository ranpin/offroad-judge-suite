# 评分算法详解

## 1. 总体分值结构

```
┌─────────────────────────────────────────────────────────────┐
│                     总分 1000 分                             │
│  ┌──────────────────────┐  ┌──────────────────────────┐    │
│  │  OD 目标检测 (500)   │  │  SU 场景理解与定位 (500)  │    │
│  │  ├── 精度分 (125)    │  │  ├── 本地定位时间 (50)     │    │
│  │  ├── 时间分 (125)    │  │  ├── 本地定位精度 (100)    │    │
│  │  ├── 图像分 (125)    │  │  ├── 场景理解时间 (50)     │    │
│  │  └── 定位分 (125)    │  │  ├── 场景理解精度 (100)    │    │
│  └──────────────────────┘  │  ├── 全局定位时间 (50)     │    │
│                             │  └── 全局定位精度 (150)    │    │
│                             └──────────────────────────┘    │
└─────────────────────────────────────────────────────────────┘
```

## 2. OD 目标检测评分

### 2.1 命中判定：IoU ≥ 0.8

```
       ┌─────────────────┐
       │   GT Box (真值)  │
       │    ┌────────────┼─────┐
       │    │  交集面积   │     │
       ├────┼────────────┤     │
       │    │            │     │
       │  Pred Box       │     │
       │    └────────────┼─────┘
       └─────────────────┘     │
                                │
                    并集面积 = 两个框总面积 - 交集
```

**公式**：
```
IoU = Area(Pred ∩ GT) / Area(Pred ∪ GT)

if IoU >= 0.8:  命中 (True Positive)
else:           未命中 (False Positive 或 False Negative)
```

**为什么阈值是 0.8 而非 COCO 的 0.5？**
- 越野场景目标物理边界明确（车辆、行人、障碍）
- 高阈值能更好区分算法水平
- 0.8 意味着预测框与真值框的重叠面积至少要占并集 80%

### 2.2 精度分（125 分）

```
precision = TP / (TP + FP)
recall    = TP / (TP + FN)
F1        = 2 * P * R / (P + R)

score_accuracy = F1 * 125
```

**匹配算法**（Hungarian）：
- 对每个 timestamp，把该帧的所有预测框和真值框做二分图匹配
- 匹配代价 = 1 - IoU
- 阈值以下的匹配视为 FP/FN

### 2.3 时间分（125 分）

```
if 参赛软件在总时限 T 内完成答题:
    score_time = (T - t_used) / T * 125
else:
    score_time = 0
```

### 2.4 图像分（125 分）

- 参赛软件必须**处理每一帧图像**（不能只处理关键帧）
- 每帧都要有对应的答案（哪怕是空答案）
- 缺失帧按比例扣分：`score_image = 125 * (处理帧数 / 总帧数)`

### 2.5 定位分（125 分）

对**已命中的目标**，额外计算 3D 定位误差：

```
Loc_error = Euclidean_distance(pred_center_3d, gt_center_3d)

线性插值计分：
  if error <= Lmin (0.5m):   125 分（满分）
  if Lmin < error < Lmax (1.0m):  线性递减
  if error >= Lmax:          0 分

公式（中间段）：
  score = 125 * (Lmax - error) / (Lmax - Lmin)
```

## 3. SU 场景理解与定位评分

### 3.1 三个子任务

| 子任务 | 定义 | 满分 |
|--------|------|------|
| **本地定位** | 相对里程计（自车运动轨迹）| 150 |
| **全局定位** | 与 GT 地图对齐的绝对坐标 | 200 |
| **场景理解** | 可通行区域/障碍物分类 | 150 |

### 3.2 本地定位评分

```
每 5s 采样一次自车位姿 (x, y, θ)
计算与 GT 位姿的误差

局部得分 (Lp):
  if error <= 0.5m: 满分
  if 0.5 < error < 1.0m: 线性递减
  if error >= 1.0m: 0

全部得分 (La):
  整个任务轨迹的累积误差

Loc_score = 0.7 * Lp + 0.3 * La
时间分：5s 内响应得满分，超时线性递减
```

### 3.3 全局定位评分

- 阈值 `Lp_min=0.5m, Lp_max=2.0m`
- 更严格：因为全局定位需要与地图匹配，误差影响下游规划
- **时间阈值 100s**：从开始任务到输出首个全局位姿的最大时长

### 3.4 场景理解评分

- 输入：某帧多传感器数据
- 输出：语义栅格图（每个 cell 标注可通行/障碍物类型）
- 评分：**语义栅格与 GT 的 mIoU**

## 4. 评分公式汇总（伪代码）

```python
def compute_od_score(pred_stream, gt_stream, time_used, total_frames):
    # 精度分
    tp, fp, fn = match_by_iou(pred_stream, gt_stream, threshold=0.8)
    precision = tp / max(tp + fp, 1)
    recall    = tp / max(tp + fn, 1)
    f1        = 2 * precision * recall / max(precision + recall, 1e-6)
    score_acc = f1 * 125

    # 时间分
    score_time = (1 - time_used / T) * 125

    # 图像分
    score_image = (processed_frames / total_frames) * 125

    # 定位分（只算命中的目标）
    loc_errors = [iou_filtered_3d_dist(p, g) for p, g in matched_pairs]
    score_loc = mean([interp(e, 0.5, 1.0) * 125 for e in loc_errors])

    return score_acc + score_time + score_image + score_loc


def compute_su_score(localization_output, gt_trajectory):
    # 本地定位
    local_errors = sample_every_5s(localization_output, gt_trajectory)
    local_partial = mean([interp_loc(e, 0.5, 1.0) for e in local_errors]) * 100
    local_time    = time_score(localization_output.first_response, 5000) * 50
    local_score   = 0.7 * local_partial + 0.3 * global_partial

    # 场景理解
    scene_miou    = compute_miou(pred_grid, gt_grid)
    scene_score   = scene_miou * 100 + time_score(...) * 50

    # 全局定位
    global_err    = global_localization_error(...)
    global_score  = interp(global_err, 0.5, 2.0) * 150 + time_score(..., 100s) * 50

    return local_score + scene_score + global_score
```

## 5. 边界情况处理

| 情况 | 处理 |
|------|------|
| 答案格式错误（JSON 解析失败）| 记 0 分，写入异常日志 |
| 答案坐标越界 | 裁剪到图像范围内再计算 IoU |
| 答案超出时间窗 | 丢弃，记 0 分 |
| 一帧内多预测框匹配同一 GT | 置信度高的胜出 |
| GT 框被完全遮挡（>70%）| 允许不检测（不计 FN）|

## 6. 参数调优与验证

评分系统的参数（如 IoU 阈值 0.8、Lmax 等）由**赛事组委会**给定。技术组负责：

- 参数变更时的**回归测试**：跑一批历史数据对比新旧分数
- **边界情况**：验证 IoU=0.79 vs 0.80 的分数跳变合理
- **性能**：单帧评分耗时 < 10ms（保证实时性）

## 7. 配置示例（脱敏后）

```yaml
# judge-app.yml（部分）
IoUThreshold: 0.8
ODLocThreshLmin: 0.5
ODLocThreshLmax: 1.0

SULocThreshLpmin: 0.5
SULocThreshLpmax: 1.0
SULocThreshLamin: 0.5
SULocThreshLamax: 1.0
SULocWeightp: 0.7
SULocWeighta: 0.3
SULocTimethresh: 5000
SUMapTimethresh: 5000
SUGlobalTimethresh: 100000
SUGlobalThreshLpmin: 0.5
SUGlobalThreshLpmax: 2.0

maxscore: 1000
scores:
 - 500
 - 500
```
