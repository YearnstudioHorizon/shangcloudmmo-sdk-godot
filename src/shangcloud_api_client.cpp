#include "shangcloud_api_client.h"

#include <godot_cpp/classes/crypto.hpp>
#include <godot_cpp/classes/hashing_context.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/classes/marshalls.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/classes/tls_options.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

namespace {
const char *DEVICE_CODE_GRANT = "urn:ietf:params:oauth:grant-type:device_code";
const char *DEFAULT_DEVICE_SCOPE = "openid profile mmo";
} // namespace

ShangCloudApiClient::ShangCloudApiClient() {
	set_process(true);
	http.instantiate();
}

ShangCloudApiClient::~ShangCloudApiClient() {
	clear_http();
}

void ShangCloudApiClient::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_base_url", "url"), &ShangCloudApiClient::set_base_url);
	ClassDB::bind_method(D_METHOD("get_base_url"), &ShangCloudApiClient::get_base_url);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "base_url"), "set_base_url", "get_base_url");

	ClassDB::bind_method(D_METHOD("set_access_token", "token"), &ShangCloudApiClient::set_access_token);
	ClassDB::bind_method(D_METHOD("get_access_token"), &ShangCloudApiClient::get_access_token);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "access_token"), "set_access_token", "get_access_token");

	ClassDB::bind_method(D_METHOD("set_token_type", "type"), &ShangCloudApiClient::set_token_type);
	ClassDB::bind_method(D_METHOD("get_token_type"), &ShangCloudApiClient::get_token_type);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "token_type"), "set_token_type", "get_token_type");

	ClassDB::bind_method(D_METHOD("set_refresh_token", "token"), &ShangCloudApiClient::set_refresh_token);
	ClassDB::bind_method(D_METHOD("get_refresh_token"), &ShangCloudApiClient::get_refresh_token);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "refresh_token"), "set_refresh_token", "get_refresh_token");

	ClassDB::bind_method(D_METHOD("set_client_id", "id"), &ShangCloudApiClient::set_client_id);
	ClassDB::bind_method(D_METHOD("get_client_id"), &ShangCloudApiClient::get_client_id);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "client_id"), "set_client_id", "get_client_id");

	ClassDB::bind_method(D_METHOD("new_room", "protocol"), &ShangCloudApiClient::new_room);
	ClassDB::bind_method(D_METHOD("join_room", "room_id", "protocol"), &ShangCloudApiClient::join_room);
	ClassDB::bind_method(D_METHOD("set_room_config", "room_id", "allow_multi_login"), &ShangCloudApiClient::set_room_config);
	ClassDB::bind_method(D_METHOD("set_room_data", "room_id", "key", "value", "type"), &ShangCloudApiClient::set_room_data);
	ClassDB::bind_method(D_METHOD("get_room_data", "room_id"), &ShangCloudApiClient::get_room_data);
	ClassDB::bind_method(D_METHOD("delete_room_data", "room_id", "key"), &ShangCloudApiClient::delete_room_data);
	ClassDB::bind_method(D_METHOD("kick_user", "room_id", "target_uid"), &ShangCloudApiClient::kick_user);
	ClassDB::bind_method(D_METHOD("get_room_user_count", "room_id"), &ShangCloudApiClient::get_room_user_count);

	ClassDB::bind_method(D_METHOD("login_with_device_auth", "client_id", "scope"), &ShangCloudApiClient::login_with_device_auth);
	ClassDB::bind_method(D_METHOD("cancel_device_login"), &ShangCloudApiClient::cancel_device_login);
	ClassDB::bind_method(D_METHOD("request_device_authorization", "client_id", "scope"), &ShangCloudApiClient::request_device_authorization);
	ClassDB::bind_method(D_METHOD("poll_device_token_once", "device_code", "code_verifier", "client_id"), &ShangCloudApiClient::poll_device_token_once);
	ClassDB::bind_method(D_METHOD("refresh_access_token", "refresh_token", "client_id"), &ShangCloudApiClient::refresh_access_token);

	ADD_SIGNAL(MethodInfo("api_success", PropertyInfo(Variant::STRING, "path"), PropertyInfo(Variant::DICTIONARY, "data")));
	ADD_SIGNAL(MethodInfo("api_failed", PropertyInfo(Variant::STRING, "path"), PropertyInfo(Variant::STRING, "error")));

	ADD_SIGNAL(MethodInfo("device_auth_started",
			PropertyInfo(Variant::STRING, "user_code"),
			PropertyInfo(Variant::STRING, "verification_uri"),
			PropertyInfo(Variant::STRING, "verification_uri_complete"),
			PropertyInfo(Variant::STRING, "device_code"),
			PropertyInfo(Variant::STRING, "code_verifier")));
	ADD_SIGNAL(MethodInfo("device_auth_completed", PropertyInfo(Variant::DICTIONARY, "token")));
	ADD_SIGNAL(MethodInfo("device_auth_failed", PropertyInfo(Variant::STRING, "error")));
	ADD_SIGNAL(MethodInfo("device_auth_pending"));
	ADD_SIGNAL(MethodInfo("token_refreshed", PropertyInfo(Variant::DICTIONARY, "token")));
}

