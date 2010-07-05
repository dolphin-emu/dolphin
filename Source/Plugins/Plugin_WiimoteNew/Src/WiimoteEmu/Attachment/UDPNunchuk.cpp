#include "UDPNunchuk.h"
#include "UDPWrapper.h"
#include "UDPWiimote.h"

#define NUNCHUK_C		0x02
#define NUNCHUK_Z		0x01

namespace WiimoteEmu
{

void UDPNunchuk::GetState( u8* const data, const bool focus )
{
	Nunchuk::GetState(data,focus);
	if (!(wrp->inst)) return;

	wm_extension* const ncdata = (wm_extension*)data;
	u8 mask;
	float x,y;
	wrp->inst->getNunchuck(x,y,mask);
	if (mask&UDPWM_NC) ncdata->bt&=~NUNCHUK_C;
	if (mask&UDPWM_NZ) ncdata->bt&=~NUNCHUK_Z;
	ncdata->jx=u8(0x80+x*127);
	ncdata->jy=u8(0x80+y*127);
}

}