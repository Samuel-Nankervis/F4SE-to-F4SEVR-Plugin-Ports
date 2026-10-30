#include "f4se/PluginAPI.h"
#include "f4se/GameAPI.h"
#include "f4se/GameData.h"
#include "f4se/GameReferences.h"
#include "f4se/GameForms.h"
#include "f4se/GameMenus.h"
#include "f4se/GameRTTI.h"
#include "f4se_common\SafeWrite.h"
#include <shlobj.h>
#include <string>
#include <vector>
#include "xbyak/xbyak.h"
#include "f4se_common\BranchTrampoline.h"
#include "main.h"
#include "f4se_common/f4se_version.h"
#include "f4se/PapyrusVM.h"
#include "f4se/PapyrusNativeFunctions.h"
#include "f4se/PapyrusEvents.h"
#include "f4se\GameExtraData.h"
#include "Globals.h"
#include "Utils.h"
#include "f4se/Translation.h"
#include "f4se/ScaleformLoader.h"
#include "f4se/CustomMenu.h"
#include "f4se\GameTypes.h"
#include "F4NVSerialization.h"
#include "F4NVForms.h"
#include "HookUtil.h"
#include "PapyrusFunctions.h"
#include "ScaleformF4NV.h"
#include "INIReader.h"

#include <functional>
#include <chrono>
#include <future>
#include <cstdio>

std::string esmName = "BulletCountedReload.esm";
std::string mName = "BulletCountedReload";
IDebugLog	gLog;


int ammoCount = 0;
int ammoCapacity = 0;
int maxAmmoCapacity = 0;
int totalAmmoCount = 999;

class TESIdleForm;
TESIdleForm* idle = nullptr;
TESIdleForm* idle3rd = nullptr;
TESIdleForm* idlePA = nullptr;
TESIdleForm* idlePA3rd = nullptr;
void* action = nullptr;
TESObjectWEAP::InstanceData* weapInstance = nullptr;
ActorValueInfo* checkAvif = nullptr;
ActorValueInfo* checkAvif2 = nullptr;

BSTEventDispatcher<TESObjectLoadedEvent>* eventDispatcherGlobal = nullptr;

bool logEnabled = true;

PluginHandle			    g_pluginHandle = kPluginHandle_Invalid;
F4SEMessagingInterface* g_messaging = nullptr;
F4SEPapyrusInterface* g_papyrus = NULL;

F4SEScaleformInterface* g_scaleform = NULL;
F4SESerializationInterface* g_serialization = NULL;


enum ReloadStates {
	Default = 0,
	BulletInsert,
	UncullBone
};


BSTEventDispatcher<void*>* GetGlobalEventDispatcher(BSTGlobalEvent* globalEvents, const char* dispatcherName)
{
	if (!globalEvents) return nullptr;
	for (int i = 0; i < globalEvents->eventSources.count; i++) {
		if (!globalEvents->eventSources[i]) continue;
		const char* className = GetObjectClassName(globalEvents->eventSources[i]);
		if (!className) continue;
		const char* name = className;
		if (strlen(name) > 15) name += 15;
		if (strstr(name, dispatcherName) == name) {
			return &globalEvents->eventSources[i]->eventDispatcher;
		}
	}
	return nullptr;
}
#define GET_EVENT_DISPATCHER(EventName) (BSTEventDispatcher<EventName>*) GetGlobalEventDispatcher(*g_globalEvents, #EventName);
//
//STATIC_ASSERT(sizeof(BSTEventSource<void*>) == 0x70);


class BSAnimationGraphEvent {
public:
	TESForm* unk00;
	BSFixedString eventName;
	//etc
};

typedef UInt8(*_tf1)(void* thissink, BSAnimationGraphEvent* evnstruct, void** dispatcher);
RelocAddr <_tf1> tf1_HookTarget(0x2D81998); // Fallout 4 VR 1.2.72.0 (PlayerCharacter vtable slot 1)
_tf1 tf1_Original;



