# 数据加密方案设计

## 1. 设计目标

**威胁模型**：
- 参赛队在赛前拿到加密考题包（提前 1-7 天）
- 参赛队不能提前解密，不能提前预知答案
- 参赛队不能反推真值（GT 数据）

**约束**：
- 比赛现场算法必须能**实时解密**（CPU 不能成为瓶颈）
- 加密后文件大小必须**不变**（保持时间戳同步机制）
- 加密密钥不能提前下发

## 2. 加密方案

### 2.1 三类数据的差异化处理

| 数据类型 | 加密算法 | Token 粒度 | 原因 |
|----------|----------|-----------|------|
| **CSV 文本**（GPS/IMU/CAN/时间戳）| ASCII 偏移 | 每行独立 token | 文本类，加密代价小 |
| **二进制**（HDL-32 PCAP / URG LMS）| 字节循环移位 | 每时间戳一个 token | 保持文件大小不变 |
| **Ground Truth** | ASCII 偏移，**固定 token** | 全局一个 | 与考题数据混淆，避免被识别 |
| **相机图像** | 不加密 | — | 加解密开销太大，且算法实时处理需要 |

### 2.2 核心加密算法

```python
def encrypt_string(s: str, token_min: int, token_max: int) -> tuple[str, int]:
    """
    对字符串每个字符做 ASCII 偏移
    """
    token = random.randint(token_min, token_max)  # 通常 [1, 9]
    encrypted = ''.join(chr(ord(c) + token) for c in s)
    return encrypted, token

def encrypt_shift_byte(byte: int, token: int) -> int:
    """
    对二进制字节做循环移位
    """
    return (byte + token) % 256
```

**解密对应操作**：
```python
def decrypt_string(s, token):
    return ''.join(chr(ord(c) - token) for c in s)

def decrypt_shift_byte(byte, token):
    return (byte - token) % 256
```

## 3. Token 机制

### 3.1 Token 生成与分发

```
【赛前】
  加密工具（裁判组）
    ├── 生成随机 token（每条数据独立）
    └── 写入两个文件：
        ├── 加密后的考题包（分发给参赛队）
        └── token.csv（裁判组内部保留）

【比赛】
  裁判客户端持有 token.csv
    ↓
  按 timestamp 每 100ms 通过 TCP 7002 下发给参赛软件
    ↓
  参赛软件实时解密处理
```

### 3.2 token.csv 结构

```csv
timestamp,type,token
1683356400123,camera_ts,7
1683356400123,gps,3
1683356400123,imu,5
1683356400123,hdl32,2
1683356400123,urg,9
1683356400223,camera_ts,4
...
```

**字段说明**：
- `timestamp`：数据帧的时间戳（毫秒）
- `type`：传感器类型
- `token`：偏移量 ∈ [1, 9]

### 3.3 关键设计：为什么 token 空间这么小？

- **在线解密** 需要在 100ms 内完成
- Token 由裁判下发时占用字节数要小（1 byte 足够）
- 参赛软件不需要"猜"，只要接收即可
- **代价**：token 空间只有 9，理论上可暴力破解（见第 5 节改进）

## 4. 加密实例

### 4.1 CSV 行加密

**原始数据**：
```
1683356400,GPS:31.2304,121.4737,45.6
```

**用 token=3 加密**：
```
1683356400JUQ3456734/4568:673/9$6*9
```
（每个字符 +3）

**接收端解密**：
```python
decrypted = ''.join(chr(ord(c) - 3) for c in encrypted_line)
```

### 4.2 PCAP 帧加密

**原始**：32 线激光雷达一帧约 1200 bytes
**加密**：每字节 `(b + token) % 256`
**大小**：不变（保持时间戳同步）

## 5. 安全性分析

### 5.1 当前方案的保护能力

| 攻击方式 | 是否防住 |
|----------|---------|
| 拿到加密数据后离线破解 | ✅ 无 token 无法解密 |
| 通过网络嗅探 token | ⚠️ 未加密传输，可嗅探 |
| 暴力枚举 9 种 token 破解 | ❌ **理论可行** |
| 从加密数据反推 GT | ✅ GT 用固定 token，混淆有效 |
| 修改答案 timestamp 提前上传 | ⚠️ 依赖裁判端时序校验 |

### 5.2 已知弱点

1. **凯撒密码可暴力破解**：token ∈ [1,9]，9 种组合秒级枚举
2. **无认证**：参赛软件可伪造答案，只要格式正确
3. **无完整性校验**：加密数据可被篡改而裁判端不感知

## 6. 改进方案（如果重做）

### 6.1 密码学升级

- 使用 **AES-256-GCM** 对称加密
- 每帧独立密钥 = HKDF(赛事根密钥, timestamp)
- 密钥通过 **ECDH** 密钥交换协议下发（防中间人）

### 6.2 传输层保护

- 所有 Socket 通信改用 **TLS** 通道
- Token 消息加 **签名**（HMAC-SHA256）

### 6.3 反重放

- 每条答案携带 **服务器 nonce** + **时间戳**
- 裁判端校验 nonce 与 timestamp 匹配

### 6.4 更严格的 GT 隔离

- GT 加密在**独立硬件**（HSM/TEE）中解密
- 参赛软件端永远接触不到 GT 数据

## 7. 加密工具使用

参见 [tools/data-encryptor/README.md](../tools/data-encryptor/README.md)
