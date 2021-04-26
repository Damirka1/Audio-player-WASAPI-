#include "..\Headers\Audio Engine.h"

#include <stdio.h>


typedef struct Data
{
	HANDLE pData;
	unsigned int DataSize;
	struct Buffers
	{
		BYTE* pBuffer[2];
		char CurrIndex;
	} Buffers;

	IMMDevice* pDevice;
	IAudioClient3* pAudioClient;
	WAVEFORMATEX* pWinAudioFormat;
	IAudioRenderClient* pAudioRender;
	IAudioStreamVolume* pAudioVolume;
	float Volume;

	BOOL KeepInMemory;
	BOOL Playing;

	// Constans
	unsigned int REFTIMES_PER_SEC; // def 10000000
	unsigned int REFTIMES_PER_MILLISEC; // def 10000

	unsigned int BufferSize;
} Data;


BOOL _Initialize(AudioEngine* This)
{
	IID IID_IMMDeviceEnumerator = { 2841011410, 38420, 20277, 167, 70, 222, 141, 182, 54, 23, 230 };
	CLSID CLSID_MMDeviceEnumerator = { 3168666517, 58671, 18044, 142, 61, 196, 87, 146, 145, 105, 46 };
	IID IID_IAudioClient3 = { 481930572, 56314, 19506, 177, 120, 194, 245, 104, 167, 3, 178 };
	IID IID_IAudioRenderClient = { 4069829884, 12614, 17539, 167, 191, 173, 220, 167, 194, 96, 226 };
	IID IID_IAudioStreamVolume = { 2466334855, 9261, 16488, 138, 21, 207, 94, 147, 185, 15, 227 };

	IMMDeviceEnumerator* Device_Enumerator = NULL;

	Data* pData = (Data*)This->pData;

	// Initialize COM.
	HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
	// Initialize device collection from COM.
	hr = CoCreateInstance(&CLSID_MMDeviceEnumerator,
		NULL, CLSCTX_ALL, &IID_IMMDeviceEnumerator, (void**)(&Device_Enumerator));

	// Get audio device.
	hr = Device_Enumerator->lpVtbl->GetDefaultAudioEndpoint(Device_Enumerator, eRender, eConsole, &pData->pDevice);
	Device_Enumerator->lpVtbl->Release(Device_Enumerator);

	// Activate AudioClient from device.
	hr = pData->pDevice->lpVtbl->Activate(pData->pDevice, &IID_IAudioClient3, CLSCTX_ALL, NULL, (void**)(&pData->pAudioClient));

	// Get Format.
	hr = pData->pAudioClient->lpVtbl->GetMixFormat(pData->pAudioClient, &pData->pWinAudioFormat);

	// Create audio buffer.
	hr = pData->pAudioClient->lpVtbl->Initialize(pData->pAudioClient, AUDCLNT_SHAREMODE_SHARED, 0, pData->REFTIMES_PER_SEC, 0, pData->pWinAudioFormat, NULL);
	hr = pData->pAudioClient->lpVtbl->GetBufferSize(pData->pAudioClient, &pData->BufferSize);

	// Get audio render.
	hr = pData->pAudioClient->lpVtbl->GetService(pData->pAudioClient, &IID_IAudioRenderClient, (void**)(&pData->pAudioRender));

	// Get volume control.
	hr = pData->pAudioClient->lpVtbl->GetService(pData->pAudioClient, &IID_IAudioStreamVolume, (void**)(&pData->pAudioVolume));


	// Set volume
	float arr[2] = { pData->Volume, pData->Volume };
	pData->pAudioVolume->lpVtbl->SetAllVolumes(pData->pAudioVolume, 2, arr);

	return 1;
}

BOOL _UnInitialize(AudioEngine* This)
{
	Data* pData = This->pData;

	pData->pDevice->lpVtbl->Release(pData->pDevice);
	pData->pAudioClient->lpVtbl->Release(pData->pAudioClient);
	pData->pAudioRender->lpVtbl->Release(pData->pAudioRender);
	pData->pAudioVolume->lpVtbl->Release(pData->pAudioVolume);

	free(pData);

	CoUninitialize();
}