bool reloadStarted = false;
bool reloadEnd = true;
bool cullBone = false;
bool uncullbone = false;
int incrementor = 0;
int toAdd = 0;
bool animationGoingToPlay = true;
bool stopPressed = false;
bool readyForStopPress = false;
int reloadState = 0;

//Play stop idle based on camera mode and handle reload end
void PlayNeededIdle() {
	bool hasPostReload = false;
	/*if (weapInstance && weapInstance->flags & TESObjectWEAP::InstanceData::kFlag_BoltAction) {
		hasPostReload = true;
	}*/
	if (weapInstance && weapInstance->skill == checkAvif2) {
		hasPostReload = true;
	}
	animationGoingToPlay = SendStopSignal(hasPostReload);

	if (animationGoingToPlay) {
		if (stopPressed) {
			reloadEndHandle(true);
		}
		else {
			reloadEndHandle();
		}
	}

	logIfNeeded("is animation going to play: " + std::to_string(animationGoingToPlay));
}

struct EquippedItem
{
	TESForm*            item;         // 00 - TESObjectWEAP*
	TBO_InstanceData*   instanceData; // 08
	BGSEquipSlot*       equipSlot;    // 10
	UInt64              equipIndex;   // 18
	void*               equippedData; // 20
};
STATIC_ASSERT(sizeof(EquippedItem) == 0x28);

typedef bool     (*_AIProcess_GetEquippedItem)(void* aiProcess, UInt32 equipIndex, EquippedItem* outItem);
typedef UInt32   (*_AIProcess_GetCurrentAmmoCount)(void* aiProcess, UInt32 equipIndex);
typedef void     (*_AIProcess_SetCurrentAmmoCount)(void* aiProcess, UInt32 equipIndex, UInt32 count);
typedef TESAmmo* (*_AIProcess_GetCurrentAmmo)(void* aiProcess, UInt32 equipIndex);
typedef void     (*_RegisterForLoadGame)(BSTEventSink<TESLoadGameEvent>* sink);

static const uintptr_t RVA_AIProcess_GetEquippedItem     = 0x0E803D0;
static const uintptr_t RVA_AIProcess_GetCurrentAmmoCount = 0x0EC48C0;
static const uintptr_t RVA_AIProcess_SetCurrentAmmoCount = 0x0EC4B90;
static const uintptr_t RVA_AIProcess_GetCurrentAmmo      = 0x0EC4870;
static const uintptr_t RVA_RegisterForLoadGame           = 0x0041EBE0;

static uintptr_t GetImageBase()
{
	static uintptr_t s_base = reinterpret_cast<uintptr_t>(GetModuleHandle(NULL));
	return s_base;
}

template <typename T>
static T GetNativeFunction(uintptr_t rva)
{
	return reinterpret_cast<T>(GetImageBase() + rva);
}

UInt32 GetPlayerReserveAmmoCount(TESAmmo* ammo) {
	if (!g_player || !*g_player || !ammo) return 999;
	auto inventory = (*g_player)->inventoryList;
	if (!inventory) return 999;
	UInt32 total = 0;
	inventory->inventoryLock.LockForRead();
	for (size_t i = 0; i < inventory->items.count; ++i) {
		if (inventory->items[i].form == ammo) {
			auto s = inventory->items[i].stack;
			if (s) {
				s->Visit([&](BGSInventoryItem::Stack* cur) {
					if (cur && cur->count > 0) total += cur->count;
					return true;
				});
			}
			break;
		}
	}
	inventory->inventoryLock.Unlock();
	return total;
}

//Plays stop idle if interrupt button was pressed
void StopIfNeeded() {
	if (stopPressed) {
		_MESSAGE("[BCR] StopIfNeeded: Interrupt active at round %d (toAdd=%d)", incrementor, toAdd);
		PlayNeededIdle();
	}
}

