project(ed25519 C)

add_library(ed25519
  ed25519/add_scalar.c
  ed25519/ge.c
  ed25519/keypair.c
  ed25519/seed.c
  ed25519/sign.c
  ed25519/fe.c
  ed25519/key_exchange.c
  ed25519/sc.c
  ed25519/sha512.c
  ed25519/verify.c)