void _ReadWav(AudioEngine* This, LPCWSTR Path)
{
	struct SoundData
	{
		short AudioFormat;
		short NumChannels;
		int SampleRate;
		int ByteRate;
		short BlockAlign;
		short BitsPerSample;
	} sData;

	memset(&sData, 0, sizeof(sData));

	Data* pData = (Data*)This->pData;

	HANDLE File = CreateFileW(Path, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_READONLY, NULL);

	void* HeaderData = malloc(12);

	DWORD Count = 0;
	BOOL r = ReadFile(File, HeaderData, 12, &Count, NULL);

	while (ReadFile(File, HeaderData, 8, &Count, NULL) && Count > 0)
	{
		switch (*((unsigned int*)HeaderData))
		{
		case (unsigned int)('f') | (unsigned int)('m') << 8 | (unsigned int)('t') << 16 | (unsigned int)(' ') << 24:
		{
			int ChunkSize = ((unsigned int*)HeaderData)[1];
			r = ReadFile(File, &sData, ChunkSize, &Count, NULL);
			continue;
		}

		case (unsigned int)('d') | (unsigned int)('a') << 8 | (unsigned int)('t') << 16 | (unsigned int)('a') << 24:
		{
			BOOL channels = pData->pWinAudioFormat->nChannels == sData.NumChannels;
			BOOL byterate = pData->pWinAudioFormat->nAvgBytesPerSec == sData.ByteRate;
			BOOL anign = pData->pWinAudioFormat->nBlockAlign == sData.BlockAlign;
			BOOL bitpersample = pData->pWinAudioFormat->wBitsPerSample == sData.BitsPerSample;

			if (!channels || !byterate || !anign || !bitpersample)
			{
				free(HeaderData);
				CloseHandle(File);
				MessageBoxW(NULL, L"Incorrect audio format", NULL, MB_ICONERROR | MB_OK);
				return;
			}

			pData->DataSize = ((unsigned int*)HeaderData)[1];

			if (pData->KeepInMemory)
			{
				pData->pData = (HANDLE)malloc(pData->DataSize);

				r = ReadFile(File, pData->pData, pData->DataSize, NULL, NULL);
				return;
			}

			pData->pData = File;
			pData->Buffers.CurrIndex = 0;

			return;
		}

		case (unsigned int)('L') | (unsigned int)('I') << 8 | (unsigned int)('S') << 16 | (unsigned int)('T') << 24:
		{
			int ChunkSize = ((unsigned int*)HeaderData)[1];
			void* ChunkData = malloc(ChunkSize);
			r = ReadFile(File, ChunkData, ChunkSize, &Count, NULL);
			free(ChunkData);
			continue;
		}
		}
	}

}



void _PlayRead(AudioEngine* This)
{
	Data* pData = (Data*)This->pData;

	// Create buffers.
	BYTE* pBuffer = NULL;
	HRESULT hr = pData->pAudioRender->lpVtbl->GetBuffer(pData->pAudioRender, pData->BufferSize, &pBuffer);


	pData->Buffers.pBuffer[0] = (BYTE*)malloc(pData->BufferSize * pData->pWinAudioFormat->nBlockAlign);
	pData->Buffers.pBuffer[1] = (BYTE*)malloc(pData->BufferSize * pData->pWinAudioFormat->nBlockAlign);


	// Fill buffer.
	BOOL r = ReadFile(pData->pData, pData->Buffers.pBuffer[0], pData->BufferSize * pData->pWinAudioFormat->nBlockAlign, NULL, NULL);
	memcpy(pBuffer, pData->Buffers.pBuffer[pData->Buffers.CurrIndex], pData->BufferSize * pData->pWinAudioFormat->nBlockAlign);
	pData->Buffers.CurrIndex = 1;

	hr = pData->pAudioRender->lpVtbl->ReleaseBuffer(pData->pAudioRender, pData->BufferSize, 0);

	// Start playing.
	hr = pData->pAudioClient->lpVtbl->Start(pData->pAudioClient);

	pData->Playing = 1;

	// Calculate padding to buffer.
	unsigned int iterator = pData->BufferSize;
	unsigned int numFramesPadding = 0u, numFramesAvailable = 0u;

	// Calculate actual time.
	REFERENCE_TIME hnsActualDuration = (REFERENCE_TIME)((double)(pData->REFTIMES_PER_SEC * pData->BufferSize / pData->pWinAudioFormat->nSamplesPerSec));

	// Wait for first data in buffer to play before starting.
	Sleep((DWORD)(hnsActualDuration / pData->REFTIMES_PER_MILLISEC / 2));

	while (pData->Playing)
	{
		// See how much buffer space is available.
		hr = pData->pAudioClient->lpVtbl->GetCurrentPadding(pData->pAudioClient, &numFramesPadding);

		// Grab all the available space in the shared buffer.

		numFramesAvailable = pData->BufferSize - numFramesPadding;

		switch (pData->Buffers.CurrIndex)
		{
		case 0:
		{
			BOOL r = ReadFile(pData->pData, pData->Buffers.pBuffer[1], numFramesAvailable * pData->pWinAudioFormat->nBlockAlign, NULL, NULL);
			pData->Buffers.CurrIndex = 1;
			break;
		}
		case 1:
		{
			BOOL r = ReadFile(pData->pData, pData->Buffers.pBuffer[0], numFramesAvailable * pData->pWinAudioFormat->nBlockAlign, NULL, NULL);
			pData->Buffers.CurrIndex = 0;
			break;
		}
		}

		if (iterator + numFramesPadding < (unsigned int)(pData->DataSize / pData->pWinAudioFormat->nBlockAlign))
		{
			hr = pData->pAudioRender->lpVtbl->GetBuffer(pData->pAudioRender, numFramesAvailable, &pBuffer);
			// Fill buffer.
			memcpy(pBuffer, pData->Buffers.pBuffer[pData->Buffers.CurrIndex], numFramesAvailable * pData->pWinAudioFormat->nBlockAlign);

			hr = pData->pAudioRender->lpVtbl->ReleaseBuffer(pData->pAudioRender, numFramesAvailable, 0);

			iterator += numFramesAvailable;
		}
		else
			pData->Playing = 0;

		Sleep((DWORD)(hnsActualDuration / pData->REFTIMES_PER_MILLISEC / 2));
	}

	CloseHandle(This->pData);

	// Stop playing.
	hr = pData->pAudioClient->lpVtbl->Stop(pData->pAudioClient);


	free(pData->Buffers.pBuffer[0]);
	free(pData->Buffers.pBuffer[1]);

}

