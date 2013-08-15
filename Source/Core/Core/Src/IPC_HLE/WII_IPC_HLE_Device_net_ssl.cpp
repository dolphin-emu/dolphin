// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#ifdef _MSC_VER
#pragma warning(disable: 4748)
#pragma optimize("",off)
#endif

#include "FileUtil.h"
#include "WII_IPC_HLE_Device_net_ssl.h"
#include "../Debugger/Debugger_SymbolMap.h"

CWII_IPC_HLE_Device_net_ssl::CWII_IPC_HLE_Device_net_ssl(u32 _DeviceID, const std::string& _rDeviceName) 
	: IWII_IPC_HLE_Device(_DeviceID, _rDeviceName)
{	
	for(int i = 0; i < NET_SSL_MAXINSTANCES; ++i)
	{
		memset(&_SSL[i], 0, sizeof(struct _SSL));
	}
}

CWII_IPC_HLE_Device_net_ssl::~CWII_IPC_HLE_Device_net_ssl() 
{
	// Cleanup sessions
	for (int i = 0; i < NET_SSL_MAXINSTANCES; i++)
	{
		if(_SSL[i].active)
		{
			ssl_close_notify(&_SSL[i].ctx);
			ssl_session_free(&_SSL[i].session);
			ssl_free(&_SSL[i].ctx);

			x509_free(&_SSL[i].cacert);
			x509_free(&_SSL[i].clicert);

			memset(&_SSL[i].ctx, 0, sizeof(ssl_context));
			memset(&_SSL[i].session, 0, sizeof(ssl_session));
			memset(&_SSL[i].hs, 0, sizeof(havege_state));
			memset(_SSL[i].hostname, 0, MAX_HOSTNAME_LEN);

			_SSL[i].active = false;
		}
	}
}

