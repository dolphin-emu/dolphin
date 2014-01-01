// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "FileUtil.h"
#include "WII_IPC_HLE_Device_net_ssl.h"
#include "WII_Socket.h"

WII_SSL CWII_IPC_HLE_Device_net_ssl::_SSL[NET_SSL_MAXINSTANCES];

CWII_IPC_HLE_Device_net_ssl::CWII_IPC_HLE_Device_net_ssl(u32 _DeviceID, const std::string& _rDeviceName)
	: IWII_IPC_HLE_Device(_DeviceID, _rDeviceName)
{
	for (int i = 0; i < NET_SSL_MAXINSTANCES; ++i)
	{
		memset(&_SSL[i], 0, sizeof(WII_SSL));
	}
}

CWII_IPC_HLE_Device_net_ssl::~CWII_IPC_HLE_Device_net_ssl()
{
	// Cleanup sessions
	for (int i = 0; i < NET_SSL_MAXINSTANCES; i++)
	{
		if (_SSL[i].active)
		{
			ssl_close_notify(&_SSL[i].ctx);
			ssl_session_free(&_SSL[i].session);
			ssl_free(&_SSL[i].ctx);

			x509_free(&_SSL[i].cacert);
			x509_free(&_SSL[i].clicert);

			memset(&_SSL[i].ctx, 0, sizeof(ssl_context));
			memset(&_SSL[i].session, 0, sizeof(ssl_session));
			memset(&_SSL[i].hs, 0, sizeof(havege_state));
			memset(_SSL[i].hostname, 0, NET_SSL_MAX_HOSTNAME_LEN);

			_SSL[i].active = false;
		}
	}
}

int CWII_IPC_HLE_Device_net_ssl::getSSLFreeID()
{
	for (int i = 0; i < NET_SSL_MAXINSTANCES; i++)
	{
		if (!_SSL[i].active)
		{
			return i + 1;
		}
	}
	return 0;
}

bool CWII_IPC_HLE_Device_net_ssl::Open(u32 _CommandAddress, u32 _Mode)
{
	Memory::Write_U32(GetDeviceID(), _CommandAddress+4);
	m_Active = true;
	return true;
}

bool CWII_IPC_HLE_Device_net_ssl::Close(u32 _CommandAddress, bool _bForce)
{
	if (!_bForce)
	{
		Memory::Write_U32(0, _CommandAddress + 4);
	}
	m_Active = false;
	return true;
}

bool CWII_IPC_HLE_Device_net_ssl::IOCtl(u32 _CommandAddress)
{
	u32 BufferIn		= Memory::Read_U32(_CommandAddress + 0x10);
	u32 BufferInSize	= Memory::Read_U32(_CommandAddress + 0x14);
	u32 BufferOut		= Memory::Read_U32(_CommandAddress + 0x18);
	u32 BufferOutSize	= Memory::Read_U32(_CommandAddress + 0x1C);
	u32 Command			= Memory::Read_U32(_CommandAddress + 0x0C);

	INFO_LOG(WII_IPC_SSL, "%s unknown %i "
	"(BufferIn: (%08x, %i), BufferOut: (%08x, %i)",
			 GetDeviceName().c_str(), Command,
			 BufferIn, BufferInSize, BufferOut, BufferOutSize);
	Memory::Write_U32(0, _CommandAddress + 0x4);
	return true;
}

