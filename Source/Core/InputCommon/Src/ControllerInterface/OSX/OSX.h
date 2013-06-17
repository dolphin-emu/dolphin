#pragma once

#include "../Device.h"

namespace ciface
{
namespace OSX
{

void Init(std::vector<Core::Device*>& devices, void *window);
void DeInit();

void DeviceElementDebugPrint(const void *, void *);

}
}
