#ifndef _STATE_H
#define _STATE_H

// None of these happen instantly - they get scheduled as an event.

void State_Init();
void State_Shutdown();

void State_Save();
void State_Load();

#endif
