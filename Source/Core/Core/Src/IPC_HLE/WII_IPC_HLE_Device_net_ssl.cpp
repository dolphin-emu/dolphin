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
	gnutls_global_init();
	for(int i = 0; i < NET_SSL_MAXINSTANCES; ++i)
	{
		_SSL[i].session = NULL;
		_SSL[i].xcred = NULL;
		memset(_SSL[i].hostname, 0, MAX_HOSTNAME_LEN);
	}
}

CWII_IPC_HLE_Device_net_ssl::~CWII_IPC_HLE_Device_net_ssl() 
{
	gnutls_global_deinit();
}

int CWII_IPC_HLE_Device_net_ssl::getSSLFreeID()
{
	for (int i = 0; i < NET_SSL_MAXINSTANCES; i++)
	{
		if (_SSL[i].session == NULL)
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
_verify_certificate_callback (gnutls_session_t session)
{
	unsigned int status;
	int ret;
	gnutls_certificate_type_t type;
	const char *hostname;
	gnutls_datum_t out;
	
	/* Read hostname. */
	hostname = (const char *)gnutls_session_get_ptr (session);
	WARN_LOG(WII_IPC_SSL, "_verify_certificate_callback: Verifying certificate for %s\n", hostname);
	
	/* This verification function uses the trusted CAs in the credentials
	 * structure.
	 */
	ret = gnutls_certificate_verify_peers3 (session, hostname, &status);
	if (ret < 0)
	{
		WARN_LOG(WII_IPC_SSL, "gnutls_certificate_verify_peers3 error %d", ret);
		return GNUTLS_E_CERTIFICATE_ERROR;
	}
	
	type = gnutls_certificate_type_get (session);
	
	ret = gnutls_certificate_verification_status_print( status, type, &out, 0);
	if (ret < 0)
	{
		WARN_LOG(WII_IPC_SSL, "gnutls_certificate_verification_status_print error %d", ret);
		return GNUTLS_E_CERTIFICATE_ERROR;
	}
	
	WARN_LOG(WII_IPC_SSL, "_verify_certificate_callback: %s", out.data);
	
	gnutls_free(out.data);
	
	if (status != 0)
	{
		/* Certificate is not trusted */
		WARN_LOG(WII_IPC_SSL, "_verify_certificate_callback: status = %d", status);
		return GNUTLS_E_CERTIFICATE_ERROR;
	}
	/* Certificate verified successfully. */
	return 0;
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
				
				int ret = gnutls_init (&_SSL[sslID].session, GNUTLS_CLIENT);
				if(ret)
				{
					_SSL[sslID].session = NULL;
					goto _SSL_NEW_ERROR;
				}
				
				gnutls_session_t session = _SSL[sslID].session;
				
				memcpy(_SSL[sslID].hostname, hostname, min((int)BufferOutSize2, MAX_HOSTNAME_LEN));
				_SSL[sslID].hostname[MAX_HOSTNAME_LEN-1] = '\0';
				
				gnutls_session_set_ptr (session, (void *) _SSL[sslID].hostname);
				gnutls_server_name_set (session, GNUTLS_NAME_DNS, _SSL[sslID].hostname, 
										strnlen(_SSL[sslID].hostname, MAX_HOSTNAME_LEN));
				
				const char *err = NULL;
				ret = gnutls_priority_set_direct (session, "NORMAL", &err);
				if(ret)
				{
					_SSL[sslID].session = NULL;
					goto _SSL_NEW_ERROR;
				}
				
				/* X509 stuff */
				ret = gnutls_certificate_allocate_credentials (&_SSL[sslID].xcred);
				if(ret)
				{
					_SSL[sslID].session = NULL;
					_SSL[sslID].xcred = NULL;
					goto _SSL_NEW_ERROR;
				}
				
				gnutls_certificate_set_verify_function (_SSL[sslID].xcred, _verify_certificate_callback);
				
				/* put the x509 credentials to the current session
				 */
				ret = gnutls_credentials_set(session, GNUTLS_CRD_CERTIFICATE, _SSL[sslID].xcred);
				if(ret)
				{
					_SSL[sslID].session = NULL;
					_SSL[sslID].xcred = NULL;
					goto _SSL_NEW_ERROR;
				}
				
				Memory::Write_U32(freeSSL, _BufferIn);
			}
			else
			{
_SSL_NEW_ERROR:
				Memory::Write_U32(-1, _BufferIn);
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
				gnutls_session_t session = _SSL[sslID].session;
				gnutls_bye (session, GNUTLS_SHUT_RDWR);
				gnutls_deinit(session);
				gnutls_certificate_free_credentials (_SSL[sslID].xcred);
				
				_SSL[sslID].session = NULL;
				_SSL[sslID].xcred = NULL;
				memset(_SSL[sslID].hostname, 0, MAX_HOSTNAME_LEN);
				
				Memory::Write_U32(0, _BufferIn);
			}
			else
			{
				Memory::Write_U32(-8, _BufferIn);
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
				std::string cert_base_path(File::GetUserPath(D_WIIUSER_IDX));
				int ret = gnutls_certificate_set_x509_trust_file (_SSL[sslID].xcred, 
														(cert_base_path + "rootca.pem").c_str(), 
														GNUTLS_X509_FMT_PEM);
				if(ret < 1)
					Memory::Write_U32(-1, _BufferIn);
				else
					Memory::Write_U32(0, _BufferIn);	
				WARN_LOG(WII_IPC_SSL, "IOCTLV_NET_SSL_SETROOTCA = %d", ret);	
			}
			else
			{
				Memory::Write_U32(-8, _BufferIn);
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
				
				int ret = gnutls_certificate_set_x509_key_file (_SSL[sslID].xcred, 
													  (cert_base_path + "clientca.pem").c_str(), 
													  (cert_base_path + "clientcakey.pem").c_str(), 
													  GNUTLS_X509_FMT_PEM);
				if(ret)
					Memory::Write_U32(-1, _BufferIn);
				else
					Memory::Write_U32(0, _BufferIn);	
					
				WARN_LOG(WII_IPC_SSL, "IOCTLV_NET_SSL_SETBUILTINCLIENTCERT = %d", ret);
			}
			else
			{
				Memory::Write_U32(-8, _BufferIn);
			}
			break;
		}

		case IOCTLV_NET_SSL_SETBUILTINROOTCA:
		{
			int sslID = Memory::Read_U32(_BufferOut) - 1;
			if (SSLID_VALID(sslID))
			{
				std::string cert_base_path(File::GetUserPath(D_WIIUSER_IDX));
				int ret = gnutls_certificate_set_x509_trust_file (_SSL[sslID].xcred, 
																  (cert_base_path + "rootca.pem").c_str(), 
																  GNUTLS_X509_FMT_PEM);
				if(ret < 1)
					Memory::Write_U32(-1, _BufferIn);
				else
					Memory::Write_U32(0, _BufferIn);	
				WARN_LOG(WII_IPC_SSL, "IOCTLV_NET_SSL_SETBUILTINROOTCA = %d", ret);
			}
			else
			{
				Memory::Write_U32(-8, _BufferIn);
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
				int sock = Memory::Read_U32(_BufferOut2);
				gnutls_session_t session = _SSL[sslID].session;
				
				gnutls_transport_set_int (session, sock);
				gnutls_handshake_set_timeout (session, GNUTLS_DEFAULT_HANDSHAKE_TIMEOUT);
				
				returnValue = 1;
				Memory::Write_U32(0, _BufferIn);
			}
			else
			{
				Memory::Write_U32(-8, _BufferIn);
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
			int sslID = Memory::Read_U32(_BufferOut) - 1;
			if (SSLID_VALID(sslID))
			{
				gnutls_session_t session = _SSL[sslID].session;
				do
				{
					returnValue = gnutls_handshake (session);
				}
				while (returnValue < 0 && gnutls_error_is_fatal (returnValue) == 0);
				
				gnutls_alert_description_t alert = gnutls_alert_get (session);
				
				WARN_LOG(WII_IPC_SSL, "IOCTLV_NET_SSL_DOHANDSHAKE "
				"%d %d", returnValue, alert);
				returnValue = returnValue == GNUTLS_E_SUCCESS;
				if (returnValue)
					Memory::Write_U32(0, _BufferIn);
				else
					Memory::Write_U32(-1, _BufferIn);
			}
			else
			{
				Memory::Write_U32(-8, _BufferIn);
			}
			WARN_LOG(WII_IPC_SSL, "IOCTLV_NET_SSL_DOHANDSHAKE "
				"BufferIn: (%08x, %i), BufferIn2: (%08x, %i), "
				"BufferIn3: (%08x, %i), BufferOut: (%08x, %i), "
				"BufferOut2: (%08x, %i), BufferOut3: (%08x, %i)",
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
				gnutls_session_t session = _SSL[sslID].session;
								
				returnValue = gnutls_record_send(session, Memory::GetPointer(_BufferOut2), BufferOutSize2);
				
				File::IOFile("ssl_write.bin", "ab").WriteBytes(Memory::GetPointer(_BufferOut2), BufferOutSize2);
				
				Memory::Write_U32(returnValue, _BufferIn);
			}
			else
			{
				Memory::Write_U32(-8, _BufferIn);
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
			int sslID = Memory::Read_U32(_BufferOut) - 1;
			if (SSLID_VALID(sslID))
			{
				gnutls_session_t session = _SSL[sslID].session;
				returnValue = gnutls_record_recv(session, Memory::GetPointer(_BufferIn2), BufferInSize2);
				if (returnValue > 0)
				{
					File::IOFile("ssl_read.bin", "ab").WriteBytes(Memory::GetPointer(_BufferIn2), returnValue);
				}
				Memory::Write_U32(returnValue, _BufferIn);
			}
			else
			{
				Memory::Write_U32(-8, _BufferIn);
			}
			
			WARN_LOG(WII_IPC_SSL, "IOCTLV_NET_SSL_READ(%d)"
				"BufferIn: (%08x, %i), BufferIn2: (%08x, %i), "
				"BufferIn3: (%08x, %i), BufferOut: (%08x, %i), "
				"BufferOut2: (%08x, %i), BufferOut3: (%08x, %i)",
				returnValue,
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
				
				Memory::Write_U32(0, _BufferIn);
			}
			else
			{
				Memory::Write_U32(-8, _BufferIn);
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
				//gnutls_session_t session = _SSL[sslID].session;
				Memory::Write_U32(0, _BufferIn);				
			}
			else
			{
				Memory::Write_U32(-8, _BufferIn);
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
	switch (_Command)
	{
		default:
		{
			WARN_LOG(WII_IPC_SSL, "%s unknown %i "
				"(BufferIn: (%08x, %i), BufferOut: (%08x, %i)",
				GetDeviceName().c_str(), _Command,
				_BufferIn, BufferInSize, _BufferOut, BufferOutSize);
			break;
		}
	}
	return 0;
}

#ifdef _MSC_VER
#pragma optimize("",on)
#endif
