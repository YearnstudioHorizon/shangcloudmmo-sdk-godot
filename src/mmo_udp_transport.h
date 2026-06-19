#ifndef MMO_UDP_TRANSPORT_H
#define MMO_UDP_TRANSPORT_H

#include "mmo_transport.h"

#include <godot_cpp/classes/packet_peer_udp.hpp>

using namespace godot;

class MMOUdpTransport : public MMOTransport {
public:
	void connect_to(const String &p_host, int p_port,
			const String &p_connect_key) override;
	void disconnect() override;
	void poll(double p_delta) override;
	void send(const PackedByteArray &p_data) override;

private:
	Ref<PacketPeerUDP> udp;
	uint64_t connect_id = 0;
	String resolved_ip;
	int edge_port = 0;
	bool auth_sent = false;
	double timeout_timer = 0.0;

	static constexpr double AUTH_TIMEOUT = 10.0;
	static constexpr double HB_TIMEOUT = 15.0;
	static constexpr int MAX_CONNECT_KEY_BYTES = 256;

	void send_auth_packet();
	void reconnect_socket();
};

#endif