void ShangCloudApiClient::_process(double p_delta) {
	(void)p_delta;
	poll_http();

	if (device_login_active && !device_poll_in_flight && pending == PendingKind::NONE) {
		double now = Time::get_singleton()->get_unix_time_from_system();
		if (now >= device_deadline) {
			device_login_active = false;
			emit_signal("device_auth_failed", "Device authorization timed out (device_code expired)");
			return;
		}
		if (now >= device_next_poll) {
			do_device_poll();
		}
	}
}

void ShangCloudApiClient::set_base_url(const String &p_url) {
	base_url = p_url;
	while (base_url.ends_with("/")) {
		base_url = base_url.substr(0, base_url.length() - 1);
	}
}
String ShangCloudApiClient::get_base_url() const { return base_url; }
void ShangCloudApiClient::set_access_token(const String &p_token) { access_token = p_token; }
String ShangCloudApiClient::get_access_token() const { return access_token; }
void ShangCloudApiClient::set_token_type(const String &p_type) { token_type = p_type; }
String ShangCloudApiClient::get_token_type() const { return token_type; }
void ShangCloudApiClient::set_refresh_token(const String &p_token) { refresh_token = p_token; }
String ShangCloudApiClient::get_refresh_token() const { return refresh_token; }
void ShangCloudApiClient::set_client_id(const String &p_id) { client_id = p_id; }
String ShangCloudApiClient::get_client_id() const { return client_id; }

String ShangCloudApiClient::make_authorization_header_value() const {
	// 仅发送 token 本身，不带 Bearer/TokenType（与 Unity 实测一致：服务端按裸 token 校验）
	String token = access_token.strip_edges();
	if (token.begins_with("Bearer ") || token.begins_with("bearer ")) {
		token = token.substr(7).strip_edges();
	}
	return token;
}

void ShangCloudApiClient::new_room(const String &p_protocol) {
	PackedStringArray headers;
	headers.push_back("Content-Type: application/json");
	headers.push_back(String("Authorization: ") + make_authorization_header_value());
	if (!p_protocol.is_empty()) {
		headers.push_back(String("X-MMO-Protoctl: ") + p_protocol);
	}
	start_request(PendingKind::JSON_API, "/api/mmo/room/new", "{}", headers);
}

void ShangCloudApiClient::join_room(const String &p_room_id, const String &p_protocol) {
	PackedStringArray headers;
	headers.push_back("Content-Type: application/json");
	headers.push_back(String("Authorization: ") + make_authorization_header_value());
	headers.push_back(String("X-MMO-Room: ") + p_room_id);
	if (!p_protocol.is_empty()) {
		headers.push_back(String("X-MMO-Protoctl: ") + p_protocol);
	}
	start_request(PendingKind::JSON_API, "/api/mmo/room/join", "{}", headers);
}

void ShangCloudApiClient::set_room_config(const String &p_room_id, bool p_allow_multi_login) {
	PackedStringArray headers;
	headers.push_back("Content-Type: application/json");
	headers.push_back(String("Authorization: ") + make_authorization_header_value());
	headers.push_back(String("X-MMO-Room: ") + p_room_id);
	String body = p_allow_multi_login ? "{\"allow_multi_login\":true}" : "{\"allow_multi_login\":false}";
	start_request(PendingKind::JSON_API, "/api/mmo/room/config", body, headers);
}

