#ifndef _GCLIBLOC_H
#define _GCLIBLOC_H

int LoadSymbolsFromO(const char* filename, unsigned int base, unsigned int count);

void Debugger_LoadSymbolTable(const char* _szFilename);
void Debugger_SaveSymbolTable(const char* _szFilename);
void Debugger_LocateSymbolTables(const char* _szFilename);
void Debugger_ResetSymbolTables();

#endif
