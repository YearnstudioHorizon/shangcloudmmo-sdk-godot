/*
 * Minimal mbedtls configuration for AES-256-GCM only.
 * Used by ShangCloud MMO GDExtension.
 *
 * SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#ifndef MBEDTLS_CONFIG_H
#define MBEDTLS_CONFIG_H

/* System support */
#define MBEDTLS_HAVE_ASM
#define MBEDTLS_HAVE_TIME

/* Core modules required for AES-256-GCM */
#define MBEDTLS_AES_C
#define MBEDTLS_GCM_C
#define MBEDTLS_CIPHER_C
#define MBEDTLS_BLOCK_CIPHER_C
#define MBEDTLS_PLATFORM_C

/* AES hardware acceleration (auto-detected by architecture) */
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#define MBEDTLS_AESNI_C
#endif

#if defined(__aarch64__) || defined(_M_ARM64)
#define MBEDTLS_AESCE_C
#endif

/* Required by GCM internals */
#define MBEDTLS_CIPHER_MODE_CTR

/* Use standard C library calloc/free directly.
 * Do NOT include check_config.h here — build_info.h handles the
 * config_adjust → check_config include chain in the correct order. */

#endif /* MBEDTLS_CONFIG_H */
