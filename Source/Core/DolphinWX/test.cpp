// ConsoleApplication1.cpp : main project file.

#include "stdafx.h"
#include "Core/State.h"
#include "test.h"
#include "Core/PatchEngine.h"

#include "Core/PowerPC/PowerPC.h"

using namespace cli;
using namespace System;
using namespace System::Threading;
using namespace RTCV;
using namespace RTCV::NetCore;

/*

public ref class Loop
{
public:
  void LoopTest()
  {
    while (1) {
      int command = TestClient::TestClient::test;

      if (command == 1) {
        State::Load(2);
        TestClient::TestClient::test = 0;
      }
      Thread::Sleep(5000);
    }
  }
};*/



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


void Test2::OnMessageReceived(Object^ sender, NetCoreEventArgs^ e)
{
  
  if (TestClient::TestClient::test == 1) {
    PowerPC::HostWrite_U32(1073741824, 2147483648);

  }
}

void Test::CommandTest()
{
  Test2^ a = gcnew Test2;
  a->NewTest();
 // Test2::NewTest();
}
 

void Test2::NewTest(){
  gcnew RTCV::NetCore::NetCoreSpec();
  TestClient::TestClient::SetSpec(spec);

  TestClient::TestClient::StartClient();
  /*
  Loop^ loop1 = gcnew Loop();
  Thread^ thread1 = gcnew Thread(gcnew ThreadStart(loop1, &Loop::LoopTest));
  thread1->Start();
  */

  spec->MessageReceived += gcnew EventHandler<NetCoreEventArgs^>(this, &Test2::OnMessageReceived);

  
  //Thread thread = gcnew Thread(gcnew ThreadStart(WorkThreadFunction));

  // spec->OnMessageReceived += gcnew EventHandler<NetCoreEventArgs^>(OnMessageReceived);

}
