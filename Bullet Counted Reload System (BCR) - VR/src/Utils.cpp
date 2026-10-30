#include "Utils.h"

typedef bool(*_IKeywordFormBase_HasKeyword)(IKeywordFormBase* keywordFormBase, BGSKeyword* keyword, UInt32 unk3);

static uintptr_t s_modListOffset = 0;
static int s_nameOffset = 0x70;
static bool s_offsetSearched = false;

uintptr_t GetModListOffset(char* pData)
{
	if (s_offsetSearched) return s_modListOffset;
	s_offsetSearched = true;

	_MESSAGE("[GetModListOffset] Scanning TESDataHandler (%p) for compiled file collection...", pData);

	// Scan candidate BSTArray offsets (entries pointer at +off, count at +off+8)
	for (uintptr_t off = 0x0500; off < 0x1500; off += 8) {
		__try {
			void** candidateArray = *reinterpret_cast<void***>(pData + off);
			UInt32 candidateCount = *reinterpret_cast<UInt32*>(pData + off + 8);

			if (candidateArray && candidateCount >= 1 && candidateCount < 500) {
				char* firstFile = reinterpret_cast<char*>(candidateArray[0]);
				if (firstFile) {
					static const int kNameOffsets[] = { 0x70, 0x58, 0x48, 0x6C, 0x50, 0x68 };
					for (int noff : kNameOffsets) {
						const char* candidateName = firstFile + noff;
						if (_stricmp(candidateName, "Fallout4.esm") == 0) {
							s_modListOffset = off;
							s_nameOffset = noff;
							_MESSAGE("[GetModListOffset] Discovered compiled file array at TESDataHandler + 0x%04X (count: %u, nameOffset: 0x%02X)",
								(UInt32)off, candidateCount, noff);
							return off;
						}
					}
				}
			}
		}
		__except (EXCEPTION_EXECUTE_HANDLER) {}
	}

	_MESSAGE("[GetModListOffset] Warning: Pointer array scan did not find Fallout4.esm, scanning linked list nodes...");
	for (uintptr_t off = 0x0500; off < 0x1500; off += 8) {
		__try {
			struct SimpleNode { void* item; SimpleNode* next; };
			SimpleNode* node = *reinterpret_cast<SimpleNode**>(pData + off);
			if (node && node->item) {
				char* firstFile = reinterpret_cast<char*>(node->item);
				static const int kNameOffsets[] = { 0x70, 0x58, 0x48, 0x6C };
				for (int noff : kNameOffsets) {
					if (_stricmp(firstFile + noff, "Fallout4.esm") == 0) {
						_MESSAGE("[GetModListOffset] Found mod BSSimpleList at TESDataHandler + 0x%04X (nameOffset: 0x%02X)", (UInt32)off, noff);
						s_modListOffset = off;
						s_nameOffset = noff;
						return off;
					}
				}
			}
		}
		__except (EXCEPTION_EXECUTE_HANDLER) {}
	}

	_MESSAGE("[GetModListOffset] Failed to locate mod list in TESDataHandler.");
	return 0;
}

