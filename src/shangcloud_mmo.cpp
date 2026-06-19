#include "shangcloud_mmo.h"
#include "mmo_tcp_transport.h"
#include "mmo_udp_transport.h"

#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <cstring>

namespace godot {

ShangCloudMMO::ShangCloudMMO() {
}

ShangCloudMMO::~ShangCloudMMO() {
	cleanup_transport();
}

void ShangCloudMMO::_bind_methods() {
	// Properties
	ClassDB::bind_method(D_METHOD("set_protocol", "protocol"), &ShangCloudMMO::set_protocol);
	ClassDB::bind_method(D_METHOD("get_protocol"), &ShangCloudMMO::get_protocol);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "protocol", PROPERTY_HINT_ENUM, "TCP,UDP"),
			"set_protocol", "get_protocol");

	ClassDB::bind_method(D_METHOD("set_connect_key", "key"), &ShangCloudMMO::set_connect_key);
	ClassDB::bind_method(D_METHOD("get_connect_key"), &ShangCloudMMO::get_connect_key);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "connect_key"),
			"set_connect_key", "get_connect_key");

	ClassDB::bind_method(D_METHOD("set_edge_host", "host"), &ShangCloudMMO::set_edge_host);
	ClassDB::bind_method(D_METHOD("get_edge_host"), &ShangCloudMMO::get_edge_host);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "edge_host"),
			"set_edge_host", "get_edge_host");

	ClassDB::bind_method(D_METHOD("set_edge_port", "port"), &ShangCloudMMO::set_edge_port);
	ClassDB::bind_method(D_METHOD("get_edge_port"), &ShangCloudMMO::get_edge_port);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "edge_port"),
			"set_edge_port", "get_edge_port");

	ClassDB::bind_method(D_METHOD("get_state"), &ShangCloudMMO::get_state);

	// Methods
	ClassDB::bind_method(D_METHOD("connect_to_edge"), &ShangCloudMMO::connect_to_edge);
	ClassDB::bind_method(D_METHOD("disconnect_from_edge"), &ShangCloudMMO::disconnect_from_edge);
	ClassDB::bind_method(D_METHOD("send_message", "message"), &ShangCloudMMO::send_message);
	ClassDB::bind_method(D_METHOD("send_raw", "data"), &ShangCloudMMO::send_raw);

	// Enums
	BIND_ENUM_CONSTANT(PROTOCOL_TCP);
	BIND_ENUM_CONSTANT(PROTOCOL_UDP);
	BIND_ENUM_CONSTANT(STATE_DISCONNECTED);
	BIND_ENUM_CONSTANT(STATE_CONNECTING);
	BIND_ENUM_CONSTANT(STATE_HANDSHAKE);
	BIND_ENUM_CONSTANT(STATE_AUTHENTICATING);
	BIND_ENUM_CONSTANT(STATE_CONNECTED);
	BIND_ENUM_CONSTANT(STATE_ERROR);

	// Signals
	ADD_SIGNAL(MethodInfo("connected"));
	ADD_SIGNAL(MethodInfo("disconnected"));
	ADD_SIGNAL(MethodInfo("connection_error",
			PropertyInfo(Variant::STRING, "error")));

	ADD_SIGNAL(MethodInfo("message_received",
			PropertyInfo(Variant::STRING, "message")));
	ADD_SIGNAL(MethodInfo("raw_message_received",
			PropertyInfo(Variant::PACKED_BYTE_ARRAY, "data")));

	ADD_SIGNAL(MethodInfo("user_joined",
			PropertyInfo(Variant::STRING, "uid"),
			PropertyInfo(Variant::STRING, "nickname")));
	ADD_SIGNAL(MethodInfo("user_left",
			PropertyInfo(Variant::STRING, "uid")));
	ADD_SIGNAL(MethodInfo("server_closed"));
}

void ShangCloudMMO::_process(double p_delta) {
	if (transport) {
		transport->poll(p_delta);

		// Drain message queue and emit signals
		Vector<MMOMessage> messages = message_queue.drain();
		for (int i = 0; i < messages.size(); i++) {
			const MMOMessage &msg = messages[i];
			switch (msg.type) {
				case MMOMessage::TEXT:
					process_business_message(msg.text);
					break;
				case MMOMessage::BINARY:
					emit_signal("raw_message_received", msg.data);
					break;
			}
		}
	}
}

void ShangCloudMMO::_notification(int p_what) {
	if (p_what == NOTIFICATION_EXIT_TREE) {
		cleanup_transport();
	}
}

// Properties