void ShangCloudApiClient::set_room_data(const String &p_room_id, const String &p_key, const String &p_value, const String &p_type) {
	Dictionary obj;
	obj["key"] = p_key;
	obj["value"] = p_value;
	if (!p_type.is_empty()) {
		obj["type"] = p_type;
	}
	PackedStringArray headers;
	headers.push_back("Content-Type: application/json");
	headers.push_back(String("Authorization: ") + make_authorization_header_value());
	headers.push_back(String("X-MMO-Room: ") + p_room_id);
	start_request(PendingKind::JSON_API, "/api/mmo/room/data/set", JSON::stringify(obj), headers);
}

void ShangCloudApiClient::get_room_data(const String &p_room_id) {
	PackedStringArray headers;
	headers.push_back("Content-Type: application/json");
	headers.push_back(String("Authorization: ") + make_authorization_header_value());
	headers.push_back(String("X-MMO-Room: ") + p_room_id);
	start_request(PendingKind::JSON_API, "/api/mmo/room/data/get", "{}", headers);
}

void ShangCloudApiClient::delete_room_data(const String &p_room_id, const String &p_key) {
	Dictionary obj;
	obj["key"] = p_key;
	PackedStringArray headers;
	headers.push_back("Content-Type: application/json");
	headers.push_back(String("Authorization: ") + make_authorization_header_value());
	headers.push_back(String("X-MMO-Room: ") + p_room_id);
	start_request(PendingKind::JSON_API, "/api/mmo/room/data/delete", JSON::stringify(obj), headers);
}

void ShangCloudApiClient::kick_user(const String &p_room_id, const String &p_target_uid) {
	Dictionary obj;
	obj["target_uid"] = p_target_uid;
	PackedStringArray headers;
	headers.push_back("Content-Type: application/json");
	headers.push_back(String("Authorization: ") + make_authorization_header_value());
	headers.push_back(String("X-MMO-Room: ") + p_room_id);
	start_request(PendingKind::JSON_API, "/api/mmo/room/kick", JSON::stringify(obj), headers);
}

void ShangCloudApiClient::get_room_user_count(const String &p_room_id) {
	PackedStringArray headers;
	headers.push_back("Content-Type: application/json");
	headers.push_back(String("Authorization: ") + make_authorization_header_value());
	headers.push_back(String("X-MMO-Room: ") + p_room_id);
	start_request(PendingKind::JSON_API, "/api/mmo/room/usercount", "{}", headers);
}

void ShangCloudApiClient::login_with_device_auth(const String &p_client_id, const String &p_scope) {
	cancel_device_login();
	String use_id = p_client_id.is_empty() ? client_id : p_client_id;
	if (use_id.is_empty()) {
		emit_signal("device_auth_failed", "client_id is required");
		return;
	}
	device_login_active = true;
	request_device_authorization(use_id, p_scope);
}

void ShangCloudApiClient::cancel_device_login() {
	device_login_active = false;
	device_poll_in_flight = false;
	device_code = "";
	code_verifier = "";
	if (pending == PendingKind::DEVICE_AUTH_REQUEST || pending == PendingKind::DEVICE_TOKEN_POLL) {
		clear_http();
		pending = PendingKind::NONE;
	}
}

void ShangCloudApiClient::request_device_authorization(const String &p_client_id, const String &p_scope) {
	String use_id = p_client_id.is_empty() ? client_id : p_client_id;
	if (use_id.is_empty()) {
		emit_signal("device_auth_failed", "client_id is required");
		return;
	}

	String challenge;
	make_pkce(code_verifier, challenge);
	client_id = use_id;

	Dictionary form;
	form["client_id"] = use_id;
	form["scope"] = p_scope.is_empty() ? String(DEFAULT_DEVICE_SCOPE) : p_scope;
	form["code_challenge"] = challenge;
	form["code_challenge_method"] = "S256";

	PackedStringArray headers;
	headers.push_back("Content-Type: application/x-www-form-urlencoded");
	start_request(PendingKind::DEVICE_AUTH_REQUEST, "/oauth/device_authorization", form_encode(form), headers);
}

