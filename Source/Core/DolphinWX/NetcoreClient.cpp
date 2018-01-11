//A basic test implementation of Netcore for IPC in Dolphin

#include "stdafx.h"

#include "Core/State.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/DSP.h"
#include "Core/PatchEngine.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/ConfigManager.h"

#include <string>  
#include <iostream>

#include "NetcoreClient.h"

#include <msclr/marshal_cppstd.h>

using namespace cli;
using namespace System;
using namespace RTCV;
using namespace RTCV::NetCore; 

#using <system.dll>
using namespace System::Diagnostics;


/*
Trace::Listeners->Add(gcnew TextWriterTraceListener(Console::Out));
Trace::AutoFlush = true;
Trace::WriteLine(filename);
*/

int main() {}


//Define this in here as it's managed and doesn't want to work in NetcoreClient.h
public ref class NetcoreClient
{
private:
  bool isWii;
public:
  static RTCV::NetCore::NetCoreSpec ^ spec = gcnew RTCV::NetCore::NetCoreSpec();
  void OnMessageReceived(Object^  sender, RTCV::NetCore::NetCoreEventArgs^  e);
  void Initialize();

  void LoadState(String^ filename);
  void NetcoreClient::SaveState(String^ filename, bool wait);
  void NetcoreClient::PokeByte(long address, Byte value, String ^ domain);
  void NetcoreClient::PeekByte(long address, String ^ domain);
};



//Create our NetcoreClient
void NetcoreClientInitializer::Initialize()
{
  NetcoreClient^ client = gcnew NetcoreClient;
  client->Initialize();
}

//Initialize it 
void NetcoreClient::Initialize() {
  gcnew RTCV::NetCore::NetCoreSpec();
  TestClient::TestClient::SetSpec(spec);
  TestClient::TestClient::StartClient();

  //Hook into MessageReceived so we can respond to commands
  spec->MessageReceived += gcnew EventHandler<NetCoreEventArgs^>(this, &NetcoreClient::OnMessageReceived);
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
void NetcoreClient::PokeByte(long address, Byte value, String ^ domain) {

  if (domain == "ARAM")
   DSP::WriteARAM(Convert::ToByte(value), Convert::ToUInt32(address));
  else
    Memory::Write_U8(Convert::ToByte(value), Convert::ToUInt32(address));

}

void NetcoreClient::PeekByte(long address, String ^ domain) {


  if(domain == "ARAM")
    TestClient::TestClient::connector->SendMessage("PEEKBYTE", DSP::ReadARAM(Convert::ToUInt32(address)));
  else
    TestClient::TestClient::connector->SendMessage("PEEKBYTE", Memory::Read_U8(Convert::ToUInt32(address)));

}


/*ENUMS FOR THE SWITCH STATEMENT*/
enum COMMANDS {
  LOADSTATE,
  SAVESTATE,
  POKEBYTE,
  PEEKBYTE,
  UNKNOWN
};

COMMANDS CheckCommand(String^ inString) {
  if (inString == "LOADSTATE") return LOADSTATE;
  if (inString == "SAVESTATE") return SAVESTATE;
  if (inString == "POKEBYTE") return POKEBYTE;
  if (inString == "PEEKBYTE") return PEEKBYTE;
  return UNKNOWN;
}

/* THIS IS WHERE YOU HANDLE ANY RECEIVED MESSAGES */
void NetcoreClient::OnMessageReceived(Object^ sender, NetCoreEventArgs^ e)
{

  Trace::Listeners->Add(gcnew TextWriterTraceListener(Console::Out));
  Trace::AutoFlush = true;

  NetCoreMessage ^ message = e->message;

  //Can't define this unless it's used as Dolphin treats warnings as errors.
  //NetCoreSimpleMessage ^ simpleMessage = (NetCoreSimpleMessage^)message;

  NetCoreAdvancedMessage ^ advancedMessage = (NetCoreAdvancedMessage^)message;

  switch (CheckCommand(message->Type)) {
    case LOADSTATE:
      LoadState((advancedMessage->objectValue)->ToString());
      break;

    case SAVESTATE:
      SaveState((advancedMessage->objectValue)->ToString(), 0);
      break;

    case POKEBYTE: {

      Trace::WriteLine("1");

      //It appears I have to copy like this because I can't seem to cast it 
      array<Object^> ^ test = (array<Object^>^)advancedMessage->objectValue;

      long address = Convert::ToInt64(test[0]);
      Byte value = Convert::ToByte(test[1]);
      String^ domain = "NULL";

      //Hardcode this logic for now.
      if (address <= 25165824)
        domain = "SRAM";
      else if (SConfig::GetInstance().bWii)
        domain = "EXRAM";
      else
        domain = "ARAM";

      PokeByte(address, value, domain);
      

    }
    case PEEKBYTE: {
      long address = Convert::ToInt64(advancedMessage->objectValue);
      String^ domain = "NULL";

      //Hardcode this logic for now.
      if (address <= 25165824)
        domain = "SRAM";
      else if (SConfig::GetInstance().bWii)
        domain = "EXRAM";
      else
        domain = "ARAM";

      PeekByte(address, domain);

    }
      break;

    default:
      break;
  }
}