bool CWII_IPC_HLE_Device_net_ssl::IOCtlV(u32 _CommandAddress)
{
	SIOCtlVBuffer CommandBuffer(_CommandAddress);

	u32 _BufferIn = 0, _BufferIn2 = 0, _BufferIn3 = 0;
	u32 BufferInSize = 0, BufferInSize2 = 0, BufferInSize3 = 0;

	u32 BufferOut = 0, BufferOut2 = 0, BufferOut3 = 0;
	u32 BufferOutSize = 0, BufferOutSize2 = 0, BufferOutSize3 = 0;

	if (CommandBuffer.InBuffer.size() > 0)
	{
		_BufferIn = CommandBuffer.InBuffer.at(0).m_Address;
		BufferInSize = CommandBuffer.InBuffer.at(0).m_Size;
	}
	if (CommandBuffer.InBuffer.size() > 1)
	{
		_BufferIn2 = CommandBuffer.InBuffer.at(1).m_Address;
		BufferInSize2 = CommandBuffer.InBuffer.at(1).m_Size;
	}
	if (CommandBuffer.InBuffer.size() > 2)
	{
		_BufferIn3 = CommandBuffer.InBuffer.at(2).m_Address;
		BufferInSize3 = CommandBuffer.InBuffer.at(2).m_Size;
	}

	if (CommandBuffer.PayloadBuffer.size() > 0)
	{
		BufferOut = CommandBuffer.PayloadBuffer.at(0).m_Address;
		BufferOutSize = CommandBuffer.PayloadBuffer.at(0).m_Size;
	}
	if (CommandBuffer.PayloadBuffer.size() > 1)
	{
		BufferOut2 = CommandBuffer.PayloadBuffer.at(1).m_Address;
		BufferOutSize2 = CommandBuffer.PayloadBuffer.at(1).m_Size;
	}
	if (CommandBuffer.PayloadBuffer.size() > 2)
	{
		BufferOut3 = CommandBuffer.PayloadBuffer.at(2).m_Address;
		BufferOutSize3 = CommandBuffer.PayloadBuffer.at(2).m_Size;
	}

	switch (CommandBuffer.Parameter)
	{
	case IOCTLV_NET_SSL_NEW:
	{
		int verifyOption = Memory::Read_U32(BufferOut);
		const char * hostname = (const char*) Memory::GetPointer(BufferOut2);

		int freeSSL = this->getSSLFreeID();
		if (freeSSL)
		{
			int sslID = freeSSL - 1;
			int ret = ssl_init(&_SSL[sslID].ctx);
			if (ret)
			{
				// Cleanup possibly dirty ctx
				memset(&_SSL[sslID].ctx, 0, sizeof(ssl_context));
				goto _SSL_NEW_ERROR;
			}

			havege_init(&_SSL[sslID].hs);
			ssl_set_rng(&_SSL[sslID].ctx, havege_random, &_SSL[sslID].hs);

			// For some reason we can't use TLSv1.2, v1.1 and below are fine!
			ssl_set_max_version(&_SSL[sslID].ctx, SSL_MAJOR_VERSION_3, SSL_MINOR_VERSION_2);

			ssl_set_ciphersuites(&_SSL[sslID].ctx, ssl_default_ciphersuites);
			ssl_set_session(&_SSL[sslID].ctx, &_SSL[sslID].session);

			ssl_set_endpoint(&_SSL[sslID].ctx, SSL_IS_CLIENT);
			ssl_set_authmode(&_SSL[sslID].ctx, SSL_VERIFY_NONE);
			ssl_set_renegotiation(&_SSL[sslID].ctx, SSL_RENEGOTIATION_ENABLED);

			memcpy(_SSL[sslID].hostname, hostname, min((int)BufferOutSize2, NET_SSL_MAX_HOSTNAME_LEN));
			_SSL[sslID].hostname[NET_SSL_MAX_HOSTNAME_LEN-1] = '\0';
			ssl_set_hostname(&_SSL[sslID].ctx, _SSL[sslID].hostname);

			_SSL[sslID].active = true;
			Memory::Write_U32(freeSSL, _BufferIn);
		}
		else
		{
_SSL_NEW_ERROR:
			Memory::Write_U32(SSL_ERR_FAILED, _BufferIn);
		}

		INFO_LOG(WII_IPC_SSL, "IOCTLV_NET_SSL_NEW (%d, %s) "
			"BufferIn: (%08x, %i), BufferIn2: (%08x, %i), "
			"BufferIn3: (%08x, %i), BufferOut: (%08x, %i), "
			"BufferOut2: (%08x, %i), BufferOut3: (%08x, %i)",
			verifyOption, hostname,
			_BufferIn, BufferInSize, _BufferIn2, BufferInSize2,
			_BufferIn3, BufferInSize3, BufferOut, BufferOutSize,
			BufferOut2, BufferOutSize2, BufferOut3, BufferOutSize3);
		break;
	}
	case IOCTLV_NET_SSL_SHUTDOWN:
	{
		int sslID = Memory::Read_U32(BufferOut) - 1;
		if (SSLID_VALID(sslID))
		{
			ssl_close_notify(&_SSL[sslID].ctx);
			ssl_session_free(&_SSL[sslID].session);
			ssl_free(&_SSL[sslID].ctx);

			x509_free(&_SSL[sslID].cacert);
			x509_free(&_SSL[sslID].clicert);

			memset(&_SSL[sslID].ctx, 0, sizeof(ssl_context));
			memset(&_SSL[sslID].session, 0, sizeof(ssl_session));
			memset(&_SSL[sslID].hs, 0, sizeof(havege_state));
			memset(_SSL[sslID].hostname, 0, NET_SSL_MAX_HOSTNAME_LEN);

			_SSL[sslID].active = false;

			Memory::Write_U32(SSL_OK, _BufferIn);
		}
		else
		{
			Memory::Write_U32(SSL_ERR_ID, _BufferIn);
		}
		INFO_LOG(WII_IPC_SSL, "IOCTLV_NET_SSL_SHUTDOWN "
			"BufferIn: (%08x, %i), BufferIn2: (%08x, %i), "
			"BufferIn3: (%08x, %i), BufferOut: (%08x, %i), "
			"BufferOut2: (%08x, %i), BufferOut3: (%08x, %i)",
			_BufferIn, BufferInSize, _BufferIn2, BufferInSize2,
			_BufferIn3, BufferInSize3, BufferOut, BufferOutSize,
			BufferOut2, BufferOutSize2, BufferOut3, BufferOutSize3);
		break;
	}
	case IOCTLV_NET_SSL_SETROOTCA:
	{
		INFO_LOG(WII_IPC_SSL, "IOCTLV_NET_SSL_SETROOTCA "
			"BufferIn: (%08x, %i), BufferIn2: (%08x, %i), "
			"BufferIn3: (%08x, %i), BufferOut: (%08x, %i), "
			"BufferOut2: (%08x, %i), BufferOut3: (%08x, %i)",
			_BufferIn, BufferInSize, _BufferIn2, BufferInSize2,
			_BufferIn3, BufferInSize3, BufferOut, BufferOutSize,
			BufferOut2, BufferOutSize2, BufferOut3, BufferOutSize3);


		int sslID = Memory::Read_U32(BufferOut) - 1;
		if (SSLID_VALID(sslID))
		{
			int ret = x509parse_crt_der(
				&_SSL[sslID].cacert,
				Memory::GetPointer(BufferOut2),
				BufferOutSize2);

			if (ret)
			{
				Memory::Write_U32(SSL_ERR_FAILED, _BufferIn);
			}
			else
			{
				ssl_set_ca_chain(&_SSL[sslID].ctx, &_SSL[sslID].cacert, NULL, _SSL[sslID].hostname);
				Memory::Write_U32(SSL_OK, _BufferIn);
			}

			INFO_LOG(WII_IPC_SSL, "IOCTLV_NET_SSL_SETROOTCA = %d", ret);
		}
		else
		{
			Memory::Write_U32(SSL_ERR_ID, _BufferIn);
		}
		break;
	}
	case IOCTLV_NET_SSL_SETBUILTINCLIENTCERT:
	{
		INFO_LOG(WII_IPC_SSL, "IOCTLV_NET_SSL_SETBUILTINCLIENTCERT "
			"BufferIn: (%08x, %i), BufferIn2: (%08x, %i), "
			"BufferIn3: (%08x, %i), BufferOut: (%08x, %i), "
			"BufferOut2: (%08x, %i), BufferOut3: (%08x, %i)",
			_BufferIn, BufferInSize, _BufferIn2, BufferInSize2,
			_BufferIn3, BufferInSize3, BufferOut, BufferOutSize,
			BufferOut2, BufferOutSize2, BufferOut3, BufferOutSize3);

		int sslID = Memory::Read_U32(BufferOut) - 1;
		if (SSLID_VALID(sslID))
		{
			std::string cert_base_path(File::GetUserPath(D_WIIUSER_IDX));
			int ret = x509parse_crtfile(&_SSL[sslID].clicert, (cert_base_path + "clientca.pem").c_str());
			int rsa_ret = x509parse_keyfile(&_SSL[sslID].rsa, (cert_base_path + "clientcakey.pem").c_str(), NULL);
			if (ret || rsa_ret)
			{
				x509_free(&_SSL[sslID].clicert);
				rsa_free(&_SSL[sslID].rsa);
				memset(&_SSL[sslID].clicert, 0, sizeof(x509_cert));
				memset(&_SSL[sslID].rsa, 0, sizeof(rsa_context));
				Memory::Write_U32(SSL_ERR_FAILED, _BufferIn);
			}
			else
			{
				ssl_set_own_cert(&_SSL[sslID].ctx, &_SSL[sslID].clicert, &_SSL[sslID].rsa);
				Memory::Write_U32(SSL_OK, _BufferIn);
			}

			INFO_LOG(WII_IPC_SSL, "IOCTLV_NET_SSL_SETBUILTINCLIENTCERT = (%d, %d)", ret, rsa_ret);
		}
		else
		{
			Memory::Write_U32(SSL_ERR_ID, _BufferIn);
			INFO_LOG(WII_IPC_SSL, "IOCTLV_NET_SSL_SETBUILTINCLIENTCERT invalid sslID = %d", sslID);
		}
		break;
	}
	case IOCTLV_NET_SSL_REMOVECLIENTCERT:
	{
		INFO_LOG(WII_IPC_SSL, "IOCTLV_NET_SSL_REMOVECLIENTCERT "
			"BufferIn: (%08x, %i), BufferIn2: (%08x, %i), "
			"BufferIn3: (%08x, %i), BufferOut: (%08x, %i), "
			"BufferOut2: (%08x, %i), BufferOut3: (%08x, %i)",
			_BufferIn, BufferInSize, _BufferIn2, BufferInSize2,
			_BufferIn3, BufferInSize3, BufferOut, BufferOutSize,
			BufferOut2, BufferOutSize2, BufferOut3, BufferOutSize3);

		int sslID = Memory::Read_U32(BufferOut) - 1;
		if (SSLID_VALID(sslID))
		{
			x509_free(&_SSL[sslID].clicert);
			rsa_free(&_SSL[sslID].rsa);
			memset(&_SSL[sslID].clicert, 0, sizeof(x509_cert));
			memset(&_SSL[sslID].rsa, 0, sizeof(rsa_context));

			ssl_set_own_cert(&_SSL[sslID].ctx, NULL, NULL);
			Memory::Write_U32(SSL_OK, _BufferIn);
		}
		else
		{
			Memory::Write_U32(SSL_ERR_ID, _BufferIn);
			INFO_LOG(WII_IPC_SSL, "IOCTLV_NET_SSL_SETBUILTINCLIENTCERT invalid sslID = %d", sslID);
		}
		break;
	}
	case IOCTLV_NET_SSL_SETBUILTINROOTCA:
	{
		int sslID = Memory::Read_U32(BufferOut) - 1;
		if (SSLID_VALID(sslID))
		{
			std::string cert_base_path(File::GetUserPath(D_WIIUSER_IDX));

			int ret = x509parse_crtfile(&_SSL[sslID].cacert, (cert_base_path + "rootca.pem").c_str());
			if (ret)
			{
				x509_free(&_SSL[sslID].clicert);
				Memory::Write_U32(SSL_ERR_FAILED, _BufferIn);
			}
			else
			{
				ssl_set_ca_chain(&_SSL[sslID].ctx, &_SSL[sslID].cacert, NULL, _SSL[sslID].hostname);
				Memory::Write_U32(SSL_OK, _BufferIn);
			}
			INFO_LOG(WII_IPC_SSL, "IOCTLV_NET_SSL_SETBUILTINROOTCA = %d", ret);
		}
		else
		{
			Memory::Write_U32(SSL_ERR_ID, _BufferIn);
		}
		INFO_LOG(WII_IPC_SSL, "IOCTLV_NET_SSL_SETBUILTINROOTCA "
			"BufferIn: (%08x, %i), BufferIn2: (%08x, %i), "
			"BufferIn3: (%08x, %i), BufferOut: (%08x, %i), "
			"BufferOut2: (%08x, %i), BufferOut3: (%08x, %i)",
			_BufferIn, BufferInSize, _BufferIn2, BufferInSize2,
			_BufferIn3, BufferInSize3, BufferOut, BufferOutSize,
			BufferOut2, BufferOutSize2, BufferOut3, BufferOutSize3);
		break;
	}
	case IOCTLV_NET_SSL_CONNECT:
	{
		int sslID = Memory::Read_U32(BufferOut) - 1;
		if (SSLID_VALID(sslID))
		{
			_SSL[sslID].sockfd = Memory::Read_U32(BufferOut2);
			INFO_LOG(WII_IPC_SSL, "IOCTLV_NET_SSL_CONNECT socket = %d", _SSL[sslID].sockfd);
			ssl_set_bio(&_SSL[sslID].ctx, net_recv, &_SSL[sslID].sockfd, net_send, &_SSL[sslID].sockfd);
			Memory::Write_U32(SSL_OK, _BufferIn);
		}
		else
		{
			Memory::Write_U32(SSL_ERR_ID, _BufferIn);
		}
		INFO_LOG(WII_IPC_SSL, "IOCTLV_NET_SSL_CONNECT "
			"BufferIn: (%08x, %i), BufferIn2: (%08x, %i), "
			"BufferIn3: (%08x, %i), BufferOut: (%08x, %i), "
			"BufferOut2: (%08x, %i), BufferOut3: (%08x, %i)",
			_BufferIn, BufferInSize, _BufferIn2, BufferInSize2,
			_BufferIn3, BufferInSize3, BufferOut, BufferOutSize,
			BufferOut2, BufferOutSize2, BufferOut3, BufferOutSize3);
		break;
	}
	case IOCTLV_NET_SSL_DOHANDSHAKE:
	{
		int sslID = Memory::Read_U32(BufferOut) - 1;
		if (SSLID_VALID(sslID))
		{
			WiiSockMan &sm = WiiSockMan::getInstance();
			sm.doSock(_SSL[sslID].sockfd, _CommandAddress, IOCTLV_NET_SSL_DOHANDSHAKE);
			return false;
		}
		else
		{
			Memory::Write_U32(SSL_ERR_ID, _BufferIn);
		}
		break;
	}
	case IOCTLV_NET_SSL_WRITE:
	{
		int sslID = Memory::Read_U32(BufferOut) - 1;
		if (SSLID_VALID(sslID))
		{
			WiiSockMan &sm = WiiSockMan::getInstance();
			sm.doSock(_SSL[sslID].sockfd, _CommandAddress, IOCTLV_NET_SSL_WRITE);
			return false;
		}
		else
		{
			Memory::Write_U32(SSL_ERR_ID, _BufferIn);
		}
		INFO_LOG(WII_IPC_SSL, "IOCTLV_NET_SSL_WRITE "
			"BufferIn: (%08x, %i), BufferIn2: (%08x, %i), "
			"BufferIn3: (%08x, %i), BufferOut: (%08x, %i), "
			"BufferOut2: (%08x, %i), BufferOut3: (%08x, %i)",
			_BufferIn, BufferInSize, _BufferIn2, BufferInSize2,
			_BufferIn3, BufferInSize3, BufferOut, BufferOutSize,
			BufferOut2, BufferOutSize2, BufferOut3, BufferOutSize3);
		INFO_LOG(WII_IPC_SSL, "%s", Memory::GetPointer(BufferOut2));
		break;
	}
	case IOCTLV_NET_SSL_READ:
	{

		int ret = 0;
		int sslID = Memory::Read_U32(BufferOut) - 1;
		if (SSLID_VALID(sslID))
		{
			WiiSockMan &sm = WiiSockMan::getInstance();
			sm.doSock(_SSL[sslID].sockfd, _CommandAddress, IOCTLV_NET_SSL_READ);
			return false;
		}
		else
		{
			Memory::Write_U32(SSL_ERR_ID, _BufferIn);
		}

		INFO_LOG(WII_IPC_SSL, "IOCTLV_NET_SSL_READ(%d)"
			"BufferIn: (%08x, %i), BufferIn2: (%08x, %i), "
			"BufferIn3: (%08x, %i), BufferOut: (%08x, %i), "
			"BufferOut2: (%08x, %i), BufferOut3: (%08x, %i)",
			ret,
			_BufferIn, BufferInSize, _BufferIn2, BufferInSize2,
			_BufferIn3, BufferInSize3, BufferOut, BufferOutSize,
			BufferOut2, BufferOutSize2, BufferOut3, BufferOutSize3);
		break;
	}
	case IOCTLV_NET_SSL_SETROOTCADEFAULT:
	{
		int sslID = Memory::Read_U32(BufferOut) - 1;
		if (SSLID_VALID(sslID))
		{
			Memory::Write_U32(SSL_OK, _BufferIn);
		}
		else
		{
			Memory::Write_U32(SSL_ERR_ID, _BufferIn);
		}
		INFO_LOG(WII_IPC_SSL, "IOCTLV_NET_SSL_SETROOTCADEFAULT "
			"BufferIn: (%08x, %i), BufferIn2: (%08x, %i), "
			"BufferIn3: (%08x, %i), BufferOut: (%08x, %i), "
			"BufferOut2: (%08x, %i), BufferOut3: (%08x, %i)",
			_BufferIn, BufferInSize, _BufferIn2, BufferInSize2,
			_BufferIn3, BufferInSize3, BufferOut, BufferOutSize,
			BufferOut2, BufferOutSize2, BufferOut3, BufferOutSize3);
		break;
	}
	case IOCTLV_NET_SSL_SETCLIENTCERTDEFAULT:
	{
		INFO_LOG(WII_IPC_SSL, "IOCTLV_NET_SSL_SETCLIENTCERTDEFAULT "
			"BufferIn: (%08x, %i), BufferIn2: (%08x, %i), "
			"BufferIn3: (%08x, %i), BufferOut: (%08x, %i), "
			"BufferOut2: (%08x, %i), BufferOut3: (%08x, %i)",
			_BufferIn, BufferInSize, _BufferIn2, BufferInSize2,
			_BufferIn3, BufferInSize3, BufferOut, BufferOutSize,
			BufferOut2, BufferOutSize2, BufferOut3, BufferOutSize3);

		int sslID = Memory::Read_U32(BufferOut) - 1;
		if (SSLID_VALID(sslID))
		{
			Memory::Write_U32(SSL_OK, _BufferIn);
		}
		else
		{
			Memory::Write_U32(SSL_ERR_ID, _BufferIn);
		}
		break;
	}
	default:
		ERROR_LOG(WII_IPC_SSL, "%i "
			"BufferIn: (%08x, %i), BufferIn2: (%08x, %i), "
			"BufferIn3: (%08x, %i), BufferOut: (%08x, %i), "
			"BufferOut2: (%08x, %i), BufferOut3: (%08x, %i)",
			CommandBuffer.Parameter,
			_BufferIn, BufferInSize, _BufferIn2, BufferInSize2,
			_BufferIn3, BufferInSize3, BufferOut, BufferOutSize,
			BufferOut2, BufferOutSize2, BufferOut3, BufferOutSize3);
		break;
	}

	// SSL return codes are written to BufferIn
	Memory::Write_U32(0, _CommandAddress+4);

	return true;
}