void ShangCloudApiClient::poll_device_token_once(const String &p_device_code, const String &p_code_verifier, const String &p_client_id) {
	String use_id = p_client_id.is_empty() ? client_id : p_client_id;
	if (use_id.is_empty() || p_device_code.is_empty() || p_code_verifier.is_empty()) {
		emit_signal("device_auth_failed", "client_id, device_code and code_verifier are required");
		return;
	}

	Dictionary form;
	form["grant_type"] = String(DEVICE_CODE_GRANT);
	form["device_code"] = p_device_code;
	form["client_id"] = use_id;
	form["code_verifier"] = p_code_verifier;

	PackedStringArray headers;
	headers.push_back("Content-Type: application/x-www-form-urlencoded");
	start_request(PendingKind::DEVICE_TOKEN_POLL_ONCE, "/oauth/token", form_encode(form), headers);
}

void ShangCloudApiClient::refresh_access_token(const String &p_refresh_token, const String &p_client_id) {
	String use_id = p_client_id.is_empty() ? client_id : p_client_id;
	String use_refresh = p_refresh_token.is_empty() ? refresh_token : p_refresh_token;
	if (use_id.is_empty() || use_refresh.is_empty()) {
		emit_signal("device_auth_failed", "client_id and refresh_token are required");
		return;
	}

	Dictionary form;
	form["grant_type"] = "refresh_token";
	form["refresh_token"] = use_refresh;
	form["client_id"] = use_id;

	PackedStringArray headers;
	headers.push_back("Content-Type: application/x-www-form-urlencoded");
	start_request(PendingKind::REFRESH_TOKEN, "/oauth/token", form_encode(form), headers);
}

void ShangCloudApiClient::apply_token_dict(const Dictionary &p_token) {
	if (p_token.has("access_token")) {
		access_token = p_token["access_token"];
	}
	if (p_token.has("token_type") && !String(p_token["token_type"]).is_empty()) {
		token_type = p_token["token_type"];
	}
	if (p_token.has("refresh_token") && !String(p_token["refresh_token"]).is_empty()) {
		refresh_token = p_token["refresh_token"];
	}
}

void ShangCloudApiClient::make_pkce(String &r_verifier, String &r_challenge) {
	Ref<Crypto> crypto;
	crypto.instantiate();
	PackedByteArray random_bytes = crypto->generate_random_bytes(32);
	r_verifier = base64_url_encode(random_bytes);

	Ref<HashingContext> ctx;
	ctx.instantiate();
	ctx->start(HashingContext::HASH_SHA256);
	ctx->update(r_verifier.to_utf8_buffer());
	PackedByteArray hash_bytes = ctx->finish();
	r_challenge = base64_url_encode(hash_bytes);
}

String ShangCloudApiClient::base64_url_encode(const PackedByteArray &p_data) {
	String s = Marshalls::get_singleton()->raw_to_base64(p_data);
	s = s.replace("+", "-").replace("/", "_").replace("=", "");
	return s;
}

String ShangCloudApiClient::form_encode(const Dictionary &p_fields) {
	Array keys = p_fields.keys();
	String result;
	for (int i = 0; i < keys.size(); i++) {
		if (i > 0) {
			result += "&";
		}
		String k = keys[i];
		String v = p_fields[k];
		result += k.uri_encode() + "=" + v.uri_encode();
	}
	return result;
}

Dictionary ShangCloudApiClient::parse_json_object(const String &p_body) {
	Ref<JSON> json;
	json.instantiate();
	Error err = json->parse(p_body);
	if (err != OK) {
		return Dictionary();
	}
	Variant data = json->get_data();
	if (data.get_type() != Variant::DICTIONARY) {
		return Dictionary();
	}
	return data;
}

void ShangCloudApiClient::start_request(PendingKind p_kind, const String &p_path, const String &p_body,
		const PackedStringArray &p_headers) {
	if (pending != PendingKind::NONE) {
		clear_http();
	}

	String rest = base_url;
	pending_tls = rest.begins_with("https://");
	if (rest.begins_with("https://")) {
		rest = rest.substr(8);
	} else if (rest.begins_with("http://")) {
		rest = rest.substr(7);
	}
	int slash = rest.find("/");
	String host_port = slash >= 0 ? rest.substr(0, slash) : rest;
	int colon = host_port.rfind(":");
	if (colon >= 0) {
		pending_host = host_port.substr(0, colon);
		pending_port = host_port.substr(colon + 1).to_int();
	} else {
		pending_host = host_port;
		pending_port = pending_tls ? 443 : 80;
	}

	pending = p_kind;
	pending_path = p_path;
	pending_body = p_body;
	pending_headers = p_headers;
	request_sent = false;
	response_body = "";
	response_code = 0;

	http.instantiate();
	Error err;
	if (pending_tls) {
		err = http->connect_to_host(pending_host, pending_port, TLSOptions::client());
	} else {
		err = http->connect_to_host(pending_host, pending_port);
	}
	if (err != OK) {
		fail_request("Failed to connect");
	}
}

