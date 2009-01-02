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


#include "Input.h"
#include "Console.h"
#include "Status.h"
#include "Playback.h"
#include "InputPlugin.h"
#include "VisPlugin.h"
#include "DspPlugin.h"
#include "VisCache.h"
#include "Output.h"

/*
#define FIXED_POINT 16
#include "kiss_fft/kiss_fftr.h"
*/

#include "fftw3/fftw3.h"
#include <math.h>


// #define SKIPPY
#ifndef SKIPPY
# define MAX_DATA_FPS	( 1000 / 12 )	// in_mp3 gives new vis data every 13 ms, so 12 leaves a little space
#else
# define MAX_DATA_FPS	( 1000 / 24 )   // will skip every second frame
#endif


#ifdef SKIPPY
bool bLastSkipped = true;
#endif

int iSpecChannels = 2;
int iWaveChannels = 2;


/*
kiss_fft_cfg kiss = { 0 };
bool bKissInitDone = false;
*/


////////////////////////////////////////////////////////////////////////////////	
///
////////////////////////////////////////////////////////////////////////////////	
int dsp_isactive()
{
////////////////////////////////////////////////////////////////////////////////	
	DspLock.Enter();
////////////////////////////////////////////////////////////////////////////////	

	int res = ( active_dsp_count > 0 );

////////////////////////////////////////////////////////////////////////////////	
	DspLock.Leave();
////////////////////////////////////////////////////////////////////////////////

	return res;
}



////////////////////////////////////////////////////////////////////////////////	
///
////////////////////////////////////////////////////////////////////////////////	
int dsp_dosamples( short int * samples, int numsamples, int bps, int nch, int srate )
{
	int num = numsamples;

////////////////////////////////////////////////////////////////////////////////	
	DspLock.Enter();
////////////////////////////////////////////////////////////////////////////////	

	for( int i = 0; i < active_dsp_count; i++ )
	{
		// Process
		num = active_dsp_mods[ i ]->mod->ModifySamples(
			active_dsp_mods[ i ]->mod,
			samples,
			numsamples,
			bps,
			nch,
			srate
		);
	}

////////////////////////////////////////////////////////////////////////////////	
	DspLock.Leave();
////////////////////////////////////////////////////////////////////////////////	

	return num;
}



////////////////////////////////////////////////////////////////////////////////	
///
////////////////////////////////////////////////////////////////////////////////	
void SAVSAInit( int maxlatency_in_ms, int srate )
{
////////////////////////////////////////////////////////////////////////////////	
	VisCache::Create();
	VisCache::EnsureLatency( maxlatency_in_ms );
	VisCache::EnsureDataFps( MAX_DATA_FPS );
	VisCache::Clean();
////////////////////////////////////////////////////////////////////////////////
}



////////////////////////////////////////////////////////////////////////////////	
///
////////////////////////////////////////////////////////////////////////////////	
void SAVSADeInit()
{
	// TODO
	// Console::Append( TEXT( "SAVSADeInit" ) );
}



////////////////////////////////////////////////////////////////////////////////	
///
////////////////////////////////////////////////////////////////////////////////	
void SAAddPCMData( void * PCMData, int nch, int bps, int timestamp )
{
	// TODO
	// Console::Append( TEXT( "SAAddPCMData" ) );
}



////////////////////////////////////////////////////////////////////////////////	
///
////////////////////////////////////////////////////////////////////////////////	
int SAGetMode()
{
	// TODO
	// Console::Append( TEXT( "SAGetMode" ) );
	return 0;
}



////////////////////////////////////////////////////////////////////////////////	
///
////////////////////////////////////////////////////////////////////////////////	
void SAAdd( void * data, int timestamp, int csa )
{
	// TODO
	// Console::Append( TEXT( "SAAdd" ) );
}