void StopLesserAmmo() {
	if ((totalAmmoCount + ammoCount) - ammoCapacity < 0) {
		toAdd = totalAmmoCount;
	}
}

//Animation event hook
UInt8 tf1_Hook(void* arg1, BSAnimationGraphEvent* arg2, void** arg3) {
	if (weapInstance && arg2) {
		const char* evnam = arg2->eventName.c_str();
		if (evnam) {
			if (g_bVerboseLogging && reloadStarted && !reloadEnd) {
				_MESSAGE("[BCR Anim] %s", evnam);
			}

			//reload ended by engine, do reload end logic
			if (!_strcmpi("reloadEnd", evnam)) {
				reloadEndHandle();
				reloadState = ReloadStates::Default;
				_MESSAGE("[BCR] reloadEnd");
			}
			//reload state enter, initialize everything
			if (!_strcmpi("Event00", evnam)) {
				Actor* player = *g_player;
				if (player && player->middleProcess) {
					auto fnGetAmmoCount = GetNativeFunction<_AIProcess_GetCurrentAmmoCount>(RVA_AIProcess_GetCurrentAmmoCount);
					auto fnGetAmmo = GetNativeFunction<_AIProcess_GetCurrentAmmo>(RVA_AIProcess_GetCurrentAmmo);

					if (fnGetAmmoCount) {
						ammoCount = fnGetAmmoCount(player->middleProcess, 0);
					}
					if (fnGetAmmo) {
						TESAmmo* curAmmo = fnGetAmmo(player->middleProcess, 0);
						if (curAmmo) {
							totalAmmoCount = GetPlayerReserveAmmoCount(curAmmo);
						}
					}
				}
				if (maxAmmoCapacity > 0) {
					ammoCapacity = maxAmmoCapacity;
				}
				else if (weapInstance) {
					ammoCapacity = weapInstance->ammoCapacity;
					maxAmmoCapacity = ammoCapacity;
				}

				reloadStartHandle();
				StopLesserAmmo();
				ResetInterruptDebounce();
				reloadState = ReloadStates::Default;
				_MESSAGE("[BCR] reload start (Event00): live ammoCount=%d, capacity=%d (max=%d), totalAmmo=%d, toAdd=%d",
					ammoCount, ammoCapacity, maxAmmoCapacity, totalAmmoCount, toAdd);

				if (toAdd <= 0) {
					_MESSAGE("[BCR] Magazine already full (ammoCount=%d, capacity=%d), aborting reload", ammoCount, ammoCapacity);
					PlayNeededIdle();
					return tf1_Original(arg1, arg2, arg3);
				}
			}
			//Bone hide
			if (reloadStarted && !reloadEnd && uncullbone && reloadState == ReloadStates::BulletInsert && !_strcmpi("CullBone", evnam)) {
				cullBone = true;
				uncullbone = false;
				incrementor++;
				_MESSAGE("[BCR] CullBone (incrementor=%d, toAdd=%d)", incrementor, toAdd);
				if (incrementor >= toAdd) {
					PlayNeededIdle();
				}
				StopIfNeeded();
				reloadState = ReloadStates::Default;
			}
			//Bone show
			if (reloadStarted && !reloadEnd && reloadState == ReloadStates::Default && !_strcmpi("UncullBone", evnam)) {
				cullBone = false;
				uncullbone = true;
				ClearInterruptDebounce();
				if (stopPressed && incrementor > 0) {
					_MESSAGE("[BCR] UncullBone: Interrupt active (incrementor=%d), canceling before inserting next round", incrementor);
					PlayNeededIdle();
					return tf1_Original(arg1, arg2, arg3);
				}
				UInt32 targetCount = ammoCount + 1 + incrementor;
				if (targetCount > (UInt32)ammoCapacity) targetCount = ammoCapacity;
				_MESSAGE("[BCR] UncullBone: inserting round %u (incrementor=%d, toAdd=%d)",
					targetCount, incrementor, toAdd);
				SetWeapAmmoCapacity(targetCount);

				if (incrementor >= toAdd) {
					PlayNeededIdle();
				}
				if (!animationGoingToPlay) {
					PlayNeededIdle();
				}
				reloadState = ReloadStates::UncullBone;
			}
			//shell insert
			if (reloadStarted && !reloadEnd && !_strcmpi("ReloadComplete", evnam)) {
				readyForStopPress = true;
				reloadState = ReloadStates::BulletInsert;
				Actor* player = *g_player;
				if (player && player->middleProcess) {
					auto fnGetAmmoCount = GetNativeFunction<_AIProcess_GetCurrentAmmoCount>(RVA_AIProcess_GetCurrentAmmoCount);
					UInt32 beforeCount = fnGetAmmoCount ? fnGetAmmoCount(player->middleProcess, 0) : 0;
					UInt8 ret = tf1_Original(arg1, arg2, arg3);
					UInt32 afterCount = fnGetAmmoCount ? fnGetAmmoCount(player->middleProcess, 0) : 0;
					UInt32 targetCount = ammoCount + 1 + incrementor;
					if (targetCount > (UInt32)ammoCapacity) targetCount = ammoCapacity;
					_MESSAGE("[BCR] ReloadComplete: before=%u, after=%u (target=%u)", beforeCount, afterCount, targetCount);
					if (afterCount == beforeCount && afterCount < targetCount) {
						auto fnSetAmmoCount = GetNativeFunction<_AIProcess_SetCurrentAmmoCount>(RVA_AIProcess_SetCurrentAmmoCount);
						if (fnSetAmmoCount) {
							fnSetAmmoCount(player->middleProcess, 0, targetCount);
							_MESSAGE("[BCR] ReloadComplete: Fallback forced ammo count to %u", targetCount);
						}
					}
					return ret;
				}
			}

			//Manually handle reload end for various situations
			if (reloadStarted && !reloadEnd && !_strcmpi("pipboyOpened", evnam)) {
				reloadEndHandle();
				reloadState = ReloadStates::Default;
				_MESSAGE("[BCR] pipboyOpened");
			}
			if (reloadStarted && !reloadEnd && !_strcmpi("weaponSwing", evnam)) {
				reloadEndHandle();
				reloadState = ReloadStates::Default;
				_MESSAGE("[BCR] weaponSwing");
			}
			if (reloadStarted && !reloadEnd && !_strcmpi("throwEnd", evnam)) {
				reloadEndHandle();
				reloadState = ReloadStates::Default;
				_MESSAGE("[BCR] throwEnd");
			}
			if (!_strcmpi("weaponDraw", evnam) && reloadStarted && !reloadEnd) {
				reloadEndHandle();
				_MESSAGE("[BCR] weaponDraw");
			}
			if (reloadStarted && !reloadEnd && !_strcmpi("impactLandEnd", evnam)) {
				reloadEndHandle();
				reloadState = ReloadStates::Default;
				_MESSAGE("[BCR] impactLandEnd");
			}
		}
	}
	return tf1_Original(arg1, arg2, arg3);
}