void ShangCloudApiClient::fail_request(const String &p_error) {
	PendingKind kind = pending;
	String path = pending_path;
	clear_http();
	pending = PendingKind::NONE;

	if (kind == PendingKind::JSON_API) {
		emit_signal("api_failed", path, p_error);
	} else if (kind != PendingKind::NONE) {
		device_login_active = false;
		device_poll_in_flight = false;
		emit_signal("device_auth_failed", p_error);
	}
}

void ShangCloudApiClient::poll_http() {
	if (pending == PendingKind::NONE || http.is_null()) {
		return;
	}

	http->poll();
	HTTPClient::Status status = http->get_status();

	if (status == HTTPClient::STATUS_RESOLVING || status == HTTPClient::STATUS_CONNECTING) {
		return;
	}

	if (status == HTTPClient::STATUS_TLS_HANDSHAKE_ERROR || status == HTTPClient::STATUS_CANT_CONNECT ||
			status == HTTPClient::STATUS_CANT_RESOLVE || status == HTTPClient::STATUS_CONNECTION_ERROR) {
		fail_request("Connection error");
		return;
	}

	if (status == HTTPClient::STATUS_CONNECTED && !request_sent) {
		Error err = http->request(HTTPClient::METHOD_POST, pending_path, pending_headers, pending_body);
		request_sent = true;
		if (err != OK) {
			fail_request("Failed to send request");
		}
		return;
	}

	if (status == HTTPClient::STATUS_REQUESTING) {
		return;
	}

	if (status == HTTPClient::STATUS_BODY) {
		if (http->has_response() && response_code == 0) {
			response_code = http->get_response_code();
		}
		PackedByteArray chunk = http->read_response_body_chunk();
		if (chunk.size() > 0) {
			response_body += String::utf8((const char *)chunk.ptr(), chunk.size());
		}
		return;
	}

	// After body is fully read, HTTPClient returns to CONNECTED (keep-alive) or DISCONNECTED
	if (request_sent && (status == HTTPClient::STATUS_CONNECTED || status == HTTPClient::STATUS_DISCONNECTED)) {
		if (response_code == 0 && http->has_response()) {
			response_code = http->get_response_code();
		}
		// Drain any leftover
		while (http->get_status() == HTTPClient::STATUS_BODY) {
			http->poll();
			PackedByteArray chunk = http->read_response_body_chunk();
			if (chunk.size() == 0) {
				break;
			}
			response_body += String::utf8((const char *)chunk.ptr(), chunk.size());
		}
		if (response_code == 0) {
			response_code = 200;
		}
		finish_request(response_code, response_body);
	}
}

void ShangCloudApiClient::finish_request(int p_status_code, const String &p_body) {
	PendingKind kind = pending;
	String path = pending_path;
	clear_http();
	pending = PendingKind::NONE;

	switch (kind) {
		case PendingKind::DEVICE_AUTH_REQUEST:
			handle_device_auth_response(p_status_code, p_body);
			break;
		case PendingKind::DEVICE_TOKEN_POLL:
			handle_token_poll_response(p_status_code, p_body, false);
			break;
		case PendingKind::DEVICE_TOKEN_POLL_ONCE:
			handle_token_poll_response(p_status_code, p_body, true);
			break;
		case PendingKind::REFRESH_TOKEN: {
			if (p_status_code < 200 || p_status_code >= 300) {
				emit_signal("device_auth_failed",
						String("Server returned error status: ") + String::num_int64(p_status_code) + ", body: " + p_body);
				break;
			}
			Dictionary token = parse_json_object(p_body);
			if (!token.has("access_token") || String(token["access_token"]).is_empty()) {
				emit_signal("device_auth_failed", "Invalid refresh_token response");
				break;
			}
			if (!token.has("refresh_token") || String(token["refresh_token"]).is_empty()) {
				token["refresh_token"] = refresh_token;
			}
			apply_token_dict(token);
			emit_signal("token_refreshed", token);
			break;
		}
		case PendingKind::JSON_API:
			if (p_status_code < 200 || p_status_code >= 300) {
				emit_signal("api_failed", path,
						String("Server returned error status: ") + String::num_int64(p_status_code) + ", body: " + p_body);
			} else {
				emit_signal("api_success", path, parse_json_object(p_body));
			}
			break;
		default:
			break;
	}
}