////////////////////////////////////////////////////////////////////////////////	
///
////////////////////////////////////////////////////////////////////////////////	
void VSAAddPCMData( void * PCMData, int nch, int bps, int timestamp )
{
	// TODO
	// Console::Append( TEXT( "VSAAddPCMData" ) );
	
	
	#ifdef SKIPPY
	if( bLastSkipped )
		bLastSkipped = false;
	else
	{
		// Skip
		bLastSkipped = true;
		return;	
	}
	#endif


////////////////////////////////////////////////////////////////////////////////
	bool bVisLockLeft = false;
	VisLock.Enter();
////////////////////////////////////////////////////////////////////////////////

	if( active_vis_count )
	{
////////////////////////////////////////////////////////////////////////////////
		VisLock.Leave();
		bVisLockLeft = true;
////////////////////////////////////////////////////////////////////////////////

		VisCache::NextFrame();
		VisCache::SetWriteTime( timestamp );
		VisCache::SetReadTime( active_input_plugin->plugin->GetOutputTime() );

		short * source = ( short * )PCMData;

		// Waveform
		static unsigned char wave_left[ 576 ];
		static unsigned char wave_right[ 576 ];
		if( nch < 2 )
		{
			// Mono
			for( int i = 0; i < 576; i++ )
			{
				wave_left[ i ] = ( source[ i ] >> 8 );
			}

			VisCache::PutWaveLeft( wave_left );		
		}
		else
		{
			int i;
			
			// Stereo or more
			for( i = 0; i < 576; i++ )
			{
				wave_left [ i ] = ( source[ i * nch     ] >> 8 );
				wave_right[ i ] = ( source[ i * nch + 1 ] >> 8 );
			}
	
			VisCache::PutWaveLeft( wave_left );
			VisCache::PutWaveRight( wave_right );
		}


		// TODO: Much to optimize!


		// Spectrum
		static unsigned char spec_left[ 576 ];
		static unsigned char spec_right[ 576 ];
		static fftw_complex * in   = NULL;
		static fftw_complex * out  = NULL;

		if( !in )
		{
			in   = ( fftw_complex * )fftw_malloc( 576 * sizeof( fftw_complex ) );
			out  = ( fftw_complex * )fftw_malloc( 576 * sizeof( fftw_complex ) );
		}

		static const double factor = 1.0 / 65536.0 / sqrt( 2.0 );


		// Put left
		int index = 0;
		for( int i = 0; i < 576; i++ )
		{
			in[ i ][ 0 ] = source[ index += nch ];
			in[ i ][ 1 ] = 0.0;
		}


		// Init FFT
		fftw_plan p = fftw_plan_dft_1d( 576, in, out, FFTW_FORWARD, FFTW_ESTIMATE );

		
		// Run left
		fftw_execute( p );

		// Get left
		for( int i = 0; i < 576; i++ )
		{
			if( i & 1 )
			{
				spec_left [ i ] = spec_left [ i - 1 ];
				continue;
			}
			const double re = out[ i >> 1 ][ 0 ];
			const double im = out[ i >> 1 ][ 1 ];
			const double root = sqrt( re*re + im*im );
			const double final = 160.0 * log10( 1.0 + root * factor );
			spec_left[ i ] = ( final < 255.0 ) ? ( unsigned char )final : 255;
		}
		VisCache::PutSpecLeft( spec_left );			


		if( nch > 1 )
		{
			// Put right
			index = 1;
			for( int i = 0; i < 576; i++ )
			{
				in[ i ][ 0 ] = source[ index += nch ];
			}
	
			// Run right
			fftw_execute( p );
	
			// Get right
			for( int i = 0; i < 576; i++ )
			{
				if( i & 1 )
				{
					spec_right[ i ] = spec_right[ i - 1 ];
					continue;
				}
				const double re = out[ i >> 1 ][ 0 ];
				const double im = out[ i >> 1 ][ 1 ];
				const double root = sqrt( re*re + im*im );
				const double final = 160.0 * log10( 1.0 + root * factor );
				spec_right[ i ] = ( final < 255.0 ) ? ( unsigned char )final : 255;
			}
			VisCache::PutSpecRight( spec_right );
		}


		// Cleanup FFT
		fftw_destroy_plan( p );

		// fftw_free(in);
		// fftw_free(out);
	}

////////////////////////////////////////////////////////////////////////////////		
	if( bVisLockLeft ) VisLock.Enter();
////////////////////////////////////////////////////////////////////////////////
		
	for( int i = 0; i < active_vis_count; i++ )
	{
		active_vis_mods[ i ]->bAllowRender = true;
	}

////////////////////////////////////////////////////////////////////////////////
	VisLock.Leave();
////////////////////////////////////////////////////////////////////////////////	
}



