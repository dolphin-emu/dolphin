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

#include <openssl/err.h>
#include "FileUtil.h"
#include "WII_IPC_HLE_Device_net_ssl.h"
#include "../Debugger/Debugger_SymbolMap.h"

CWII_IPC_HLE_Device_net_ssl::CWII_IPC_HLE_Device_net_ssl(u32 _DeviceID, const std::string& _rDeviceName) 
	: IWII_IPC_HLE_Device(_DeviceID, _rDeviceName)
{
	SSL_library_init();
	sslfds[0] = NULL;
	sslfds[1] = NULL;
	sslfds[2] = NULL;
	sslfds[3] = NULL;
}

CWII_IPC_HLE_Device_net_ssl::~CWII_IPC_HLE_Device_net_ssl() 
{
}

int CWII_IPC_HLE_Device_net_ssl::getSSLFreeID()
{
	for (int i = 0; i < NET_SSL_MAXINSTANCES; i++)
	{
		if (sslfds[i] == NULL)
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
			int freeSSL = this->getSSLFreeID();
			if (freeSSL)
			{
				Memory::Write_U32(freeSSL, _BufferIn);
				
				SSL_CTX* ctx = SSL_CTX_new(SSLv23_method());

				//SSL_CTX_set_options(ctx,0);

				SSL* ssl = SSL_new(ctx);
				sslfds[freeSSL-1] = ssl;

			}

			INFO_LOG(WII_IPC_SSL, "IOCTLV_NET_SSL_NEW "
				"BufferIn: (%08x, %i), BufferIn2: (%08x, %i), "
				"BufferIn3: (%08x, %i), BufferOut: (%08x, %i), "
				"BufferOut2: (%08x, %i), BufferOut3: (%08x, %i)",
				_BufferIn, BufferInSize, _BufferIn2, BufferInSize2,
				_BufferIn3, BufferInSize3, _BufferOut, BufferOutSize,
				_BufferOut2, BufferOutSize2, _BufferOut3, BufferOutSize3);
			break;
		}

		case IOCTLV_NET_SSL_SHUTDOWN:
		{
			int sslID = Memory::Read_U32(_BufferOut) - 1;
			if (sslID >= 0 && sslID < NET_SSL_MAXINSTANCES && sslfds[sslID] != NULL)
			{
				SSL_CTX* ctx = sslfds[sslID]->ctx;
				SSL_shutdown(sslfds[sslID]);
				if (ctx)
					SSL_CTX_free(ctx);
				sslfds[sslID] = NULL;
				Memory::Write_U32(0, _BufferIn);
			}
			INFO_LOG(WII_IPC_SSL, "IOCTLV_NET_SSL_SHUTDOWN "
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
			INFO_LOG(WII_IPC_SSL, "IOCTLV_NET_SSL_SETROOTCA "
				"BufferIn: (%08x, %i), BufferIn2: (%08x, %i), "
				"BufferIn3: (%08x, %i), BufferOut: (%08x, %i), "
				"BufferOut2: (%08x, %i), BufferOut3: (%08x, %i)",
				_BufferIn, BufferInSize, _BufferIn2, BufferInSize2,
				_BufferIn3, BufferInSize3, _BufferOut, BufferOutSize,
				_BufferOut2, BufferOutSize2, _BufferOut3, BufferOutSize3);
			

			int sslID = Memory::Read_U32(_BufferOut) - 1;
			void* certca = malloc(BufferOutSize2);
			if (sslID >= 0 && sslID < NET_SSL_MAXINSTANCES && sslfds[sslID] != NULL)
			{
				
				std::string cert_base_path(File::GetUserPath(D_WIIUSER_IDX));

				SSL* ssl = sslfds[sslID];
				
				
				FILE *wiiclientca = fopen((cert_base_path + "clientca.cer").c_str(), "rb");
				if (wiiclientca == NULL)
					break;

				X509 *cert = d2i_X509_fp(wiiclientca, NULL);
				fclose(wiiclientca);
				if (SSL_use_certificate(ssl,cert) <= 0)
					break;
				if (cert)
					X509_free(cert);



				FILE * clientcakey = fopen((cert_base_path + "clientcakey.der").c_str(), "rb");
				if (clientcakey == NULL)
					break;

				EVP_PKEY * key = d2i_PrivateKey_fp(clientcakey, NULL);

				if (SSL_use_PrivateKey(ssl,key) <= 0)
					break;
				if (!SSL_check_private_key(ssl))
					break;

				
				if (key)
					EVP_PKEY_free(key);

				Memory::Write_U32(0, _BufferIn);		
			}
			free(certca);
			break;
		}


		case IOCTLV_NET_SSL_SETBUILTINCLIENTCERT:
		{
			INFO_LOG(WII_IPC_SSL, "IOCTLV_NET_SSL_SETBUILTINCLIENTCERT "
				"BufferIn: (%08x, %i), BufferIn2: (%08x, %i), "
				"BufferIn3: (%08x, %i), BufferOut: (%08x, %i), "
				"BufferOut2: (%08x, %i), BufferOut3: (%08x, %i)",
				_BufferIn, BufferInSize, _BufferIn2, BufferInSize2,
				_BufferIn3, BufferInSize3, _BufferOut, BufferOutSize,
				_BufferOut2, BufferOutSize2, _BufferOut3, BufferOutSize3);

			int sslID = Memory::Read_U32(_BufferOut) - 1;
			if (sslID >= 0 && sslID < NET_SSL_MAXINSTANCES && sslfds[sslID] != NULL)
			{
				SSL* ssl = sslfds[sslID];

				std::string cert_base_path(File::GetUserPath(D_WIIUSER_IDX));
				FILE * clientca = fopen((cert_base_path + "clientca.cer").c_str(), "rb");
				if (clientca == NULL)
					break;

				X509 *cert = d2i_X509_fp(clientca, NULL);
				fclose(clientca);

				FILE * clientcakey = fopen((cert_base_path + "clientcakey.der").c_str(), "rb");
				if (clientcakey == NULL)
					break;


				EVP_PKEY * key = d2i_PrivateKey_fp(clientcakey, NULL);

				
				if (SSL_use_certificate(ssl,cert) <= 0)
					break;
				if (SSL_use_PrivateKey(ssl,key) <= 0)
					break;

		
				if (!SSL_check_private_key(ssl))
					break;

				if (cert)
					X509_free(cert);
				if (key)
					EVP_PKEY_free(key);

				Memory::Write_U32(0, _BufferIn);				
			}
			break;
		}

		case IOCTLV_NET_SSL_SETBUILTINROOTCA:
		{
			int sslID = Memory::Read_U32(_BufferOut) - 1;
			if (sslID >= 0 && sslID < NET_SSL_MAXINSTANCES && sslfds[sslID] != NULL){
				
				Memory::Write_U32(0, _BufferIn);
			}
			INFO_LOG(WII_IPC_SSL, "IOCTLV_NET_SSL_SETBUILTINROOTCA "
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
			if (sslID >= 0 && sslID < NET_SSL_MAXINSTANCES && sslfds[sslID] != NULL)
			{
				int sock = Memory::Read_U32(_BufferOut2);
				SSL* ssl = sslfds[sslID];
				SSL_set_fd(ssl,sock);

				returnValue = SSL_connect(ssl);
				Memory::Write_U32(0, _BufferIn);
			}
			INFO_LOG(WII_IPC_SSL, "IOCTLV_NET_SSL_CONNECT "
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
			if (sslID >= 0 && sslID < NET_SSL_MAXINSTANCES && sslfds[sslID] != NULL)
			{
				SSL* ssl = sslfds[sslID];
				SSL_set_verify(ssl, SSL_VERIFY_NONE, NULL);
				returnValue = SSL_do_handshake(ssl);
				
//				if (returnValue == 1)
					Memory::Write_U32(0, _BufferIn);
			}
			INFO_LOG(WII_IPC_SSL, "IOCTLV_NET_SSL_DOHANDSHAKE "
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
			if (sslID >= 0 && sslID < NET_SSL_MAXINSTANCES && sslfds[sslID] != NULL)
			{
				SSL* ssl = sslfds[sslID];
								
				returnValue = SSL_write(ssl, Memory::GetPointer(_BufferOut2), BufferOutSize2);
				
				File::IOFile("ssl_write.bin", "ab").WriteBytes(Memory::GetPointer(_BufferOut2), BufferOutSize2);

				if (returnValue == -1)
					returnValue = -SSL_get_error(ssl, returnValue);
				Memory::Write_U32(returnValue, _BufferIn);
			}
			INFO_LOG(WII_IPC_SSL, "IOCTLV_NET_SSL_WRITE "
				"BufferIn: (%08x, %i), BufferIn2: (%08x, %i), "
				"BufferIn3: (%08x, %i), BufferOut: (%08x, %i), "
				"BufferOut2: (%08x, %i), BufferOut3: (%08x, %i)",
				_BufferIn, BufferInSize, _BufferIn2, BufferInSize2,
				_BufferIn3, BufferInSize3, _BufferOut, BufferOutSize,
				_BufferOut2, BufferOutSize2, _BufferOut3, BufferOutSize3);
			INFO_LOG(WII_IPC_SSL, "%s", Memory::GetPointer(_BufferOut2));
			break;
		}

		case IOCTLV_NET_SSL_READ:
		{
			int sslID = Memory::Read_U32(_BufferOut) - 1;
			if (sslID >= 0 && sslID < NET_SSL_MAXINSTANCES && sslfds[sslID] != NULL)
			{
				SSL* ssl = sslfds[sslID];
				returnValue = SSL_read(ssl, Memory::GetPointer(_BufferIn2), BufferInSize2);
				if (returnValue == -1)
				{
					returnValue = -SSL_get_error(ssl, returnValue);
					INFO_LOG(WII_IPC_SSL, "IOCTLV_NET_SSL_READ errorVal= %d", returnValue);
				}else{
					File::IOFile("ssl_read.bin", "ab").WriteBytes(Memory::GetPointer(_BufferIn2), returnValue);
				}

				// According to OpenSSL docs, all TLS calls (including reads) can cause
				// writing on a socket, so we need to handle SSL_ERROR_WANT_WRITE too
				// (which happens when OpenSSL writes on a nonblocking busy socket). The
				// Wii does not like -SSL_ERROR_WANT_WRITE though, so we convert it to
				// a read error.
				if (returnValue == -SSL_ERROR_WANT_WRITE)
					returnValue = -SSL_ERROR_WANT_READ;

				if (returnValue == -SSL_ERROR_SYSCALL)
				{
#ifdef _WIN32
					int errorCode = WSAGetLastError();
					bool notConnected = (errorCode == WSAENOTCONN);
#else
					int errorCode = errno;
					bool notConnected = (errorCode == ENOTCONN);
#endif
					if (notConnected)
					{
						returnValue = -SSL_ERROR_WANT_READ;
					}
					else
					{
						INFO_LOG(WII_IPC_SSL, "IOCTLV_NET_SSL_READ ERRORCODE= %d", errorCode);
					}
				}
				Memory::Write_U32(returnValue, _BufferIn);
			}
			
			INFO_LOG(WII_IPC_SSL, "IOCTLV_NET_SSL_READ(%d)"
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
			if (sslID >= 0 && sslID < NET_SSL_MAXINSTANCES && sslfds[sslID] != NULL){
				
				Memory::Write_U32(0, _BufferIn);
			}
			INFO_LOG(WII_IPC_SSL, "IOCTLV_NET_SSL_SETROOTCADEFAULT "
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
			INFO_LOG(WII_IPC_SSL, "IOCTLV_NET_SSL_SETCLIENTCERTDEFAULT "
				"BufferIn: (%08x, %i), BufferIn2: (%08x, %i), "
				"BufferIn3: (%08x, %i), BufferOut: (%08x, %i), "
				"BufferOut2: (%08x, %i), BufferOut3: (%08x, %i)",
				_BufferIn, BufferInSize, _BufferIn2, BufferInSize2,
				_BufferIn3, BufferInSize3, _BufferOut, BufferOutSize,
				_BufferOut2, BufferOutSize2, _BufferOut3, BufferOutSize3);

			int sslID = Memory::Read_U32(_BufferOut) - 1;
			if (sslID >= 0 && sslID < NET_SSL_MAXINSTANCES && sslfds[sslID] != NULL)
			{
				SSL* ssl = sslfds[sslID];

				std::string cert_base_path(File::GetUserPath(D_WIIUSER_IDX));
				FILE * clientca = fopen((cert_base_path + "clientca.cer").c_str(), "rb");
				if (clientca == NULL)
					break;

				X509 *cert = d2i_X509_fp(clientca, NULL);
				fclose(clientca);

				FILE * clientcakey = fopen((cert_base_path + "clientcakey.der").c_str(), "rb");
				if (clientcakey == NULL)
					break;


				EVP_PKEY * key = d2i_PrivateKey_fp(clientcakey, NULL);

				
				if (SSL_use_certificate(ssl,cert) <= 0)
					break;
				if (SSL_use_PrivateKey(ssl,key) <= 0)
					break;

		
				if (!SSL_check_private_key(ssl))
					break;

				if (cert)
					X509_free(cert);
				if (key)
					EVP_PKEY_free(key);

				Memory::Write_U32(0, _BufferIn);				
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
			INFO_LOG(WII_IPC_SSL, "%s unknown %i "
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
