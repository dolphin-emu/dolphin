// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// wmhcid: a software "DolphinBar" for macOS.
//
// Drives a USB Bluetooth adapter (e.g. CSR8510) directly through libusb with a
// minimal HCI/L2CAP host, so macOS's own Bluetooth stack is never involved.
// Each connected Wii Remote is exposed as a Unix socket in /tmp/wmhci/; the
// matching Dolphin backend (IOwmhci) picks those up as real Wii Remotes.
//
// Socket framing: 1 length byte, then the HID report including its header
// byte (0xA1 from the remote, 0xA2 to the remote).
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <libusb-1.0/libusb.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#define SOCK_DIR "/tmp/wmhci"
#define EP_EVT 0x81
#define EP_ACL_IN 0x82
#define EP_ACL_OUT 0x02
#define MAX_REMOTES 4
#define MAX_KNOWN 16
#define WIIMOTE_COD 0x002504

static libusb_context* ctx;
static libusb_device_handle* dev;
static uint8_t local_addr[6];
static char keyfile[512];
static volatile sig_atomic_t quit;
static int debug;

// ---------------------------------------------------------------- logging

static void stamp(void) {
  time_t t = time(NULL);
  char b[16];
  strftime(b, sizeof b, "%H:%M:%S", localtime(&t));
  printf("%s ", b);
}
#define LOG(...) do { stamp(); printf(__VA_ARGS__); printf("\n"); fflush(stdout); } while (0)

static double now(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec + ts.tv_nsec / 1e9;
}

static void addr_str(const uint8_t* a, char* out) {
  sprintf(out, "%02X:%02X:%02X:%02X:%02X:%02X", a[5], a[4], a[3], a[2], a[1], a[0]);
}
static int parse_addr(const char* s, uint8_t* a) {
  unsigned v[6];
  if (sscanf(s, "%x:%x:%x:%x:%x:%x", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]) != 6) return 0;
  for (int i = 0; i < 6; i++) a[5 - i] = (uint8_t)v[i];
  return 1;
}

// ---------------------------------------------------------------- known remotes / link keys

typedef struct {
  uint8_t addr[6];
  uint8_t key[16];
  int has_key;
} Known;
static Known known[MAX_KNOWN];
static int nknown;

static Known* known_get(const uint8_t* a, int create) {
  for (int i = 0; i < nknown; i++)
    if (!memcmp(known[i].addr, a, 6)) return &known[i];
  if (!create || nknown == MAX_KNOWN) return NULL;
  memset(&known[nknown], 0, sizeof known[nknown]);
  memcpy(known[nknown].addr, a, 6);
  return &known[nknown++];
}

static void keys_load(void) {
  FILE* f = fopen(keyfile, "r");
  if (!f) return;
  char line[128];
  while (fgets(line, sizeof line, f)) {
    char* eq = strchr(line, '=');
    if (eq) *eq = 0;
    uint8_t a[6];
    if (!parse_addr(line, a)) continue;
    Known* k = known_get(a, 1);
    if (k && eq && strlen(eq + 1) >= 32) {
      for (int j = 0; j < 16; j++) sscanf(eq + 1 + 2 * j, "%2hhx", &k->key[j]);
      k->has_key = 1;
    }
  }
  fclose(f);
}

static void keys_save(void) {
  FILE* f = fopen(keyfile, "w");
  if (!f) return;
  for (int i = 0; i < nknown; i++) {
    char a[20];
    addr_str(known[i].addr, a);
    fprintf(f, "%s", a);
    if (known[i].has_key) {
      fputc('=', f);
      for (int j = 0; j < 16; j++) fprintf(f, "%02x", known[i].key[j]);
    }
    fputc('\n', f);
  }
  fclose(f);
}

// ---------------------------------------------------------------- HCI transport

static int acl_credits = 8;  // replaced by Read_Buffer_Size

typedef struct {
  uint8_t data[260];
  int len;
} CmdPkt;
#define CMDQ 32
static CmdPkt cmdq[CMDQ];
static int cmdq_head, cmdq_tail;