struct TESEquipEvent
{
	TESObjectREFR* owner;        // 00
	UInt32            FormID;        // 08
	UInt32				refFormID;			// 0C
	UInt8				unk10;			// 10
	UInt8				unk11;			// 11
	UInt8				status;
	char              padding2[94]; 

	BGSInventoryItem::Stack* invItem; //78
	char			  padding3[8];
	TESObject* item;
	TESObjectWEAP::InstanceData* instanceData;
};
STATIC_ASSERT(offsetof(TESEquipEvent, instanceData) == 0x90);
STATIC_ASSERT(offsetof(TESEquipEvent, invItem) == 0x78);
STATIC_ASSERT(offsetof(TESEquipEvent, item) == 0x88);

DECLARE_EVENT_DISPATCHER(TESEquipEvent, 0x0042AF70); // Fallout 4 VR 1.2.72.0 (QSource<TESEquipEvent>)

void HanldeWeaponEquip(TESObjectWEAP* f, BGSInventoryItem::Stack* stack);
void UpdateEquippedWeapon();
void FillEquipDataFromEquippedItem();

//Equip Handler
void HanldeWeaponEquip(TESObjectWEAP* f, BGSInventoryItem::Stack* stack) {
	if (!f || !stack) return;
	TESObjectWEAP::InstanceData* equippedWeaponInstance = GetInstanceDataFromExtraDataList(stack->extraData, f, true);
	if (equippedWeaponInstance) {
		if (f->weapData.skill == checkAvif || f->weapData.skill == checkAvif2) {
			equippedWeaponInstance->skill = f->weapData.skill;
		}
		if (equippedWeaponInstance->skill == checkAvif || equippedWeaponInstance->skill == checkAvif2) {
			maxAmmoCapacity = equippedWeaponInstance->ammoCapacity;
			ammoCapacity = maxAmmoCapacity;
			weapInstance = equippedWeaponInstance;
			_MESSAGE("[BCR] Weapon instance equipped: '%s' (0x%08X), capacity: %d",
				f->GetFullName(), f->formID, maxAmmoCapacity);
			return;
		}
	}
	_MESSAGE("[BCR] Non-BCR weapon or problem with equip: 0x%08X", f ? f->formID : 0);
}

