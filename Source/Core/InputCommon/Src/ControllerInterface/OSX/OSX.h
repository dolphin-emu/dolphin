#pragma once

#include "../ControllerInterface.h"

namespace ciface
{
namespace OSX
{

void Init(std::vector<ControllerInterface::Device*>& devices, void *window);
void DeInit();

void DeviceElementDebugPrint(const void *, void *);

}
}
