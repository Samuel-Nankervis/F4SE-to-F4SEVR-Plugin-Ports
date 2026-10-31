#include "Utils.h"

typedef bool(*_IKeywordFormBase_HasKeyword)(IKeywordFormBase* keywordFormBase, BGSKeyword* keyword, UInt32 unk3);

static bool DoesFormMatchMod(TESForm* form, const std::string& modName)
{
	if (!form) return false;
	__try {
		// 1. Check form->unk08 (array of ModInfo* pointers of mods modifying this form)
		if (form->unk08 && form->unk08->entries && form->unk08->size > 0 && form->unk08->size < 100) {
			for (UInt64 i = 0; i < form->unk08->size; ++i) {
				ModInfo* info = form->unk08->entries[i];
				if (info && info->name[0] != '\0') {
					if (_stricmp(info->name, modName.c_str()) == 0) {
						return true;
					}
				}
			}
		}

		// 2. Check virtual GetLastModifiedMod
		ModInfo* lastMod = form->GetLastModifiedMod();
		if (lastMod && lastMod->name[0] != '\0') {
			if (_stricmp(lastMod->name, modName.c_str()) == 0) {
				return true;
			}
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {}
	return false;
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
			outInfo->smallFileCompileIndex = 0xFFFF;
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
		// Strategy 1: Daytripper4 / VR SmallFileLoader collection at TESDataHandler + 0x17B8
		void** pSmallCollection = *reinterpret_cast<void***>(pData + 0x17B8);
		if (pSmallCollection) {
			void** smallFiles = reinterpret_cast<void**>(pSmallCollection[0]);
			UInt32 smallCount = *reinterpret_cast<UInt32*>(reinterpret_cast<char*>(pSmallCollection) + 0x10);
			if (smallFiles && smallCount > 0 && smallCount < 0x1000) {
				for (UInt32 i = 0; i < smallCount; ++i) {
					char* pFile = reinterpret_cast<char*>(smallFiles[i]);
					if (pFile) {
						const char* fname = pFile + 0x70; // ModInfo::name is at offset 0x70
						if (_stricmp(fname, modName) == 0) {
							if (outInfo) {
								outInfo->name = fname;
								outInfo->compileIndex = 0xFE;
								outInfo->smallFileCompileIndex = static_cast<UInt16>(i);
								outInfo->isLight = true;
							}
							_MESSAGE("[VR_LookupLoadedMod] Found light mod '%s' at smallFileIndex 0x%03X via SmallFiles collection",
								fname, i);
							return true;
						}
					}
				}
			}
		}

		// Strategy 2: Check F4SE's loadedMods array (regular full plugins 0x00 to 0xFD)
		const ModInfo* f4seMod = (*g_dataHandler)->LookupLoadedModByName(modName);
		if (f4seMod && f4seMod->modIndex != 0xFF) {
			bool isLight = (f4seMod->modIndex == 0xFE);
			if (outInfo) {
				outInfo->name = f4seMod->name;
				outInfo->compileIndex = f4seMod->modIndex;
				outInfo->smallFileCompileIndex = f4seMod->lightIndex;
				outInfo->isLight = isLight;
			}
			_MESSAGE("[VR_LookupLoadedMod] Found mod '%s' via loadedMods at compileIndex 0x%02X (light: %d, lightIndex: 0x%04X)",
				f4seMod->name, f4seMod->modIndex, isLight ? 1 : 0, f4seMod->lightIndex);
			return true;
		}

		// Strategy 3: Check DataHandler's modInfoList (linked list of all loaded plugins)
		const ModInfo* listMod = (*g_dataHandler)->LookupModByName(modName);
		if (listMod) {
			bool isLight = (listMod->modIndex == 0xFE) || (listMod->lightIndex != 0xFFFF && listMod->lightIndex != 0);
			if (outInfo) {
				outInfo->name = listMod->name;
				outInfo->compileIndex = listMod->modIndex;
				outInfo->smallFileCompileIndex = listMod->lightIndex;
				outInfo->isLight = isLight;
			}
			_MESSAGE("[VR_LookupLoadedMod] Found mod '%s' via modInfoList at compileIndex 0x%02X (light: %d, lightIndex: 0x%04X)",
				listMod->name, listMod->modIndex, isLight ? 1 : 0, listMod->lightIndex);
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
			_MESSAGE("[GetFormFromIdentifier] Resolved '%s|%s' -> FormID 0x%08X (form: %p, name: '%s')",
				modName.c_str(), modForm.c_str(), formID, form,
				(form->formType == kFormType_WEAP) ? static_cast<TESObjectWEAP*>(form)->GetFullName() : "");
			return form;
		}
		_MESSAGE("[GetFormFromIdentifier] VR_LookupLoadedMod found '%s' but LookupFormByID(0x%08X) returned null, proceeding to fallback sweeps...",
			modName.c_str(), formID);
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

	// 3. Fallback FormID sweep:
	// 3a. Sweep light mod space (0x000 to 0x200): 0xFE000000 | (lightIdx << 12) | (rawID & 0xFFF)
	TESForm* candidateLightForm = nullptr;
	for (UInt32 lightIdx = 0; lightIdx < 0x200; ++lightIdx) {
		UInt32 testID = 0xFE000000 | ((lightIdx & 0xFFF) << 12) | (rawID & 0xFFF);
		TESForm* form = LookupFormByID(testID);
		if (form && form->formType == kFormType_WEAP) {
			if (DoesFormMatchMod(form, modName)) {
				_MESSAGE("[GetFormFromIdentifier] Verified light sweep resolved '%s|%s' -> FormID 0x%08X (form: %p, name: '%s')",
					modName.c_str(), modForm.c_str(), testID, form, static_cast<TESObjectWEAP*>(form)->GetFullName());
				return form;
			}
			if (!candidateLightForm) candidateLightForm = form;
		}
	}

	// 3b. Sweep regular mod space (0x01 to 0xFD): (idx << 24) | (rawID & 0x00FFFFFF)
	TESForm* candidateRegularForm = nullptr;
	for (UInt32 idx = 1; idx < 0xFE; ++idx) {
		UInt32 testID = (idx << 24) | (rawID & 0x00FFFFFF);
		TESForm* form = LookupFormByID(testID);
		if (form && form->formType == kFormType_WEAP) {
			if (DoesFormMatchMod(form, modName)) {
				_MESSAGE("[GetFormFromIdentifier] Verified regular sweep resolved '%s|%s' -> FormID 0x%08X (form: %p, name: '%s')",
					modName.c_str(), modForm.c_str(), testID, form, static_cast<TESObjectWEAP*>(form)->GetFullName());
				return form;
			}
			if (!candidateRegularForm) candidateRegularForm = form;
		}
	}

	// If no exact mod name match, but we found a valid WEAP candidate matching the local ID:
	if (candidateLightForm) {
		_MESSAGE("[GetFormFromIdentifier] Light sweep candidate resolved '%s|%s' -> FormID 0x%08X (form: %p, name: '%s')",
			modName.c_str(), modForm.c_str(), candidateLightForm->formID, candidateLightForm,
			static_cast<TESObjectWEAP*>(candidateLightForm)->GetFullName());
		return candidateLightForm;
	}
	if (candidateRegularForm) {
		_MESSAGE("[GetFormFromIdentifier] Regular sweep candidate resolved '%s|%s' -> FormID 0x%08X (form: %p, name: '%s')",
			modName.c_str(), modForm.c_str(), candidateRegularForm->formID, candidateRegularForm,
			static_cast<TESObjectWEAP*>(candidateRegularForm)->GetFullName());
		return candidateRegularForm;
	}

	_MESSAGE("[GetFormFromIdentifier] Failed to resolve '%s|%s'", modName.c_str(), modForm.c_str());
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