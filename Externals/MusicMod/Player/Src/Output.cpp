////////////////////////////////////////////////////////////////////////////////
// Plainamp, Open source Winamp core
// 
// Copyright © 2005  Sebastian Pipping <webmaster@hartwork.org>
// 
// -->  http://www.hartwork.org
// 
// This source code is released under the GNU General Public License (GPL).
// See GPL.txt for details. Any non-GPL usage is strictly forbidden.
////////////////////////////////////////////////////////////////////////////////


#include "Output.h"
#include "OutputPlugin.h"
#include "Console.h"
#include <time.h>



int iNullSampleRate;
int iNullSumBytesPerSample;
int iNullWrittenMillis;
bool bNullPlaying;
int bNullPaused;
DWORD dwNullOpenTimestamp;
DWORD dwNullPauseTimestamp;

const int NULL_DEFAULT_LATENCY = 1000;


int  Output_Open( int samplerate, int numchannels, int bitspersamp, int bufferlenms, int prebufferms );
void Output_Close();
int  Output_Write( char * buf, int len );
int  Output_CanWrite();
int  Output_IsPlaying();
int  Output_Pause( int pause );
void Output_SetVolume( int volume );
void Output_SetPan( int pan );
void Output_Flush( int t );
int  Output_GetOutputTime();
int  Output_GetWrittenTime();



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
Out_Module output_server = {
	OUT_VER,                        // int version
	"Plainamp Output Server",       // char * description;
	0x7fffffff,                     // int id
	NULL,                           // HWND hMainWindow
	NULL,                           // HINSTANCE hDllInstance
	NULL,                           // void (*Config)(HWND hwndParent);
	NULL,                           // void (*About)(HWND hwndParent);
	NULL,                           // void (*Init)();
	NULL,                           // void (*Quit)();
	Output_Open,                    // int (*Open)(int samplerate, int numchannels, int bitspersamp, int bufferlenms, int prebufferms); 
	Output_Close,                   // void (*Close)();
	Output_Write,                   // int (*Write)(char *buf, int len);
	Output_CanWrite,                // int (*CanWrite)();
	Output_IsPlaying,               // int (*IsPlaying)();
	Output_Pause,                   // int (*Pause)(int pause);
	Output_SetVolume,               // void (*SetVolume)(int volume);
	Output_SetPan,                  // void (*SetPan)(int pan);
	Output_Flush,                   // void (*Flush)(int t);
	Output_GetOutputTime,           // int (*GetOutputTime)();
	Output_GetWrittenTime,          // int (*GetWrittenTime)();
};



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
int Output_Open( int samplerate, int numchannels, int bitspersamp, int bufferlenms, int prebufferms )
{
	if( active_output_count > 0 )
	{
		// Maximum
		int res = 0;
		int res_temp;
		for( int i = 0; i < active_output_count; i++ )
		{
			res_temp = active_output_plugins[ i ]->plugin->Open( samplerate, numchannels, bitspersamp, bufferlenms, prebufferms );
			if( res_temp > res ) res = res_temp;
		}
		return res;
	}
	else
	{
		iNullSampleRate         = samplerate;
		iNullSumBytesPerSample  = numchannels * ( bitspersamp / 8 );
		iNullWrittenMillis      = 0;
		bNullPlaying            = false;
		bNullPaused             = 0;
		dwNullOpenTimestamp     = GetTickCount();
		
		return NULL_DEFAULT_LATENCY;
	}
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
void Output_Close()
{
	if( active_output_count > 0 )
	{
		for( int i = 0; i < active_output_count; i++ )
		{
			active_output_plugins[ i ]->plugin->Close();
		}
	}
	else
	{
		bNullPlaying = false;
	}
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
int Output_Write( char * buf, int len )
{
	if( active_output_count > 0 )
	{
		// Maximum
		int res = 0;
		int res_temp;
		for( int i = 0; i < active_output_count; i++ )
		{
			res_temp = active_output_plugins[ i ]->plugin->Write( buf, len );
			if( res_temp > res ) res = res_temp;
		}
		return res;
	}
	else
	{
		if( iNullWrittenMillis == 0 )
		{
			dwNullOpenTimestamp = GetTickCount();
			bNullPlaying = true;
		}
		
		iNullWrittenMillis += ( len / iNullSumBytesPerSample ) * 1000 / iNullSampleRate;
		return 0; // 0 == Success
	}
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
int Output_CanWrite()
{
	if( active_output_count > 0 )
	{
		// Minimum
		int res = 0x7fffffff;
		int res_temp;
		for( int i = 0; i < active_output_count; i++ )
		{
			res_temp = active_output_plugins[ i ]->plugin->CanWrite();
			if( res_temp < res ) res = res_temp;
		}
		return res;
	}
	else
	{
		const int diff = Output_GetWrittenTime() - Output_GetOutputTime();
		if( diff >= NULL_DEFAULT_LATENCY )
		{
			return 0;
		}
		else
		{
			return ( NULL_DEFAULT_LATENCY - diff ) * iNullSumBytesPerSample * iNullSampleRate / 1000;
		}
	}
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
int Output_IsPlaying()
{
	if( active_output_count > 0 )
	{
		// Maximum	
		int res = 0;
		int res_temp;
		for( int i = 0; i < active_output_count; i++ )
		{
			res_temp = active_output_plugins[ i ]->plugin->IsPlaying();
			if( res_temp > res ) res = res_temp;
		}
		return res;
	}
	else
	{
		return ( Output_GetOutputTime() < Output_GetWrittenTime() ) ? 1 : 0;
	}
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
int Output_Pause( int pause )
{
	if( active_output_count > 0 )
	{
		// Maximum
		int res = 0;
		int res_temp;
		for( int i = 0; i < active_output_count; i++ )
		{
			res_temp = active_output_plugins[ i ]->plugin->Pause( pause );
			if( res_temp > res ) res = res_temp;
		}
		return res;
	}
	else
	{
		int res = bNullPaused;
		if( pause && !bNullPaused )
		{
			// Playback should be paused
			dwNullPauseTimestamp  = GetTickCount();
			bNullPaused           = 1;
		}
		else if( !pause && bNullPaused )
		{
			// Playback should be continued
			// Add the gap length to the open timestamp like no gap exists
			dwNullOpenTimestamp += ( GetTickCount() - dwNullPauseTimestamp );
			bNullPaused = 0;
		}
		return res; // Previous state
	}
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
void Output_SetVolume( int volume )
{
	/* There was a call here with the volume value -666 that I could not see the source of,
	   so I added this check to see that we got a positive volume. */
	if(volume >= 0)
	{
		// =======================================================================================
		// The volume goes from 0 to 255
		//TCHAR szBuffer[ 5000 ];
		//_stprintf( szBuffer, TEXT( "DLL > Output_SetVolume <%i>" ), volume );
		//Console::Append( szBuffer );
		//Console::Append( TEXT( " " ) );
		//INFO_LOG(AUDIO, "DLL > Output_SetVolume <%i>\n", volume );
		// =======================================================================================

		for( int i = 0; i < active_output_count; i++ )
		{
			active_output_plugins[ i ]->plugin->SetVolume( volume );
		}
	}

}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
void Output_SetPan( int pan )
{
	for( int i = 0; i < active_output_count; i++ )
	{
		active_output_plugins[ i ]->plugin->SetPan( pan );
	}
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
void Output_Flush( int t )
{
	if( active_output_count > 0 )
	{
		for( int i = 0; i < active_output_count; i++ )
		{
			active_output_plugins[ i ]->plugin->Flush( t );
		}
	}
	else
	{
		dwNullOpenTimestamp  = GetTickCount() - t;
		iNullWrittenMillis   = t;
	}
}



////////////////////////////////////////////////////////////////////////////////
/// Returns the number of milliseconds the song would be at
/// if we had zero latency. This value is never bigger than
/// the one returned by Output_GetWrittenTime().
////////////////////////////////////////////////////////////////////////////////
int Output_GetOutputTime() // <= GetWrittenTime()
{
	if( active_output_count > 0 )
	{
		// Minimum
		int res = 0x7fffffff;
		int res_temp;
		for( int i = 0; i < active_output_count; i++ )
		{
			res_temp = active_output_plugins[ i ]->plugin->GetOutputTime();
			if( res_temp < res ) res = res_temp;
		}
		return res;
	}
	else
	{
		if( bNullPlaying )
		{
			int res;
			if( bNullPaused )
			{
				res = dwNullPauseTimestamp - dwNullOpenTimestamp;
			}
			else
			{
				res = GetTickCount() - dwNullOpenTimestamp;
			}
			return MIN( res, iNullWrittenMillis );
		}
		else
		{
			return 0;
		}
	}
}



////////////////////////////////////////////////////////////////////////////////
/// Returns the number of milliseconds already written.
/// Due to latency this value is always bigger than
/// the value returned by Output_GetOutputTime().
////////////////////////////////////////////////////////////////////////////////
int Output_GetWrittenTime()
{
	if( active_output_count > 0 )
	{
		// Maximum
		int res = 0;
		int res_temp;
		for( int i = 0; i < active_output_count; i++ )
		{
			res_temp = active_output_plugins[ i ]->plugin->GetWrittenTime();
			if( res_temp > res ) res = res_temp;
		}
		return res;
	}
	else
	{
		return iNullWrittenMillis;
	}
}
