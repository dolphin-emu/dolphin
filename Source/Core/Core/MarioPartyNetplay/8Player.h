#ifndef MPN_8PLAYER_H
#define MPN_8PLAYER_H

#include <stdlib.h>
#include "InputCommon/GCPadStatus.h"

#define MPN_TEAM_ACTIVE  (1 << 0)
#define MPN_TEAM_L_READY (1 << 1)
#define MPN_TEAM_R_READY (1 << 2)

typedef struct mpn_team_t
{
  uint8_t     Flags;
  GCPadStatus LeftPad;
  GCPadStatus RightPad;
} mpn_team_t;

bool mpn_8p_active                ();
GCPadStatus mpn_8p_combined_input (uint8_t Port);
void mpn_8p_free                  ();
void mpn_8p_init                  ();
bool mpn_8p_port_ready            (uint8_t Port);
void mpn_8p_push_back_input       (GCPadStatus* Pad, uint8_t Port);
void mpn_8p_set_port_active       (uint8_t Port, bool Active);

extern mpn_team_t* Teams;

#endif