// Queued: libusb must not be re-entered from inside its own event callbacks.
static int hci_cmd(uint16_t opcode, const void* params, uint8_t len) {
  int next = (cmdq_tail + 1) % CMDQ;
  if (next == cmdq_head) {
    LOG("HCI command queue full, dropping 0x%04x", opcode);
    return -1;
  }
  CmdPkt* c = &cmdq[cmdq_tail];
  c->data[0] = opcode & 0xff;
  c->data[1] = opcode >> 8;
  c->data[2] = len;
  if (len) memcpy(c->data + 3, params, len);
  c->len = 3 + len;
  cmdq_tail = next;
  return 0;
}

static void cmd_flush(void) {
  while (cmdq_head != cmdq_tail) {
    CmdPkt* c = &cmdq[cmdq_head];
    int r = libusb_control_transfer(dev, 0x20, 0, 0, 0, c->data, c->len, 1000);
    if (debug) LOG("cmd %02x%02x -> %d", c->data[1], c->data[0], r);
    if (r < 0) LOG("HCI command 0x%02x%02x failed: %s", c->data[1], c->data[0], libusb_error_name(r));
    cmdq_head = (cmdq_head + 1) % CMDQ;
  }
}

// Outgoing ACL queue (needed because the controller only has a few buffers).
typedef struct {
  uint8_t data[96];
  int len;
} AclPkt;
#define ACLQ 64
static AclPkt aclq[ACLQ];
static int aclq_head, aclq_tail;

static void acl_flush(void) {
  while (acl_credits > 0 && aclq_head != aclq_tail) {
    AclPkt* p = &aclq[aclq_head];
    int sent = 0;
    int r = libusb_bulk_transfer(dev, EP_ACL_OUT, p->data, p->len, &sent, 200);
    if (r < 0) LOG("ACL send failed: %s", libusb_error_name(r));
    aclq_head = (aclq_head + 1) % ACLQ;
    acl_credits--;
  }
}

static void acl_send(uint16_t handle, uint16_t cid, const uint8_t* data, uint16_t len) {
  if (len + 8 > (int)sizeof aclq[0].data) return;
  int next = (aclq_tail + 1) % ACLQ;
  if (next == aclq_head) return;  // queue full: drop (only happens with speaker floods)
  AclPkt* p = &aclq[aclq_tail];
  uint16_t h = (handle & 0x0fff) | 0x2000;
  p->data[0] = h & 0xff;
  p->data[1] = h >> 8;
  p->data[2] = (len + 4) & 0xff;
  p->data[3] = (len + 4) >> 8;
  p->data[4] = len & 0xff;
  p->data[5] = len >> 8;
  p->data[6] = cid & 0xff;
  p->data[7] = cid >> 8;
  memcpy(p->data + 8, data, len);
  p->len = len + 8;
  aclq_tail = next;
}

// ---------------------------------------------------------------- connections

typedef struct {
  int used;
  uint16_t handle;
  uint8_t addr[6];
  int incoming;
  double connected_at;
  uint16_t ctl_local, ctl_remote, intr_local, intr_remote;
  int ctl_cfg_in, ctl_cfg_out, intr_cfg_in, intr_cfg_out;
  int ready;
  int listen_fd, client_fd;
  char sock_path[128];
  uint8_t rx[64];
  int rx_len;
  unsigned in_ids[256], out_ids[256], ir_seen, write_errs;
  double next_stats;
} Conn;
static Conn conns[MAX_REMOTES];
static uint8_t sig_id = 1;

static Conn* conn_by_handle(uint16_t h) {
  for (int i = 0; i < MAX_REMOTES; i++)
    if (conns[i].used && conns[i].handle == h) return &conns[i];
  return NULL;
}
static Conn* conn_by_addr(const uint8_t* a) {
  for (int i = 0; i < MAX_REMOTES; i++)
    if (conns[i].used && !memcmp(conns[i].addr, a, 6)) return &conns[i];
  return NULL;
}
static int conn_slot(Conn* c) { return (int)(c - conns); }

static void sig_send(Conn* c, uint8_t code, uint8_t id, const uint8_t* data, uint16_t len) {
  uint8_t buf[64];
  buf[0] = code;
  buf[1] = id;
  buf[2] = len & 0xff;
  buf[3] = len >> 8;
  memcpy(buf + 4, data, len);
  acl_send(c->handle, 0x0001, buf, 4 + len);
}
static void l2cap_connect(Conn* c, uint16_t psm, uint16_t scid) {
  uint8_t d[4] = {psm & 0xff, psm >> 8, scid & 0xff, scid >> 8};
  sig_send(c, 0x02, sig_id++, d, 4);
}
static void l2cap_config_req(Conn* c, uint16_t dcid) {
  uint8_t d[8] = {dcid & 0xff, dcid >> 8, 0, 0, 0x01, 0x02, 185, 0};  // MTU 185 like the Wii
  sig_send(c, 0x04, sig_id++, d, 8);
}

