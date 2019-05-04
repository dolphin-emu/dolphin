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

set_target_properties(ed25519 PROPERTIES FOLDER Externals)
disable_all_warnings(ed25519)
