//A basic test implementation of Netcore for IPC in Dolphin

#pragma warning(disable:4564) 

#include "stdafx.h"

#include "Core/State.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/DSP.h"
#include "Core/PatchEngine.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"

#include <string>  
#include <iostream>

#include "NetcoreClient.h"

#include <msclr/marshal_cppstd.h>

using namespace cli;
using namespace System;
using namespace RTCV;
using namespace RTCV::NetCore;
using namespace System::Runtime::InteropServices;

#using <system.dll>
using namespace System::Diagnostics;

#define SRAM_SIZE 25165824
#define ARAM_SIZE 16777216
#define EXRAM_SIZE 67108864

/*
Trace::Listeners->Add(gcnew TextWriterTraceListener(Console::Out));
Trace::AutoFlush = true;
Trace::WriteLine(filename);
*/



public ref class MemoryDomain
{
public:
  int size = 0;
  int offset = 0;
  String ^ name = "NULL";
};


//Define this in here as it's managed and it can't be in NetcoreClient.h as that's included in unmanaged code. Could probably move this to a header
public ref class NetcoreClient
{
public:
	static RTCV::NetCore::NetCoreSpec ^ spec;
	static RTCV::NetCore::NetCoreConnector ^ connector;

  void OnMessageReceived(Object^  sender, RTCV::NetCore::NetCoreEventArgs^  e);
  
  void NetcoreClient::StartClient();
  void NetcoreClient::RestartClient();

  void NetcoreClient::LoadState(String^ filename);
  void NetcoreClient::SaveState(String^ filename, bool wait);
  Byte NetcoreClient::PeekByte(long address, MemoryDomain ^ domain);
  void NetcoreClient::PokeByte(long address, Byte ^ value, MemoryDomain ^ domain);
  array<Byte>^ NetcoreClient::PeekBytes(long address, int range, MemoryDomain ^ domain);
  void NetcoreClient::PokeBytes(long address, array<Byte>^ value, int range, MemoryDomain ^ domain);


  array<Byte>^ NetcoreClient::PeekAddresses(array<long>^ addresses);
  void NetcoreClient::PokeAddresses(array<long>^ addresses, array<Byte>^ values);


  MemoryDomain^ NetcoreClient::GetDomain(long address, int range, MemoryDomain ^ domain);
};



//Create our NetcoreClient
void NetcoreClientInitializer::Initialize()
{
  NetcoreClient^ client = gcnew NetcoreClient;
  client->StartClient();
}

void NetcoreClientInitializer::isWii()
{
  if (SConfig::GetInstance().bWii)
    NetcoreClient::connector->SendSyncedMessage("WII");
  else
    NetcoreClient::connector->SendSyncedMessage("GAMECUBE");
}

//Initialize it 
void NetcoreClient::StartClient() 
{
	NetcoreClient::spec = gcnew RTCV::NetCore::NetCoreSpec();
	NetcoreClient::spec->Side = NetworkSide::CLIENT;
	NetcoreClient::spec->MessageReceived += gcnew EventHandler<NetCoreEventArgs^>(this, &NetcoreClient::OnMessageReceived);

	NetcoreClient::connector = gcnew RTCV::NetCore::NetCoreConnector(spec);
}

void NetcoreClient::RestartClient()
{
  NetcoreClient::connector->Kill();
  NetcoreClient::connector = nullptr;
  StartClient();
}

/* IMPLEMENT YOUR COMMANDS HERE */
void NetcoreClient::LoadState(String^ filename) {

  std::string converted_filename = msclr::interop::marshal_as< std::string >(filename);
  State::LoadAs(converted_filename);

  Trace::Listeners->Add(gcnew TextWriterTraceListener(Console::Out));
  Trace::AutoFlush = true;
  Trace::WriteLine(filename);
}
void NetcoreClient::SaveState(String^ filename, bool wait) {

  std::string converted_filename = msclr::interop::marshal_as< std::string >(filename);
  State::SaveAs(converted_filename, wait);
}

//MEMORY OFFSETS ARE ABSOLUTE
//THIS IS CURRENTLY BEING HANDLED ON THE DOLPHIN SIDE
//SRAM = 0x80000000-0x817FFFFF
//EXRAM = 0x90000000-0x93FFFFFF

//ARAM = 0x80000000-0x81000000
//ARAM HAS TO BE WRITTEN TO WITH A DIFFERENT FUNCTION THAN THE OTHERS