static void wm_send(Conn* c, const uint8_t* report_with_a2, int len) {
  if (!c->intr_remote) return;
  acl_send(c->handle, c->intr_remote, report_with_a2, len);
}

static void expose_socket(Conn* c) {
  char a[20];
  addr_str(c->addr, a);
  snprintf(c->sock_path, sizeof c->sock_path, SOCK_DIR "/%02X%02X%02X%02X%02X%02X.sock",
           c->addr[5], c->addr[4], c->addr[3], c->addr[2], c->addr[1], c->addr[0]);
  unlink(c->sock_path);
  c->listen_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  struct sockaddr_un sa = {0};
  sa.sun_family = AF_UNIX;
  snprintf(sa.sun_path, sizeof sa.sun_path, "%s", c->sock_path);
  if (bind(c->listen_fd, (struct sockaddr*)&sa, sizeof sa) < 0 || listen(c->listen_fd, 1) < 0) {
    LOG("could not create %s: %s", c->sock_path, strerror(errno));
    close(c->listen_fd);
    c->listen_fd = -1;
    return;
  }
  fcntl(c->listen_fd, F_SETFL, O_NONBLOCK);
  c->client_fd = -1;
  LOG("Wii Remote %s ready -> %s", a, c->sock_path);
}

static void conn_free(Conn* c) {
  if (c->client_fd >= 0) close(c->client_fd);
  if (c->listen_fd >= 0) close(c->listen_fd);
  if (c->sock_path[0]) unlink(c->sock_path);
  memset(c, 0, sizeof *c);
  c->listen_fd = c->client_fd = -1;
}

static void channel_ready_check(Conn* c) {
  if (c->ready || !c->intr_cfg_in || !c->intr_cfg_out || !c->ctl_cfg_in || !c->ctl_cfg_out) return;
  c->ready = 1;
  // Show the slot on the LEDs until Dolphin takes over.
  uint8_t led[3] = {0xa2, 0x11, (uint8_t)(0x10 << (conn_slot(c) & 3))};
  wm_send(c, led, 3);
  expose_socket(c);
}

static void handle_signaling(Conn* c, const uint8_t* p, int len) {
  while (len >= 4) {
    uint8_t code = p[0], id = p[1];
    uint16_t clen = p[2] | (p[3] << 8);
    const uint8_t* d = p + 4;
    if (4 + clen > len) break;
    switch (code) {
      case 0x02: {  // connection request (remote opens a channel to us)
        uint16_t psm = d[0] | (d[1] << 8), scid = d[2] | (d[3] << 8);
        uint16_t lcid = psm == 0x11 ? 0x0040 : 0x0041;
        if (psm == 0x11) c->ctl_remote = scid, c->ctl_local = lcid;
        else c->intr_remote = scid, c->intr_local = lcid;
        uint8_t r[8] = {lcid & 0xff, lcid >> 8, scid & 0xff, scid >> 8, 0, 0, 0, 0};
        sig_send(c, 0x03, id, r, 8);
        l2cap_config_req(c, scid);
        break;
      }
      case 0x03: {  // connection response
        uint16_t dcid = d[0] | (d[1] << 8), scid = d[2] | (d[3] << 8), res = d[4] | (d[5] << 8);
        if (res == 1) break;  // pending
        if (res != 0) {
          LOG("L2CAP connect refused (result %d)", res);
          break;
        }
        if (scid == 0x0040) c->ctl_remote = dcid;
        else c->intr_remote = dcid;
        l2cap_config_req(c, dcid);
        break;
      }
      case 0x04: {  // configuration request
        uint16_t dcid = d[0] | (d[1] << 8);
        int is_ctl = dcid == c->ctl_local;
        uint16_t their = is_ctl ? c->ctl_remote : c->intr_remote;
        uint8_t r[6] = {their & 0xff, their >> 8, 0, 0, 0, 0};
        sig_send(c, 0x05, id, r, 6);
        if (is_ctl) c->ctl_cfg_in = 1;
        else c->intr_cfg_in = 1;
        channel_ready_check(c);
        break;
      }
      case 0x05: {  // configuration response
        uint16_t scid = d[0] | (d[1] << 8);
        if (scid == c->ctl_local) {
          c->ctl_cfg_out = 1;
          if (!c->intr_local) {  // we are opening the channels: interrupt next
            c->intr_local = 0x0041;
            l2cap_connect(c, 0x13, 0x0041);
          }
        } else {
          c->intr_cfg_out = 1;
        }
        channel_ready_check(c);
        break;
      }
      case 0x06: sig_send(c, 0x07, id, d, 4); break;      // disconnection request
      case 0x08: sig_send(c, 0x09, id, d, clen); break;   // echo
      case 0x0a: {                                        // information request
        uint8_t r[4] = {d[0], d[1], 1, 0};                // not supported
        sig_send(c, 0x0b, id, r, 4);
        break;
      }
      default: break;
    }
    p += 4 + clen;
    len -= 4 + clen;
  }
}

