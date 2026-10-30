#include "f4se/ScaleformValue.h"
#include "f4se/ScaleformMovie.h"
#include "f4se/ScaleformCallbacks.h"
#include "f4se/PapyrusScaleformAdapter.h"

#include "f4se/PapyrusEvents.h"
#include "f4se/PapyrusUtilities.h"
#include "main.h"
#include "f4se/GameData.h"
#include "f4se/GameRTTI.h"
#include "f4se/GameMenus.h"
#include "f4se/GameInput.h"
#include "f4se/InputMap.h"
#include "f4se/ScaleformLoader.h"
#include "HookUtil.h"
#include "ScaleformF4NV.h"
#include "f4se/GameReferences.h"
#include "Globals.h"
#include "PapyrusFunctions.h"
#include "VRHookAPI.h"
#include <shlobj.h>

#include <string>
#include <algorithm>

typedef void (*_PlayerControls_RegisterHandler)(PlayerControls* controls, PlayerInputHandler* handler, bool isHeldStateHandler);
RelocAddr<_PlayerControls_RegisterHandler> PlayerControls_RegisterHandler(0x0FC3490); // Fallout 4 VR 1.2.72.0
RelocPtr <UI*> g_ui(0x05932320); // Fallout 4 VR 1.2.72.0

static OpenVRHookManagerAPI* g_vrHookManager = nullptr;
static bool g_vrHookRegistered = false;

// Configurable settings
bool g_bEnableVRInterrupt = true;
int g_iWeaponHand = 0;              // 0 = Auto-detect, 1 = Force Right, 2 = Force Left
int g_iControllerTypeOverride = 0; // 0 = Auto-detect, 1 = Index, 2 = Oculus, 3 = Vive, 4 = PSVR2, 5 = Frame, 6 = WMR
bool g_bInterruptOnTrigger = true;
bool g_bInterruptOnReloadButton = true;
bool g_bInterruptOnGrip = false;
bool g_bInterruptOnTouchpad = false;
bool g_bInterruptOnAltButton = false;
float g_fTriggerThreshold = 0.50f;
bool g_bVerboseLogging = false;

enum VRControllerModel {
	kVRModel_Unknown = 0,
	kVRModel_Index,       // Valve Index (Knuckles)
	kVRModel_OculusTouch, // Meta / Oculus Touch (Quest 1/2/3/Pro, Rift CV1/S)
	kVRModel_ViveWand,    // HTC Vive Wands
	kVRModel_PSVR2,       // PlayStation VR2 Sense
	kVRModel_SteamFrame,  // Steam Frame
	kVRModel_WMR          // Windows Mixed Reality
};

const char* GetControllerModelName(VRControllerModel model)
{
	switch (model)
	{
	case kVRModel_Index: return "Valve Index (Knuckles)";
	case kVRModel_OculusTouch: return "Oculus / Meta Touch";
	case kVRModel_ViveWand: return "HTC Vive Wand";
	case kVRModel_PSVR2: return "PlayStation VR2 Sense";
	case kVRModel_SteamFrame: return "Steam Frame";
	case kVRModel_WMR: return "Windows Mixed Reality";
	default: return "Generic / Unknown VR Controller";
	}
}

// Debounce state to filter out buttons held from before reload started
static bool s_wasTriggerHeldAtStart = false;
static bool s_wasReloadHeldAtStart = false;

void ResetInterruptDebounce()
{
	s_wasTriggerHeldAtStart = true;
	s_wasReloadHeldAtStart = true;
}

void ClearInterruptDebounce()
{
	s_wasTriggerHeldAtStart = false;
	s_wasReloadHeldAtStart = false;
}

// Fallback direct OpenVR system resolver if FO4VRTools is absent
vr::IVRSystem* GetOpenVRSystemDirect()
{
	static vr::IVRSystem* s_directVR = nullptr;
	if (!s_directVR)
	{
		HMODULE hOpenVR = GetModuleHandleA("openvr_api.dll");
		if (hOpenVR)
		{
			typedef void* (*_VR_GetGenericInterface)(const char* pchInterfaceVersion, vr::EVRInitError* peError);
			auto fnGet = (_VR_GetGenericInterface)GetProcAddress(hOpenVR, "VR_GetGenericInterface");
			if (fnGet)
			{
				vr::EVRInitError err = vr::VRInitError_None;
				s_directVR = (vr::IVRSystem*)fnGet(vr::IVRSystem_Version, &err);
				if (s_directVR) {
					_MESSAGE("[BCR VR] Resolved direct IVRSystem (%p) from openvr_api.dll", s_directVR);
				}
			}
		}
	}
	return s_directVR;
}

