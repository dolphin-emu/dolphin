//A basic test implementation of Netcore for IPC in Dolphin

#include "stdafx.h"
#include "Core/State.h"
#include "NetcoreClient.h"

#include <string>  
#include <iostream>  

#include "Core/PatchEngine.h"
#include "Core/PowerPC/PowerPC.h"

#include <msclr/marshal_cppstd.h>

using namespace cli;
using namespace System;
using namespace RTCV;
using namespace RTCV::NetCore; 

#ifdef DEBUG
  #using <system.dll>
  using namespace System::Diagnostics;
#endif


/*
Trace::Listeners->Add(gcnew TextWriterTraceListener(Console::Out));
Trace::AutoFlush = true;
Trace::WriteLine(filename);
*/

int main() {}


//Define this in here as it's managed and doesn't want to work in NetcoreClient.h
public ref class NetcoreClient
{
public:
  static RTCV::NetCore::NetCoreSpec ^ spec = gcnew RTCV::NetCore::NetCoreSpec();
  void OnMessageReceived(Object^  sender, RTCV::NetCore::NetCoreEventArgs^  e);
  void Initialize();
  void LoadState(String^ filename);
  void NetcoreClient::SaveState(String^ filename, bool wait);
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
}
void NetcoreClient::SaveState(String^ filename, bool wait) {

  std::string converted_filename = msclr::interop::marshal_as< std::string >(filename);
  State::SaveAs(converted_filename, wait);
}


/*ENUMS FOR THE SWITCH STATEMENT*/
enum COMMANDS {
  LOADSTATE,
  SAVESTATE,
  UNKNOWN
};

COMMANDS CheckCommand(String^ inString) {
  if (inString == "LOADSTATE") return LOADSTATE;
  if (inString == "SAVESTATE") return SAVESTATE;
  return UNKNOWN;
}

/* THIS IS WHERE YOU HANDLE ANY RECEIVED MESSAGES */
void NetcoreClient::OnMessageReceived(Object^ sender, NetCoreEventArgs^ e)
{
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

    default:
      break;
  }
}