static int ir_offset(uint8_t id) {
  switch (id) {
    case 0x33: return 7;   // buttons + accel + 12 bytes extended IR
    case 0x36: return 4;   // buttons + 10 bytes basic IR + ext
    case 0x37: return 7;   // buttons + accel + 10 bytes basic IR + ext
    case 0x3e: case 0x3f: return 4;
    default: return -1;
  }
}

static void stats_in(Conn* c, const uint8_t* p, int len) {
  if (len < 2) return;
  c->in_ids[p[1]]++;
  int off = ir_offset(p[1]);
  if (off > 0 && len > off + 3) {
    for (int i = off; i < off + 10 && i < len; i++)
      if (p[i] != 0xff) { c->ir_seen++; break; }
  }
  if (p[1] == 0x22 && len >= 6 && p[5] != 0) c->write_errs++;
}

static void stats_print(Conn* c) {
  if (now() < c->next_stats) return;
  c->next_stats = now() + 3;
  char in[256] = "", out[256] = "", t[16];
  for (int i = 0; i < 256; i++) {
    if (c->in_ids[i]) { snprintf(t, sizeof t, "%02x:%u ", i, c->in_ids[i]); strcat(in, t); }
    if (c->out_ids[i]) { snprintf(t, sizeof t, "%02x:%u ", i, c->out_ids[i]); strcat(out, t); }
  }
  LOG("slot %d  from remote [%s]  to remote [%s]  IR-points-seen %u  write-errors %u",
      conn_slot(c) + 1, in, out, c->ir_seen, c->write_errs);
  memset(c->in_ids, 0, sizeof c->in_ids);
  memset(c->out_ids, 0, sizeof c->out_ids);
  c->ir_seen = 0;
}

static void forward_to_client(Conn* c, const uint8_t* p, int len) {
  stats_in(c, p, len);
  if (c->client_fd < 0 || len <= 0 || len > 255) return;
  uint8_t frame[256];
  frame[0] = (uint8_t)len;
  memcpy(frame + 1, p, len);
  if (write(c->client_fd, frame, len + 1) < 0 && errno != EAGAIN) {
    close(c->client_fd);
    c->client_fd = -1;
    LOG("Dolphin released slot %d", conn_slot(c) + 1);
  }
}

static void handle_acl(const uint8_t* buf, int len) {
  if (len < 8) return;
  uint16_t handle = (buf[0] | (buf[1] << 8)) & 0x0fff;
  Conn* c = conn_by_handle(handle);
  if (!c) return;
  uint16_t l2len = buf[4] | (buf[5] << 8), cid = buf[6] | (buf[7] << 8);
  const uint8_t* p = buf + 8;
  int plen = len - 8 < l2len ? len - 8 : l2len;
  if (cid == 0x0001) handle_signaling(c, p, plen);
  else if (cid == c->intr_local && plen > 0 && p[0] == 0xa1) forward_to_client(c, p, plen);
}

// ---------------------------------------------------------------- discovery / paging

static int paging;  // a Create_Connection is outstanding
static double paging_since;
static int inquiring;
static double inquiry_since;
static uint8_t found[8][6];
static int nfound;