vr::IVRSystem* GetActiveVRSystem()
{
	if (g_vrHookManager)
	{
		vr::IVRSystem* sys = g_vrHookManager->GetVRSystem();
		if (sys) return sys;
	}
	return GetOpenVRSystemDirect();
}

VRControllerModel DetectControllerModel(vr::IVRSystem* vrSys, vr::TrackedDeviceIndex_t deviceIndex)
{
	if (!vrSys || deviceIndex == vr::k_unTrackedDeviceIndexInvalid)
		return kVRModel_Unknown;

	vr::TrackedPropertyError propErr = vr::TrackedProp_Success;

	char ctrlBuf[128] = { 0 };
	vrSys->GetStringTrackedDeviceProperty(deviceIndex, vr::Prop_ControllerType_String, ctrlBuf, sizeof(ctrlBuf), &propErr);
	std::string ctrlType = ctrlBuf;
	std::transform(ctrlType.begin(), ctrlType.end(), ctrlType.begin(), ::tolower);

	char modelBuf[128] = { 0 };
	vrSys->GetStringTrackedDeviceProperty(deviceIndex, vr::Prop_ModelNumber_String, modelBuf, sizeof(modelBuf), &propErr);
	std::string modelStr = modelBuf;
	std::transform(modelStr.begin(), modelStr.end(), modelStr.begin(), ::tolower);

	char mfgBuf[128] = { 0 };
	vrSys->GetStringTrackedDeviceProperty(deviceIndex, vr::Prop_ManufacturerName_String, mfgBuf, sizeof(mfgBuf), &propErr);
	std::string mfgStr = mfgBuf;
	std::transform(mfgStr.begin(), mfgStr.end(), mfgStr.begin(), ::tolower);

	std::string combined = ctrlType + " " + modelStr + " " + mfgStr;

	if (combined.find("knuckles") != std::string::npos || combined.find("index") != std::string::npos) {
		return kVRModel_Index;
	}
	if (combined.find("oculus") != std::string::npos || combined.find("quest") != std::string::npos || combined.find("touch") != std::string::npos || combined.find("meta") != std::string::npos || combined.find("miramar") != std::string::npos) {
		return kVRModel_OculusTouch;
	}
	if (combined.find("psvr2") != std::string::npos || combined.find("playstation") != std::string::npos) {
		return kVRModel_PSVR2;
	}
	if (combined.find("frame") != std::string::npos || combined.find("steam_frame") != std::string::npos) {
		return kVRModel_SteamFrame;
	}
	if (combined.find("vive_controller") != std::string::npos || combined.find("vive controller") != std::string::npos || combined.find("vive wand") != std::string::npos) {
		return kVRModel_ViveWand;
	}
	if (combined.find("holographic") != std::string::npos || combined.find("wmr") != std::string::npos || combined.find("reverb") != std::string::npos) {
		return kVRModel_WMR;
	}

	return kVRModel_Unknown;
}

VRControllerModel GetCachedControllerModel(vr::IVRSystem* vrSys, vr::TrackedDeviceIndex_t deviceIndex)
{
	if (g_iControllerTypeOverride >= 1 && g_iControllerTypeOverride <= 6)
	{
		return (VRControllerModel)g_iControllerTypeOverride;
	}

	static VRControllerModel s_cachedModel = kVRModel_Unknown;
	static vr::TrackedDeviceIndex_t s_cachedDevice = vr::k_unTrackedDeviceIndexInvalid;

	if (s_cachedModel == kVRModel_Unknown || s_cachedDevice != deviceIndex)
	{
		s_cachedModel = DetectControllerModel(vrSys, deviceIndex);
		s_cachedDevice = deviceIndex;
		_MESSAGE("[BCR VR] Auto-detected weapon hand controller: %s (device index %u)",
			GetControllerModelName(s_cachedModel), deviceIndex);
	}

	return s_cachedModel;
}

