extends Node

@onready var mmo: ShangCloudMMO = $ShangCloudMMO

func _ready():
	mmo.protocol = ShangCloudMMO.PROTOCOL_TCP
	mmo.connect_key = "your_connect_key_here"
	mmo.edge_host = "localhost"
	mmo.edge_port = 8080

	mmo.connected.connect(_on_connected)
	mmo.disconnected.connect(_on_disconnected)
	mmo.message_received.connect(_on_message)
	mmo.user_joined.connect(_on_user_joined)
	mmo.user_left.connect(_on_user_left)
	mmo.server_closed.connect(_on_server_closed)
	mmo.connection_error.connect(_on_error)

	print("[MMO] Connecting to edge server...")
	mmo.connect_to_edge()

func _on_connected():
	print("[MMO] Connected!")
	mmo.send_message(JSON.stringify({
		"type": "__join__",
		"uid": "player1",
		"nickname": "Player One"
	}))

func _on_message(message: String):
	print("[MMO] Message: ", message)

func _on_user_joined(uid: String, nickname: String):
	print("[MMO] User joined: ", uid, " (", nickname, ")")

func _on_user_left(uid: String):
	print("[MMO] User left: ", uid)

func _on_server_closed():
	print("[MMO] Server closed the connection")

func _on_disconnected():
	print("[MMO] Disconnected")

func _on_error(error: String):
	printerr("[MMO] Error: ", error)
