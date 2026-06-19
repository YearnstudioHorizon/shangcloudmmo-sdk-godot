#include "mmo_transport.h"

#include <godot_cpp/variant/utility_functions.hpp>

PackedByteArray MMOTransport::encrypt_data(const PackedByteArray &p_data) {
	return MMOCrypto::encrypt(aes_key, p_data);
}

PackedByteArray MMOTransport::decrypt_data(const PackedByteArray &p_encrypted) {
	return MMOCrypto::decrypt(aes_key, p_encrypted);
}

void MMOTransport::process_decrypted_message(const PackedByteArray &p_decrypted) {
	if (p_decrypted.size() == 0) {
		return;
	}

	String msg;
	msg.parse_utf8((const char *)p_decrypted.ptr(), p_decrypted.size());

	// Intercept transport-internal system messages
	if (msg == "__auth_ok__") {
		state = STATE_CONNECTED;
		if (listener) {
			listener->on_transport_connected();
		}
		return;
	}

	if (msg == "__hb__") {
		return; // swallow heartbeat responses
	}

	if (msg == "__closed__") {
		state = STATE_DISCONNECTED;
		if (listener) {
			listener->on_transport_server_closed();
			listener->on_transport_disconnected();
		}
		return;
	}

	// Business message — push to queue
	if (msg_queue) {
		MMOMessage m;
		m.type = MMOMessage::TEXT;
		m.text = msg;
		msg_queue->push(m);
	}
}

void MMOTransport::send_heartbeat() {
	PackedByteArray hb_data;
	String hb_str = "__hb__";
	CharString hb_utf8 = hb_str.utf8();
	hb_data.resize(hb_utf8.length());
	memcpy(hb_data.ptrw(), hb_utf8.ptr(), hb_utf8.length());
	send(hb_data);
}
