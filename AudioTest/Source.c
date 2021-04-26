#include "..\Audio Engine\Headers\Audio Engine.h"


void cls(HANDLE hConsole)
{
	COORD coordScreen = { 0, 0 };    // home for the cursor 
	DWORD cCharsWritten;
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	DWORD dwConSize;

	// Get the number of character cells in the current buffer. 

	if (!GetConsoleScreenBufferInfo(hConsole, &csbi))
	{
		return;
	}

	dwConSize = csbi.dwSize.X * csbi.dwSize.Y;

	// Fill the entire screen with blanks.

	if (!FillConsoleOutputCharacter(hConsole,        // Handle to console screen buffer 
		(TCHAR)' ',     // Character to write to the buffer
		dwConSize,       // Number of cells to write 
		coordScreen,     // Coordinates of first cell 
		&cCharsWritten))// Receive number of characters written
	{
		return;
	}

	// Get the current text attribute.

	if (!GetConsoleScreenBufferInfo(hConsole, &csbi))
	{
		return;
	}

	// Set the buffer's attributes accordingly.

	if (!FillConsoleOutputAttribute(hConsole,         // Handle to console screen buffer 
		csbi.wAttributes, // Character attributes to use
		dwConSize,        // Number of cells to set attribute 
		coordScreen,      // Coordinates of first cell 
		&cCharsWritten)) // Receive number of characters written
	{
		return;
	}

	// Put the cursor at its home coordinates.

	SetConsoleCursorPosition(hConsole, coordScreen);
}

BOOL CompareWSTR(const wchar_t* str1, const wchar_t* str2)
{
	short it = 0;
	while (str1[it] != *"\r" && str1[it] != *L"\n" && str1[it] != NULL && str1[it] != *L" ")
	{
		if (str1[it] != str2[it])
			return 0;
		it++;
	}
	return 1;
}

typedef struct
{
	AudioEngine* ae;
	HANDLE ConsoleInput, ConsoleOutput;
} HandleMessages;

void ProcessMessages(LPVOID HM)
{
	HandleMessages* msg = HM;
	HANDLE ConsoleInput = msg->ConsoleInput;
	HANDLE ConsoleOutput = msg->ConsoleOutput;
	AudioEngine* ae = msg->ae;

	void* Buffer = malloc(sizeof(wchar_t) * 64);
	DWORD Count = 0;

	while (ReadConsoleW(ConsoleInput, Buffer, 64, &Count, NULL))
	{
		if (CompareWSTR(Buffer, L"Exit"))
		{
			ae->Stop(ae);
			break;
		}
		else if (CompareWSTR(Buffer, L"Stop"))
		{
			ae->Stop(ae);
		}
		else if (CompareWSTR(Buffer, L"Volume"))
		{
			float vol = wcstof((const wchar_t*)(Buffer) + _countof(L"Volume"), NULL);
			ae->SetVolume(ae, vol);
		}
		else if (CompareWSTR((const wchar_t*)(Buffer), L"Clear"))
		{
			cls(ConsoleOutput);
		}
		else if (CompareWSTR((const wchar_t*)(Buffer), L"Help"))
		{
			WriteConsoleW(ConsoleOutput, L"All commands\n", 13, NULL, NULL);
			WriteConsoleW(ConsoleOutput, L"Exit\nStop\nVolume v\nClear\n", 25, NULL, NULL);
		}
		//else if (CompareWSTR((const wchar_t*)(Buffer), L"Pause"))
		//{
		//	ae->Pause();
		//}
		//else if (CompareWSTR((const wchar_t*)(Buffer), L"Play"))
		//{
		//	ae->Play(ae);
		//}
		//else if (CompareWSTR(static_cast<const wchar_t*>(Buffer), L"Repeat"))
		//{
		//	const wchar_t* value = reinterpret_cast<const wchar_t*>(Buffer) + _countof(L"Repeat");
		//	if (CompareWSTR(value, L"on"))
		//		ae->Repeat(true);
		//	else if (CompareWSTR(value, L"off"))
		//		ap->Repeat(false);

		//}
		else
		{
			WriteConsoleW(ConsoleOutput, L"Unknown command\n", 16, NULL, NULL);
			WriteConsoleW(ConsoleOutput, L"Use \"Help\" to see all commands\n", 31, NULL, NULL);
		}

		continue;

	}
	free(Buffer);
}


int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE prevhInst, _In_ LPWSTR pCmdLine, _In_ int CmdShow)
{

	AllocConsole();

	HANDLE ConInput = GetStdHandle(STD_INPUT_HANDLE);
	HANDLE ConOutput = GetStdHandle(STD_OUTPUT_HANDLE);

	SetConsoleTitleW(L"AudioPlayer");
	WriteConsoleW(ConOutput, L"Welcome to audio room\n", 22, NULL, NULL);

l:;
	LPWSTR Path = malloc((DWORD)wcslen(pCmdLine) * 2);
	memset(Path, 0, (DWORD)wcslen(pCmdLine) * 2);
	memcpy(Path, pCmdLine + 1, ((DWORD)wcslen(pCmdLine) - 2) * 2);

	char Attemps = 0;

	LPWSTR Folder = malloc(1024 * 2);

	GetModuleFileNameW(NULL, Folder, 2048);

	wchar_t it = (wchar_t)wcslen(Folder);
	while(1)
	{
		if (Folder[it] != L'\\')
		{
			it--;
			continue;
		}
		memcpy(&Folder[it], L"\\Settings.txt", 28);
		break;
	}


openfile:;
	HANDLE File = CreateFileW(Folder, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if (File == 0xffffffffffffffff)
	{
		File = CreateFileW(Folder, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
		LPWSTR str = L"KeepInMemory: 1";
		WriteFile(File, str, 30, NULL, NULL);
		CloseHandle(File);

		if (Attemps > 2)
			return 1;

		Attemps++;
		goto openfile;
	}

	LPWSTR str = malloc(34);
	memset(str, 0, 34);
	ReadFile(File, str, 32, NULL, NULL);

	BOOL value = _wtoi(&str[wcslen(str) - 1]);

	AudioEngine* ae = CreateAudioEngine(value);

	free(str);

	HandleMessages msg;
	msg.ae = ae;
	msg.ConsoleInput = ConInput;
	msg.ConsoleOutput = ConOutput;

	HANDLE Thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ProcessMessages, &msg, 0, NULL);

	ae->Initialize(ae);
	ae->ReadWav(ae, Path);
	ae->Play(ae);
	ae->UnInitialize(ae);
	free(ae);

	free(Path);
	WaitForSingleObject(Thread, INFINITE);
	goto l;
	FreeConsole();
}
