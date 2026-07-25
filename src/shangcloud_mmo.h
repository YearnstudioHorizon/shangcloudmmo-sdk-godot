#ifndef SHANGCLOUD_MMO_H
#define SHANGCLOUD_MMO_H

#include "mmo_message_queue.h"
#include "mmo_transport.h"
#include "mmo_interp_engine.h"

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/templates/vector.hpp>
#include <mutex>

namespace godot {

class ShangCloudMMO : public Node, public MMOTransportListener {
	GDCLASS(ShangCloudMMO, Node)

public:
	enum Protocol {
		PROTOCOL_TCP = 0,
		PROTOCOL_UDP = 1,
	};

	enum ConnectionState {
		STATE_DISCONNECTED = 0,
		STATE_CONNECTING = 1,
		STATE_HANDSHAKE = 2,
		STATE_AUTHENTICATING = 3,
		STATE_CONNECTED = 4,
		STATE_ERROR = 5,
	};

	ShangCloudMMO();
	~ShangCloudMMO();

	void _process(double p_delta) override;
	void _notification(int p_what);

	// Properties
	void set_protocol(Protocol p_protocol);
	Protocol get_protocol() const;
	void set_connect_key(const String &p_key);
	String get_connect_key() const;
	void set_edge_host(const String &p_host);
	String get_edge_host() const;
	void set_edge_port(int p_port);
	int get_edge_port() const;
	ConnectionState get_state() const;

	// Methods
	void connect_to_edge();
	void disconnect_from_edge();
	void send_message(const String &p_message);
	void send_raw(const PackedByteArray &p_data);

	// 高级封装：广播消息（wire 格式：{"uid","message","extra"}）
	void send_broadcast(const String &p_uid, const String &p_message, const String &p_extra);
	// 高级封装：同步变量（wire 格式：{"type":"__sync_var__","uid","vars","interp"}）
	void send_sync_var(const String &p_uid, const Dictionary &p_vars, const PackedStringArray &p_interp);
	// 高级封装：发送加入房间通知（wire 格式：{"type":"__join__","uid","nickname"}）
	void send_join_announcement(const String &p_uid, const String &p_nickname);

	// 查询房间成员：发送 __ping__，服务端以 __pong__:N:membersJSON 响应并更新本地缓存
	void query_members();
	// 返回本地缓存的成员列表（Array[{uid, nickname}]，参考扩展 getMemberList）
	Array get_member_list() const;
	// 返回最近一次 __pong__ 的房间人数（无缓存时为成员列表长度）
	int get_room_user_count() const;

	// 插帧读取：返回 uid 的同步变量平滑后的 current 值；未插帧时回退到最近原始值
	double get_sync_var(const String &p_uid, const String &p_name) const;
	// 插帧读取：返回 uid 的同步变量原始字符串值（不做插帧）
	String get_sync_var_raw(const String &p_uid, const String &p_name) const;
	// 清理指定 uid 的插帧状态（玩家离开时调用）
	void clear_sync_var_state(const String &p_uid);

protected:
	static void _bind_methods();

	// MMOTransportListener interface
	void on_transport_connected() override;
	void on_transport_disconnected() override;
	void on_transport_error(const String &p_error) override;
	void on_transport_server_closed() override;

private:
	Protocol protocol = PROTOCOL_TCP;
	String connect_key;
	String edge_host;
	int edge_port = 0;

	MMOTransport *transport = nullptr;
	MMOMessageQueue message_queue;
	MmoInterpEngine interp_engine;

	// 房间成员缓存（参考 extension 的 window._mmoMembers）
	Array members;
	int room_user_count = 0;

	// 传输层回调若来自非主线程，先入队，_process 再 emit（与 Unity 一致）
	enum PendingEventKind {
		PENDING_CONNECTED = 0,
		PENDING_DISCONNECTED = 1,
		PENDING_ERROR = 2,
		PENDING_SERVER_CLOSED = 3,
	};
	struct PendingTransportEvent {
		PendingEventKind kind = PENDING_CONNECTED;
		String error;
	};
	Vector<PendingTransportEvent> pending_events;
	std::mutex pending_events_mutex;

	void cleanup_transport();
	void process_business_message(const String &p_message);
	void clear_members();
	void upsert_member(const String &p_uid, const String &p_nickname);
	void remove_member(const String &p_uid);
	void apply_members_from_pong(const String &p_members_json, int p_count);
	void enqueue_transport_event(PendingEventKind p_kind, const String &p_error = String());
	void drain_transport_events();
};

} // namespace godot

VARIANT_ENUM_CAST(ShangCloudMMO::Protocol);
VARIANT_ENUM_CAST(ShangCloudMMO::ConnectionState);

#endif
