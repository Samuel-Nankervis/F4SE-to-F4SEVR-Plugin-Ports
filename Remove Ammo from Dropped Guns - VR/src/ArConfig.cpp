#include "ArConfig.h"

#include <fstream>
#include <ShlObj_core.h>
#include <Shlwapi.h>

#include "INIReader.h"

static constexpr char DEFAULT_CONFIG_PATH[] = R"(\AmmoRemoverDefaults.ini)";

// VR config paths
static constexpr char CONFIG_PATH_VR[] = R"(\My Games\Fallout4VR\F4SE\AmmoRemover.ini)";
static constexpr char CONFIG_PATH_TRUE_VR[] = R"(\My Games\Fallout4VR\F4SE\TrueAmmoRemover.ini)";
static constexpr char CONFIG_PATH_TRUE2_VR[] = R"(\My Games\Fallout4VR\F4SE\TrueTrueAmmoRemover.ini)";

// Fallback flat config paths
static constexpr char CONFIG_PATH_FLAT[] = R"(\My Games\Fallout4\F4SE\AmmoRemover.ini)";
static constexpr char CONFIG_PATH_TRUE_FLAT[] = R"(\My Games\Fallout4\F4SE\TrueAmmoRemover.ini)";
static constexpr char CONFIG_PATH_TRUE2_FLAT[] = R"(\My Games\Fallout4\F4SE\TrueTrueAmmoRemover.ini)";

static constexpr SInt32 AMMO_PERCENT_MAX = 1000;

ArConfig::InitializeResult ArConfig::Initialize(SInt32& wepAmmoPercent, SInt32& invAmmoPercent, ArLogger::LogLevel& logLevel)
{
	// Default fallback values
	wepAmmoPercent = 100;
	invAmmoPercent = 100;
	logLevel = ArLogger::Message;

	char defaultConfigPath[MAX_PATH];
	const bool hasDefaults = GetLocalPath(defaultConfigPath, MAX_PATH, DEFAULT_CONFIG_PATH)
		&& Load(defaultConfigPath, wepAmmoPercent, invAmmoPercent, logLevel);

	const auto relativeLocalVR = wepAmmoPercent == 0 ? (invAmmoPercent == 0 ? R"(\TrueTrueAmmoRemover.ini)" : R"(\TrueAmmoRemover.ini)") : R"(\AmmoRemover.ini)";
	const auto relativePathVR = wepAmmoPercent == 0 ? (invAmmoPercent == 0 ? CONFIG_PATH_TRUE2_VR : CONFIG_PATH_TRUE_VR) : CONFIG_PATH_VR;
	const auto relativePathFlat = wepAmmoPercent == 0 ? (invAmmoPercent == 0 ? CONFIG_PATH_TRUE2_FLAT : CONFIG_PATH_TRUE_FLAT) : CONFIG_PATH_FLAT;

	char configPath[MAX_PATH];

	// 1. Check local plugin directory (e.g. Data/F4SE/Plugins/AmmoRemover.ini - preferred for MO2 / Vortex)
	if (GetLocalPath(configPath, MAX_PATH, relativeLocalVR) && Exists(configPath) && Load(configPath, wepAmmoPercent, invAmmoPercent, logLevel))
		return ConfigFile;

	// 2. Check if VR INI exists in My Games/Fallout4VR/F4SE/
	if (GetDocumentsPath(configPath, MAX_PATH, relativePathVR) && Exists(configPath) && Load(configPath, wepAmmoPercent, invAmmoPercent, logLevel))
		return ConfigFile;

	// 3. Check if flat INI exists in My Games/Fallout4/F4SE/
	if (GetDocumentsPath(configPath, MAX_PATH, relativePathFlat) && Exists(configPath) && Load(configPath, wepAmmoPercent, invAmmoPercent, logLevel))
		return ConfigFile;

	// 4. Try to copy default INI to VR Documents path if neither exists
	if (hasDefaults && GetDocumentsPath(configPath, MAX_PATH, relativePathVR) && Copy(defaultConfigPath, configPath) && Load(configPath, wepAmmoPercent, invAmmoPercent, logLevel))
		return ConfigFile;

	// 5. Fall back to loading default INI
	if (hasDefaults)
		return Defaults;

	return Failed;
}

bool ArConfig::GetDocumentsPath(char* path, const rsize_t pathSize, const char* relativePath)
{
	const HRESULT error = SHGetFolderPath(nullptr, CSIDL_MYDOCUMENTS | CSIDL_FLAG_CREATE,
		nullptr, SHGFP_TYPE_CURRENT, path);
	if (FAILED(error))
		return false;

	if (strlen(path) + strlen(relativePath) > pathSize)
		return false;

	strcat_s(path, pathSize, relativePath);
	return true;
}

bool ArConfig::Exists(const char* path)
{
	if (FILE* file = fopen(path, "r"))
	{
		fclose(file);
		return true;
	}

	return false;
}

bool ArConfig::Load(const char* path, SInt32& wepAmmoPercent, SInt32& invAmmoPercent, ArLogger::LogLevel& logLevel)
{
	const auto iniReader = INIReader(path);
	if (iniReader.ParseError() < 0)
		return false;

	const auto wepVal = iniReader.GetInteger("Config", "Weapon Ammo %", wepAmmoPercent);
	if (wepVal >= 0 && wepVal <= AMMO_PERCENT_MAX)
		wepAmmoPercent = static_cast<SInt32>(wepVal);

	const auto invVal = iniReader.GetInteger("Config", "Inventory Ammo %", invAmmoPercent);
	if (invVal >= 0 && invVal <= AMMO_PERCENT_MAX)
		invAmmoPercent = static_cast<SInt32>(invVal);

	const auto logVal = iniReader.GetInteger("Config", "Log Level", static_cast<long>(logLevel));
	if (logVal >= ArLogger::Fatal && logVal <= ArLogger::Debug)
		logLevel = static_cast<ArLogger::LogLevel>(logVal);

	return true;
}

bool ArConfig::GetLocalPath(char* path, const rsize_t pathSize, const char* relativePath)
{
	HMODULE hModule;
	if (!GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		reinterpret_cast<LPCSTR>(&GetLocalPath), &hModule)
		|| !GetModuleFileName(hModule, path, MAX_PATH)
		|| !PathRemoveFileSpec(path))
	{
		return false;
	}

	if (strlen(path) + strlen(relativePath) > pathSize)
		return false;

	strcat_s(path, pathSize, relativePath);
	return true;
}

bool ArConfig::Copy(const char* sourcePath, const char* destinationPath)
{
	std::ifstream src(sourcePath, std::ios::binary);
	if (!src.is_open())
		return false;

	// Create directory if it doesn't exist
	char dirPath[MAX_PATH];
	strcpy_s(dirPath, MAX_PATH, destinationPath);
	PathRemoveFileSpec(dirPath);
	SHCreateDirectoryEx(nullptr, dirPath, nullptr);

	std::ofstream dst(destinationPath, std::ios::binary);
	if (!dst.is_open())
		return false;

	dst << src.rdbuf();
	return true;
}
