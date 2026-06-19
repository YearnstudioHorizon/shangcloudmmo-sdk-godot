#ifndef MMO_TRANSPORT_H
#define MMO_TRANSPORT_H

#include "mmo_crypto.h"
#include "mmo_message_queue.h"

#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/string.hpp>

using namespace godot;

class MMOTransportListener {
public:
	virtual ~MMOTransportListener() = default;
	virtual void on_transport_connected() = 0;
	virtual void on_transport_disconnected() = 0;
	virtual void on_transport_error(const String &p_error) = 0;
	virtual void on_transport_server_closed() = 0;
};

class MMOTransport {
public:
	enum State {
		STATE_IDLE,
		STATE_CONNECTING,
		STATE_HANDSHAKE,
		STATE_AUTHENTICATING,
		STATE_CONNECTED,
		STATE_DISCONNECTED,
		STATE_ERROR,
	};

	virtual ~MMOTransport() = default;

	virtual void connect_to(const String &p_host, int p_port,
			const String &p_connect_key) = 0;
	virtual void disconnect() = 0;
	virtual void poll(double p_delta) = 0;
	virtual void send(const PackedByteArray &p_data) = 0;

	State get_state() const { return state; }

	void set_message_queue(MMOMessageQueue *p_queue) { msg_queue = p_queue; }
	void set_listener(MMOTransportListener *p_listener) { listener = p_listener; }

protected:
	State state = STATE_IDLE;
	PackedByteArray aes_key;
	String connect_key;
	MMOMessageQueue *msg_queue = nullptr;
	MMOTransportListener *listener = nullptr;
	double heartbeat_timer = 0.0;

	static constexpr double HEARTBEAT_INTERVAL = 3.0;
	static constexpr int MAX_FRAME_SIZE = 1024 * 1024; // 1MB

	PackedByteArray encrypt_data(const PackedByteArray &p_data);
	PackedByteArray decrypt_data(const PackedByteArray &p_encrypted);
	void process_decrypted_message(const PackedByteArray &p_decrypted);

	void send_heartbeat();
};

#endif
