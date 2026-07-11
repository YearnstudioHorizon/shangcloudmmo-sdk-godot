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
    mmo.broadcast_received.connect(_on_broadcast)
    mmo.sync_var_received.connect(_on_sync_var)
    mmo.sync_var_interpolated.connect(_on_sync_var_interpolated)
    mmo.user_joined.connect(_on_user_joined)
    mmo.user_left.connect(_on_user_left)
    mmo.server_closed.connect(_on_server_closed)
    mmo.connection_error.connect(_on_error)

    # 发起连接
    mmo.connect_to_edge()

func _on_connected():
    print("已连接")
    # 连接成功后发送加入通知（封装版，等价于手动 JSON.stringify __join__）
    mmo.send_join_announcement("player1", "玩家一号")

    # 发送广播消息（封装版，wire 格式：{"uid","message","extra"}）
    mmo.send_broadcast("player1", "hello", "")

    # 发送同步变量（封装版，插帧列表 ["x"]）
    var vars := {"x": str(position.x), "y": str(position.y)}
    mmo.send_sync_var("player1", vars, PackedStringArray(["x", "y"]))

func _on_message(message: String):
    print("收到原始消息: ", message)

func _on_broadcast(uid: String, message: String, extra: String):
    print("收到广播: ", uid, " -> ", message, " (", extra, ")")

func _on_sync_var(uid: String, vars: Dictionary, interp: PackedStringArray):
    print("收到同步变量: ", uid, " vars=", vars, " interp=", interp)

func _on_sync_var_interpolated(uid: String, var_name: String, value: float):
    # 逐帧平滑推进，回写克隆体。如：
    # if var_name == "x": clone.position.x = value
    pass

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
| `send_message(message: String)` | 发送文本消息（自动加密），原始字符串作为明文帧 |
| `send_raw(data: PackedByteArray)` | 发送二进制数据（自动加密） |
| `send_broadcast(uid, message, extra)` | 封装广播消息，wire：`{"uid","message","extra"}`（无 `type`） |
| `send_sync_var(uid, vars: Dictionary, interp: PackedStringArray)` | 封装同步变量，wire：`{"type":"__sync_var__","uid","vars","interp"}` |
| `send_join_announcement(uid, nickname)` | 封装加入通知，wire：`{"type":"__join__","uid","nickname"}` |
| `get_sync_var(uid, name) -> float` | 读取插帧变量平滑后的当前值（移植自 core.js 的插帧引擎） |
| `get_sync_var_raw(uid, name) -> String` | 读取同步变量原始值（不做插帧） |
| `clear_sync_var_state(uid)` | 清理指定 uid 的插帧状态（玩家离开时调用） |
| `get_state() -> ConnectionState` | 获取当前连接状态 |

#### 信号

| 信号 | 参数 | 说明 |
|------|------|------|
| `connected` | 无 | 认证成功，可以收发消息 |
| `disconnected` | 无 | 连接已断开 |
| `connection_error` | `error: String` | 连接或认证失败 |
| `message_received` | `message: String` | 收到未识别为广播/同步变量的文本消息 |
| `raw_message_received` | `data: PackedByteArray` | 收到二进制消息 |
| `broadcast_received` | `uid, message, extra` | 收到广播消息（wire：`{"uid","message","extra"}`） |
| `sync_var_received` | `uid, vars: Dictionary, interp: PackedStringArray` | 收到同步变量（wire：`__sync_var__`） |
| `sync_var_interpolated` | `uid, var_name: String, value: float` | 插帧引擎逐帧推进时触发，回写克隆体即可 |
| `user_joined` | `uid: String, nickname: String` | 有用户加入房间 |
| `user_left` | `uid: String` | 有用户离开房间 |
| `server_closed` | 无 | 服务端主动关闭连接 |

#### 插帧引擎

`sync_var_interpolated` 信号在每帧 `_process` 中触发，对应 core.js 的 `_ensureInterpLoop`：
- 收到 `__sync_var__` 后，数值且在 `interp` 列表中的变量进入 `{current, target}` 状态
- 首次收到与瞬移阈值（差值绝对值 ≥ 200）时直接 snap
- 其余帧用帧率无关的指数平滑（基础因子 0.15，|diff|>50 时动态追赶，上限 0.8）把 current 逼近 target
- 全部收敛后自动休眠；玩家离开时 `__leave__` 会自动清理其状态
- 也可用 `get_sync_var(uid, name)` 在任意时刻读取平滑后的值

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