static void page(const uint8_t* a) {
  uint8_t cc[13] = {0};
  memcpy(cc, a, 6);
  cc[6] = 0x18;
  cc[7] = 0xcc;   // DM1/DH1/DM3/DH3/DM5/DH5
  cc[8] = 0x01;   // page scan repetition mode R1
  cc[12] = 0x01;  // allow role switch
  if (hci_cmd(0x0405, cc, 13) >= 0) {
    paging = 1;
    paging_since = now();
  }
}

static void inquiry(int limited) {
  // LIAC (what the Wii uses) or GIAC, 2 x 1.28 s
  uint8_t q[5] = {0x00, 0x8b, 0x9e, 0x02, 0x00};
  if (limited) q[0] = 0x00;
  else q[0] = 0x33;
  if (hci_cmd(0x0401, q, 5) >= 0) {
    inquiring = 1;
    inquiry_since = now();
  }
}

static void found_add(const uint8_t* a, uint32_t cod) {
  if ((cod & 0x1ffc) != (WIIMOTE_COD & 0x1ffc)) return;  // peripheral / joystick
  if (conn_by_addr(a)) return;
  for (int i = 0; i < nfound; i++)
    if (!memcmp(found[i], a, 6)) return;
  if (nfound < 8) memcpy(found[nfound++], a, 6);
  char s[20];
  addr_str(a, s);
  LOG("found Wii Remote %s in pairing mode", s);
}

// ---------------------------------------------------------------- HCI events


static void handle_event(const uint8_t* e, int len) {
  if (len < 2) return;
  if (debug && e[0] != 0x13) { stamp(); printf("evt %02x:", e[0]); for (int i = 2; i < len && i < 14; i++) printf(" %02x", e[i]); printf("\n"); fflush(stdout); }
  uint8_t code = e[0];
  const uint8_t* p = e + 2;
  char a[20];
  switch (code) {
    case 0x01:  // inquiry complete
      inquiring = 0;
      break;
    case 0x02:  // inquiry result
      for (int i = 0; i < p[0]; i++) {
        const uint8_t* r = p + 1 + i * 6;
        const uint8_t* cod = p + 1 + p[0] * 9 + i * 3;
        found_add(r, cod[0] | (cod[1] << 8) | (cod[2] << 16));
      }
      break;
    case 0x22:  // inquiry result with RSSI
      for (int i = 0; i < p[0]; i++) {
        const uint8_t* r = p + 1 + i * 14;
        found_add(r, r[8] | (r[9] << 8) | (r[10] << 16));
      }
      break;
    case 0x03: {  // connection complete
      paging = 0;
      if (p[0] != 0) break;
      uint16_t h = p[1] | (p[2] << 8);
      for (int i = 0; i < MAX_REMOTES; i++)
        if (!conns[i].used) {
          Conn* c = &conns[i];
          int was_incoming = c->incoming;
          memset(c, 0, sizeof *c);
          c->listen_fd = c->client_fd = -1;
          c->used = 1;
          c->incoming = was_incoming;
          c->handle = h;
          memcpy(c->addr, p + 3, 6);
          c->connected_at = now();
          addr_str(c->addr, a);
          LOG("connected to %s", a);
          known_get(c->addr, 1);
          if (!c->incoming) {  // we paged it, so we open the channels
            c->ctl_local = 0x0040;
            l2cap_connect(c, 0x11, 0x0040);
          }
          break;
        }
      // remove from the "found" list
      for (int i = 0; i < nfound; i++)
        if (!memcmp(found[i], p + 3, 6)) {
          memmove(found[i], found[i + 1], (nfound - i - 1) * 6);
          nfound--;
          break;
        }
      break;
    }
    case 0x04: {  // incoming connection (a paired remote pressed a button)
      addr_str(p, a);
      LOG("incoming connection from %s", a);
      for (int i = 0; i < MAX_REMOTES; i++)
        if (!conns[i].used) {
          conns[i].incoming = 1;  // consumed by the connection-complete event
          break;
        }
      uint8_t r[7];
      memcpy(r, p, 6);
      r[6] = 0x00;  // become master
      hci_cmd(0x0409, r, 7);
      break;
    }
    case 0x05: {  // disconnection complete
      Conn* c = conn_by_handle(p[1] | (p[2] << 8));
      if (c) {
        addr_str(c->addr, a);
        LOG("Wii Remote %s disconnected (reason 0x%02x)", a, p[3]);
        conn_free(c);
      }
      break;
    }
    case 0x13:  // number of completed packets
      for (int i = 0; i < p[0]; i++) acl_credits += p[1 + i * 4 + 2] | (p[1 + i * 4 + 3] << 8);
      break;
    case 0x16: {  // PIN request: SYNC-button pairing uses the host address
      addr_str(p, a);
      LOG("pairing %s (SYNC)", a);
      uint8_t r[23] = {0};
      memcpy(r, p, 6);
      r[6] = 6;
      memcpy(r + 7, local_addr, 6);
      hci_cmd(0x040d, r, 23);
      break;
    }
    case 0x17: {  // link key request
      Known* k = known_get(p, 0);
      if (k && k->has_key) {
        uint8_t r[22];
        memcpy(r, p, 6);
        memcpy(r + 6, k->key, 16);
        hci_cmd(0x040b, r, 22);
      } else {
        hci_cmd(0x040c, p, 6);
      }
      break;
    }
    case 0x18: {  // link key notification: remember for button-press reconnects
      Known* k = known_get(p, 1);
      if (k) {
        memcpy(k->key, p + 6, 16);
        k->has_key = 1;
        keys_save();
        addr_str(p, a);
        LOG("paired %s - from now on any button reconnects it", a);
      }
      break;
    }
    case 0x0e: {  // command complete
      uint16_t op = p[1] | (p[2] << 8);
      if (op == 0x1009 && p[3] == 0) memcpy(local_addr, p + 4, 6);
      if (op == 0x1005 && p[3] == 0) acl_credits = p[7] | (p[8] << 8);
      break;
    }
    case 0x0f: {  // command status
      uint16_t op = p[2] | (p[3] << 8);
      if (p[0] != 0 && op == 0x0405) paging = 0;
      if (p[0] != 0 && op == 0x0401) inquiring = 0;
      break;
    }
    default: break;
  }
}

