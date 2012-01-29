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
	u32 returnValue = 0;

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
			char in1[32];
			char out1[32];
			char out2[256];

			Memory::ReadBigEData((u8*)in1, _BufferIn, 32);
			Memory::ReadBigEData((u8*)out1, _BufferOut, 32);
			Memory::ReadBigEData((u8*)out2, _BufferOut2, 256);
			
			int freeSSL = this->getSSLFreeID();
			if (freeSSL)
			{
				Memory::Write_U32(freeSSL, _BufferIn);
				
				SSL_CTX* ctx = SSL_CTX_new(SSLv23_method());

				//SSL_CTX_set_options(ctx,0);

				SSL* ssl = SSL_new(ctx);
				sslfds[freeSSL-1] = ssl;

			}

			INFO_LOG(WII_IPC_NET, "/dev/net/ssl::IOCtlV request IOCTLV_NET_SSL_NEW "
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
			char in1[32];
			char out1[32];
			Memory::ReadBigEData((u8*)in1, _BufferIn, 32);
			Memory::ReadBigEData((u8*)out1, _BufferOut, 32);
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
			INFO_LOG(WII_IPC_NET, "/dev/net/ssl::IOCtlV request IOCTLV_NET_SSL_SHUTDOWN "
				"BufferIn: (%08x, %i), BufferIn2: (%08x, %i), "
				"BufferIn3: (%08x, %i), BufferOut: (%08x, %i), "
				"BufferOut2: (%08x, %i), BufferOut3: (%08x, %i)",
				_BufferIn, BufferInSize, _BufferIn2, BufferInSize2,
				_BufferIn3, BufferInSize3, _BufferOut, BufferOutSize,
				_BufferOut2, BufferOutSize2, _BufferOut3, BufferOutSize3);
			break;
		}
		// a BufferIn: (007ad6a0, 32), BufferIn2: (00000000, 0), BufferIn3: (00000000, 0), BufferOut: (007ad680, 32), BufferOut2: (00765920, 924), BufferOut3: (00000000, 0)
		case IOCTLV_NET_SSL_SETROOTCA:
		{
			INFO_LOG(WII_IPC_NET, "/dev/net/ssl::IOCtlV request IOCTLV_NET_SSL_SETROOTCA "
				"BufferIn: (%08x, %i), BufferIn2: (%08x, %i), "
				"BufferIn3: (%08x, %i), BufferOut: (%08x, %i), "
				"BufferOut2: (%08x, %i), BufferOut3: (%08x, %i)",
				_BufferIn, BufferInSize, _BufferIn2, BufferInSize2,
				_BufferIn3, BufferInSize3, _BufferOut, BufferOutSize,
				_BufferOut2, BufferOutSize2, _BufferOut3, BufferOutSize3);
			
//Dolphin_Debugger::PrintCallstack(LogTypes::WII_IPC_NET, LogTypes::LINFO);

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
			INFO_LOG(WII_IPC_NET, "/dev/net/ssl::IOCtlV request IOCTLV_NET_SSL_SETBUILTINCLIENTCERT "
				"BufferIn: (%08x, %i), BufferIn2: (%08x, %i), "
				"BufferIn3: (%08x, %i), BufferOut: (%08x, %i), "
				"BufferOut2: (%08x, %i), BufferOut3: (%08x, %i)",
				_BufferIn, BufferInSize, _BufferIn2, BufferInSize2,
				_BufferIn3, BufferInSize3, _BufferOut, BufferOutSize,
				_BufferOut2, BufferOutSize2, _BufferOut3, BufferOutSize3);

			char in1[32];
			char out1[32];
			char out2[32];
			Memory::ReadBigEData((u8*)in1, _BufferIn, 32);
			Memory::ReadBigEData((u8*)out1, _BufferOut, 32);
			Memory::ReadBigEData((u8*)out2, _BufferOut2, 32);
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
				
				/*PKCS12 *p12;

				FILE* f = fopen("c:\\wii_retail.p12", "rb");

				if (!f)
					break;

				
				p12 = d2i_PKCS12_fp(f, NULL);
				fclose(f);
				if (!p12)
					break;
				EVP_PKEY *pkey = NULL;
				X509 *cert = NULL;
				STACK_OF(X509) *ca = sk_X509_new_null();
				
				PKCS12_PBE_add();

				if (!PKCS12_parse(p12, "", &pkey, &cert, &ca))
					break;

				if (cert) {
					SSL_CTX_use_certificate(ssl->ctx, cert);
					X509_free(cert);
				}
				if (pkey)
{
					SSL_CTX_use_PrivateKey(ssl->ctx, pkey);
					EVP_PKEY_free(pkey);
				}
				
				
				for (int i = 0; i < sk_X509_num(ca); i++) {
					X509 *cert2 = sk_X509_value(ca, i);
					  
					char buf[200];

					X509_NAME_oneline(X509_get_subject_name(cert2), buf, sizeof(buf));
					SSL_CTX_add_extra_chain_cert(ssl->ctx, cert2);
					  
				}
				sk_X509_free(ca);
				
				PKCS12_free(p12);
				*/

				//SSL_CTX_use_certificate_chain_file(ssl->ctx, (char*)ssl->cert);
				Memory::Write_U32(0, _BufferIn);				
			}
			break;
		}

		case IOCTLV_NET_SSL_SETBUILTINROOTCA:
		{
			char in1[32];
			char out1[32];
			char out2[32];
			Memory::ReadBigEData((u8*)in1, _BufferIn, 32);
			Memory::ReadBigEData((u8*)out1, _BufferOut, 32);
			Memory::ReadBigEData((u8*)out2, _BufferOut2, 32);
			int sslID = Memory::Read_U32(_BufferOut) - 1;
			if (sslID >= 0 && sslID < NET_SSL_MAXINSTANCES && sslfds[sslID] != NULL)
{
				
				Memory::Write_U32(0, _BufferIn);
			}
			INFO_LOG(WII_IPC_NET, "/dev/net/ssl::IOCtlV request IOCTLV_NET_SSL_SETBUILTINROOTCA "
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
			char in1[32];
			char out1[32];
			char out2[32];
			Memory::ReadBigEData((u8*)in1, _BufferIn, 32);
			Memory::ReadBigEData((u8*)out1, _BufferOut, 32);
			Memory::ReadBigEData((u8*)out2, _BufferOut2, 32);
			int sslID = Memory::Read_U32(_BufferOut) - 1;
			if (sslID >= 0 && sslID < NET_SSL_MAXINSTANCES && sslfds[sslID] != NULL)
			{
				int sock = Memory::Read_U32(_BufferOut2);
				SSL* ssl = sslfds[sslID];
				SSL_set_fd(ssl,sock);

				FILE *ssl_write = fopen("ssl_write.txt", "ab");
				fprintf(ssl_write, "%s", "###############\n");
				fclose(ssl_write);

				FILE *ssl_read = fopen("ssl_read.txt", "ab");
				fprintf(ssl_read, "%s", "###############\n");
				fclose(ssl_read);


				returnValue = SSL_connect(ssl);
				Memory::Write_U32(0, _BufferIn);
			}
			INFO_LOG(WII_IPC_NET, "/dev/net/ssl::IOCtlV request IOCTLV_NET_SSL_CONNECT "
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
			char in1[32];
			char out1[32];
			Memory::ReadBigEData((u8*)in1, _BufferIn, 32);
			Memory::ReadBigEData((u8*)out1, _BufferOut, 32);

			int sslID = Memory::Read_U32(_BufferOut) - 1;
			if (sslID >= 0 && sslID < NET_SSL_MAXINSTANCES && sslfds[sslID] != NULL)
			{
				SSL* ssl = sslfds[sslID];
				SSL_set_verify(ssl, SSL_VERIFY_NONE, NULL);
				returnValue = SSL_do_handshake(ssl);
				SSL_load_error_strings();
				FILE *quickDump = fopen("quickdump.txt", "wb");
				ERR_print_errors_fp(quickDump);
				fclose(quickDump);
//				if (returnValue == 1)
					Memory::Write_U32(0, _BufferIn);
			}
			INFO_LOG(WII_IPC_NET, "/dev/net/ssl::IOCtlV request IOCTLV_NET_SSL_DOHANDSHAKE "
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
			char in1[32];
			char out1[32];
			char out2[256];
			
			//Dolphin_Debugger::PrintCallstack(LogTypes::WII_IPC_NET, LogTypes::LINFO);

			Memory::ReadBigEData((u8*)in1, _BufferIn, 32);
			Memory::ReadBigEData((u8*)out1, _BufferOut, 32);
			Memory::ReadBigEData((u8*)out2, _BufferOut2, BufferOutSize2);
			
			int sslID = Memory::Read_U32(_BufferOut) - 1;
			if (sslID >= 0 && sslID < NET_SSL_MAXINSTANCES && sslfds[sslID] != NULL)
			{
				SSL* ssl = sslfds[sslID];
				
				FILE *ssl_write = fopen("ssl_write.txt", "ab");
				fwrite(Memory::GetPointer(_BufferOut2), 1, BufferOutSize2, ssl_write);
				//fprintf(ssl_write, "----(%d)----\n", BufferOutSize2);
				fclose(ssl_write);
				
				returnValue = SSL_write(ssl, Memory::GetPointer(_BufferOut2), BufferOutSize2);
				if (returnValue == -1)
					returnValue = -SSL_get_error(ssl, returnValue);
				Memory::Write_U32(returnValue, _BufferIn);
				//returnValue = 0;
			}
			INFO_LOG(WII_IPC_NET, "/dev/net/ssl::IOCtlV request IOCTLV_NET_SSL_WRITE "
				"BufferIn: (%08x, %i), BufferIn2: (%08x, %i), "
				"BufferIn3: (%08x, %i), BufferOut: (%08x, %i), "
				"BufferOut2: (%08x, %i), BufferOut3: (%08x, %i)",
				_BufferIn, BufferInSize, _BufferIn2, BufferInSize2,
				_BufferIn3, BufferInSize3, _BufferOut, BufferOutSize,
				_BufferOut2, BufferOutSize2, _BufferOut3, BufferOutSize3);
			INFO_LOG(WII_IPC_NET, "%s", Memory::GetPointer(_BufferOut2));
			break;
		}

		case IOCTLV_NET_SSL_READ:
		{
			u32 in1[32];
			u32 in2[256];
			u32 out1[32];
			memset(in2, 0, 256);
			Memory::ReadBigEData((u8*)in1, _BufferIn, 32);
			//Memory::ReadBigEData((u8*)in2, _BufferIn2, BufferInSize2);
			Memory::ReadBigEData((u8*)out1, _BufferOut, 32);
			
			
			/*DumpCommands(_BufferIn,4,LogTypes::WII_IPC_NET,LogTypes::LDEBUG);
			
			DumpCommands(_BufferIn2,2,LogTypes::WII_IPC_NET,LogTypes::LDEBUG);
			DumpCommands(_BufferOut,4,LogTypes::WII_IPC_NET,LogTypes::LDEBUG);
			*/


			int sslID = Memory::Read_U32(_BufferOut) - 1;
			if (sslID >= 0 && sslID < NET_SSL_MAXINSTANCES && sslfds[sslID] != NULL)
			{
				SSL* ssl = sslfds[sslID];
				returnValue = SSL_read(ssl, Memory::GetPointer(_BufferIn2), BufferInSize2);
				if (returnValue == -1){
					returnValue = -SSL_get_error(ssl, returnValue);
					WARN_LOG(WII_IPC_NET, "/dev/net/ssl::IOCtlV request IOCTLV_NET_SSL_READ errorVal= %d", returnValue);
				}
				if (returnValue == -1)
					returnValue = -SSL_ERROR_WANT_READ;
				if (returnValue == -5){
					int errorCode = WSAGetLastError();
					if (errorCode == 10057){
						returnValue = -SSL_ERROR_WANT_READ;
					}else{
						WARN_LOG(WII_IPC_NET, "/dev/net/ssl::IOCtlV request IOCTLV_NET_SSL_READ ERRORCODE= %d", WSAGetLastError());
					}
				}
				SSL_load_error_strings();
				FILE *quickDump = fopen("quickdump.txt", "ab");
				ERR_print_errors_fp(quickDump);
				fclose(quickDump);
				Memory::Write_U32(returnValue, _BufferIn);
				//returnValue = 0;
			}
			memcpy(in2, (char*)Memory::GetPointer(_BufferIn2), BufferInSize2);


			FILE *ssl_read = fopen("ssl_read.txt", "ab");
			if((s32)returnValue >0)
				fwrite(in2, 1, returnValue, ssl_read);
//fprintf(ssl_read, "%s", "--------\n");
			fclose(ssl_read);
			
			INFO_LOG(WII_IPC_NET, "/dev/net/ssl::IOCtlV request IOCTLV_NET_SSL_READ(%d) %s "
				"BufferIn: (%08x, %i), BufferIn2: (%08x, %i), "
				"BufferIn3: (%08x, %i), BufferOut: (%08x, %i), "
				"BufferOut2: (%08x, %i), BufferOut3: (%08x, %i)",
				returnValue, in2,
				_BufferIn, BufferInSize, _BufferIn2, BufferInSize2,
				_BufferIn3, BufferInSize3, _BufferOut, BufferOutSize,
				_BufferOut2, BufferOutSize2, _BufferOut3, BufferOutSize3);
			break;
		}

		default:
		{
			ERROR_LOG(WII_IPC_NET, "/dev/net/ssl::IOCtlV request %i "
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
			INFO_LOG(WII_IPC_NET, "/dev/net/ssl::IOCtl request unknown %i "
				"(BufferIn: (%08x, %i), BufferOut: (%08x, %i)",
				_Command,
				_BufferIn, BufferInSize, _BufferOut, BufferOutSize);
			break;
		}
	}
	return 0;
}

#ifdef _MSC_VER
#pragma optimize("",on)
#endif