bool IsGameLeftHandedMode()
{
	char prefPath[MAX_PATH];
	// 1. Try MO2 / working directory fallout4prefs.ini
	if (GetPrivateProfileIntA("VR", "bLeftHandedMode", 0, ".\\fallout4prefs.ini") != 0)
		return true;

	// 2. Try My Games\Fallout4VR\Fallout4Prefs.ini
	if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_MYDOCUMENTS, NULL, 0, prefPath)))
	{
		strcat_s(prefPath, "\\My Games\\Fallout4VR\\Fallout4Prefs.ini");
		if (GetPrivateProfileIntA("VR", "bLeftHandedMode", 0, prefPath) != 0)
			return true;
	}

	return false;
}

vr::TrackedDeviceIndex_t GetWeaponHandDeviceIndex(vr::IVRSystem* vrSys)
{
	if (!vrSys) return vr::k_unTrackedDeviceIndexInvalid;

	if (g_iWeaponHand == 1)
	{
		return vrSys->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_RightHand);
	}
	if (g_iWeaponHand == 2)
	{
		return vrSys->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_LeftHand);
	}

	// Auto-detect left-handed mode
	if (IsGameLeftHandedMode())
	{
		return vrSys->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_LeftHand);
	}
	else
	{
		return vrSys->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_RightHand);
	}
}

bool IsTriggerPressed(const vr::VRControllerState_t* pControllerState)
{
	if (!pControllerState || !g_bInterruptOnTrigger)
		return false;

	return (pControllerState->rAxis[1].x > g_fTriggerThreshold) ||
	       ((pControllerState->ulButtonPressed & (1ull << vr::k_EButton_SteamVR_Trigger)) != 0);
}

bool IsReloadButtonPressed(VRControllerModel model, const vr::VRControllerState_t* pControllerState)
{
	if (!pControllerState)
		return false;

	uint64_t pressed = pControllerState->ulButtonPressed;

	switch (model)
	{
	case kVRModel_ViveWand:
		// On Vive wands, reload is typically Grip or Trackpad
		if (g_bInterruptOnReloadButton && (pressed & (1ull << vr::k_EButton_Grip))) return true;
		if (g_bInterruptOnTouchpad && (pressed & (1ull << vr::k_EButton_Axis0))) return true;
		break;

	case kVRModel_Index:
	case kVRModel_OculusTouch:
	case kVRModel_PSVR2:
	case kVRModel_SteamFrame:
	case kVRModel_WMR:
	case kVRModel_Unknown:
	default:
		// On all modern VR controllers, the primary reload button is the A button (bit 7)
		if (g_bInterruptOnReloadButton && (pressed & (1ull << vr::k_EButton_A))) return true;
		break;
	}

	// Configurable extra buttons (disabled by default, customizable in INI):
	if (g_bInterruptOnGrip)
	{
		if ((pressed & (1ull << vr::k_EButton_Grip)) || (pressed & (1ull << 34)) || (pControllerState->rAxis[2].x > 0.5f))
			return true;
	}
	if (g_bInterruptOnTouchpad)
	{
		if (pressed & (1ull << vr::k_EButton_Axis0))
			return true;
	}
	if (g_bInterruptOnAltButton)
	{
		if (pressed & (1ull << vr::k_EButton_ApplicationMenu))
			return true;
	}

	return false;
}