// ---------------------------------------------------------------- USB event plumbing

static struct libusb_transfer *evt_xfer, *acl_xfer;
static uint8_t evt_buf[1024], acl_buf[1024];

static void LIBUSB_CALL on_evt(struct libusb_transfer* t) {
  if (debug && t->status != LIBUSB_TRANSFER_COMPLETED) LOG("event transfer status %d", t->status);
  if (t->status == LIBUSB_TRANSFER_COMPLETED && t->actual_length > 0)
    handle_event(t->buffer, t->actual_length);
  if (t->status != LIBUSB_TRANSFER_NO_DEVICE && !quit) libusb_submit_transfer(t);
  else quit = 1;
}
static void LIBUSB_CALL on_acl(struct libusb_transfer* t) {
  if (t->status == LIBUSB_TRANSFER_COMPLETED && t->actual_length > 0)
    handle_acl(t->buffer, t->actual_length);
  if (t->status != LIBUSB_TRANSFER_NO_DEVICE && !quit) libusb_submit_transfer(t);
  else quit = 1;
}

static void pump_usb(int ms) {
  struct timeval tv = {0, ms * 1000};
  libusb_handle_events_timeout_completed(ctx, &tv, NULL);
  cmd_flush();
  acl_flush();
}

// ---------------------------------------------------------------- Dolphin-side sockets

