#ifndef SHANGCLOUD_MMO_H
#define SHANGCLOUD_MMO_H

#include "mmo_message_queue.h"
#include "mmo_transport.h"

#include <godot_cpp/classes/node.hpp>

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

	void cleanup_transport();
	void process_business_message(const String &p_message);
};

} // namespace godot

VARIANT_ENUM_CAST(ShangCloudMMO::Protocol);
VARIANT_ENUM_CAST(ShangCloudMMO::ConnectionState);

#endif
