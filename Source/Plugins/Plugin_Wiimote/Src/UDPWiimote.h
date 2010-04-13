#ifndef UDPWIIMOTE_H
#define UDPWIIMOTE_H

#include "Common.h"

#define UDPWM_B1 (1<<0)
#define UDPWM_B2 (1<<1)
#define UDPWM_BA (1<<2)
#define UDPWM_BB (1<<3)
#define UDPWM_BP (1<<4)
#define UDPWM_BM (1<<5)
#define UDPWM_BH (1<<6)
#define UDPWM_BU (1<<7)
#define UDPWM_BD (1<<8)
#define UDPWM_BL (1<<9)
#define UDPWM_BR (1<<10)
#define UDPWM_SK (1<<11)
#define UDPWM_NC (1<<0)
#define UDPWM_NZ (1<<1)

class UDPWiimote
{
public:
	UDPWiimote(const char * port);
	virtual ~UDPWiimote();
	void update();
	void getAccel(int &x, int &y, int &z);
	u32 getButtons();
	void getNunchuck(float &x, float &y, u8 &mask);
	void getIR(float &x, float &y);
	int getErrNo() {return err;};
private:
	int readPack(void * data, int *size);
	struct _d; //using pimpl because SOCKET on windows is defined in Winsock2.h, witch doesen't have include guards
	_d *d; 
	double x,y,z;
	double nunX,nunY;
	double pointerX,pointerY;
	u8 nunMask;
	u32 mask;
	int err;
	static int noinst;
};
#endif