static void pump_sockets(void) {
  for (int i = 0; i < MAX_REMOTES; i++) {
    Conn* c = &conns[i];
    if (!c->used || !c->ready || c->listen_fd < 0) continue;
    if (c->client_fd < 0) {
      int fd = accept(c->listen_fd, NULL, NULL);
      if (fd >= 0) {
        fcntl(fd, F_SETFL, O_NONBLOCK);
        int one = 1;
        setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof one);
        c->client_fd = fd;
        c->rx_len = 0;
        LOG("Dolphin attached to slot %d", i + 1);
      }
      continue;
    }
    for (;;) {
      ssize_t n = read(c->client_fd, c->rx + c->rx_len, sizeof c->rx - c->rx_len);
      if (n == 0 || (n < 0 && errno != EAGAIN)) {
        close(c->client_fd);
        c->client_fd = -1;
        LOG("Dolphin released slot %d", i + 1);
        break;
      }
      if (n < 0) break;
      c->rx_len += n;
      while (c->rx_len > 0 && c->rx_len >= 1 + c->rx[0]) {
        int flen = c->rx[0];
        if (flen > 1) {
          uint8_t id = c->rx[2];
          c->out_ids[id]++;
          if (debug && (id == 0x13 || id == 0x1a || id == 0x16 || id == 0x12)) {
            stamp(); printf("slot %d out:", i + 1);
            for (int j = 1; j <= flen && j <= 12; j++) printf(" %02x", c->rx[j]);
            printf("\n"); fflush(stdout);
          }
        }
        if (flen > 0) wm_send(c, c->rx + 1, flen);
        memmove(c->rx, c->rx + 1 + flen, c->rx_len - 1 - flen);
        c->rx_len -= 1 + flen;
      }
    }
  }
}

// ---------------------------------------------------------------- main

static void on_signal(int s) {
  (void)s;
  quit = 1;
}