// Direct live weapon resolver via AIProcess
void UpdateEquippedWeapon() {
	if (!g_player || !*g_player) return;
	Actor* player = *g_player;

	void* aiProcess = player->middleProcess;
	if (aiProcess) {
		auto fnGetEquipped = GetNativeFunction<_AIProcess_GetEquippedItem>(RVA_AIProcess_GetEquippedItem);
		auto fnGetAmmoCount = GetNativeFunction<_AIProcess_GetCurrentAmmoCount>(RVA_AIProcess_GetCurrentAmmoCount);

		for (UInt32 slot = 0; slot <= 1; ++slot) {
			EquippedItem eq = { 0 };
			if (fnGetEquipped && fnGetEquipped(aiProcess, slot, &eq) && eq.item) {
				if (eq.item->formType == kFormType_WEAP) {
					TESObjectWEAP* weap = static_cast<TESObjectWEAP*>(eq.item);
					TESObjectWEAP::InstanceData* inst = static_cast<TESObjectWEAP::InstanceData*>(eq.instanceData);

					bool isBCR = (weap->weapData.skill == checkAvif || weap->weapData.skill == checkAvif2);
					if (inst && (inst->skill == checkAvif || inst->skill == checkAvif2)) {
						isBCR = true;
					}

					_MESSAGE("[UpdateEquippedWeapon] Slot %u: weapon '%s' (0x%08X, isBCR: %d, inst: %p)",
						slot, weap->GetFullName(), weap->formID, isBCR ? 1 : 0, inst);

					if (isBCR && inst) {
						if (inst->skill != checkAvif && inst->skill != checkAvif2) {
							inst->skill = weap->weapData.skill;
						}
						weapInstance = inst;
						maxAmmoCapacity = inst->ammoCapacity;
						ammoCapacity = maxAmmoCapacity;
						if (fnGetAmmoCount) {
							ammoCount = fnGetAmmoCount(aiProcess, slot);
						}
						_MESSAGE("[BCR] Recognized equipped BCR weapon via AIProcess: '%s' (0x%08X), loaded ammo: %d/%d",
							weap->GetFullName(), weap->formID, ammoCount, maxAmmoCapacity);
						return;
					}
				}
			}
		}
	}

	// Fallback to inventory scan
	FillEquipDataFromEquippedItem();
}

