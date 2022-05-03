#include "StateAuxillary.h"
#include "Core/State.h"
#include <thread>

void StateAuxillary::saveState(const std::string& filename, bool wait) {
  std::thread t1(&State::SaveAs, filename, wait);
  //State::SaveAs(filename, wait);
  t1.detach();
}

void StateAuxillary::startRecording(const Movie::ControllerTypeArray& controllers, const Movie::WiimoteEnabledArray& wiimotes)
{
  std::thread t2(&Movie::BeginRecordingInput, controllers, wiimotes);
  t2.detach();
}