bool VR_LookupLoadedMod(const char* modName, VRModInfo* outInfo)
{
	if (!modName || !*modName)
		return false;

	// Fast check for Fallout4.esm (always compileIndex 0)
	if (_stricmp(modName, "Fallout4.esm") == 0) {
		if (outInfo) {
			outInfo->name = "Fallout4.esm";
			outInfo->compileIndex = 0;
			outInfo->smallFileCompileIndex = 0;
			outInfo->isLight = false;
		}
		return true;
	}

	if (!g_dataHandler || !*g_dataHandler) {
		_MESSAGE("[VR_LookupLoadedMod] g_dataHandler is null!");
		return false;
	}

	char* pData = reinterpret_cast<char*>(*g_dataHandler);

	__try {
		uintptr_t arrOffset = GetModListOffset(pData);
		if (arrOffset != 0) {
			void** files = *reinterpret_cast<void***>(pData + arrOffset);
			UInt32 count = *reinterpret_cast<UInt32*>(pData + arrOffset + 8);

			if (files && count > 0 && count < 1000) {
				for (UInt32 i = 0; i < count; ++i) {
					char* pFile = reinterpret_cast<char*>(files[i]);
					if (!pFile) continue;

					const char* fname = pFile + s_nameOffset;
					UInt8 cIdx = *reinterpret_cast<UInt8*>(pFile + 0x370);
					UInt16 sIdx = *reinterpret_cast<UInt16*>(pFile + 0x372);
					UInt32 flags = *reinterpret_cast<UInt32*>(pFile + 0x334);
					bool isLight = (flags & (1 << 9)) != 0;

					if (_stricmp(fname, modName) == 0) {
						if (outInfo) {
							outInfo->name = fname;
							outInfo->compileIndex = cIdx;
							outInfo->smallFileCompileIndex = sIdx;
							outInfo->isLight = isLight;
						}
						_MESSAGE("[VR_LookupLoadedMod] Found mod '%s' at compileIndex 0x%02X (light: %d)",
							fname, cIdx, isLight ? 1 : 0);
						return true;
					}
				}
			}
		}

		// Fallback check on F4SE's modList at 0xFB0
		const ModInfo* f4seMod = (*g_dataHandler)->LookupLoadedModByName(modName);
		if (f4seMod && f4seMod->modIndex != 0xFF) {
			if (outInfo) {
				outInfo->name = f4seMod->name;
				outInfo->compileIndex = f4seMod->modIndex;
				outInfo->smallFileCompileIndex = 0xFFFF;
				outInfo->isLight = false;
			}
			_MESSAGE("[VR_LookupLoadedMod] Found mod '%s' via F4SE modList at compileIndex 0x%02X",
				f4seMod->name, f4seMod->modIndex);
			return true;
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		_MESSAGE("[VR_LookupLoadedMod] Exception encountered while searching for mod '%s'", modName);
	}

	_MESSAGE("[VR_LookupLoadedMod] Mod '%s' was NOT found in active load order", modName);
	return false;
}

TESForm * GetFormFromIdentifier(const std::string & identifier)
{
	auto delimiter = identifier.find('|');
	if (delimiter == std::string::npos)
		return nullptr;

	std::string modName = identifier.substr(0, delimiter);
	std::string modForm = identifier.substr(delimiter + 1);

	// Trim whitespace and \r\n from modName
	while (!modName.empty() && (modName.back() == ' ' || modName.back() == '\r' || modName.back() == '\n' || modName.back() == '\t'))
		modName.pop_back();
	while (!modName.empty() && (modName.front() == ' ' || modName.front() == '\t'))
		modName.erase(modName.begin());

	// Trim whitespace and \r\n from modForm
	while (!modForm.empty() && (modForm.back() == ' ' || modForm.back() == '\r' || modForm.back() == '\n' || modForm.back() == '\t'))
		modForm.pop_back();
	while (!modForm.empty() && (modForm.front() == ' ' || modForm.front() == '\t'))
		modForm.erase(modForm.begin());

	if (modName.empty() || modForm.empty())
		return nullptr;

	UInt32 rawID = 0;
	try {
		rawID = std::stoul(modForm, nullptr, 16);
	}
	catch (...) {
		_MESSAGE("[GetFormFromIdentifier] Invalid hex form ID: '%s'", modForm.c_str());
		return nullptr;
	}

	// 1. Try lookup via VR_LookupLoadedMod
	VRModInfo modInfo;
	if (VR_LookupLoadedMod(modName.c_str(), &modInfo)) {
		UInt32 formID = 0;
		if (modInfo.isLight) {
			formID = (0xFE << 24) | ((modInfo.smallFileCompileIndex & 0xFFF) << 12) | (rawID & 0xFFF);
		} else {
			formID = (static_cast<UInt32>(modInfo.compileIndex) << 24) | (rawID & 0x00FFFFFF);
		}
		TESForm* form = LookupFormByID(formID);
		if (form) {
			_MESSAGE("[GetFormFromIdentifier] Resolved '%s|%s' -> FormID 0x%08X (form: %p)",
				modName.c_str(), modForm.c_str(), formID, form);
			return form;
		}
	}

	// 2. Direct fallback for Fallout4.esm
	if (_stricmp(modName.c_str(), "Fallout4.esm") == 0) {
		UInt32 formID = rawID & 0x00FFFFFF;
		TESForm* form = LookupFormByID(formID);
		if (form) {
			_MESSAGE("[GetFormFromIdentifier] Fallback Fallout4.esm '%s' -> FormID 0x%08X (form: %p)",
				modForm.c_str(), formID, form);
			return form;
		}
	}

	// 3. Fallback FormID sweep across active load order indices (0x01 to 0x20)
	for (UInt32 idx = 0; idx < 0x20; ++idx) {
		UInt32 testID = (idx << 24) | (rawID & 0x00FFFFFF);
		TESForm* form = LookupFormByID(testID);
		if (form && form->formType == kFormType_WEAP) {
			_MESSAGE("[GetFormFromIdentifier] FormID sweep resolved '%s|%s' -> FormID 0x%08X (form: %p, name: '%s')",
				modName.c_str(), modForm.c_str(), testID, form, static_cast<TESObjectWEAP*>(form)->GetFullName());
			return form;
		}
	}

	return nullptr;
}

BSFixedString GetDisplayName(ExtraDataList* extraDataList, TESForm * kbaseForm)
{
	TESForm * baseForm = kbaseForm;

	if (baseForm)
	{
		if (extraDataList)
		{
			BSExtraData * extraData = extraDataList->GetByType(ExtraDataType::kExtraData_TextDisplayData);
			if (extraData)
			{
				ExtraTextDisplayData * displayText = DYNAMIC_CAST(extraData, BSExtraData, ExtraTextDisplayData);
				if (displayText)
				{
					return *CALL_MEMBER_FN(displayText, GetReferenceName)(baseForm);
				}
			}
		}

		TESFullName* pFullName = DYNAMIC_CAST(baseForm, TESForm, TESFullName);
		if (pFullName)
			return pFullName->name;
	}

	return BSFixedString();
}

//
bool HasKeyword(TESForm* form, BGSKeyword* keyword) {
	IKeywordFormBase* keywordFormBase = DYNAMIC_CAST(form, TESForm, IKeywordFormBase);
	if (keywordFormBase) {
		auto HasKeyword_Internal = GetVirtualFunction<_IKeywordFormBase_HasKeyword>(keywordFormBase, 1);
		if (HasKeyword_Internal(keywordFormBase, keyword, 0)) {
			return true;
		}
	}
	return false;
}

bool InstWEAPHasKeyword(TESObjectWEAP::InstanceData* thisInstance, BGSKeyword* kwdToCheck) {
	BGSKeywordForm * keywordForm = nullptr;

	keywordForm = thisInstance->keywords;
	if (!keywordForm)
		return false;

	for (UInt32 i = 0; i < keywordForm->numKeywords; i++)
	{
		BGSKeyword* curr = keywordForm->keywords[i];
		if (curr == kwdToCheck) {
			return true;
		}
	}

	return false;
}

//
BSFixedString GetFormDescription(TESForm * thisForm)
{
	if (!thisForm)
		return BSFixedString();

	TESDescription * pDescription = DYNAMIC_CAST(thisForm, TESForm, TESDescription);
	if (pDescription) {
		BSString str;
		CALL_MEMBER_FN(pDescription, Get)(&str, nullptr);
		return str.Get();
	}

	return BSFixedString();
}


//helper function to get instance data of weapon from extradatalist
TESObjectWEAP::InstanceData* GetInstanceDataFromExtraDataList(ExtraDataList* extraDataList, TESForm* form, bool shouldCreateInstanceData) {
	TESObjectWEAP::InstanceData* currweapInst = nullptr;

	TBO_InstanceData* neededInst = nullptr;
	if (extraDataList && extraDataList->m_data && extraDataList->m_refCount && extraDataList->m_presence) {
		BSExtraData * extraData = extraDataList->GetByType(ExtraDataType::kExtraData_InstanceData);
		if (extraData) {
			ExtraInstanceData * objectModData = DYNAMIC_CAST(extraData, BSExtraData, ExtraInstanceData);
			if (objectModData)
				neededInst = objectModData->instanceData;
		}
		else {
			if (form && shouldCreateInstanceData) {
				TESBoundObject * boundObject = DYNAMIC_CAST(form, TESForm, TESBoundObject);
				TBO_InstanceData * instanceData = nullptr;
				if (boundObject) {
					instanceData = boundObject->CloneInstanceData(nullptr);
					if (instanceData) {
						ExtraInstanceData * objectModData = ExtraInstanceData::Create(form, instanceData);
						if (objectModData) {
							extraDataList->Add(ExtraDataType::kExtraData_InstanceData, objectModData);
							neededInst = instanceData;
						}
					}
				}
			}
		}
	}
	currweapInst = (TESObjectWEAP::InstanceData*)Runtime_DynamicCast(neededInst, RTTI_TBO_InstanceData, RTTI_TESObjectWEAP__InstanceData);
	return currweapInst;
}
//same for armor
TESObjectARMO::InstanceData* GetInstanceDataFromExtraDataListARMO(ExtraDataList* extraDataList) {
	TESObjectARMO::InstanceData* currweapInst = nullptr;

	TBO_InstanceData* neededInst = nullptr;
	if (extraDataList) {
		BSExtraData * extraData = extraDataList->GetByType(ExtraDataType::kExtraData_InstanceData);
		if (extraData) {
			ExtraInstanceData * objectModData = DYNAMIC_CAST(extraData, BSExtraData, ExtraInstanceData);
			if (objectModData)
				neededInst = objectModData->instanceData;
		}
		else {
			//_MESSAGE("Item dont have instance data");
		}
	}
	currweapInst = (TESObjectARMO::InstanceData*)Runtime_DynamicCast(neededInst, RTTI_TBO_InstanceData, RTTI_TESObjectARMO__InstanceData);
	return currweapInst;
}

void logIfNeeded(std::string text) {
	if (logEnabled) {
		_MESSAGE(text.c_str());
	}
}