////////////////////////////////////////////////////////////////////////////////	
///
////////////////////////////////////////////////////////////////////////////////	
int VSAGetMode( int * specNch, int * waveNch )
{
	iSpecChannels = 0;
	iWaveChannels = 0;

////////////////////////////////////////////////////////////////////////////////	
	VisLock.Enter();
////////////////////////////////////////////////////////////////////////////////	

	for( int i = 0; i < active_vis_count; i++ )
	{
		const int iSpec = active_vis_mods[ i ]->mod->spectrumNch;
		const int iWave = active_vis_mods[ i ]->mod->waveformNch;
		
		if( iSpec > iSpecChannels ) iSpecChannels = iSpec;
		if( iWave > iWaveChannels ) iWaveChannels = iWave;
	}

////////////////////////////////////////////////////////////////////////////////	
	VisLock.Leave();
////////////////////////////////////////////////////////////////////////////////

	*specNch = iSpecChannels;
	*waveNch = iWaveChannels;

	return 1;
}



////////////////////////////////////////////////////////////////////////////////	
///
////////////////////////////////////////////////////////////////////////////////	
void VSAAdd( void * data, int timestamp )
{
	#ifdef SKIPPY
	if( bLastSkipped )
		bLastSkipped = false;
	else
	{
		// Skip
		bLastSkipped = true;
		return;	
	}
	#endif


////////////////////////////////////////////////////////////////////////////////
	bool bVisLockLeft = false;
	VisLock.Enter();
////////////////////////////////////////////////////////////////////////////////

	if( active_vis_count )
	{
////////////////////////////////////////////////////////////////////////////////
		VisLock.Leave();
		bVisLockLeft = true;
////////////////////////////////////////////////////////////////////////////////

		VisCache::NextFrame();
		VisCache::SetWriteTime( timestamp );
		VisCache::SetReadTime( active_input_plugin->plugin->GetOutputTime() );

		unsigned char * source = ( unsigned char * )data;
		if( iSpecChannels > 0 )
		{
			VisCache::PutSpecLeft( source );
			source += 576;
		}
		if( iSpecChannels > 1 )
		{
			VisCache::PutSpecRight( source );
			source += 576;
		}
		if( iWaveChannels > 0 )
		{
			VisCache::PutWaveLeft( source );
			source += 576;
		}
		if( iWaveChannels > 1 )
		{
			VisCache::PutWaveRight( source );
		}
	}

////////////////////////////////////////////////////////////////////////////////		
	if( bVisLockLeft ) VisLock.Enter();
////////////////////////////////////////////////////////////////////////////////
		
	for( int i = 0; i < active_vis_count; i++ )
	{
		active_vis_mods[ i ]->bAllowRender = true;
	}

////////////////////////////////////////////////////////////////////////////////
	VisLock.Leave();
////////////////////////////////////////////////////////////////////////////////
}



////////////////////////////////////////////////////////////////////////////////	
///
////////////////////////////////////////////////////////////////////////////////	
void VSASetInfo( int nch, int srate )
{
	// TODO
	
////////////////////////////////////////////////////////////////////////////////	
	VisCache::Create();
	VisCache::EnsureDataFps( MAX_DATA_FPS );
	VisCache::Clean();
////////////////////////////////////////////////////////////////////////////////
}



////////////////////////////////////////////////////////////////////////////////	
///
////////////////////////////////////////////////////////////////////////////////	
void SetInfo( int bitrate, int srate, int stereo, int synched )
{
	// TODO
	static int last_valid_srate = 0;

	if( bitrate < 0 ) return;

	if( srate < 0 )
		srate = last_valid_srate;
	else
		last_valid_srate = srate;

	TCHAR szBuffer[ 5000 ] = TEXT( "" );
	_stprintf( szBuffer, TEXT( " %i kbps, %i kHz" ), bitrate, srate );
	StatusUpdate( szBuffer );
}
