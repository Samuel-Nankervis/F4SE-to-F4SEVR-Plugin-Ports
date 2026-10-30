//#ifndef _Plugin_Commands_H
//#define _Plugin_Commands_H
//
//#include "Console.h"
//
//#define USE_OUR_MUTEX
//
//#ifdef USE_OUR_MUTEX
//#include <mutex>
//#define LOCK_MUTEX(a) std::lock_guard<std::recursive_mutex> _gLock(a)
//std::recursive_mutex mtx;
//#endif
//
//std::string Replace(std::string str, std::string first, std::string second)
//{
//	if (first.empty() || str.empty())
//		return str;
//
//	size_t pos = 0;
//	while ((pos = str.find(first, pos)) != std::string::npos)
//	{
//		str.replace(pos, first.length(), second);
//		pos += second.length();
//	}
//	return str;
//}
//
//void i_PrintConsole(StaticFunctionTag * base, BSFixedString text)
//{
//	if (text.data && *text.data)
//	{
//		std::string str = text.data;
//		str = Replace(str, "%", "%%");
//
//#ifdef USE_OUR_MUTEX
//		LOCK_MUTEX(mtx);
//#endif
//
//		while (!str.empty())
//		{
//			int len = str.length();
//			if (len > 1023)
//				len = 1023;
//
//			std::string fh = str.substr(0, len);
//			str = str.substr(len);
//
//			ConsoleFunctions::PrintMessage(fh.c_str());
//		}
//	}
//}
//
//BSFixedString i_ReadConsole(StaticFunctionTag * base)
//{
//	static char c_msg[1024];
//
//#ifdef USE_OUR_MUTEX
//	LOCK_MUTEX(mtx);
//#endif
//
//	int len = ConsoleFunctions::ReadMessage(c_msg);
//
//	if (len <= 0)
//		return BSFixedString();
//
//	return BSFixedString(c_msg);
//}
//
//void i_ExecuteCommand(StaticFunctionTag * base, BSFixedString text)
//{
//	if (text.data && *text.data)
//	{
//#ifdef USE_OUR_MUTEX
//		LOCK_MUTEX(mtx);
//#endif
//
//		ConsoleFunctions::ExecuteCommand(text.data);
//	}
//}
//
//void i_SetSelectedReference(StaticFunctionTag * base, TESObjectREFR * obj)
//{
//#ifdef USE_OUR_MUTEX
//	LOCK_MUTEX(mtx);
//#endif
//
//	ConsoleFunctions::SetSelectedRef(obj);
//}
//
//TESObjectREFR * i_GetSelectedReference(StaticFunctionTag * base)
//{
//	int refHandle = *((int*)0x1B3E6EC);
//	TESObjectREFR * obj = NULL;
//
//	{
//		int invHandle = *((int*)0x1310630);
//
//		if (refHandle != invHandle && refHandle != 0)
//		{
//			TESObjectREFR * refPtr = NULL;
//
//			const int lookupRef = 0x4951F0;
//
//			_asm
//			{
//				pushad
//				pushfd
//				lea eax, refPtr
//				push eax
//				lea eax, refHandle
//				push eax
//				call lookupRef
//				add esp, 8
//				popfd
//				popad
//			}
//
//			if (refPtr != NULL)
//			{
//				obj = refPtr;
//
//				const int decRefFunc = 0x40B110;
//				_asm
//				{
//					pushad
//					pushfd
//					lea ecx, refPtr
//					call decRefFunc
//					popfd
//					popad
//				}
//			}
//		}
//	}
//
//	return obj;
//}
//
//UInt32 i_GetVersion(StaticFunctionTag * base)
//{
//	return CONSOLE_PLUGIN_VERSION;
//}
//
//#endif
