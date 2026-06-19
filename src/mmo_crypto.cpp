#include "mmo_crypto.h"

#include <godot_cpp/classes/crypto.hpp>
#include <godot_cpp/classes/hashing_context.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <mbedtls/gcm.h>

#include <chrono>
#include <cstring>

PackedByteArray MMOCrypto::derive_key(const PackedByteArray &p_seed) {
	Ref<HashingContext> ctx;
	ctx.instantiate();
	ctx->start(HashingContext::HASH_SHA256);
	ctx->update(p_seed);
	return ctx->finish();
}

PackedByteArray MMOCrypto::generate_seed() {
	Ref<Crypto> crypto;
	crypto.instantiate();
	return crypto->generate_random_bytes(SEED_SIZE);
}

PackedByteArray MMOCrypto::generate_nonce() {
	Ref<Crypto> crypto;
	crypto.instantiate();
	return crypto->generate_random_bytes(NONCE_SIZE);
}

PackedByteArray MMOCrypto::encrypt(const PackedByteArray &p_key,
		const PackedByteArray &p_data) {
	PackedByteArray result;

	if (p_key.size() != KEY_SIZE) {
		UtilityFunctions::printerr("MMOCrypto::encrypt: key must be 32 bytes");
		return result;
	}

	int data_len = p_data.size();
	int pt_len = TIMESTAMP_SIZE + data_len;
	int total_len = NONCE_SIZE + pt_len + TAG_SIZE;

	result.resize(total_len);
	uint8_t *out = result.ptrw();

	// [0..12) = nonce
	PackedByteArray nonce = generate_nonce();
	memcpy(out, nonce.ptr(), NONCE_SIZE);

	// Build plaintext: [8B timestamp BE][data]
	PackedByteArray plaintext;
	plaintext.resize(pt_len);
	uint8_t *pt = plaintext.ptrw();

	auto now = std::chrono::system_clock::now();
	auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
			now.time_since_epoch())
					  .count();
	write_u64_be(pt, (uint64_t)ms);

	if (data_len > 0) {
		memcpy(pt + TIMESTAMP_SIZE, p_data.ptr(), data_len);
	}

	// AES-256-GCM encrypt
	mbedtls_gcm_context ctx;
	mbedtls_gcm_init(&ctx);

	int ret = mbedtls_gcm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES,
			p_key.ptr(), 256);
	if (ret != 0) {
		UtilityFunctions::printerr("MMOCrypto::encrypt: gcm_setkey failed: ", ret);
		mbedtls_gcm_free(&ctx);
		result.resize(0);
		return result;
	}

	// ciphertext at out[12..12+pt_len), tag at out[12+pt_len..12+pt_len+16)
	ret = mbedtls_gcm_crypt_and_tag(&ctx,
			MBEDTLS_GCM_ENCRYPT,
			pt_len,
			out, NONCE_SIZE, // nonce
			nullptr, 0, // no AAD
			pt, // plaintext input
			out + NONCE_SIZE, // ciphertext output at offset 12
			TAG_SIZE,
			out + NONCE_SIZE + pt_len); // tag output right after ciphertext

	mbedtls_gcm_free(&ctx);

	if (ret != 0) {
		UtilityFunctions::printerr("MMOCrypto::encrypt: gcm_crypt_and_tag failed: ", ret);
		result.resize(0);
		return result;
	}

	return result;
}

PackedByteArray MMOCrypto::decrypt(const PackedByteArray &p_key,
		const PackedByteArray &p_packet) {
	PackedByteArray result;

	if (p_key.size() != KEY_SIZE) {
		UtilityFunctions::printerr("MMOCrypto::decrypt: key must be 32 bytes");
		return result;
	}

	int pkt_len = p_packet.size();
	int min_len = NONCE_SIZE + TIMESTAMP_SIZE + TAG_SIZE; // 12 + 8 + 16 = 36
	if (pkt_len < min_len) {
		UtilityFunctions::printerr("MMOCrypto::decrypt: packet too short");
		return result;
	}

	const uint8_t *pkt = p_packet.ptr();

	// nonce = pkt[0..12)
	const uint8_t *nonce = pkt;
	// ciphertext = pkt[12..pkt_len-16)
	int ct_len = pkt_len - NONCE_SIZE - TAG_SIZE;
	const uint8_t *ciphertext = pkt + NONCE_SIZE;
	// tag = pkt[pkt_len-16..pkt_len)
	const uint8_t *tag = pkt + pkt_len - TAG_SIZE;

	PackedByteArray plaintext;
	plaintext.resize(ct_len);

	mbedtls_gcm_context ctx;
	mbedtls_gcm_init(&ctx);

	int ret = mbedtls_gcm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES,
			p_key.ptr(), 256);
	if (ret != 0) {
		UtilityFunctions::printerr("MMOCrypto::decrypt: gcm_setkey failed: ", ret);
		mbedtls_gcm_free(&ctx);
		return result;
	}

	ret = mbedtls_gcm_auth_decrypt(&ctx,
			ct_len,
			nonce, NONCE_SIZE,
			nullptr, 0, // no AAD
			tag, TAG_SIZE,
			ciphertext,
			plaintext.ptrw());

	mbedtls_gcm_free(&ctx);

	if (ret != 0) {
		UtilityFunctions::printerr("MMOCrypto::decrypt: GCM auth failed: ", ret);
		return result;
	}

	// Strip 8-byte timestamp prefix, return only actual data
	if (ct_len <= TIMESTAMP_SIZE) {
		return result;
	}

	int data_len = ct_len - TIMESTAMP_SIZE;
	result.resize(data_len);
	memcpy(result.ptrw(), plaintext.ptr() + TIMESTAMP_SIZE, data_len);

	return result;
}