// OpenVR controller state callback invoked by FO4VRTools on every controller poll
bool BCR_OnVRControllerState(vr::TrackedDeviceIndex_t unControllerDeviceIndex, const vr::VRControllerState_t* pControllerState, uint32_t unControllerStateSize, vr::VRControllerState_t* pOutputControllerState)
{
	if (!pControllerState || !g_bEnableVRInterrupt)
		return true;

	if (!reloadStarted || reloadEnd)
		return true;

	vr::IVRSystem* vrSys = GetActiveVRSystem();
	if (!vrSys)
		return true;

	// Determine active weapon hand controller index
	vr::TrackedDeviceIndex_t weaponHandIndex = GetWeaponHandDeviceIndex(vrSys);
	if (weaponHandIndex == vr::k_unTrackedDeviceIndexInvalid || unControllerDeviceIndex != weaponHandIndex)
	{
		// Ignore any inputs from the off-hand controller
		return true;
	}

	// Detect controller model on weapon hand
	VRControllerModel model = GetCachedControllerModel(vrSys, weaponHandIndex);

	bool triggerDown = IsTriggerPressed(pControllerState);
	bool reloadDown = IsReloadButtonPressed(model, pControllerState);

	// Initial press filtering (debounce button held from prior shot/reload start)
	if (s_wasTriggerHeldAtStart) {
		if (!triggerDown) {
			s_wasTriggerHeldAtStart = false;
		}
	}
	if (s_wasReloadHeldAtStart) {
		if (!reloadDown) {
			s_wasReloadHeldAtStart = false;
		}
	}

	bool isInterruptTrigger = triggerDown && !s_wasTriggerHeldAtStart;
	bool isInterruptReload = reloadDown && !s_wasReloadHeldAtStart;

	if (isInterruptTrigger || isInterruptReload)
	{
		if (!stopPressed)
		{
			stopPressed = true;
			_MESSAGE("[BCR VR] Reload interrupt triggered on weapon hand (%s)! Reason: %s (trigger=%.2f, buttons=0x%llX)",
				GetControllerModelName(model),
				isInterruptTrigger ? "FIRE / TRIGGER" : "RELOAD BUTTON",
				pControllerState->rAxis[1].x,
				pControllerState->ulButtonPressed);
		}
	}

	return true;
}

void LoadBCRSettings()
{
	static bool s_loaded = false;
	if (s_loaded) return;
	s_loaded = true;

	const char* iniPath = "Data\\F4SE\\Plugins\\BulletCountedReload.ini";

	DWORD attr = GetFileAttributesA(iniPath);
	if (attr == INVALID_FILE_ATTRIBUTES)
	{
		FILE* f = nullptr;
		fopen_s(&f, iniPath, "w");
		if (f)
		{
			fputs("; Bullet Counted Reload System (BCR) - VR Configuration\n\n"
				  "[VR]\n"
				  "; Enable VR reload interruption\n"
				  "bEnableVRInterrupt = 1\n\n"
				  "; Weapon Hand: 0 = Auto-detect (matches Fallout 4 VR setting), 1 = Force Right Hand, 2 = Force Left Hand\n"
				  "iWeaponHand = 0\n\n"
				  "; Controller Type: 0 = Auto-detect, 1 = Index, 2 = Oculus/Quest, 3 = Vive Wand, 4 = PSVR2, 5 = Steam Frame, 6 = WMR\n"
				  "iControllerType = 0\n\n"
				  "; Weapon hand buttons that interrupt reload (1 = enabled, 0 = disabled)\n"
				  "; Off-hand buttons are always ignored.\n"
				  "bInterruptOnTrigger = 1\n"
				  "bInterruptOnReloadButton = 1\n"
				  "bInterruptOnGrip = 0\n"
				  "bInterruptOnTouchpad = 0\n"
				  "bInterruptOnAltButton = 0\n\n"
				  "; Analog trigger sensitivity threshold (0.1 to 0.9)\n"
				  "fTriggerThreshold = 0.50\n", f);
			fclose(f);
			_MESSAGE("[BCR VR] Created default configuration INI at '%s'", iniPath);
		}
	}

	g_bEnableVRInterrupt = GetPrivateProfileIntA("VR", "bEnableVRInterrupt", 1, iniPath) != 0;
	g_iWeaponHand = GetPrivateProfileIntA("VR", "iWeaponHand", 0, iniPath);
	g_iControllerTypeOverride = GetPrivateProfileIntA("VR", "iControllerType", 0, iniPath);
	g_bInterruptOnTrigger = GetPrivateProfileIntA("VR", "bInterruptOnTrigger", 1, iniPath) != 0;
	g_bInterruptOnReloadButton = GetPrivateProfileIntA("VR", "bInterruptOnReloadButton", 1, iniPath) != 0;
	g_bInterruptOnGrip = GetPrivateProfileIntA("VR", "bInterruptOnGrip", 0, iniPath) != 0;
	g_bInterruptOnTouchpad = GetPrivateProfileIntA("VR", "bInterruptOnTouchpad", 0, iniPath) != 0;
	g_bInterruptOnAltButton = GetPrivateProfileIntA("VR", "bInterruptOnAltButton", 0, iniPath) != 0;
	g_bVerboseLogging = GetPrivateProfileIntA("VR", "bVerboseLogging", 0, iniPath) != 0;

	char threshBuf[32] = { 0 };
	GetPrivateProfileStringA("VR", "fTriggerThreshold", "0.50", threshBuf, sizeof(threshBuf), iniPath);
	g_fTriggerThreshold = (float)atof(threshBuf);
	if (g_fTriggerThreshold <= 0.05f || g_fTriggerThreshold > 0.95f) {
		g_fTriggerThreshold = 0.50f;
	}

	_MESSAGE("[BCR VR] Loaded settings: enabled=%d, handMode=%d, ctrlOverride=%d, trigger=%d (thresh=%.2f), reloadBtn=%d, grip=%d, touchpad=%d, alt=%d, verboseLog=%d",
		g_bEnableVRInterrupt, g_iWeaponHand, g_iControllerTypeOverride,
		g_bInterruptOnTrigger, g_fTriggerThreshold, g_bInterruptOnReloadButton,
		g_bInterruptOnGrip, g_bInterruptOnTouchpad, g_bInterruptOnAltButton, g_bVerboseLogging);
}

