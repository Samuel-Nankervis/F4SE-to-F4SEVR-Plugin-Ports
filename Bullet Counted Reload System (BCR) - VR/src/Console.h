//
//#ifndef _Plugin_Console_H
//#define _Plugin_Console_H
//
//#include "f4se/GameMenus.h"
//
//#include <time.h>
//#include <fstream>
//
//#define CONSOLE_PLUGIN_VERSION 3
//
//// Added this function here because otherwise we need to include a lot of CPP files that aren't really needed.
//// Also I changed a bit to increase ref count while we use console.
//IMenu * UI::GetMenu(BSFixedString * menuName)
//{
//	if (!menuName->data)
//		return NULL;
//
//	MenuTableItem * item = menuTable.Find(menuName);
//
//	if (!item)
//		return NULL;
//
//	IMenu * menu = item->menuInstance;
//	if (!menu)
//		return NULL;
//
//	const int incRef = 0x93F050;
//	_asm
//	{
//		pushad
//		pushfd
//		mov ecx, menu
//		call incRef
//		popfd
//		popad
//	}
//
//	return menu;
//}
//
//// Structure to get and keep console pointer until we don't need it anymore.
//struct ConsoleHolder
//{
//	ConsoleHolder()
//	{
//		static BSFixedString * consoleStr = NULL;
//		if (consoleStr == NULL)
//			consoleStr = new BSFixedString("Console");
//
//		_cptr = (int)ISingleton<*int>::GetSingleton()->GetMenu(consoleStr);
//	}
//
//	~ConsoleHolder()
//	{
//		if (_cptr != 0)
//		{
//			const int decRef = 0x9241A0;
//			int lPtr = _cptr;
//			_cptr = 0;
//			_asm
//			{
//				pushad
//				pushfd
//				mov ecx, lPtr
//				call decRef
//				popfd
//				popad
//			}
//		}
//	}
//
//	int GetPointer() { return _cptr; }
//
//private:
//	int _cptr;
//};
//
//struct ConsoleFunctions
//{
//	// Set console selected reference or clear it if NULL.
//	static void SetSelectedRef(TESObjectREFR * obj)
//	{
//		int refHandle = 0;
//		if (obj != NULL)
//		{
//			const int getRefHandle = 0x6BD6C0;
//			_asm
//			{
//				pushad
//				pushfd
//				mov ecx, obj
//				lea eax, refHandle
//				push eax
//				call getRefHandle
//				popfd
//				popad
//			}
//		}
//
//		// Use PickRefById.
//		if (refHandle != 0)
//		{
//			const int setSelectedRef = 0x848680;
//			_asm
//			{
//				pushad
//				pushfd
//				mov eax, refHandle
//				push eax
//				call setSelectedRef
//				add esp, 4
//				popfd
//				popad
//			}
//		}
//		else
//		{
//			// Clearing reference isn't allowed through PickRefById so we must directly set it.
//			ConsoleHolder con;
//			int conPtr = con.GetPointer();
//			if (conPtr == 0)
//			{
//				WriteDebug("Failed to get console pointer in set reference!");
//				return;
//			}
//
//			const int setRefDirect = 0x847CA0;
//
//			_asm
//			{
//				pushad
//				pushfd
//				mov ecx, conPtr
//				push 0
//				call setRefDirect
//				popfd
//				popad
//			}
//		}
//	}
//
//	// Execute command in console.
//	static void ExecuteCommand(const char * text)
//	{
//		ConsoleHolder con;
//
//		int ptr = con.GetPointer();
//		if (ptr == 0)
//		{
//			WriteDebug("Failed to get console pointer!");
//			return;
//		}
//
//		int * unkStruct = (int*)malloc(64);
//		memset(unkStruct, 0, 64);
//
//		unkStruct[4] = ptr;
//		unkStruct[6] = (int)(&unkStruct[7]); // Lazy, there's actually another structure here
//											 //unkStruct[8] = 0; // If this is 1 << 6 then [9] is char ** instead of char *
//		unkStruct[9] = (int)text;
//
//		const int runScript = 0x847080;
//		_asm
//		{
//			pushad
//			pushfd
//			mov eax, unkStruct
//			push eax
//			call runScript
//			add esp, 4
//			popfd
//			popad
//		}
//
//		free(unkStruct);
//	}
//
//	static int ReadMessage(char * text)
//	{
//		memset(text, 0, 1020);
//
//		int consoleManager = *((int*)0x1B10B58);
//		if (consoleManager == 0)
//			return 0;
//
//		memcpy(text, (void*)(consoleManager + 1), 1020);
//		return strlen(text);
//	}
//
//	// Print message to console.
//	static void PrintMessage(const char * text)
//	{
//		int consoleManager = *((int*)0x1B10B58);
//		if (consoleManager == 0)
//			return;
//
//		va_list args2;
//		const int printConsole = 0x8486F0;
//		_asm
//		{
//			mov ecx, consoleManager
//			mov eax, args2
//			push eax
//			mov eax, text
//			push eax
//			call printConsole
//		}
//	}
//
//	// Write to debug log if user has created it.
//	static void WriteDebug(const char * fmt, ...)
//	{
//		const char * FileName = "ConsolePlugin.txt";
//
//		{
//			std::ifstream exists(FileName);
//			if (!exists.is_open())
//				return;
//
//			exists.close();
//		}
//
//		std::ofstream file;
//		file.open(FileName, std::ios::out | std::ios::app);
//
//		if (!file.is_open())
//			return;
//
//		{
//			time_t rawtime;
//			struct tm timeinfo;
//			char buffer[80];
//
//			time(&rawtime);
//			errno_t err = localtime_s(&timeinfo, &rawtime);
//
//			if (err == 0)
//			{
//				strftime(buffer, 80, "%d %b %H:%M:%S ", &timeinfo);
//				file << buffer;
//			}
//		}
//
//		char writeBuf[512];
//
//		va_list	argl;
//		va_start(argl, fmt);
//		vsprintf_s(writeBuf, sizeof(writeBuf), fmt, argl);
//		file << writeBuf;
//		va_end(argl);
//
//		file << "\r\n";
//
//		file.close();
//	}
//};
//
//#endif