void ShangCloudApiClient::handle_device_auth_response(int p_status_code, const String &p_body) {
	if (p_status_code < 200 || p_status_code >= 300) {
		device_login_active = false;
		emit_signal("device_auth_failed",
				String("Server returned error status: ") + String::num_int64(p_status_code) + ", body: " + p_body);
		return;
	}

	Dictionary da = parse_json_object(p_body);
	if (!da.has("device_code") || String(da["device_code"]).is_empty()) {
		device_login_active = false;
		emit_signal("device_auth_failed", "Invalid device_authorization response");
		return;
	}

	device_code = da["device_code"];
	String user_code = da.get("user_code", "");
	String verification_uri = da.get("verification_uri", "");
	String verification_uri_complete = da.get("verification_uri_complete", "");
	int expires_in = (int)da.get("expires_in", 900);
	device_interval = (int)da.get("interval", 5);
	if (expires_in <= 0) {
		expires_in = 900;
	}
	if (device_interval <= 0) {
		device_interval = 5;
	}

	double now = Time::get_singleton()->get_unix_time_from_system();
	device_deadline = now + expires_in;
	device_next_poll = now + device_interval;

	emit_signal("device_auth_started", user_code, verification_uri, verification_uri_complete, device_code, code_verifier);
}

void ShangCloudApiClient::handle_token_poll_response(int p_status_code, const String &p_body, bool p_once) {
	device_poll_in_flight = false;

	if (p_status_code >= 200 && p_status_code < 300) {
		Dictionary token = parse_json_object(p_body);
		if (!token.has("access_token") || String(token["access_token"]).is_empty()) {
			device_login_active = false;
			emit_signal("device_auth_failed",
					String("Server returned error status: ") + String::num_int64(p_status_code) + ", body: " + p_body);
			return;
		}
		apply_token_dict(token);
		device_login_active = false;
		emit_signal("device_auth_completed", token);
		return;
	}

	Dictionary err = parse_json_object(p_body);
	String error = err.get("error", "");
	if (error == "authorization_pending") {
		emit_signal("device_auth_pending");
		if (!p_once && device_login_active) {
			schedule_device_poll();
		}
		return;
	}
	if (error == "slow_down") {
		device_interval += 5;
		if (!p_once && device_login_active) {
			schedule_device_poll();
		}
		return;
	}

	device_login_active = false;
	emit_signal("device_auth_failed",
			String("Server returned error status: ") + String::num_int64(p_status_code) + ", body: " + p_body);
}

void ShangCloudApiClient::handle_json_api_response(int p_status_code, const String &p_body) {
	(void)p_status_code;
	(void)p_body;
}

void ShangCloudApiClient::schedule_device_poll() {
	device_next_poll = Time::get_singleton()->get_unix_time_from_system() + device_interval;
}

void ShangCloudApiClient::do_device_poll() {
	if (!device_login_active || device_poll_in_flight || pending != PendingKind::NONE) {
		return;
	}
	device_poll_in_flight = true;
	device_next_poll = Time::get_singleton()->get_unix_time_from_system() + device_interval;

	Dictionary form;
	form["grant_type"] = String(DEVICE_CODE_GRANT);
	form["device_code"] = device_code;
	form["client_id"] = client_id;
	form["code_verifier"] = code_verifier;

	PackedStringArray headers;
	headers.push_back("Content-Type: application/x-www-form-urlencoded");
	start_request(PendingKind::DEVICE_TOKEN_POLL, "/oauth/token", form_encode(form), headers);
}

void ShangCloudApiClient::clear_http() {
	if (http.is_valid()) {
		http->close();
	}
	request_sent = false;
	response_body = "";
	response_code = 0;
}

} // namespace godot