void InitVRInput()
{
	LoadBCRSettings();

	if (g_vrHookRegistered)
		return;

	g_vrHookManager = RequestOpenVRHookManagerObject();
	if (g_vrHookManager)
	{
		g_vrHookManager->RegisterControllerStateCB(&BCR_OnVRControllerState);
		g_vrHookRegistered = true;
		_MESSAGE("[BCR VR] Successfully registered OpenVR controller state callback with FO4VRTools!");
	}
	else
	{
		_MESSAGE("[BCR VR] FO4VRTools not detected or GetVRHookManager failed; fallback direct OpenVR polling active.");
	}
}

void HandleButtonEvent(ButtonEvent * inputEvent)
{
	if (!inputEvent)
		return;

	// Check if a reload is currently in progress
	if (stopPressed || !reloadStarted || reloadEnd)
		return;

	// Check if button is pressed
	if (inputEvent->isDown != 1.0f)
		return;

	BSFixedString* pControl = inputEvent->GetControlID();
	const char* controlStr = (pControl && pControl->c_str()) ? pControl->c_str() : "";

	bool isInterrupt = false;

	// Check control action name
	if (!_strcmpi(controlStr, "PrimaryAttack") ||
	    !_strcmpi(controlStr, "ReadyWeapon") ||
	    !_strcmpi(controlStr, "Attack") ||
	    !_strcmpi(controlStr, "Reload"))
	{
		isInterrupt = true;
	}

	// Fallback check on hardware masks/device types:
	// Mouse: LMB is keyMask 0
	if (inputEvent->deviceType == InputEvent::kDeviceType_Mouse && inputEvent->keyMask == 0) {
		isInterrupt = true;
	}
	// Keyboard: R key is scancode 0x13 or VK 0x52
	if (inputEvent->deviceType == InputEvent::kDeviceType_Keyboard && (inputEvent->keyMask == 0x13 || inputEvent->keyMask == 0x52)) {
		isInterrupt = true;
	}
	// Gamepad: RT (0x000A) or X button (0x4000)
	if (inputEvent->deviceType == InputEvent::kDeviceType_Gamepad && (inputEvent->keyMask == 0x000A || inputEvent->keyMask == 0x4000)) {
		isInterrupt = true;
	}

	if (isInterrupt)
	{
		stopPressed = true;
		_MESSAGE("[BCR] Reload interrupt button pressed via input event! (control: %s, device: %u, mask: 0x%X)",
			controlStr, inputEvent->deviceType, inputEvent->keyMask);
	}
}

class F4SEInputHandler2 : public PlayerInputHandler
{
public:
	F4SEInputHandler2() : PlayerInputHandler()
	{
		enabled = true;
	}

	virtual ~F4SEInputHandler2() override = default;

	// Slot 01 (0x08): IsEnabled / ShouldHandleEvent
	virtual bool IsEnabled(InputEvent* inputEvent = nullptr) override
	{
		return enabled;
	}