//Equip event
class EquipEventSink : public BSTEventSink<TESEquipEvent>
{
public:
	virtual	EventResult	ReceiveEvent(TESEquipEvent* evn, void* dispatcher) override
	{
		if (evn && evn->owner && evn->owner == (*g_player)) {
			if (!evn->status) {
				// Unequip
				if (evn->FormID) {
					TESForm* ff = LookupFormByID(evn->FormID);
					if (ff && ff->formType == kFormType_WEAP) {
						reloadEndHandle();
						weapInstance = nullptr;
						maxAmmoCapacity = 0;
						_MESSAGE("[BCR] Player unequipped weapon 0x%08X", evn->FormID);
					}
				}
			}
			else if (evn->FormID) {
				// Equip
				TESForm* ff = LookupFormByID(evn->FormID);
				if (ff && ff->formType == kFormType_WEAP) {
					TESObjectWEAP* f = (TESObjectWEAP*)ff;
					reloadEndHandle();
					if (f->weapData.skill == checkAvif || f->weapData.skill == checkAvif2) {
						if (evn->instanceData) {
							if (evn->instanceData->skill != checkAvif && evn->instanceData->skill != checkAvif2) {
								evn->instanceData->skill = f->weapData.skill;
							}
							weapInstance = evn->instanceData;
							maxAmmoCapacity = weapInstance->ammoCapacity;
							ammoCapacity = maxAmmoCapacity;
							_MESSAGE("[BCR] Equipped BCR weapon via event: '%s' (0x%08X), capacity: %d",
								f->GetFullName(), f->formID, maxAmmoCapacity);
						}
						else if (evn->invItem) {
							HanldeWeaponEquip(f, evn->invItem);
						}
						else {
							UpdateEquippedWeapon();
						}
					}
					else {
						weapInstance = nullptr;
					}
				}
			}
		}
		return kEvent_Continue;
	}
};
EquipEventSink equipEventSink;

//Called after game load to fill weapon instance if its equipped
void FillEquipDataFromEquippedItem() {
	if (!g_player || !*g_player) return;
	auto inventory = (*g_player)->inventoryList;
	if (inventory)
	{
		inventory->inventoryLock.LockForRead();
		for (size_t i = 0; i < inventory->items.count; i++)
		{
			TESForm* form = inventory->items[i].form;
			BGSInventoryItem::Stack* stack = inventory->items[i].stack;

			if (form && form->formType == kFormType_WEAP && stack)
			{
				TESObjectWEAP* weap = (TESObjectWEAP*)form;
				if (weap->weapData.skill == checkAvif || weap->weapData.skill == checkAvif2) {
					stack->Visit([&](BGSInventoryItem::Stack* stackx)
					{
						if (stackx && (stackx->flags & 7)) {
							HanldeWeaponEquip(weap, stackx);
						}
						return true;
					});
				}
			}
		}
		inventory->inventoryLock.Unlock();
	}
}


struct PlayerAmmoCountEvent
{
	UInt32							ammoCount;        // 00
	UInt32							totalAmmoCount;    // 04
	UInt64							unk08;            // 08
	TESObjectWEAP* weapon;            // 10
	TESObjectWEAP::InstanceData* weaponInstance;
	//...
};
STATIC_ASSERT(offsetof(PlayerAmmoCountEvent, weapon) == 0x10);

//Ammo count handle
class PlayerAmmoCountEventSink : public BSTEventSink<PlayerAmmoCountEvent>
{
public:
	virtual ~PlayerAmmoCountEventSink() { };

	virtual    EventResult    ReceiveEvent(PlayerAmmoCountEvent* evn, void* dispatcher) override
	{
		if (evn) {
			if (evn->weapon) {
				bool isBCR = (evn->weapon->weapData.skill == checkAvif || evn->weapon->weapData.skill == checkAvif2);
				if (isBCR && evn->weaponInstance) {
					if (evn->weaponInstance->skill != checkAvif && evn->weaponInstance->skill != checkAvif2) {
						evn->weaponInstance->skill = evn->weapon->weapData.skill;
					}
					weapInstance = evn->weaponInstance;
					maxAmmoCapacity = weapInstance->ammoCapacity;
					ammoCapacity = maxAmmoCapacity;
				}
			}
			ammoCount = evn->ammoCount;
			totalAmmoCount = evn->totalAmmoCount;
		}
		return kEvent_Continue;
	}
};
PlayerAmmoCountEventSink playerAmmoCountEventSink;