int CWII_IPC_HLE_Device_net_ssl::getSSLFreeID()
{
	for (int i = 0; i < NET_SSL_MAXINSTANCES; i++)
	{
		if (!_SSL[i].active)
			return i + 1;
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
		Memory::Write_U32(0, _CommandAddress + 4);
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
	
	u32 ReturnValue = ExecuteCommand(Command, BufferIn, BufferInSize, BufferOut, BufferOutSize);	
	Memory::Write_U32(ReturnValue, _CommandAddress + 0x4);
	return true;
}

bool CWII_IPC_HLE_Device_net_ssl::IOCtlV(u32 _CommandAddress) 
{ 
	u32 ReturnValue = 0;
	SIOCtlVBuffer CommandBuffer(_CommandAddress);
	
	ReturnValue = ExecuteCommandV(CommandBuffer.Parameter, CommandBuffer);
		
	Memory::Write_U32(ReturnValue, _CommandAddress+4);
	return true; 
}

static int
_verify_certificate_callback (void *data, x509_cert *crt, int depth, int *flags)
{
	char buf[1024];
    ((void) data);
	std::string verify_info = "Verify requested for (Depth ";
	sprintf(buf, "%d", depth);
	verify_info += buf;
	verify_info += "):\n";

    x509parse_cert_info( buf, sizeof( buf ) - 1, "", crt );
	verify_info += buf;

    if( ( (*flags) & BADCERT_EXPIRED ) != 0 )
        verify_info += "  ! server certificate has expired";

    if( ( (*flags) & BADCERT_REVOKED ) != 0 )
        verify_info += "  ! server certificate has been revoked";

    if( ( (*flags) & BADCERT_CN_MISMATCH ) != 0 )
        verify_info += "  ! CN mismatch\n";

    if( ( (*flags) & BADCERT_NOT_TRUSTED ) != 0 )
        verify_info += "  ! self-signed or not signed by a trusted CA\n";

    if( ( (*flags) & BADCRL_NOT_TRUSTED ) != 0 )
        verify_info += "  ! CRL not trusted\n";

    if( ( (*flags) & BADCRL_EXPIRED ) != 0 )
        verify_info += "  ! CRL expired\n";

    if( ( (*flags) & BADCERT_OTHER ) != 0 )
        verify_info += "  ! other (unknown) flag\n";

    if ( ( *flags ) == 0 )
        verify_info += "  This certificate has no flags\n";
	
    WARN_LOG(WII_IPC_SSL, verify_info.c_str() );

    return( 0 );
}

u32 CWII_IPC_HLE_Device_net_ssl::ExecuteCommandV(u32 _Parameter, SIOCtlVBuffer CommandBuffer) 
{
	s32 returnValue = 0;

	u32 _BufferIn = 0, _BufferIn2 = 0, _BufferIn3 = 0;
	u32 BufferInSize = 0, BufferInSize2 = 0, BufferInSize3 = 0;

	u32 _BufferOut = 0, _BufferOut2 = 0, _BufferOut3 = 0;
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
		_BufferOut = CommandBuffer.PayloadBuffer.at(0).m_Address;
		BufferOutSize = CommandBuffer.PayloadBuffer.at(0).m_Size;
	}
	if (CommandBuffer.PayloadBuffer.size() > 1)
	{
		_BufferOut2 = CommandBuffer.PayloadBuffer.at(1).m_Address;
		BufferOutSize2 = CommandBuffer.PayloadBuffer.at(1).m_Size;
	}
	if (CommandBuffer.PayloadBuffer.size() > 2)
	{
		_BufferOut3 = CommandBuffer.PayloadBuffer.at(2).m_Address;
		BufferOutSize3 = CommandBuffer.PayloadBuffer.at(2).m_Size;
	}
	
	switch (_Parameter)
	{
		case IOCTLV_NET_SSL_NEW:
		{
			int verifyOption = Memory::Read_U32(_BufferOut);
			const char * hostname = (const char*) Memory::GetPointer(_BufferOut2);
			
			int freeSSL = this->getSSLFreeID();
			if (freeSSL)
			{
				int sslID = freeSSL - 1;
				int ret = ssl_init(&_SSL[sslID].ctx);
				if(ret)
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

				ssl_set_verify(&_SSL[sslID].ctx, _verify_certificate_callback, NULL);
				
				ssl_set_endpoint(&_SSL[sslID].ctx, SSL_IS_CLIENT);
				ssl_set_authmode(&_SSL[sslID].ctx, SSL_VERIFY_OPTIONAL);
				ssl_set_renegotiation(&_SSL[sslID].ctx, SSL_RENEGOTIATION_ENABLED);

				memcpy(_SSL[sslID].hostname, hostname, min((int)BufferOutSize2, MAX_HOSTNAME_LEN));
				_SSL[sslID].hostname[MAX_HOSTNAME_LEN-1] = '\0';
				ssl_set_hostname(&_SSL[sslID].ctx, _SSL[sslID].hostname);

				_SSL[sslID].active = true;
				Memory::Write_U32(freeSSL, _BufferIn);
			}
			else
			{
_SSL_NEW_ERROR:
				Memory::Write_U32(SSL_ERR_FAILED, _BufferIn);
			}

			WARN_LOG(WII_IPC_SSL, "IOCTLV_NET_SSL_NEW (%d, %s) "
				"BufferIn: (%08x, %i), BufferIn2: (%08x, %i), "
				"BufferIn3: (%08x, %i), BufferOut: (%08x, %i), "
				"BufferOut2: (%08x, %i), BufferOut3: (%08x, %i)",
				verifyOption, hostname,
				_BufferIn, BufferInSize, _BufferIn2, BufferInSize2,
				_BufferIn3, BufferInSize3, _BufferOut, BufferOutSize,
				_BufferOut2, BufferOutSize2, _BufferOut3, BufferOutSize3);
			break;
		}
		case IOCTLV_NET_SSL_SHUTDOWN:
		{
			int sslID = Memory::Read_U32(_BufferOut) - 1;
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
				memset(_SSL[sslID].hostname, 0, MAX_HOSTNAME_LEN);
				
				_SSL[sslID].active = false;

				Memory::Write_U32(SSL_OK, _BufferIn);
			}
			else
			{
				Memory::Write_U32(SSL_ERR_ID, _BufferIn);
			}
			WARN_LOG(WII_IPC_SSL, "IOCTLV_NET_SSL_SHUTDOWN "
				"BufferIn: (%08x, %i), BufferIn2: (%08x, %i), "
				"BufferIn3: (%08x, %i), BufferOut: (%08x, %i), "
				"BufferOut2: (%08x, %i), BufferOut3: (%08x, %i)",
				_BufferIn, BufferInSize, _BufferIn2, BufferInSize2,
				_BufferIn3, BufferInSize3, _BufferOut, BufferOutSize,
				_BufferOut2, BufferOutSize2, _BufferOut3, BufferOutSize3);
			break;
		}
		case IOCTLV_NET_SSL_SETROOTCA:
		{
			WARN_LOG(WII_IPC_SSL, "IOCTLV_NET_SSL_SETROOTCA "
				"BufferIn: (%08x, %i), BufferIn2: (%08x, %i), "
				"BufferIn3: (%08x, %i), BufferOut: (%08x, %i), "
				"BufferOut2: (%08x, %i), BufferOut3: (%08x, %i)",
				_BufferIn, BufferInSize, _BufferIn2, BufferInSize2,
				_BufferIn3, BufferInSize3, _BufferOut, BufferOutSize,
				_BufferOut2, BufferOutSize2, _BufferOut3, BufferOutSize3);
			

			int sslID = Memory::Read_U32(_BufferOut) - 1;
			if (SSLID_VALID(sslID))
			{				
				int ret = x509parse_crt_der(
					&_SSL[sslID].cacert, 
					Memory::GetPointer(_BufferOut2),
					BufferOutSize2);

				if(ret)
				{
					Memory::Write_U32(SSL_ERR_FAILED, _BufferIn);
				}
				else
				{
					ssl_set_ca_chain(&_SSL[sslID].ctx, &_SSL[sslID].cacert, NULL, _SSL[sslID].hostname);
					Memory::Write_U32(SSL_OK, _BufferIn);
				}

				WARN_LOG(WII_IPC_SSL, "IOCTLV_NET_SSL_SETROOTCA = %d", ret);	
			}
			else
			{
				Memory::Write_U32(SSL_ERR_ID, _BufferIn);
			}
			break;
		}
		case IOCTLV_NET_SSL_SETBUILTINCLIENTCERT:
		{
			WARN_LOG(WII_IPC_SSL, "IOCTLV_NET_SSL_SETBUILTINCLIENTCERT "
				"BufferIn: (%08x, %i), BufferIn2: (%08x, %i), "
				"BufferIn3: (%08x, %i), BufferOut: (%08x, %i), "
				"BufferOut2: (%08x, %i), BufferOut3: (%08x, %i)",
				_BufferIn, BufferInSize, _BufferIn2, BufferInSize2,
				_BufferIn3, BufferInSize3, _BufferOut, BufferOutSize,
				_BufferOut2, BufferOutSize2, _BufferOut3, BufferOutSize3);

			int sslID = Memory::Read_U32(_BufferOut) - 1;
			if (SSLID_VALID(sslID))
			{
				std::string cert_base_path(File::GetUserPath(D_WIIUSER_IDX));
				int ret = x509parse_crtfile(&_SSL[sslID].clicert, (cert_base_path + "clientca.pem").c_str());
				int rsa_ret = x509parse_keyfile(&_SSL[sslID].rsa, (cert_base_path + "clientcakey.pem").c_str(), NULL);
				if(ret || rsa_ret)
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
					
				WARN_LOG(WII_IPC_SSL, "IOCTLV_NET_SSL_SETBUILTINCLIENTCERT = (%d, %d)", ret, rsa_ret);
			}
			else
			{
				Memory::Write_U32(SSL_ERR_ID, _BufferIn);
				WARN_LOG(WII_IPC_SSL, "IOCTLV_NET_SSL_SETBUILTINCLIENTCERT invalid sslID = %d", sslID);
			}
			break;
		}
		case IOCTLV_NET_SSL_REMOVECLIENTCERT:
		{
			WARN_LOG(WII_IPC_SSL, "IOCTLV_NET_SSL_REMOVECLIENTCERT "
				"BufferIn: (%08x, %i), BufferIn2: (%08x, %i), "
				"BufferIn3: (%08x, %i), BufferOut: (%08x, %i), "
				"BufferOut2: (%08x, %i), BufferOut3: (%08x, %i)",
				_BufferIn, BufferInSize, _BufferIn2, BufferInSize2,
				_BufferIn3, BufferInSize3, _BufferOut, BufferOutSize,
				_BufferOut2, BufferOutSize2, _BufferOut3, BufferOutSize3);

			int sslID = Memory::Read_U32(_BufferOut) - 1;
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
				WARN_LOG(WII_IPC_SSL, "IOCTLV_NET_SSL_SETBUILTINCLIENTCERT invalid sslID = %d", sslID);
			}
			break;
		}
		case IOCTLV_NET_SSL_SETBUILTINROOTCA:
		{
			int sslID = Memory::Read_U32(_BufferOut) - 1;
			if (SSLID_VALID(sslID))
			{
				std::string cert_base_path(File::GetUserPath(D_WIIUSER_IDX));
				
				int ret = x509parse_crtfile(&_SSL[sslID].cacert, (cert_base_path + "rootca.pem").c_str());
				if(ret)
				{
					x509_free(&_SSL[sslID].clicert);
					Memory::Write_U32(SSL_ERR_FAILED, _BufferIn);
				}
				else
				{
					ssl_set_ca_chain(&_SSL[sslID].ctx, &_SSL[sslID].cacert, NULL, _SSL[sslID].hostname);
					Memory::Write_U32(SSL_OK, _BufferIn);
				}
				WARN_LOG(WII_IPC_SSL, "IOCTLV_NET_SSL_SETBUILTINROOTCA = %d", ret);
			}
			else
			{
				Memory::Write_U32(SSL_ERR_ID, _BufferIn);
			}
			WARN_LOG(WII_IPC_SSL, "IOCTLV_NET_SSL_SETBUILTINROOTCA "
				"BufferIn: (%08x, %i), BufferIn2: (%08x, %i), "
				"BufferIn3: (%08x, %i), BufferOut: (%08x, %i), "
				"BufferOut2: (%08x, %i), BufferOut3: (%08x, %i)",
				_BufferIn, BufferInSize, _BufferIn2, BufferInSize2,
				_BufferIn3, BufferInSize3, _BufferOut, BufferOutSize,
				_BufferOut2, BufferOutSize2, _BufferOut3, BufferOutSize3);
			break;
		}
		case IOCTLV_NET_SSL_CONNECT:
		{
			int sslID = Memory::Read_U32(_BufferOut) - 1;
			if (SSLID_VALID(sslID))
			{
				_SSL[sslID].sockfd = Memory::Read_U32(_BufferOut2);
				WARN_LOG(WII_IPC_SSL, "IOCTLV_NET_SSL_CONNECT socket = %d", _SSL[sslID].sockfd);
				ssl_set_bio(&_SSL[sslID].ctx, net_recv, &_SSL[sslID].sockfd, net_send, &_SSL[sslID].sockfd);
				Memory::Write_U32(SSL_OK, _BufferIn);
			}
			else
			{
				Memory::Write_U32(SSL_ERR_ID, _BufferIn);
			}
			WARN_LOG(WII_IPC_SSL, "IOCTLV_NET_SSL_CONNECT "
				"BufferIn: (%08x, %i), BufferIn2: (%08x, %i), "
				"BufferIn3: (%08x, %i), BufferOut: (%08x, %i), "
				"BufferOut2: (%08x, %i), BufferOut3: (%08x, %i)",
				_BufferIn, BufferInSize, _BufferIn2, BufferInSize2,
				_BufferIn3, BufferInSize3, _BufferOut, BufferOutSize,
				_BufferOut2, BufferOutSize2, _BufferOut3, BufferOutSize3);
			break;
		}
		case IOCTLV_NET_SSL_DOHANDSHAKE:
		{
			int ret = 0;
			int sslID = Memory::Read_U32(_BufferOut) - 1;
			if (SSLID_VALID(sslID))
			{
				ret = ssl_handshake(&_SSL[sslID].ctx);
				switch (ret)
				{
					case 0:
						Memory::Write_U32(SSL_OK, _BufferIn);
						break;
					case POLARSSL_ERR_NET_WANT_READ:
						Memory::Write_U32(SSL_ERR_RAGAIN, _BufferIn);
						break;
					case POLARSSL_ERR_NET_WANT_WRITE:
						Memory::Write_U32(SSL_ERR_WAGAIN, _BufferIn);
						break;
					default:
						Memory::Write_U32(SSL_ERR_FAILED, _BufferIn);
						break;
				}
			}
			else
			{
				Memory::Write_U32(SSL_ERR_ID, _BufferIn);
			}
			WARN_LOG(WII_IPC_SSL, "IOCTLV_NET_SSL_DOHANDSHAKE = (%d) "
				"BufferIn: (%08x, %i), BufferIn2: (%08x, %i), "
				"BufferIn3: (%08x, %i), BufferOut: (%08x, %i), "
				"BufferOut2: (%08x, %i), BufferOut3: (%08x, %i)",
				ret,
				_BufferIn, BufferInSize, _BufferIn2, BufferInSize2,
				_BufferIn3, BufferInSize3, _BufferOut, BufferOutSize,
				_BufferOut2, BufferOutSize2, _BufferOut3, BufferOutSize3);
			break;
		}
		case IOCTLV_NET_SSL_WRITE:
		{
			int sslID = Memory::Read_U32(_BufferOut) - 1;
			if (SSLID_VALID(sslID))
			{
				int ret = ssl_write( &_SSL[sslID].ctx, Memory::GetPointer(_BufferOut2), BufferOutSize2);
				
				File::IOFile("ssl_write.bin", "ab").WriteBytes(Memory::GetPointer(_BufferOut2), BufferOutSize2);
				if(ret >= 0)
				{
					// Return bytes written or SSL_ERR_ZERO if none
					Memory::Write_U32((ret == 0) ? SSL_ERR_ZERO : ret, _BufferIn);
				}
				else 
				{
					switch (ret)
					{
						case POLARSSL_ERR_NET_WANT_READ:
							Memory::Write_U32(SSL_ERR_RAGAIN, _BufferIn);
							break;
						case POLARSSL_ERR_NET_WANT_WRITE:
							Memory::Write_U32(SSL_ERR_WAGAIN, _BufferIn);
							break;
						default:
							Memory::Write_U32(SSL_ERR_FAILED, _BufferIn);
							break;
					}
				}
			}
			else
			{
				Memory::Write_U32(SSL_ERR_ID, _BufferIn);
			}
			WARN_LOG(WII_IPC_SSL, "IOCTLV_NET_SSL_WRITE "
				"BufferIn: (%08x, %i), BufferIn2: (%08x, %i), "
				"BufferIn3: (%08x, %i), BufferOut: (%08x, %i), "
				"BufferOut2: (%08x, %i), BufferOut3: (%08x, %i)",
				_BufferIn, BufferInSize, _BufferIn2, BufferInSize2,
				_BufferIn3, BufferInSize3, _BufferOut, BufferOutSize,
				_BufferOut2, BufferOutSize2, _BufferOut3, BufferOutSize3);
			WARN_LOG(WII_IPC_SSL, "%s", Memory::GetPointer(_BufferOut2));
			break;
		}
		case IOCTLV_NET_SSL_READ:
		{
			
			int ret = 0;
			int sslID = Memory::Read_U32(_BufferOut) - 1;
			if (SSLID_VALID(sslID))
			{
				ret = ssl_read( &_SSL[sslID].ctx, Memory::GetPointer(_BufferIn2), BufferInSize2);
				if (ret > 0)
				{
					File::IOFile("ssl_read.bin", "ab").WriteBytes(Memory::GetPointer(_BufferIn2), ret);
				}

				if(ret >= 0)
				{
					// Return bytes read or SSL_ERR_ZERO if none
					Memory::Write_U32((ret == 0) ? SSL_ERR_ZERO : ret, _BufferIn);
				}
				else 
				{
					switch (ret)
					{
						case POLARSSL_ERR_NET_WANT_READ:
							Memory::Write_U32(SSL_ERR_RAGAIN, _BufferIn);
							break;
						case POLARSSL_ERR_NET_WANT_WRITE:
							Memory::Write_U32(SSL_ERR_WAGAIN, _BufferIn);
							break;
						default:
							Memory::Write_U32(SSL_ERR_FAILED, _BufferIn);
							break;
					}
				}
			}
			else
			{
				Memory::Write_U32(SSL_ERR_ID, _BufferIn);
			}

			WARN_LOG(WII_IPC_SSL, "IOCTLV_NET_SSL_READ(%d)"
				"BufferIn: (%08x, %i), BufferIn2: (%08x, %i), "
				"BufferIn3: (%08x, %i), BufferOut: (%08x, %i), "
				"BufferOut2: (%08x, %i), BufferOut3: (%08x, %i)",
				ret,
				_BufferIn, BufferInSize, _BufferIn2, BufferInSize2,
				_BufferIn3, BufferInSize3, _BufferOut, BufferOutSize,
				_BufferOut2, BufferOutSize2, _BufferOut3, BufferOutSize3);
			break;
		}
		case IOCTLV_NET_SSL_SETROOTCADEFAULT:
		{
			int sslID = Memory::Read_U32(_BufferOut) - 1;
			if (SSLID_VALID(sslID))
			{
				Memory::Write_U32(SSL_OK, _BufferIn);
			}
			else
			{
				Memory::Write_U32(SSL_ERR_ID, _BufferIn);
			}
			WARN_LOG(WII_IPC_SSL, "IOCTLV_NET_SSL_SETROOTCADEFAULT "
				"BufferIn: (%08x, %i), BufferIn2: (%08x, %i), "
				"BufferIn3: (%08x, %i), BufferOut: (%08x, %i), "
				"BufferOut2: (%08x, %i), BufferOut3: (%08x, %i)",
				_BufferIn, BufferInSize, _BufferIn2, BufferInSize2,
				_BufferIn3, BufferInSize3, _BufferOut, BufferOutSize,
				_BufferOut2, BufferOutSize2, _BufferOut3, BufferOutSize3);
			break;
		}
		case IOCTLV_NET_SSL_SETCLIENTCERTDEFAULT:
		{
			WARN_LOG(WII_IPC_SSL, "IOCTLV_NET_SSL_SETCLIENTCERTDEFAULT "
				"BufferIn: (%08x, %i), BufferIn2: (%08x, %i), "
				"BufferIn3: (%08x, %i), BufferOut: (%08x, %i), "
				"BufferOut2: (%08x, %i), BufferOut3: (%08x, %i)",
				_BufferIn, BufferInSize, _BufferIn2, BufferInSize2,
				_BufferIn3, BufferInSize3, _BufferOut, BufferOutSize,
				_BufferOut2, BufferOutSize2, _BufferOut3, BufferOutSize3);

			int sslID = Memory::Read_U32(_BufferOut) - 1;
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
		{
			ERROR_LOG(WII_IPC_SSL, "%i "
				"BufferIn: (%08x, %i), BufferIn2: (%08x, %i), "
				"BufferIn3: (%08x, %i), BufferOut: (%08x, %i), "
				"BufferOut2: (%08x, %i), BufferOut3: (%08x, %i)",
				_Parameter,
				_BufferIn, BufferInSize, _BufferIn2, BufferInSize2,
				_BufferIn3, BufferInSize3, _BufferOut, BufferOutSize,
				_BufferOut2, BufferOutSize2, _BufferOut3, BufferOutSize3);
			break;
		}
	}
	return returnValue;
}

u32 CWII_IPC_HLE_Device_net_ssl::ExecuteCommand(u32 _Command,
												u32 _BufferIn, u32 BufferInSize,
												u32 _BufferOut, u32 BufferOutSize)
{
	WARN_LOG(WII_IPC_SSL, "%s unknown %i "
		"(BufferIn: (%08x, %i), BufferOut: (%08x, %i)",
		GetDeviceName().c_str(), _Command,
		_BufferIn, BufferInSize, _BufferOut, BufferOutSize);
	return 0;
}

#ifdef _MSC_VER
#pragma optimize("",on)
#endif