//Memory::Write_U8 and Memory::Read_U8 for SRAM and EXRAM
//DSP::ReadARAM and DSP::WriteAram for ARAM


//THE INTERNAL FUNCTIONS TAKE VALUE, ADDRESS NOT ADDRESS,VALUE

void NetcoreClient::PokeByte(long address, Byte ^ value, MemoryDomain ^ domain) {

  if (domain->name == "SRAM" && (address - domain->offset) < domain->size)
    Memory::Write_U8((Convert::ToByte(value)), Convert::ToUInt32(address));
  else if (domain->name == "EXRAM" && (address - domain->offset) < domain->size)
    Memory::Write_U8(Convert::ToByte(value), Convert::ToUInt32(address));
  else if (domain->name == "ARAM" && (address - domain->size - domain->offset) < domain->size)
    DSP::WriteARAM(Convert::ToByte(value), Convert::ToUInt32(address - SRAM_SIZE));
}

Byte NetcoreClient::PeekByte(long address, MemoryDomain ^ domain) {

  if (domain->name == "SRAM" && (address - domain->offset) < domain->size)
    return Memory::Read_U8(Convert::ToUInt32(address));

  else if (domain->name == "EXRAM" && (address - domain->offset) < domain->size)
    return Memory::Read_U8(Convert::ToUInt32(address));

  else if (domain->name == "ARAM" && (address - domain->size - domain->offset) < domain->size)
    return DSP::ReadARAM(Convert::ToUInt32(address - SRAM_SIZE));

  return -1;
}


void NetcoreClient::PokeBytes(long address, array<Byte>^ value, int range, MemoryDomain ^ domain){

 // array<Byte>^ byte = gcnew array<Byte>(range);

  for (int i = 0; i < range; i++)
    PokeByte(address + i, value[i], domain);

}


array<Byte>^ NetcoreClient::PeekBytes(long address, int range, MemoryDomain ^ domain) {

  array<Byte>^ byte = gcnew array<Byte>(range);
  for (int i = 0; i < range; i++)
    byte[i] = PeekByte(address + i, domain);

  return byte;
}



array<Byte>^ NetcoreClient::PeekAddresses(array<long>^ addresses) {

  MemoryDomain ^ domain = gcnew MemoryDomain;
  array<Byte>^ bytes = gcnew array<Byte>(sizeof(addresses));

  for (int i = 0; i < sizeof(addresses); i++) {
    domain = GetDomain(addresses[i], 1, domain);
    bytes[i] = PeekByte(addresses[i], domain);
  }
    
  return bytes;
}

void NetcoreClient::PokeAddresses(array<long>^ addresses, array<Byte>^ values) {

  MemoryDomain ^ domain = gcnew MemoryDomain;
  for (int i = 0; i < sizeof(addresses); i++) {
    domain = GetDomain(addresses[i], 1, domain);
    PokeByte(addresses[i], values[i], domain);
  }
    
}


MemoryDomain^ NetcoreClient::GetDomain(long address, int range, MemoryDomain ^ domain) {
  //Hardcode this logic for now.
  if (address + range <= SRAM_SIZE) {
    domain->name = ("SRAM");
    domain->size = SRAM_SIZE;
  }
  else if (SConfig::GetInstance().bWii) {
    domain->name = "EXRAM";
    domain->offset = SRAM_SIZE;
    domain->size = EXRAM_SIZE;
  }
  else {
    domain->name = "ARAM";
    domain->size = ARAM_SIZE;
  }
  return domain;
}

/*ENUMS FOR THE SWITCH STATEMENT*/
enum COMMANDS {
  LOADSTATE,
  SAVESTATE,
  POKEBYTE,
  PEEKBYTE,
  POKEBYTES,
  PEEKBYTES,
  POKEADDRESSES,
  PEEKADDRESSES,
  UNKNOWN
};

COMMANDS CheckCommand(String^ inString) {
  if (inString == "LOADSTATE") return LOADSTATE;
  if (inString == "SAVESTATE") return SAVESTATE;
  if (inString == "POKEBYTE") return POKEBYTE;
  if (inString == "PEEKBYTE") return PEEKBYTE;
  if (inString == "POKEBYTES") return POKEBYTES;
  if (inString == "PEEKBYTES") return PEEKBYTES;
  if (inString == "POKEADDRESSES") return POKEADDRESSES;
  if (inString == "PEEKADDRESSES") return PEEKADDRESSES;
  return UNKNOWN;
}

