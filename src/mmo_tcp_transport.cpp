#include "mmo_tcp_transport.h"

#include <godot_cpp/variant/utility_functions.hpp>

#include <cstring>

void MMOTcpTransport::connect_to(const String &p_host, int p_port,
		const String &p_connect_key) {
	connect_key = p_connect_key;
	seed_sent = false;
	auth_sent = false;
	heartbeat_timer = 0.0;
	recv_buffer.resize(0);

	tcp.instantiate();
	Error err = tcp->connect_to_host(p_host, p_port);
	if (err != OK) {
		state = STATE_ERROR;
		if (listener) {
			listener->on_transport_error("TCP connect_to_host failed");
		}
		return;
	}
	state = STATE_CONNECTING;
}

void MMOTcpTransport::disconnect() {
	if (tcp.is_valid()) {
		tcp->disconnect_from_host();
	}
	state = STATE_DISCONNECTED;
	recv_buffer.resize(0);
}

void MMOTcpTransport::poll(double p_delta) {
	if (!tcp.is_valid()) {
		return;
	}

	tcp->poll();
	StreamPeerTCP::Status status = tcp->get_status();

	if (status == StreamPeerTCP::STATUS_ERROR) {
		state = STATE_ERROR;
		if (listener) {
			listener->on_transport_error("TCP connection error");
			listener->on_transport_disconnected();
		}
		return;
	}

	if (status == StreamPeerTCP::STATUS_NONE) {
		if (state != STATE_IDLE && state != STATE_DISCONNECTED) {
			state = STATE_DISCONNECTED;
			if (listener) {
				listener->on_transport_disconnected();
			}
		}
		return;
	}

	if (status == StreamPeerTCP::STATUS_CONNECTING) {
		return; // still connecting
	}

	// STATUS_CONNECTED
	if (state == STATE_CONNECTING && !seed_sent) {
		// Step 1: Send 32-byte seed (plaintext)
		PackedByteArray seed = MMOCrypto::generate_seed();
		aes_key = MMOCrypto::derive_key(seed);
		send_raw_bytes(seed);
		seed_sent = true;
		state = STATE_HANDSHAKE;
	}

	if (state == STATE_HANDSHAKE && !auth_sent) {
		CharString key_utf8 = connect_key.utf8();
		PackedByteArray key_data;
		key_data.resize(key_utf8.length());
		memcpy(key_data.ptrw(), key_utf8.ptr(), key_utf8.length());

		PackedByteArray encrypted = encrypt_data(key_data);
		if (encrypted.size() == 0) {
			state = STATE_ERROR;
			if (listener) {
				listener->on_transport_error("Failed to encrypt connect_key");
			}
			return;
		}

		send_frame(encrypted);
		auth_sent = true;
		state = STATE_AUTHENTICATING;
	}

	// Read available data
	int available = tcp->get_available_bytes();
	if (available > 0) {
		Array result = tcp->get_data(available);
		Error err = (Error)(int)result[0];
		if (err == OK) {
			PackedByteArray data = result[1];
			int old_size = recv_buffer.size();
			recv_buffer.resize(old_size + data.size());
			memcpy(recv_buffer.ptrw() + old_size, data.ptr(), data.size());
			process_recv_buffer();
		}
	}

	// Heartbeat
	if (state == STATE_CONNECTED) {
		heartbeat_timer += p_delta;
		if (heartbeat_timer >= HEARTBEAT_INTERVAL) {
			heartbeat_timer = 0.0;
			send_heartbeat();
		}
	}
}

void MMOTcpTransport::send(const PackedByteArray &p_data) {
	if (state != STATE_CONNECTED) {
		UtilityFunctions::printerr("MMOTcpTransport::send: not connected");
		return;
	}

	PackedByteArray encrypted = encrypt_data(p_data);
	if (encrypted.size() == 0) {
		return;
	}
	send_frame(encrypted);
}

void MMOTcpTransport::send_raw_bytes(const PackedByteArray &p_bytes) {
	if (!tcp.is_valid()) {
		return;
	}
	tcp->put_data(p_bytes);
}

void MMOTcpTransport::send_frame(const PackedByteArray &p_payload) {
	// [4B uint32 BE length][payload]
	uint32_t len = (uint32_t)p_payload.size();
	PackedByteArray frame;
	frame.resize(4 + p_payload.size());
	uint8_t *out = frame.ptrw();
	MMOCrypto::write_u32_be(out, len);
	memcpy(out + 4, p_payload.ptr(), p_payload.size());
	send_raw_bytes(frame);
}

void MMOTcpTransport::process_recv_buffer() {
	while (true) {
		if (recv_buffer.size() < 4) {
			break; // need at least 4 bytes for length prefix
		}

		uint32_t payload_len = MMOCrypto::read_u32_be(recv_buffer.ptr());

		// DoS protection: enforce 1MB max
		if (payload_len > (uint32_t)MAX_FRAME_SIZE) {
			state = STATE_ERROR;
			if (listener) {
				listener->on_transport_error(
						"TCP frame length exceeds 1MB limit. Possible malicious packet.");
			}
			disconnect();
			return;
		}

		uint32_t total_frame = 4 + payload_len;
		if ((uint32_t)recv_buffer.size() < total_frame) {
			break; // incomplete frame
		}

		// Extract payload
		PackedByteArray encrypted_payload;
		encrypted_payload.resize(payload_len);
		memcpy(encrypted_payload.ptrw(), recv_buffer.ptr() + 4, payload_len);

		// Remove consumed bytes from recv_buffer
		int remaining = recv_buffer.size() - total_frame;
		if (remaining > 0) {
			PackedByteArray new_buffer;
			new_buffer.resize(remaining);
			memcpy(new_buffer.ptrw(),
					recv_buffer.ptr() + total_frame, remaining);
			recv_buffer = new_buffer;
		} else {
			recv_buffer.resize(0);
		}

		// Decrypt and process
		PackedByteArray decrypted = decrypt_data(encrypted_payload);
		if (decrypted.size() > 0) {
			process_decrypted_message(decrypted);
		}
	}
}
