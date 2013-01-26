// Based off of twintig http://git.infradead.org/?p=users/segher/wii.git
// Copyright 2007,2008  Segher Boessenkool  <segher@kernel.crashing.org>
// Licensed under the terms of the GNU GPL, version 2
// http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt

void get_ng_cert(u8* ng_cert_out, u32 NG_id, u32 NG_key_id, u8* NG_priv, u8* NG_sig);
void get_ap_sig_and_cert(u8 *sig_out, u8 *ap_cert_out, u64 title_id, u8 *data, u32 data_size, u8 *NG_priv, u32 NG_id);

void make_blanksig_ec_cert(u8 *cert_out, const char *signer, const char *name, u8 *private_key, u32 key_id);
void get_shared_secret(u8* shared_secret_out, u8* remote_public_key, u8* NG_priv);
