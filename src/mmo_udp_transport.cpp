#include "mmo_udp_transport.h"

#include <godot_cpp/classes/ip.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <cstring>

void MMOUdpTransport::connect_to(const String &p_host, int p_port,
		const String &p_connect_key) {
	connect_key = p_connect_key;
	connect_id = 0;
	auth_sent = false;
	heartbeat_timer = 0.0;
	timeout_timer = 0.0;
	edge_port = p_port;

	// Resolve hostname to IP once, cache it
	resolved_ip = IP::get_singleton()->resolve_hostname(p_host);
	if (resolved_ip.is_empty()) {
		state = STATE_ERROR;
		if (listener) {
			listener->on_transport_error("Failed to resolve hostname: " + p_host);
		}
		return;
	}

	udp.instantiate();
	Error err = udp->bind(0); // bind to any available local port
	if (err != OK) {
		state = STATE_ERROR;
		if (listener) {
			listener->on_transport_error("UDP bind failed");
		}
		return;
	}

	udp->set_dest_address(resolved_ip, p_port);
	state = STATE_CONNECTING;
}

void MMOUdpTransport::disconnect() {
	if (udp.is_valid()) {
		udp->close();
	}
	state = STATE_DISCONNECTED;
	connect_id = 0;
}

void MMOUdpTransport::poll(double p_delta) {
	if (!udp.is_valid()) {
		return;
	}

	if (state == STATE_CONNECTING && !auth_sent) {
		send_auth_packet();
		auth_sent = true;
		state = STATE_AUTHENTICATING;
		timeout_timer = 0.0;
	}

	// Auth timeout
	if (state == STATE_AUTHENTICATING) {
		timeout_timer += p_delta;
		if (timeout_timer >= AUTH_TIMEOUT) {
			state = STATE_ERROR;
			if (listener) {
				listener->on_transport_error("UDP authentication timed out");
			}
			return;
		}
	}

	// Read incoming packets
	while (udp->get_available_packet_count() > 0) {
		PackedByteArray packet = udp->get_packet();
		if (packet.size() < 8) {
			continue; // too short, skip
		}

		// Strip 8-byte connectId header
		uint64_t pkt_connect_id = MMOCrypto::read_u64_be(packet.ptr());

		PackedByteArray encrypted_payload;
		int payload_size = packet.size() - 8;
		if (payload_size <= 0) {
			continue;
		}
		encrypted_payload.resize(payload_size);
		memcpy(encrypted_payload.ptrw(), packet.ptr() + 8, payload_size);

		PackedByteArray decrypted = decrypt_data(encrypted_payload);
		if (decrypted.size() == 0) {
			continue; // decryption failed, drop
		}

		if (state == STATE_AUTHENTICATING) {
			// First successful decrypted response after auth
			connect_id = pkt_connect_id;
		}

		process_decrypted_message(decrypted);
	}

	// Heartbeat
	if (state == STATE_CONNECTED) {
		heartbeat_timer += p_delta;
		if (heartbeat_timer >= HEARTBEAT_INTERVAL) {
			heartbeat_timer = 0.0;
			send_heartbeat();
		}

		// NAT resilience: detect prolonged silence
		timeout_timer += p_delta;
		if (timeout_timer >= HB_TIMEOUT) {
			reconnect_socket();
			timeout_timer = 0.0;
		}
	}
}

void MMOUdpTransport::send(const PackedByteArray &p_data) {
	if (state != STATE_CONNECTED) {
		UtilityFunctions::printerr("MMOUdpTransport::send: not connected");
		return;
	}

	PackedByteArray encrypted = encrypt_data(p_data);
	if (encrypted.size() == 0) {
		return;
	}

	// [8B connectId BE][encrypted_payload]
	PackedByteArray packet;
	packet.resize(8 + encrypted.size());
	uint8_t *out = packet.ptrw();
	MMOCrypto::write_u64_be(out, connect_id);
	memcpy(out + 8, encrypted.ptr(), encrypted.size());

	udp->put_packet(packet);
}

void MMOUdpTransport::send_auth_packet() {
	// Validate connect_key byte length
	CharString key_utf8 = connect_key.utf8();
	if (key_utf8.length() > MAX_CONNECT_KEY_BYTES) {
		state = STATE_ERROR;
		if (listener) {
			listener->on_transport_error(
					"connect_key byte length exceeds 256 bytes");
		}
		return;
	}

	// Generate seed and derive key
	PackedByteArray seed = MMOCrypto::generate_seed();
	aes_key = MMOCrypto::derive_key(seed);

	// Encrypt connect_key
	PackedByteArray key_data;
	key_data.resize(key_utf8.length());
	memcpy(key_data.ptrw(), key_utf8.ptr(), key_utf8.length());

	PackedByteArray encrypted_key = MMOCrypto::encrypt(aes_key, key_data);
	if (encrypted_key.size() == 0) {
		state = STATE_ERROR;
		if (listener) {
			listener->on_transport_error("Failed to encrypt connect_key");
		}
		return;
	}

	// Assemble auth packet:
	// [8B connectId=0 BE][32B seed][encrypted(12B nonce + ciphertext + 16B tag)]
	int total = 8 + 32 + encrypted_key.size();
	PackedByteArray auth_packet;
	auth_packet.resize(total);
	uint8_t *out = auth_packet.ptrw();

	// connectId = 0
	MMOCrypto::write_u64_be(out, 0);

	// seed
	memcpy(out + 8, seed.ptr(), 32);

	// encrypted connect_key
	memcpy(out + 8 + 32, encrypted_key.ptr(), encrypted_key.size());

	udp->put_packet(auth_packet);
}

void MMOUdpTransport::reconnect_socket() {
	if (!udp.is_valid()) {
		return;
	}

	udp->close();

	udp.instantiate();
	Error err = udp->bind(0);
	if (err != OK) {
		state = STATE_ERROR;
		if (listener) {
			listener->on_transport_error("UDP rebind failed during NAT recovery");
		}
		return;
	}

	// Use cached resolved_ip — no DNS resolution here
	udp->set_dest_address(resolved_ip, edge_port);
	// Keep connect_id and aes_key intact
}
