#ifndef MMO_TCP_TRANSPORT_H
#define MMO_TCP_TRANSPORT_H

#include "mmo_transport.h"

#include <godot_cpp/classes/stream_peer_tcp.hpp>

using namespace godot;

class MMOTcpTransport : public MMOTransport {
public:
	void connect_to(const String &p_host, int p_port,
			const String &p_connect_key) override;
	void disconnect() override;
	void poll(double p_delta) override;
	void send(const PackedByteArray &p_data) override;

private:
	Ref<StreamPeerTCP> tcp;
	PackedByteArray recv_buffer;
	bool seed_sent = false;
	bool auth_sent = false;

	void send_raw_bytes(const PackedByteArray &p_bytes);
	void send_frame(const PackedByteArray &p_payload);
	void process_recv_buffer();
};

#endif
