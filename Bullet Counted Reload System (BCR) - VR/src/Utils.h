#pragma once
#include <time.h>
#include "f4se\PapyrusVM.h"
#include "f4se\PapyrusInterfaces.h"
#include "f4se\PapyrusArgs.h"
#include "f4se\GameRTTI.h"
#include "f4se\GameData.h"
#include <string> 
#include "Globals.h"

template <typename T>
T GetVirtualFunction(void* baseObject, int vtblIndex) {
	uintptr_t* vtbl = reinterpret_cast<uintptr_t**>(baseObject)[0];
	return reinterpret_cast<T>(vtbl[vtblIndex]);
}

template <typename T>
T GetOffset(const void* baseObject, int offset) {
	return *reinterpret_cast<T*>((uintptr_t)baseObject + offset);
}
class VMScript
{
public:
	VMScript(TESForm* form, const char* scriptName = "ScriptObject") {
		if (!scriptName || scriptName[0] == '\0') scriptName = "ScriptObject";
		VirtualMachine* vm = (*g_gameVM)->m_virtualMachine;
		UInt64 handle = vm->GetHandlePolicy()->Create(form->formType, form);
		vm->GetObjectIdentifier(handle, scriptName, 1, &m_identifier, 0);
	}

	~VMScript() {
		if (m_identifier && !m_identifier->DecrementLock())
			m_identifier->Destroy();
	}

	VMIdentifier* m_identifier = nullptr;
};

struct VRModInfo {
	std::string name;
	UInt8 compileIndex = 0xFF;
	UInt16 smallFileCompileIndex = 0xFFFF;
	bool isLight = false;
};

bool VR_LookupLoadedMod(const char* modName, VRModInfo* outInfo = nullptr);

TESForm * GetFormFromIdentifier(const std::string & identifier);


BSFixedString GetDisplayName(ExtraDataList* extraDataList, TESForm * kbaseForm);

//
bool HasKeyword(TESForm* form, BGSKeyword* keyword);

bool InstWEAPHasKeyword(TESObjectWEAP::InstanceData* thisInstance, BGSKeyword* kwdToCheck);

//
BSFixedString GetFormDescription(TESForm * thisForm);


//helper function to get instance data of weapon from extradatalist
TESObjectWEAP::InstanceData* GetInstanceDataFromExtraDataList(ExtraDataList* extraDataList, TESForm* form, bool shouldCreateInstanceData);
//same for armor
TESObjectARMO::InstanceData* GetInstanceDataFromExtraDataListARMO(ExtraDataList* extraDataList);

void logIfNeeded(std::string text);