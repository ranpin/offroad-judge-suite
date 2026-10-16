# 数据加密工具

**用途**：将标准格式的自动驾驶考题数据加密，生成加密考题包 + 对应的 token 文件。

**注意**：本工具生成的是**离线调试版**加密数据，与现场系统的加密结果不完全一致。仅用于前期联调。

---

## 1. 输入格式要求

考题需按如下目录组织（以 `Raw-Example-001` 为例）：

```
Raw-Example-001/
├── Raw-Example-001-Calib/                   # 标定参数（不加密）
│   ├── GPS.txt
│   ├── HDL32-E.txt / HDL32-I.txt
│   ├── Laser.txt / Odom.txt / Steer.txt
├── Raw-Example-001-Camera/                  # 图片（不加密）
│   └── *.jpg
├── Raw-Example-001-Groundtruth/             # 真值（加密，方式对参赛队不透明）
│   ├── gt_detection.csv
│   └── gt_global.csv
├── Raw-Example-001-Camera-Timestamp.csv     # 加密
├── Raw-Example-001-Canbus-Gear.csv          # 加密
├── Raw-Example-001-Canbus-Odom.csv          # 加密
├── Raw-Example-001-Canbus-Steer.csv         # 加密
├── Raw-Example-001-GPS.csv                  # 加密
├── Raw-Example-001-IMU.csv                  # 加密
├── Raw-Example-001-HDL32.pcap               # 加密（二进制）
├── Raw-Example-001-URG.lms                  # 加密（二进制）
├── Raw-Example-001-infor.xlsx               # 不加密
└── Raw-Example-001-Task.txt                 # 不加密
```

## 2. 运行

### 2.1 修改 `config.ini`

```ini
[Paths]
raw_data_dir=./data/exams/Raw-Example-001
encrypt_data_dir=./data/exams/Raw-Example-001-Enc
token_file_name=./data/exams/Raw-Example-001-Enc/token.csv
groundtruth_file_name=./data/exams/Raw-Example-001/Raw-Example-001-Groundtruth/gt_detection.csv

[Names]
data_name=Raw-Example-001
```

### 2.2 执行

```bash
python encrypt_data.py
```

预计耗时：单套考题约 **5 分钟**（视数据量而定）。

## 3. 输出验证

生成后需检查：

1. `token.csv` 是否包含每行/每帧的独立 token
2. 打开加密后的 `*.csv` 文件，内容应为乱码（每字符 ASCII 偏移）
3. 对比加密前后 `.pcap` / `.lms` 文件大小应**完全一致**（循环移位不改变大小）

## 4. 加密算法核心

```python
def encrypt_string(s, token_min, token_max):
    token = random.randint(token_min, token_max)
    encrypted = ''.join(chr(ord(c) + token) for c in s)
    return encrypted, token

def encrypt_shift_byte(byte, token):
    return (byte + token) % 256
```

## 5. 依赖

- Python 3.7+
- **无第三方依赖**（仅使用 `os`, `configparser`, `shutil`, `random`, `struct`, `time`, `copy`）

## 6. 相关文档

- 加密方案设计：[../../docs/encryption-scheme.md](../../docs/encryption-scheme.md)
- 数据格式规范：[../../README.md](../../README.md#数据格式规范)
