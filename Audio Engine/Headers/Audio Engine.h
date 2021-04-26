#pragma once
#ifndef AUDIOENGINE_HEADER
#define AUDIOENGINE_HEADER

#ifdef DllExport
#define DLL __declspec(dllexport)
#else
#define DLL __declspec(dllimport)
#endif 

#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <audiopolicy.h>


typedef struct AudioEngine
{
	// Initialize all data.
	BOOL (*Initialize)(void* This);
	// Uninitialize data.
	BOOL (*UnInitialize)(void* This);
	// Read wav file.
	void (*ReadWav)(void* This, LPCWSTR Path);
	// Set volume.
	void (*SetVolume)(void* This, float Value);
	// Start play.
	void (*Play)(void* This);
	// Stop play.
	void (*Stop)(void* This);
	// Print info in console.
	void (*Print)(void* This, char* Str);

	void* pData;
} AudioEngine;

DLL AudioEngine* CreateAudioEngine(BOOL KeepInMemory);


#endif