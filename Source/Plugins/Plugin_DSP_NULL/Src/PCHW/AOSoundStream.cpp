#include <ao/ao.h>
#include <pthread.h>
#include "AOSoundStream.h"

namespace AOSound
{
	pthread_t thread;
	StreamCallback callback;

	char *buffer;
	int buf_size;
	
	ao_device *device;
	ao_sample_format format;
	int default_driver;
	
	int bufferSize;     
	int totalRenderedBytes;
	int sampleRate;
	volatile int threadData;
	int currentPos;
	int lastPos;
	short realtimeBuffer[1024 * 1024];
	int AOSound_GetSampleRate()
	{
		return sampleRate;
	}
	bool WriteDataToBuffer(int dwOffset,char* soundData, int dwSoundBytes) 
	{
		//void* ptr1, * ptr2;
		//DWORD numBytes1, numBytes2;
		// Obtain memory address of write block. This will be in two parts if the block wraps around.
		//HRESULT hr = dsBuffer->Lock(dwOffset, dwSoundBytes, &ptr1, &numBytes1, &ptr2, &numBytes2, 0);

		// If the buffer was lost, restore and retry lock.
		/*if (DSERR_BUFFERLOST == hr)
		{
			dsBuffer->Restore();
			hr = dsBuffer->Lock(dwOffset, dwSoundBytes, &ptr1, &numBytes1, &ptr2, &numBytes2, 0);
		}

		if (SUCCEEDED(hr))
		{
			memcpy(ptr1, soundData, numBytes1);

			if (ptr2 != 0)
			{
				memcpy(ptr2, soundData + numBytes1, numBytes2);
			}

			// Release the data back to DirectSound.
			dsBuffer->Unlock(ptr1, numBytes1, ptr2, numBytes2);
			return(true);
		}

		return(false);*/
		ao_play(device, soundData, dwSoundBytes);
		return true;
		
	}

	void* soundThread(void*)
	{
		currentPos = 0;
		lastPos = 0;

		// Prefill buffer?
		//writeDataToBuffer(0,realtimeBuffer,bufferSize);
		//  dsBuffer->Lock(0, bufferSize, (void **)&p1, &num1, (void **)&p2, &num2, 0);
		//dsBuffer->Play(0, 0, DSBPLAY_LOOPING);

		while (!threadData)
		{
			// No blocking inside the csection
			//dsBuffer->GetCurrentPosition((DWORD*)&currentPos, 0);
			int numBytesToRender = 256;

			if (numBytesToRender >= 256)
			{
				(*callback)(realtimeBuffer, numBytesToRender >> 2, 16, 44100, 2);

				WriteDataToBuffer(0, (char*)realtimeBuffer, numBytesToRender);
				//currentPos = ModBufferSize(lastPos + numBytesToRender);
				//totalRenderedBytes += numBytesToRender;

				//lastPos = currentPos;
			}

			//WaitForSingleObject(soundSyncEvent, MAXWAIT);
		}

		//dsBuffer->Stop();
		return(0); //hurra!
	}
	bool AOSound_StartSound(int _sampleRate, StreamCallback _callback)
	{
		callback   = _callback;
		threadData = 0;
		sampleRate = _sampleRate;

		//no security attributes, automatic resetting, init state nonset, untitled
		//soundSyncEvent = CreateEvent(0, false, false, 0);
		ao_initialize();
		default_driver = ao_default_driver_id();
		format.bits = 16;
		format.channels = 2;
		format.rate = sampleRate;
		format.byte_format = AO_FMT_LITTLE;
		
		//vi vill ha access till DSOUND så...
		device = ao_open_live(default_driver, &format, NULL /* no options */);
		if (device == NULL) {
			fprintf(stderr, "Error opening device.\n");
			return 1;
		}   
		buf_size = format.bits/8 * format.channels * format.rate;
		buffer = (char*)calloc(buf_size, sizeof(char));
		pthread_create(&thread, NULL, soundThread, (void *)NULL);
		return(true);
	}
	void AOSound_StopSound()
	{
		ao_close(device);
		ao_shutdown();
	}
}
