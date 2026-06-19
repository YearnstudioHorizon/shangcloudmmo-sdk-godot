#ifndef MMO_CRYPTO_H
#define MMO_CRYPTO_H

#include <godot_cpp/variant/packed_byte_array.hpp>
#include <cstdint>

using namespace godot;

class MMOCrypto {
public:
	static PackedByteArray derive_key(const PackedByteArray &p_seed);
	static PackedByteArray generate_seed();
	static PackedByteArray generate_nonce();

	static PackedByteArray encrypt(const PackedByteArray &p_key,
			const PackedByteArray &p_data);

	static PackedByteArray decrypt(const PackedByteArray &p_key,
			const PackedByteArray &p_packet);

	static inline uint32_t to_be32(uint32_t v) {
		return ((v >> 24) & 0xFFu) |
				((v >> 8) & 0xFF00u) |
				((v << 8) & 0xFF0000u) |
				((v << 24) & 0xFF000000u);
	}

	static inline uint64_t to_be64(uint64_t v) {
		return (((v >> 56) & 0xFFULL) |
				((v >> 48) & 0xFF00ULL) |
				((v >> 40) & 0xFF0000ULL) |
				((v >> 32) & 0xFF000000ULL) |
				((v << 8) & 0xFF00000000ULL) |
				((v << 24) & 0xFF0000000000ULL) |
				((v << 40) & 0xFF000000000000ULL) |
				((v << 56) & 0xFF00000000000000ULL));
	}

	static inline uint32_t from_be32(uint32_t v) { return to_be32(v); }
	static inline uint64_t from_be64(uint64_t v) { return to_be64(v); }

	static inline void write_u32_be(uint8_t *dst, uint32_t v) {
		dst[0] = (uint8_t)(v >> 24);
		dst[1] = (uint8_t)(v >> 16);
		dst[2] = (uint8_t)(v >> 8);
		dst[3] = (uint8_t)(v);
	}

	static inline void write_u64_be(uint8_t *dst, uint64_t v) {
		dst[0] = (uint8_t)(v >> 56);
		dst[1] = (uint8_t)(v >> 48);
		dst[2] = (uint8_t)(v >> 40);
		dst[3] = (uint8_t)(v >> 32);
		dst[4] = (uint8_t)(v >> 24);
		dst[5] = (uint8_t)(v >> 16);
		dst[6] = (uint8_t)(v >> 8);
		dst[7] = (uint8_t)(v);
	}

	static inline uint32_t read_u32_be(const uint8_t *src) {
		return ((uint32_t)src[0] << 24) |
				((uint32_t)src[1] << 16) |
				((uint32_t)src[2] << 8) |
				((uint32_t)src[3]);
	}

	static inline uint64_t read_u64_be(const uint8_t *src) {
		return ((uint64_t)src[0] << 56) |
				((uint64_t)src[1] << 48) |
				((uint64_t)src[2] << 40) |
				((uint64_t)src[3] << 32) |
				((uint64_t)src[4] << 24) |
				((uint64_t)src[5] << 16) |
				((uint64_t)src[6] << 8) |
				((uint64_t)src[7]);
	}

private:
	static constexpr int NONCE_SIZE = 12;
	static constexpr int TAG_SIZE = 16;
	static constexpr int TIMESTAMP_SIZE = 8;
	static constexpr int SEED_SIZE = 32;
	static constexpr int KEY_SIZE = 32;
};

#endif