bool RegisterAfterLoadEvents() {
	_MESSAGE("[BCR] Registering after-load events...");

	auto eventDispatcher4 = GET_EVENT_DISPATCHER(PlayerAmmoCountEvent);
	if (eventDispatcher4) {
		eventDispatcher4->AddEventSink(&playerAmmoCountEventSink);
		_MESSAGE("[BCR] Registered PlayerAmmoCountEvent sink successfully.");
	}
	else {
		logMessage("cant register PlayerAmmoCountEvent");
		return false;
	}

	auto eventDispatcher3 = GetEventDispatcher<TESEquipEvent>();
	if (eventDispatcher3) {
		eventDispatcher3->eventSinks.Push(&equipEventSink);
		_MESSAGE("[BCR] Registered TESEquipEvent sink successfully.");
	}
	else {
		logMessage("cant register TESEquipEvent");
		return false;
	}

	return true;
}


bool oncePerSession = false;

class TESLoadGameHandler : public BSTEventSink<TESLoadGameEvent>
{
public:
	virtual ~TESLoadGameHandler() { };
	virtual    EventResult    ReceiveEvent(TESLoadGameEvent* evn, void* dispatcher) override
	{
		_MESSAGE("[BCR] TESLoadGameEvent received.");
		UpdateEquippedWeapon();
		if (!oncePerSession) {
			if (!RegisterAfterLoadEvents()) {
				logMessage("unable to register for events");
			}
			oncePerSession = true;
		}
		return kEvent_Continue;
	}
};




void logMessage(std::string aString)
{
	_MESSAGE(("[" + mName + "] " + aString).c_str());
}

void GameDataReady()
{
	static auto pLoadGameHandler = new TESLoadGameHandler();
	auto fnRegisterLoadGame = GetNativeFunction<_RegisterForLoadGame>(RVA_RegisterForLoadGame);
	if (fnRegisterLoadGame) {
		fnRegisterLoadGame(pLoadGameHandler);
		_MESSAGE("[BCR] Registered TESLoadGameHandler via native RegisterForLoadGame (0x0041EBE0)");
	}
	else {
		_MESSAGE("[BCR] Warning: Could not resolve RegisterForLoadGame");
	}

	HandleIniFiles();
}





void F4SEMessageHandler(F4SEMessagingInterface::Message* msg)
{
	switch (msg->type)
	{
	case F4SEMessagingInterface::kMessage_GameDataReady:
	{
		bool isReady = (msg->data != nullptr);
		if (isReady)
		{
			_MESSAGE("[BCR] kMessage_GameDataReady received.");
			if (!GetForms()) {
				logMessage("Unable to resolve marker forms, BCR will not function properly");
			}

			GameDataReady();
			ReceiveKeys();
		}
		break;
	}
	case F4SEMessagingInterface::kMessage_PostLoadGame:
	{
		_MESSAGE("[BCR] kMessage_PostLoadGame received.");
		InitVRInput();
		UpdateEquippedWeapon();
		if (!oncePerSession) {
			if (!RegisterAfterLoadEvents()) {
				logMessage("unable to register for events");
			}
			oncePerSession = true;
		}
		break;
	}
	case F4SEMessagingInterface::kMessage_NewGame:
	{
		_MESSAGE("[BCR] kMessage_NewGame received.");
		InitVRInput();
		UpdateEquippedWeapon();
		if (!oncePerSession) {
			if (!RegisterAfterLoadEvents()) {
				logMessage("unable to register for events");
			}
			oncePerSession = true;
		}
		break;
	}
	case F4SEMessagingInterface::kMessage_GameLoaded:
	{
		_MESSAGE("[BCR] kMessage_GameLoaded received.");
		InitVRInput();
		UpdateEquippedWeapon();
		break;
	}
	}
}



