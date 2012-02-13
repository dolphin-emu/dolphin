#include "ICMP.h"

enum
{
	ICMP_ECHOREPLY = 0,
	ICMP_ECHOREQ = 8
};

enum
{
	// id/seq/data come from wii
	ICMP_HDR_LEN = 4,
	IP_HDR_LEN = 20
};

#pragma pack(push, 1)
struct icmp_hdr
{ 
	u8 type;
	u8 code;
	u16 checksum;
	u16 id;
	u16 seq;
	char data[1];
};
#pragma pack(pop)

static u8 workspace[56];

/*
* Description:
* Calculate Internet checksum for data buffer and length (one's
* complement sum of 16-bit words). Used in IP, ICMP, UDP, IGMP.
*
* NOTE: to handle odd number of bytes, last (even) byte in
* buffer have a value of 0 (we assume that it does)
*/
u16 cksum(u16 *buffer, int length)
{ 
	u32 sum = 0;

	while (length > 0)
	{
		sum += *(buffer++);
		length -= 2;
	}

	sum = (sum & 0xffff) + (sum >> 16);
	sum += sum >> 16;

	return (u16)~sum;
}

int icmp_echo_req(u32 s, sockaddr_in *addr, u8 *data, u32 data_length)
{
	memset(workspace, 0, sizeof(workspace));
	icmp_hdr *header	= (icmp_hdr *)workspace;
	header->type		= ICMP_ECHOREQ;
	header->code		= 0;
	header->checksum	= 0;
	memcpy(&header->id, data, data_length);

	header->checksum = cksum((u16 *)header, ICMP_HDR_LEN + data_length);

	return sendto((SOCKET)s, (LPSTR)header, ICMP_HDR_LEN + data_length, 0,
		(sockaddr *)addr, sizeof(sockaddr));
}

int icmp_echo_rep(u32 s, sockaddr_in *addr, const u8 *data, u32 data_length)
{
	memset(workspace, 0, sizeof(workspace));
	int addr_length = sizeof(sockaddr_in);
	int ret = recvfrom((SOCKET)s, (LPSTR)workspace,
		IP_HDR_LEN + ICMP_HDR_LEN + data_length,
		0, (sockaddr *)addr, &addr_length);

	// TODO do we need to memcmp the data?
	return ret;
}
