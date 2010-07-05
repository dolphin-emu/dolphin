#ifndef UDPNUNCHUCK_H
#define UDPNUNCHUCK_H

#include "Nunchuk.h"

class UDPWrapper;

namespace WiimoteEmu
{

class UDPNunchuk : public Nunchuk
{
public:
	UDPNunchuk(UDPWrapper * _wrp) : wrp(_wrp ) {name="UDP Nunchuk";}; //sorry for this :p I just dont' feel like rewriting the whole class for a name :p
	virtual void GetState( u8* const data, const bool focus );
private:
	UDPWrapper * wrp;
};

}
#endif