void ShangCloudMMO::set_protocol(Protocol p_protocol) {
	protocol = p_protocol;
}

ShangCloudMMO::Protocol ShangCloudMMO::get_protocol() const {
	return protocol;
}

void ShangCloudMMO::set_connect_key(const String &p_key) {
	connect_key = p_key;
}

String ShangCloudMMO::get_connect_key() const {
	return connect_key;
}

void ShangCloudMMO::set_edge_host(const String &p_host) {
	edge_host = p_host;
}

String ShangCloudMMO::get_edge_host() const {
	return edge_host;
}

void ShangCloudMMO::set_edge_port(int p_port) {
	edge_port = p_port;
}

int ShangCloudMMO::get_edge_port() const {
	return edge_port;
}

ShangCloudMMO::ConnectionState ShangCloudMMO::get_state() const {
	if (!transport) {
		return STATE_DISCONNECTED;
	}

	switch (transport->get_state()) {
		case MMOTransport::STATE_IDLE:
			return STATE_DISCONNECTED;
		case MMOTransport::STATE_CONNECTING:
			return STATE_CONNECTING;
		case MMOTransport::STATE_HANDSHAKE:
			return STATE_HANDSHAKE;
		case MMOTransport::STATE_AUTHENTICATING:
			return STATE_AUTHENTICATING;
		case MMOTransport::STATE_CONNECTED:
			return STATE_CONNECTED;
		case MMOTransport::STATE_ERROR:
			return STATE_ERROR;
		case MMOTransport::STATE_DISCONNECTED:
			return STATE_DISCONNECTED;
		default:
			return STATE_DISCONNECTED;
	}
}

// Methods

void ShangCloudMMO::connect_to_edge() {
	ERR_FAIL_COND_MSG(edge_host.is_empty(), "edge_host must be set before connecting");
	ERR_FAIL_COND_MSG(edge_port <= 0, "edge_port must be set before connecting");
	ERR_FAIL_COND_MSG(connect_key.is_empty(), "connect_key must be set before connecting");

	cleanup_transport();

	if (protocol == PROTOCOL_TCP) {
		transport = new MMOTcpTransport();
	} else {
		transport = new MMOUdpTransport();
	}

	transport->set_message_queue(&message_queue);
	transport->set_listener(this);
	transport->connect_to(edge_host, edge_port, connect_key);
}

void ShangCloudMMO::disconnect_from_edge() {
	if (transport) {
		transport->disconnect();
	}
}

void ShangCloudMMO::send_message(const String &p_message) {
	ERR_FAIL_COND_MSG(!transport || transport->get_state() != MMOTransport::STATE_CONNECTED,
			"Cannot send message: not connected");

	CharString utf8 = p_message.utf8();
	PackedByteArray data;
	data.resize(utf8.length());
	memcpy(data.ptrw(), utf8.ptr(), utf8.length());
	transport->send(data);
}

void ShangCloudMMO::send_raw(const PackedByteArray &p_data) {
	ERR_FAIL_COND_MSG(!transport || transport->get_state() != MMOTransport::STATE_CONNECTED,
			"Cannot send data: not connected");

	transport->send(p_data);
}

// Transport listener callbacks

void ShangCloudMMO::on_transport_connected() {
	emit_signal("connected");
}

void ShangCloudMMO::on_transport_disconnected() {
	emit_signal("disconnected");
}

void ShangCloudMMO::on_transport_error(const String &p_error) {
	emit_signal("connection_error", p_error);
}

void ShangCloudMMO::on_transport_server_closed() {
	emit_signal("server_closed");
}

// Private

void ShangCloudMMO::cleanup_transport() {
	if (transport) {
		transport->disconnect();
		delete transport;
		transport = nullptr;
	}
}

void ShangCloudMMO::process_business_message(const String &p_message) {
	// Parse __join__ and __leave__ system events
	if (p_message.begins_with("{")) {
		Ref<JSON> json;
		json.instantiate();
		Error err = json->parse(p_message);
		if (err == OK) {
			Variant data = json->get_data();
			if (data.get_type() == Variant::DICTIONARY) {
				Dictionary dict = data;
				if (dict.has("type")) {
					String type = dict["type"];
					if (type == "__join__") {
						String uid = dict.get("uid", "");
						String nickname = dict.get("nickname", "");
						emit_signal("user_joined", uid, nickname);
						return;
					}
					if (type == "__leave__") {
						String uid = dict.get("uid", "");
						emit_signal("user_left", uid);
						return;
					}
				}
			}
		}
	}

	// Regular business message
	emit_signal("message_received", p_message);
}

} // namespace godot
