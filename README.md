## ShangCloudMMO SDK for Godot
该SDK仅用于建立MMO连接, 获取`connect_key`需要使用`shangcloud-sdk`系列

### 示例.gdextension

```ini
[configuration]
entry_symbol = "shangcloud_mmo_library_init"
compatibility_minimum = "4.2"

[libraries]
windows.debug.x86_64 = "res://bin/shangcloud_mmo.dll"
windows.release.x86_64 = "res://bin/shangcloud_mmo.release.dll"
```

### 获取链接库

请前往`Github Action`获取