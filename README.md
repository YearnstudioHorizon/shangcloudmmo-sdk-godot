## ShangCloudMMO SDK for Godot

基于 C++ GDExtension 的 ShangCloud MMO 实时传输 SDK，提供 AES-256-GCM 加密的 TCP/UDP 通信。

该 SDK 仅负责建立与边缘节点的加密连接、收发消息。获取 `connect_key` 等房间信息需通过 `shangcloud-sdk` 系列 HTTP SDK 完成。

### 获取链接库

前往 `GitHub Actions` 下载对应平台的构建产物（Windows / Linux / macOS）。

### 集成到 Godot 项目

1. 将下载的动态库放入项目目录（如 `res://bin/`）

2. 创建 `.gdextension` 文件：

```ini
[configuration]
entry_symbol = "shangcloud_mmo_library_init"
compatibility_minimum = "4.2"

[libraries]
windows.debug.x86_64 = "res://bin/shangcloud_mmo.dll"
windows.release.x86_64 = "res://bin/shangcloud_mmo.release.dll"
linux.debug.x86_64 = "res://bin/libshangcloud_mmo.so"
linux.release.x86_64 = "res://bin/libshangcloud_mmo.release.so"
macos.debug = "res://bin/libshangcloud_mmo.dylib"
macos.release = "res://bin/libshangcloud_mmo.release.dylib"
```

3. 在场景中添加 `ShangCloudMMO` 节点

### 快速上手

```gdscript
@onready var mmo: ShangCloudMMO = $ShangCloudMMO

func _ready():
    # 选择协议
    mmo.protocol = ShangCloudMMO.PROTOCOL_TCP  # 或 PROTOCOL_UDP

    # 设置连接参数（通过 HTTP SDK 获取）
    mmo.connect_key = "a1b2c3d4..."
    mmo.edge_host = "edge.example.com"
    mmo.edge_port = 8080

    # 连接信号
    mmo.connected.connect(_on_connected)
    mmo.disconnected.connect(_on_disconnected)
    mmo.message_received.connect(_on_message)
    mmo.user_joined.connect(_on_user_joined)
    mmo.user_left.connect(_on_user_left)
    mmo.server_closed.connect(_on_server_closed)
    mmo.connection_error.connect(_on_error)

    # 发起连接
    mmo.connect_to_edge()

func _on_connected():
    print("已连接")
    mmo.send_message(JSON.stringify({
        "type": "__join__",
        "uid": "player1",
        "nickname": "玩家一号"
    }))

func _on_message(message: String):
    print("收到消息: ", message)

func _on_user_joined(uid: String, nickname: String):
    print(uid, " 加入了房间 (", nickname, ")")

func _on_user_left(uid: String):
    print(uid, " 离开了房间")

func _on_server_closed():
    print("服务器关闭了连接")

func _on_disconnected():
    print("已断开连接")

func _on_error(error: String):
    printerr("连接错误: ", error)
```

### API

#### 属性

| 属性 | 类型 | 说明 |
|------|------|------|
| `protocol` | `PROTOCOL_TCP` / `PROTOCOL_UDP` | 传输协议，连接前设置 |
| `connect_key` | `String` | 认证密钥，通过 HTTP API 获取 |
| `edge_host` | `String` | 边缘节点地址 |
| `edge_port` | `int` | 边缘节点端口 |

#### 方法

| 方法 | 说明 |
|------|------|
| `connect_to_edge()` | 建立连接（自动完成握手、密钥派生、认证） |
| `disconnect_from_edge()` | 断开连接 |
| `send_message(message: String)` | 发送文本消息（自动加密） |
| `send_raw(data: PackedByteArray)` | 发送二进制数据（自动加密） |
| `get_state() -> ConnectionState` | 获取当前连接状态 |

#### 信号

| 信号 | 参数 | 说明 |
|------|------|------|
| `connected` | 无 | 认证成功，可以收发消息 |
| `disconnected` | 无 | 连接已断开 |
| `connection_error` | `error: String` | 连接或认证失败 |
| `message_received` | `message: String` | 收到文本消息 |
| `raw_message_received` | `data: PackedByteArray` | 收到二进制消息 |
| `user_joined` | `uid: String, nickname: String` | 有用户加入房间 |
| `user_left` | `uid: String` | 有用户离开房间 |
| `server_closed` | 无 | 服务端主动关闭连接 |

#### 连接状态枚举

| 状态 | 值 | 说明 |
|------|------|------|
| `STATE_DISCONNECTED` | 0 | 未连接 |
| `STATE_CONNECTING` | 1 | 正在建立 Socket 连接 |
| `STATE_HANDSHAKE` | 2 | 密钥种子已发送 |
| `STATE_AUTHENTICATING` | 3 | connect_key 已发送，等待认证 |
| `STATE_CONNECTED` | 4 | 已连接，可以收发数据 |
| `STATE_ERROR` | 5 | 连接出错 |

### 协议细节

- 加密算法：AES-256-GCM，12 字节随机 Nonce，16 字节 GCM Tag
- 密钥派生：SHA-256(32 字节随机 Seed)
- 防重放：载荷前 8 字节为毫秒级时间戳（大端序），20 秒滑动窗口
- TCP 帧格式：`[4B 大端长度前缀][加密载荷]`，最大 1MB
- UDP 包格式：`[8B 大端 ConnectId][加密载荷]`
- 心跳：每 3 秒自动发送 `__hb__`

### 从源码构建

```bash
# 克隆（含 godot-cpp 子模块）
git clone --recursive https://github.com/YourOrg/shangcloud-sdk-mmo-godot.git
cd shangcloud-sdk-mmo-godot

# 构建（需要 C++ 编译器 + SCons + Python）
scons platform=windows target=template_debug    # Windows
scons platform=linux target=template_debug      # Linux
scons platform=macos target=template_debug      # macOS

# 产物在 bin/ 目录
```

### 许可证

MIT License
