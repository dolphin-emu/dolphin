// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

/* wolfSSL configuration for Dolphin.
 *
 * wolfSSL is always built from Externals (there is no system wolfSSL option): IOS SSL needs
 * features that distribution builds do not enable (TLS 1.0, static RSA key exchange...).
 *
 * This file is also read by the assembler (.S files): preprocessor directives and C comments only. */

#ifndef DOLPHIN_WOLFSSL_USER_SETTINGS_H
#define DOLPHIN_WOLFSSL_USER_SETTINGS_H

/* Dolphin is only a TLS client (IOS SSL, curl). */
#define NO_WOLFSSL_SERVER

/* ---- Host probes ----
 * wolfSSL's CMake passes these to the library only (compile definitions, config.h), but public
 * headers depend on them: type sizes (limits.h) and SP_WORD_SIZE (__uint128_t), hence sizeof(mp_int).
 * Define them here so that every user of the headers sees the same configuration. */
#ifndef HAVE_LIMITS_H
#define HAVE_LIMITS_H 1
#endif
/* settings.h drops it for MSVC. */
#ifndef HAVE_SYS_TIME_H
#define HAVE_SYS_TIME_H 1
#endif
#if defined(__SIZEOF_INT128__) && !defined(HAVE___UINT128_T)
#define HAVE___UINT128_T 1
#endif

/* ---- Hardening ---- */
#define TFM_TIMING_RESISTANT
#define ECC_TIMING_RESISTANT
#define WC_RSA_BLINDING

/* ---- Big integers (IOSC root key) ---- */
#define WOLFSSL_SP_MATH_ALL
#define SP_INT_BITS 4096
/* mp_* functions exported for MotionPlus.cpp and IOSC.cpp. */
#define WOLFSSL_PUBLIC_MP

/* ---- Random (getrandom/urandom, CryptGenRandom) ---- */
#define HAVE_HASHDRBG

/* ---- Hashes: MD5 (Common/Hash.cpp, Movie.cpp, TLS 1.0/1.1), SHA-1 and SHA-256 are on by default ---- */
#define WOLFSSL_SHA384
#define WOLFSSL_SHA512
#define HAVE_HKDF
#define NO_MD4

/* ---- Symmetric ciphers ---- */
/* AES-CBC (Common::AES, /dev/aes, TLS), AES-OFB (KD/WC24), AES-GCM (TLS 1.2). */
#define WOLFSSL_AES_OFB
#define HAVE_AESGCM
#define NO_DES3
#define NO_RC4

/* ---- Public key ---- */
/* RSA: PKCS#1 v1.5 verify (IOSC, TLS), raw RSA without padding (IOSC), PSS (TLS 1.2 servers,
 * required by TLS 1.3). */
#define WC_RSA_NO_PADDING
#define WC_RSA_PSS
/* ECDHE/ECDSA for TLS 1.2 servers (P-256/P-384 by default). */
#define HAVE_ECC
#define HAVE_SUPPORTED_CURVES
/* Ed25519 signature of the update manifests (UpdaterCommon). */
#define HAVE_ED25519
/* X25519 key exchange, preferred by TLS 1.3 servers (curl). */
#define HAVE_CURVE25519
#define NO_DSA

/* ---- TLS ---- */
/* IOS SSL: the real Wii negotiates TLS 1.0 with static RSA (AES256-SHA).
 * TLS 1.0 is kept available and static RSA key exchange is enabled. */
#define WOLFSSL_ALLOW_TLSV10
#define WOLFSSL_STATIC_RSA
#define HAVE_TLS_EXTENSIONS
#define HAVE_SNI
#define HAVE_EXTENDED_MASTER
#define HAVE_ENCRYPT_THEN_MAC
#define HAVE_SECURE_RENEGOTIATION
/* Despite its name, also needed on the client side: wolfSSL_[CTX_]UseSecureRenegotiation()
 * and wolfSSL_Rehandshake() depend on it. */
#define HAVE_SERVER_RENEGOTIATION_INFO
#define NO_SESSION_CACHE_REF
/* IOS SSL: Nintendo CA (sent in the chain by *.shop.wii.com) has serial number 0, which is
 * rejected otherwise (RFC 5280 requires a positive serial; mbedTLS accepted it). */
#define WOLFSSL_ASN_ALLOW_0_SERIAL

/* TLS 1.3 for curl (Dolphin's services). */
#define WOLFSSL_TLS13
/* Required by TLS 1.3 as long as DH is enabled (DHE suites). */
#define HAVE_FFDHE_2048

/* curl: its wolfSSL backend uses the OpenSSL compatibility layer (X509_STORE cache,
 * set_min_proto_version, SSL_SESSION import/export, MD5 for digest auth). */
#define OPENSSL_EXTRA
#define HAVE_ALPN
/* Accept chains with a cross-signed intermediate whose issuer is not trusted, as long as the
 * server certificate chains to a trusted root (e.g. retroachievements.org). */
#define WOLFSSL_ALT_CERT_CHAINS
/* Use the system CA store at run time. */
#if !defined(_WIN32) && !defined(__APPLE__)
#define WOLFSSL_SYS_CA_CERTS
#endif

#define NO_PSK
#define NO_PWDBASED

/* ---- Assembly ---- */
#if defined(DOLPHIN_WOLFSSL_AESNI) && DOLPHIN_WOLFSSL_AESNI
#define WOLFSSL_AESNI
#define WOLFSSL_X86_64_BUILD
#else
#define WOLFSSL_NO_ASM
#define TFM_NO_ASM
#endif

#endif /* DOLPHIN_WOLFSSL_USER_SETTINGS_H */
