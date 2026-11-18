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
/* RSA: PKCS#1 v1.5 verify (IOSC, TLS), raw RSA without padding (IOSC), PSS (TLS 1.2 servers). */
#define WC_RSA_NO_PADDING
#define WC_RSA_PSS
/* ECDHE/ECDSA for TLS 1.2 servers (P-256/P-384 by default). */
#define HAVE_ECC
#define HAVE_SUPPORTED_CURVES
/* Ed25519 signature of the update manifests (UpdaterCommon). */
#define HAVE_ED25519
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
