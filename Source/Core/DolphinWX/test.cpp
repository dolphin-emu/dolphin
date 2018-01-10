// ConsoleApplication1.cpp : main project file.

#using <system.dll>

#include "stdafx.h"
#include "Core/State.h"
#include "test.h"

#include <string>  
#include <iostream>  

#include "Core/PatchEngine.h"
#include "Core/PowerPC/PowerPC.h"

#include <msclr/marshal_cppstd.h>

using namespace cli;
using namespace System;
using namespace System::Threading;
using namespace System::Diagnostics;
using namespace RTCV;
using namespace RTCV::NetCore;

/*
Trace::Listeners->Add(gcnew TextWriterTraceListener(Console::Out));
Trace::AutoFlush = true;
Trace::WriteLine(filename);
*/

public ref class Test2
{
private:
  static int const test = 1;
public:
  static RTCV::NetCore::NetCoreSpec ^ spec = gcnew RTCV::NetCore::NetCoreSpec();
  void OnMessageReceived(Object^  sender, RTCV::NetCore::NetCoreEventArgs^  e);
  void NewTest();
};


int main(array<System::String ^> ^args)
{
  return 0;
}


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


void LoadState(String^ filename) {

  std::string converted_filename = msclr::interop::marshal_as< std::string >(filename);
  State::LoadAs(converted_filename);
}

void SaveState(String^ filename, bool wait) {

  std::string converted_filename = msclr::interop::marshal_as< std::string >(filename);
  State::SaveAs(converted_filename, wait);
}


void Test2::OnMessageReceived(Object^ sender, NetCoreEventArgs^ e)
{
  NetCoreMessage ^ message = e->message;
  NetCoreSimpleMessage ^ simpleMessage = (NetCoreSimpleMessage^)message;
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

void Test2::NewTest() {
  gcnew RTCV::NetCore::NetCoreSpec();
  TestClient::TestClient::SetSpec(spec);
  TestClient::TestClient::StartClient();

  spec->MessageReceived += gcnew EventHandler<NetCoreEventArgs^>(this, &Test2::OnMessageReceived);

}


void Test::CommandTest()
{
  Test2^ a = gcnew Test2;
  a->NewTest();
}
 