	// Slot 02 (0x10): KinectEvent
	virtual void OnKinectEvent(KinectEvent* inputEvent) override { }

	// Slot 03 (0x18): DeviceConnectEvent
	virtual void OnDeviceConnectEvent(DeviceConnectEvent* inputEvent) override { }

	// Slot 04 (0x20): ThumbstickEvent
	virtual void OnThumbstickEvent(ThumbstickEvent* inputEvent) override { }

	// Slot 05 (0x28): CursorMoveEvent
	virtual void OnCursorMoveEvent(CursorMoveEvent* inputEvent) override { }

	// Slot 06 (0x30): MouseMoveEvent
	virtual void OnMouseMoveEvent(MouseMoveEvent* inputEvent) override { }

	// Slot 07 (0x38): CharacterEvent
	virtual void OnCharacterEvent(CharacterEvent* inputEvent) override { }

	// Slot 08 (0x40): Desktop F4SE OnButtonEvent
	virtual void OnButtonEvent(ButtonEvent* inputEvent) override
	{
		ProcessButton(inputEvent);
	}

	// Slot 09 (0x48): F4SE Unk_09
	virtual void Unk_09() override { }

	// Slot 10 (0x50): F4SE Unk_0A
	virtual void Unk_0A(void* unk1) override { }

	// Slot 11 (0x58): Fallout 4 VR ButtonEvent handler (matches JumpHandler & SprintHandler at 0x58 in Fallout 4 VR)
	virtual void OnVRButtonEvent(const ButtonEvent* inputEvent)
	{
		ProcessButton(const_cast<ButtonEvent*>(inputEvent));
	}

	// Slot 12 (0x60): Fallout 4 VR PlayerInputHandler::PerFrameUpdate (matches RVA 0x0FC207D call [rax+0x60])
	virtual void PerFrameUpdate()
	{
		// If FO4VRTools is not hooked, poll OpenVR controllers directly per frame
		if (!g_vrHookRegistered && reloadStarted && !reloadEnd)
		{
			vr::IVRSystem* vrSys = GetOpenVRSystemDirect();
			if (vrSys)
			{
				vr::TrackedDeviceIndex_t weaponHand = GetWeaponHandDeviceIndex(vrSys);
				if (weaponHand != vr::k_unTrackedDeviceIndexInvalid)
				{
					vr::VRControllerState_t st = { 0 };
					if (vrSys->GetControllerState(weaponHand, &st, sizeof(st)))
					{
						BCR_OnVRControllerState(weaponHand, &st, sizeof(st), nullptr);
					}
				}
			}
		}
	}

	// Extra safety slots 13-19 (0x68 - 0x98) to ensure any future/extended vtable calls never read out of bounds
	virtual void VtableSlot13(void* unk = nullptr) { }
	virtual void VtableSlot14(void* unk = nullptr) { }
	virtual void VtableSlot15(void* unk = nullptr) { }
	virtual void VtableSlot16(void* unk = nullptr) { }
	virtual void VtableSlot17(void* unk = nullptr) { }
	virtual void VtableSlot18(void* unk = nullptr) { }
	virtual void VtableSlot19(void* unk = nullptr) { }

private:
	void ProcessButton(ButtonEvent* inputEvent)
	{
		if (!inputEvent)
			return;

		if (g_ui && *g_ui && (*g_ui)->numPauseGame)
		{
			return;
		}

		HandleButtonEvent(inputEvent);
	}
};

F4SEInputHandler2 ehandler;

void ReceiveKeys()
{
	static bool s_registered = false;
	if (s_registered)
		return;

	LoadBCRSettings();

	ehandler.enabled = true;
	if (g_playerControls && *g_playerControls)
	{
		ehandler.unk10 = reinterpret_cast<UInt64>(reinterpret_cast<char*>(*g_playerControls) + 0x44);
		ehandler.unk18 = 0;

		PlayerInputHandler* pHandler = &ehandler;
		PlayerControls_RegisterHandler(*g_playerControls, pHandler, false);
		s_registered = true;
		_MESSAGE("Registered for input events via PlayerControls::RegisterHandler.");
	}
	else
	{
		_MESSAGE("Warning: g_playerControls is null in ReceiveKeys.");
	}

	InitVRInput();
}
