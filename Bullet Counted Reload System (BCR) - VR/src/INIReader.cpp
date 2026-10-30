#include <map>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include "f4se/GameData.h"
#include "Utils.h"
#include "Globals.h"

void Getinis(std::vector<WIN32_FIND_DATA>* arr) {
	const char* modSettingsDirectory = "Data\\BCR\\*.ini";

	HANDLE hFind;
	WIN32_FIND_DATA data;
	hFind = FindFirstFileA(modSettingsDirectory, &data);
	if (hFind != INVALID_HANDLE_VALUE) {
		do {
			arr->push_back(data);
		} while (FindNextFileA(hFind, &data));
		FindClose(hFind);
	}
}

void HandleIniFiles() {
	_MESSAGE("[HandleIniFiles] Scanning for BCR INI configuration files...");
	std::vector<WIN32_FIND_DATA> modSettingFiles;
	Getinis(&modSettingFiles);
	_MESSAGE("[HandleIniFiles] Found %zu INI file(s) in Data\\BCR\\", modSettingFiles.size());

	for (size_t i = 0; i < modSettingFiles.size(); i++)
	{
		std::string fileName = modSettingFiles[i].cFileName;
		if (fileName.length() < 5) continue;
		std::string fileNameNoSuffix = fileName.substr(0, fileName.size() - 4);

		_MESSAGE("[HandleIniFiles] Processing INI: '%s' (mod: '%s')", fileName.c_str(), fileNameNoSuffix.c_str());

		VRModInfo modInfo;
		bool isLoaded = VR_LookupLoadedMod(fileNameNoSuffix.c_str(), &modInfo);
		if (!isLoaded) {
			_MESSAGE("[HandleIniFiles] Note: VR_LookupLoadedMod did not find '%s', attempting FormID sweep...", fileNameNoSuffix.c_str());
		}

		std::string fullPath = "Data\\BCR\\" + fileName;
		std::ifstream in(fullPath);
		if (!in.is_open()) {
			_MESSAGE("[HandleIniFiles] Failed to open '%s'", fullPath.c_str());
			continue;
		}

		std::string line;
		int weaponsConfigured = 0;
		while (std::getline(in, line)) {
			// Trim trailing whitespace and newlines
			while (!line.empty() && (line.back() == ' ' || line.back() == '\r' || line.back() == '\n' || line.back() == '\t'))
				line.pop_back();
			// Trim leading whitespace
			while (!line.empty() && (line.front() == ' ' || line.front() == '\t'))
				line.erase(line.begin());

			if (line.empty() || line[0] == ';' || line[0] == '#' || line[0] == '[')
				continue;

			auto eqPos = line.find('=');
			if (eqPos == std::string::npos)
				continue;

			std::string key = line.substr(0, eqPos);
			std::string val = line.substr(eqPos + 1);

			// Trim key
			while (!key.empty() && (key.back() == ' ' || key.back() == '\t')) key.pop_back();
			while (!key.empty() && (key.front() == ' ' || key.front() == '\t')) key.erase(key.begin());

			// Trim val
			while (!val.empty() && (val.back() == ' ' || val.back() == '\t')) val.pop_back();
			while (!val.empty() && (val.front() == ' ' || val.front() == '\t')) val.erase(val.begin());

			if (key.find("weaponFormId") != std::string::npos) {
				bool hasPostReload = (key.find("(PostReloadAnim)") != std::string::npos);
				std::string combined = fileNameNoSuffix + "|" + val;
				TESForm* form = GetFormFromIdentifier(combined);
				if (form && form->formType == kFormType_WEAP) {
					TESObjectWEAP* weapForm = static_cast<TESObjectWEAP*>(form);
					ActorValueInfo* targetAvif = hasPostReload ? checkAvif2 : checkAvif;
					weapForm->weapData.skill = targetAvif;
					weaponsConfigured++;
					_MESSAGE("[HandleIniFiles] Configured weapon '%s' (0x%08X) with %s",
						weapForm->GetFullName(), weapForm->formID,
						hasPostReload ? "checkAvif2 (PostReloadAnim)" : "checkAvif");
				}
				else {
					_MESSAGE("[HandleIniFiles] Warning: Could not resolve weapon '%s'", combined.c_str());
				}
			}
		}
		_MESSAGE("[HandleIniFiles] Finished processing '%s': %d weapon(s) configured.", fileName.c_str(), weaponsConfigured);
	}
}