#ifndef SHANGCLOUD_API_CLIENT_H
#define SHANGCLOUD_API_CLIENT_H

#include <godot_cpp/classes/http_client.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>

namespace godot {

/**
 * ShangCloud HTTP API client + OAuth device authorization (RFC 8628)
 * with public-client PKCE (S256, no client_secret).
 */
class ShangCloudApiClient : public Node {
	GDCLASS(ShangCloudApiClient, Node)

public:
	ShangCloudApiClient();
	~ShangCloudApiClient();

	void _process(double p_delta) override;

	void set_base_url(const String &p_url);
	String get_base_url() const;
	void set_access_token(const String &p_token);
	String get_access_token() const;
	void set_token_type(const String &p_type);
	String get_token_type() const;
	void set_refresh_token(const String &p_token);
	String get_refresh_token() const;
	void set_client_id(const String &p_id);
	String get_client_id() const;

	// MMO API
	void new_room(const String &p_protocol);
	void join_room(const String &p_room_id, const String &p_protocol);
	void set_room_config(const String &p_room_id, bool p_allow_multi_login);
	void set_room_data(const String &p_room_id, const String &p_key, const String &p_value, const String &p_type);
	void get_room_data(const String &p_room_id);
	void delete_room_data(const String &p_room_id, const String &p_key);
	void kick_user(const String &p_room_id, const String &p_target_uid);
	void get_room_user_count(const String &p_room_id);

	// Device auth (public PKCE, no secret)
	// Emits device_auth_started(user_code, verification_uri, verification_uri_complete)
	// then device_auth_completed(token_dict) or device_auth_failed(error)
	void login_with_device_auth(const String &p_client_id, const String &p_scope);
	void cancel_device_login();
	void request_device_authorization(const String &p_client_id, const String &p_scope);
	void poll_device_token_once(const String &p_device_code, const String &p_code_verifier, const String &p_client_id);
	void refresh_access_token(const String &p_refresh_token, const String &p_client_id);

protected:
	static void _bind_methods();

private:
	enum class PendingKind {
		NONE,
		JSON_API,
		DEVICE_AUTH_REQUEST,
		DEVICE_TOKEN_POLL,
		DEVICE_TOKEN_POLL_ONCE,
		REFRESH_TOKEN,
	};

	String base_url = "https://api.yearnstudio.cn";
	String access_token;
	String token_type = "Bearer";
	String refresh_token;
	String client_id;

	Ref<HTTPClient> http;
	PendingKind pending = PendingKind::NONE;
	String pending_host;
	int pending_port = 443;
	bool pending_tls = true;
	String pending_path;
	String pending_body;
	PackedStringArray pending_headers;
	bool request_sent = false;
	String response_body;

	// Device login state
	bool device_login_active = false;
	String device_code;
	String code_verifier;
	int device_interval = 5;
	double device_deadline = 0.0;
	double device_next_poll = 0.0;
	bool device_poll_in_flight = false;

	void apply_token_dict(const Dictionary &p_token);
	/** Authorization 头值：仅 token 本身，不带 Bearer/TokenType（与 Unity 实测一致） */
	String make_authorization_header_value() const;
	static void make_pkce(String &r_verifier, String &r_challenge);
	static String base64_url_encode(const PackedByteArray &p_data);
	static String form_encode(const Dictionary &p_fields);
	static Dictionary parse_json_object(const String &p_body);

	void start_request(PendingKind p_kind, const String &p_path, const String &p_body,
			const PackedStringArray &p_headers);
	void poll_http();
	void finish_request(int p_status_code, const String &p_body);
	void fail_request(const String &p_error);
	void handle_device_auth_response(int p_status_code, const String &p_body);
	void handle_token_poll_response(int p_status_code, const String &p_body, bool p_once);
	void handle_json_api_response(int p_status_code, const String &p_body);
	void schedule_device_poll();
	void do_device_poll();
	void clear_http();

	int response_code = 0;
};

} // namespace godot

#endif