/* THIS IS WHERE YOU HANDLE ANY RECEIVED MESSAGES */
void NetcoreClient::OnMessageReceived(Object^ sender, NetCoreEventArgs^ e)
{

  Trace::Listeners->Add(gcnew TextWriterTraceListener(Console::Out));
  Trace::AutoFlush = true;

  NetCoreMessage ^ message = e->message;

  //Can't define this unless it's used as SLN is set to treat warnings as errors.
  //NetCoreSimpleMessage ^ simpleMessage = (NetCoreSimpleMessage^)message;

  NetCoreAdvancedMessage ^ advancedMessage = (NetCoreAdvancedMessage^)message;

  switch (CheckCommand(message->Type)) {
  case LOADSTATE: {
    if (Core::GetState() == Core::State::Running)
      LoadState((advancedMessage->objectValue)->ToString());
  }
    
    break;

  case SAVESTATE: {
    if (Core::GetState() == Core::State::Running)
      SaveState((advancedMessage->objectValue)->ToString(), 0);
  }
    break;

  case POKEBYTE: {

    if (Core::GetState() == Core::State::Running) {

      Trace::WriteLine("Entering POKEBYTE");

      long address = Convert::ToInt64(((array<Object^>^)advancedMessage->objectValue)[0]);
      Byte value = Convert::ToByte(((array<Object^>^)advancedMessage->objectValue)[1]);

      MemoryDomain ^ domain = gcnew MemoryDomain;
      domain = GetDomain(address,1,domain);

      PokeByte(address, value, domain);
      Trace::WriteLine("Exiting POKEBYTE");
    }
    break;

  }

  case PEEKBYTE:
  {
    if (Core::GetState() == Core::State::Running) {
      Trace::WriteLine("Entering PEEKBYTE");
      

      long address = Convert::ToInt64(advancedMessage->objectValue);
      MemoryDomain ^ domain = gcnew MemoryDomain;

      domain = GetDomain(address, 1, domain);
      e->setReturnValue(PeekByte(address, domain));
       Trace::WriteLine("Exiting PEEKBYTE");
    }
    break;
  }
  case POKEBYTES: {

    if (Core::GetState() == Core::State::Running) {
      Trace::WriteLine("Entering POKEBYTES");

      long address = Convert::ToInt64(((array<Object^>^)advancedMessage->objectValue)[0]);
      int range = Convert::ToInt32(((array<Object^>^)advancedMessage->objectValue)[1]);
      array<Byte>^ value = (array<Byte>^)((array<Object^>^)advancedMessage->objectValue)[2];

      MemoryDomain ^ domain = gcnew MemoryDomain;
      domain = GetDomain(address, range, domain);

      PokeBytes(address, value, range, domain);
      Trace::WriteLine("Exiting POKEBYTES");
    }
    break;
  }

  case PEEKBYTES:
  {

    if (Core::GetState() == Core::State::Running) {
      Trace::WriteLine("Entering PEEKBYTES");

      long address = Convert::ToInt64(((array<Object^>^)advancedMessage->objectValue)[0]);
      int range = Convert::ToByte(((array<Object^>^)advancedMessage->objectValue)[1]);

      MemoryDomain ^ domain = gcnew MemoryDomain;
      domain = GetDomain(address, range, domain);

      e->setReturnValue(PeekBytes(address, range, domain));
      Trace::WriteLine("Exiting PEEKBYTES");
    }
    break;
  }

  case POKEADDRESSES: {

    if (Core::GetState() == Core::State::Running) {
      Trace::WriteLine("Entering POKEADDRESSES");

      array<long>^ addresses = (array<long>^)((array<Object^>^)advancedMessage->objectValue)[0];
      array<Byte>^ values = (array<Byte>^)((array<Object^>^)advancedMessage->objectValue)[1];


      PokeAddresses(addresses, values);
      Trace::WriteLine("Exiting POKEADDRESSES");
    }
    break;
  }

  case PEEKADDRESSES:
  {

    if (Core::GetState() == Core::State::Running) {
      Trace::WriteLine("Entering PEEKADDRESSES");

      array<long>^ addresses = (array<long>^)((array<Object^>^)advancedMessage->objectValue)[0];
      
      e->setReturnValue(PeekAddresses(addresses));
      Trace::WriteLine("Exiting PEEKADDRESSES");
    }
    break;
  }

  default:
    break;
  }
}