int main(int argc, char** argv) {
  debug = getenv("WMHCI_DEBUG") != NULL;
  signal(SIGINT, on_signal);
  signal(SIGTERM, on_signal);
  signal(SIGPIPE, SIG_IGN);
  setvbuf(stdout, NULL, _IOLBF, 0);

  const char* home = getenv("HOME");
  char dir[480];
  snprintf(dir, sizeof dir, "%s/.config/wmhci", home ? home : "/tmp");
  char parent[480];
  snprintf(parent, sizeof parent, "%s/.config", home ? home : "/tmp");
  mkdir(parent, 0755);
  mkdir(dir, 0700);
  snprintf(keyfile, sizeof keyfile, "%s/keys", dir);
  keys_load();
  for (int i = 1; i < argc; i++) {  // optional extra "AA:BB:..[=key]" entries
    char tmp[128];
    snprintf(tmp, sizeof tmp, "%s", argv[i]);
    char* eq = strchr(tmp, '=');
    if (eq) *eq = 0;
    uint8_t a[6];
    if (!parse_addr(tmp, a)) continue;
    Known* k = known_get(a, 1);
    if (k && eq && strlen(eq + 1) == 32) {
      for (int j = 0; j < 16; j++) sscanf(eq + 1 + 2 * j, "%2hhx", &k->key[j]);
      k->has_key = 1;
    }
  }
  keys_save();

  mkdir(SOCK_DIR, 0755);
  DIR* d = opendir(SOCK_DIR);  // remove stale sockets from a previous run
  if (d) {
    struct dirent* de;
    while ((de = readdir(d)))
      if (strstr(de->d_name, ".sock")) {
        char path[300];
        snprintf(path, sizeof path, SOCK_DIR "/%s", de->d_name);
        unlink(path);
      }
    closedir(d);
  }
  for (int i = 0; i < MAX_REMOTES; i++) conns[i].listen_fd = conns[i].client_fd = -1;

  if (libusb_init(&ctx) < 0) return 1;
  // Any of the common USB Bluetooth dongles that speak plain HCI.
  static const uint16_t ids[][2] = {{0x0a12, 0x0001}, {0x0a5c, 0x21e8}, {0x0b05, 0x190e}};
  for (size_t i = 0; i < sizeof ids / sizeof ids[0] && !dev; i++)
    dev = libusb_open_device_with_vid_pid(ctx, ids[i][0], ids[i][1]);
  if (!dev) {
    LOG("no supported Bluetooth adapter found (is Dolphin using it in passthrough mode?)");
    return 2;
  }
  libusb_set_auto_detach_kernel_driver(dev, 1);
  int r = libusb_claim_interface(dev, 0);
  if (r < 0) {
    LOG("cannot claim adapter: %s (close Dolphin or other programs using it)", libusb_error_name(r));
    return 3;
  }

  evt_xfer = libusb_alloc_transfer(0);
  acl_xfer = libusb_alloc_transfer(0);
  libusb_fill_interrupt_transfer(evt_xfer, dev, EP_EVT, evt_buf, sizeof evt_buf, on_evt, NULL, 0);
  libusb_fill_bulk_transfer(acl_xfer, dev, EP_ACL_IN, acl_buf, sizeof acl_buf, on_acl, NULL, 0);
  libusb_submit_transfer(evt_xfer);
  libusb_submit_transfer(acl_xfer);

  hci_cmd(0x0c03, NULL, 0);  // reset
  for (int i = 0; i < 50; i++) pump_usb(10);
  hci_cmd(0x1009, NULL, 0);  // read BD_ADDR
  hci_cmd(0x1005, NULL, 0);  // read buffer size
  for (int i = 0; i < 20; i++) pump_usb(10);
  uint8_t cod[3] = {0x04, 0x02, 0x00};  // present ourselves like a Wii
  hci_cmd(0x0c24, cod, 3);
  uint8_t scan = 0x02;  // page scan: paired remotes reconnect on a button press
  hci_cmd(0x0c1a, &scan, 1);
  uint8_t page_timeout[2] = {0xa0, 0x0f};  // 4000 slots = 2.5 s
  hci_cmd(0x0c18, page_timeout, 2);
  uint8_t interlaced = 0x01;
  hci_cmd(0x0c47, &interlaced, 1);
  char name[248] = "Nintendo Wii";
  hci_cmd(0x0c13, name, 248);
  for (int i = 0; i < 20; i++) pump_usb(10);

  char a[20];
  addr_str(local_addr, a);
  LOG("wmhcid running on adapter %s (%d ACL buffers), %d known remote(s)", a, acl_credits, nknown);
  LOG("pair: press SYNC on the remote. reconnect: press any button on a paired remote.");

  int known_turn = 0, liac = 1;
  while (!quit) {
    pump_usb(2);
    pump_sockets();
    for (int i = 0; i < MAX_REMOTES; i++)
      if (conns[i].used && conns[i].client_fd >= 0) stats_print(&conns[i]);

    // Connections where we paged but the remote never opened channels (or vice versa):
    // give it a second, then open them ourselves.
    for (int i = 0; i < MAX_REMOTES; i++) {
      Conn* c = &conns[i];
      if (c->used && c->incoming && !c->ctl_remote && now() - c->connected_at > 1.0) {
        c->incoming = 0;
        c->ctl_local = 0x0040;
        l2cap_connect(c, 0x11, 0x0040);
      }
    }

    if (paging && now() - paging_since > 4) paging = 0;
    if (inquiring && now() - inquiry_since > 6) inquiring = 0;
    if (paging || inquiring) continue;

    int free_slot = 0;
    for (int i = 0; i < MAX_REMOTES; i++)
      if (!conns[i].used) free_slot = 1;
    if (!free_slot) continue;

    // 1) remotes found in pairing mode get connected first
    if (nfound) {
      page(found[0]);
      memmove(found[0], found[1], (nfound - 1) * 6);
      nfound--;
      continue;
    }
    // 2) Leave the radio idle most of the time: while we page or inquire the adapter
    //    cannot page-scan, so remotes reconnecting on a button press would not get through.
    //    Every 4 s: page one known remote (catches remotes with SYNC/1+2 pressed).
    //    Every 30 s: one inquiry for remotes we have never seen.
    static double next_page, next_inquiry;
    if (now() >= next_inquiry) {
      next_inquiry = now() + 30;
      next_page = now() + 6;
      inquiry(liac);
      liac = !liac;
    } else if (nknown && now() >= next_page) {
      next_page = now() + 4;
      for (int tries = 0; tries < nknown; tries++) {
        Known* k = &known[known_turn++ % nknown];
        if (!conn_by_addr(k->addr)) {
          page(k->addr);
          break;
        }
      }
    }
  }

  LOG("shutting down");
  for (int i = 0; i < MAX_REMOTES; i++)
    if (conns[i].used) {
      uint8_t dd[3] = {conns[i].handle & 0xff, conns[i].handle >> 8, 0x13};
      hci_cmd(0x0406, dd, 3);
    }
  for (int i = 0; i < 30; i++) pump_usb(10);
  for (int i = 0; i < MAX_REMOTES; i++)
    if (conns[i].used) conn_free(&conns[i]);
  libusb_cancel_transfer(evt_xfer);
  libusb_cancel_transfer(acl_xfer);
  for (int i = 0; i < 10; i++) pump_usb(10);
  libusb_release_interface(dev, 0);
  libusb_close(dev);
  libusb_exit(ctx);
  return 0;
}
