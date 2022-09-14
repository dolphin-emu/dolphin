project(ed25519 C)

add_library(ed25519
  add_scalar.c
  ge.c
  keypair.c
  seed.c
  sign.c
  fe.c
  key_exchange.c
  sc.c
  sha512.c
  verify.c)
dolphin_disable_warnings_msvc(ed25519)