void _PlayKeep(AudioEngine* This)
{
	Data* pData = (Data*)This->pData;

	// Get pointer to our file.
	BYTE* ptr = pData->pData;
	// Create buffers.
	BYTE* pBuffer = NULL;
	HRESULT hr = pData->pAudioRender->lpVtbl->GetBuffer(pData->pAudioRender, pData->BufferSize, &pBuffer);

	// Copy from file to buffer.
	memcpy(pBuffer, ptr, pData->BufferSize * pData->pWinAudioFormat->nBlockAlign);
	
	hr = pData->pAudioRender->lpVtbl->ReleaseBuffer(pData->pAudioRender, pData->BufferSize, 0);

	// Move pointer.
	ptr += pData->BufferSize * pData->pWinAudioFormat->nBlockAlign;

	// Start playing.
	hr = pData->pAudioClient->lpVtbl->Start(pData->pAudioClient);

	pData->Playing = 1;

	// Calculate padding to buffer.
	unsigned int iterator = pData->BufferSize;
	unsigned int numFramesPadding = 0u, numFramesAvailable = 0u;

	// Calculate actual time.
	REFERENCE_TIME hnsActualDuration = (REFERENCE_TIME)((double)(pData->REFTIMES_PER_SEC * pData->BufferSize / pData->pWinAudioFormat->nSamplesPerSec));

	// Wait for first data in buffer to play before starting.
	Sleep((DWORD)(hnsActualDuration / pData->REFTIMES_PER_MILLISEC / 2));

	while (pData->Playing)
	{
		// See how much buffer space is available.
		hr = pData->pAudioClient->lpVtbl->GetCurrentPadding(pData->pAudioClient, &numFramesPadding);

		// Grab all the available space in the shared buffer.

		numFramesAvailable = pData->BufferSize - numFramesPadding;


		if (iterator + numFramesPadding < (unsigned int)(pData->DataSize / pData->pWinAudioFormat->nBlockAlign))
		{
			hr = pData->pAudioRender->lpVtbl->GetBuffer(pData->pAudioRender, numFramesAvailable, &pBuffer);
			// Fill buffer.
			memcpy(pBuffer, ptr, numFramesAvailable * pData->pWinAudioFormat->nBlockAlign);
			hr = pData->pAudioRender->lpVtbl->ReleaseBuffer(pData->pAudioRender, numFramesAvailable, 0);

			// Move pointer.
			ptr += numFramesAvailable * pData->pWinAudioFormat->nBlockAlign;

			iterator += numFramesAvailable;
		}
		else
			pData->Playing = 0;

		Sleep((DWORD)(hnsActualDuration / pData->REFTIMES_PER_MILLISEC / 2));
	}

	// Stop playing.
	hr = pData->pAudioClient->lpVtbl->Stop(pData->pAudioClient);

	free(pData->pData);
}


void _Print(AudioEngine* This, char* str)
{
	printf(str);
}

void _SetVolume(AudioEngine* This, float Value)
{
	Data* pData = This->pData;
	pData->Volume = Value;
	// Set volume
	float arr[2] = { pData->Volume, pData->Volume };
	pData->pAudioVolume->lpVtbl->SetAllVolumes(pData->pAudioVolume, 2, arr);
}
void _Stop(AudioEngine* This)
{
	((Data*)(This->pData))->Playing = 0;
}

AudioEngine* CreateAudioEngine(BOOL KeepInMemory)
{
	AudioEngine* AE = (AudioEngine*)malloc(sizeof(AudioEngine));
	Data* pData = (Data*)malloc(sizeof(Data));

	AE->Initialize = &_Initialize;
	AE->ReadWav = &_ReadWav;
	AE->UnInitialize = &_UnInitialize;
	AE->SetVolume = &_SetVolume;
	AE->Stop = &_Stop;

	if (KeepInMemory)
	{
		AE->Play = &_PlayKeep;
		pData->KeepInMemory = 1;
	}
	else
	{
		AE->Play = &_PlayRead;
		pData->KeepInMemory = 0;
	}

	pData->Playing = 0;
	pData->Volume = 0.03f;
	pData->REFTIMES_PER_SEC = 10000000;
	pData->REFTIMES_PER_MILLISEC = 10000;
	pData->BufferSize = 0;

	AE->pData = pData;

	return AE;
}
