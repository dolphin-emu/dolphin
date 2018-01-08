// ConsoleApplication1.cpp : main project file.

#include "stdafx.h"
#include "Core/State.h"
#include "test.h"

using namespace cli;
using namespace System;
using namespace System::Threading;
using namespace RTCV;
using namespace RTCV::NetCore;


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
};




int main(array<System::String ^> ^args)
{

  return 0;
}


void Test::CommandTest()
{
  auto spec = gcnew NetCore::NetCoreSpec();
  TestClient::TestClient::SetSpec(spec);

  TestClient::TestClient::StartClient();

  Loop^ loop1 = gcnew Loop();
  Thread^ thread1 = gcnew Thread(gcnew ThreadStart(loop1, &Loop::LoopTest));
  thread1->Start();

  //Thread thread = gcnew Thread(gcnew ThreadStart(WorkThreadFunction));
  
  //spec->OnMessageReceived += gcnew EventHandler<NetCoreEventArgs^>(Test::EventTest)
    
}