extern "C"
{
	bool F4SEPlugin_Query(const F4SEInterface* f4se, PluginInfo* info)
	{
		// Open log in Fallout4VR directory
		gLog.OpenRelative(CSIDL_MYDOCUMENTS, (const char*)("\\My Games\\Fallout4VR\\F4SE\\" + mName + ".log").c_str());

		_MESSAGE("%s v2.05-VR", mName.c_str());
		logMessage("F4SEPlugin_Query");

		// Populate info structure
		info->infoVersion = PluginInfo::kInfoVersion;
		info->name = mName.c_str();
		info->version = 205;

		// Store plugin handle so we can identify ourselves later
		g_pluginHandle = f4se->GetPluginHandle();

		// Check runtime version for Fallout 4 VR (F4SEVR reports 0x010A08A0; also accept 0x01020482)
		if (f4se->runtimeVersion != 0x010A08A0 && f4se->runtimeVersion != RUNTIME_VR_VERSION_1_2_72)
		{
			char errBuf[256];
			sprintf_s(errBuf, "Unsupported runtime version %08X (expected 010A08A0 / %08X - Fallout 4 VR 1.2.72).\n%s will be disabled.",
				f4se->runtimeVersion, RUNTIME_VR_VERSION_1_2_72, mName.c_str());
			_FATALERROR("%s", errBuf);
			MessageBoxA(NULL, errBuf, mName.c_str(), MB_OK | MB_ICONEXCLAMATION);
			return false;
		}

		// Check F4SE version
		if (f4se->f4seVersion < PACKED_F4SE_VERSION)
		{
			_FATALERROR("F4SE version too old (got %08X expected at least %08X)", f4se->f4seVersion, PACKED_F4SE_VERSION);
			return false;
		}

		// Query interfaces
		g_scaleform = (F4SEScaleformInterface*)f4se->QueryInterface(kInterface_Scaleform);
		if (!g_scaleform) {
			_MESSAGE("Note: couldn't get scaleform interface (expected in VR)");
		}
		else {
			_MESSAGE("Scaleform interface acquired");
		}

		g_papyrus = (F4SEPapyrusInterface*)f4se->QueryInterface(kInterface_Papyrus);
		if (!g_papyrus) {
			_FATALERROR("couldn't get papyrus interface");
			return false;
		}
		else {
			_MESSAGE("Papyrus interface acquired");
		}

		g_messaging = (F4SEMessagingInterface*)f4se->QueryInterface(kInterface_Messaging);
		if (!g_messaging)
		{
			_FATALERROR("couldn't get messaging interface");
			return false;
		}

		g_serialization = (F4SESerializationInterface*)f4se->QueryInterface(kInterface_Serialization);
		if (!g_serialization) {
			_MESSAGE("Note: serialization interface not available");
		}

		return true;
	}

	bool F4SEPlugin_Load(const F4SEInterface* f4se)
	{
		logMessage("F4SEPlugin_Load");

		if (g_messaging)
			g_messaging->RegisterListener(g_pluginHandle, "F4SE", F4SEMessageHandler);

		if (!g_localTrampoline.Create(1024 * 64, nullptr)) {
			_WARNING("couldn't create codegen buffer (unused, continuing)");
		}

		if (!g_branchTrampoline.Create(1024 * 64)) {
			_WARNING("couldn't create branch trampoline (unused, continuing)");
		}

		tf1_Original = HookUtil::SafeWrite64(tf1_HookTarget.GetUIntPtr(), &tf1_Hook);
		_MESSAGE("Hook installed at 0x%016llX (original: 0x%016llX)", tf1_HookTarget.GetUIntPtr(), (uintptr_t)tf1_Original);
		logMessage("Loaded successfully.");
		return true;
	}

};
