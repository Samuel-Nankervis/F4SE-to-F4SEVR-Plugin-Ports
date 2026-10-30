#include "TESDeathEventHandler.h"

#include <bitset>

#include "f4se/GameReferences.h"
#include "f4se/GameRTTI.h"
#include "f4se/PapyrusEvents.h"
#include "f4se/PapyrusUtilities.h"

#include "ArLogger.h"

static constexpr UInt64 LOWER_32_MASK = 0x00000000FFFFFFFF;
static constexpr UInt64 UPPER_32_MASK = 0xFFFFFFFF00000000;
static constexpr size_t CANT_DROP_FLAG_POS = 2;

static constexpr char PREFIX_RE[] = R"(Event   | )";
static constexpr char PREFIX_UA[] = R"(Unload  | )";
static constexpr char PREFIX_RA[] = R"(Reload  | )";
static constexpr char PREFIX_SA[] = R"(SetAmmo | )";

SInt32 TESDeathEventHandler::wepAmmoPercent = 100;
SInt32 TESDeathEventHandler::invAmmoPercent = 100;

// Fallout 4 VR 1.2.72.0 verified RVAs from Address Library (version-1-2-72-0.csv) and Fallout4VR.pdb:
// ID 4042:   AIProcess::GetEquippedItem(BGSEquipIndex, EquippedItem&) -> 0x0E803D0
// ID 184463: AIProcess::GetCurrentAmmoCount(BGSEquipIndex)            -> 0x0EC48C0
// ID 940905: AIProcess::SetCurrentAmmoCount(BGSEquipIndex, uint)      -> 0x0EC4B90
// ID 1154936: AIProcess::GetCurrentAmmo(BGSEquipIndex)                -> 0x0EC4870
// ID 795983: AIProcess::SetCurrentAmmo(BGSEquipIndex, TESAmmo*)       -> 0x0EC4910
static const uintptr_t RVA_AIProcess_GetEquippedItem     = 0x0E803D0;
static const uintptr_t RVA_AIProcess_GetCurrentAmmoCount = 0x0EC48C0;
static const uintptr_t RVA_AIProcess_SetCurrentAmmoCount = 0x0EC4B90;
static const uintptr_t RVA_AIProcess_GetCurrentAmmo      = 0x0EC4870;
static const uintptr_t RVA_AIProcess_SetCurrentAmmo      = 0x0EC4910;

struct EquippedItem
{
	TESForm*            item;         // 00 - TESObjectWEAP*
	TBO_InstanceData*   instanceData; // 08
	BGSEquipSlot*       equipSlot;    // 10
	UInt64              equipIndex;   // 18
	EquippedWeaponData* equippedData; // 20
};
STATIC_ASSERT(sizeof(EquippedItem) == 0x28);

typedef bool     (*_AIProcess_GetEquippedItem)(void* aiProcess, UInt32 equipIndex, EquippedItem* outItem);
typedef UInt32   (*_AIProcess_GetCurrentAmmoCount)(void* aiProcess, UInt32 equipIndex);
typedef void     (*_AIProcess_SetCurrentAmmoCount)(void* aiProcess, UInt32 equipIndex, UInt32 count);
typedef TESAmmo* (*_AIProcess_GetCurrentAmmo)(void* aiProcess, UInt32 equipIndex);
typedef void     (*_AIProcess_SetCurrentAmmo)(void* aiProcess, UInt32 equipIndex, TESAmmo* ammo);

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

static bool SafeAIProcessGetEquippedItem(void* aiProcess, UInt32 equipIndex, EquippedItem* outItem)
{
	__try {
		auto fn = GetNativeFunction<_AIProcess_GetEquippedItem>(RVA_AIProcess_GetEquippedItem);
		if (fn && aiProcess && outItem) {
			return fn(aiProcess, equipIndex, outItem);
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
	}
	return false;
}

static UInt32 SafeAIProcessGetCurrentAmmoCount(void* aiProcess, UInt32 equipIndex)
{
	__try {
		auto fn = GetNativeFunction<_AIProcess_GetCurrentAmmoCount>(RVA_AIProcess_GetCurrentAmmoCount);
		return (fn && aiProcess) ? fn(aiProcess, equipIndex) : 0;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return 0;
	}
}

static void SafeAIProcessSetCurrentAmmoCount(void* aiProcess, UInt32 equipIndex, UInt32 count)
{
	__try {
		auto fn = GetNativeFunction<_AIProcess_SetCurrentAmmoCount>(RVA_AIProcess_SetCurrentAmmoCount);
		if (fn && aiProcess) {
			fn(aiProcess, equipIndex, count);
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
	}
}

static TESAmmo* SafeAIProcessGetCurrentAmmo(void* aiProcess, UInt32 equipIndex)
{
	__try {
		auto fn = GetNativeFunction<_AIProcess_GetCurrentAmmo>(RVA_AIProcess_GetCurrentAmmo);
		return (fn && aiProcess) ? fn(aiProcess, equipIndex) : nullptr;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return nullptr;
	}
}

static void SafeAIProcessSetCurrentAmmo(void* aiProcess, UInt32 equipIndex, TESAmmo* ammo)
{
	__try {
		auto fn = GetNativeFunction<_AIProcess_SetCurrentAmmo>(RVA_AIProcess_SetCurrentAmmo);
		if (fn && aiProcess) {
			fn(aiProcess, equipIndex, ammo);
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
	}
}

static const char* SafeGetFormName(TESForm* form)
{
	__try {
		if (form) {
			const char* name = form->GetFullName();
			if (name && name[0] != '\0') {
				return name;
			}
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
	}
	return "<Unnamed>";
}

EventResult TESDeathEventHandler::ReceiveEvent(TESDeathEvent* event, void* dispatcher)
{
	if (!event || !event->source)
		return kEvent_Continue;

	const auto actor = DYNAMIC_CAST(event->source, TESObjectREFR, Actor);
	if (!actor)
		return kEvent_Continue;

	ArLogger::LogP(ArLogger::Verbose, PREFIX_RE, "Death event received for %s (0x%08X).",
		SafeGetFormName(actor), actor->formID);

	AmmoStack ammoStack = { nullptr, 0 };
	if (!UnloadReloadAmmo(actor, ammoStack, true))
	{
		ArLogger::LogP(ArLogger::Verbose, PREFIX_RE, "No loaded weapon ammo found to unload on %s (0x%08X).",
			SafeGetFormName(actor), actor->formID);
		return kEvent_Continue;
	}

	if ((wepAmmoPercent > 0 || invAmmoPercent != 100) && !SetAmmo(event->source, actor, ammoStack))
	{
		ArLogger::LogP(ArLogger::Verbose, PREFIX_RE, "SetAmmo failed, attempting to reload...");
		UnloadReloadAmmo(actor, ammoStack, false);
	}
	else
	{
		ArLogger::LogP(ArLogger::Verbose, PREFIX_RE, "Successfully removed ammo from %s (0x%08X).",
			SafeGetFormName(actor), actor->formID);
	}

	return kEvent_Continue;
}

bool TESDeathEventHandler::UnloadReloadAmmo(const Actor* actor, AmmoStack& ammoStack, const bool unload)
{
	const auto prefix = unload ? PREFIX_UA : PREFIX_RA;
	const auto action = unload ? "unload" : "reload";

	if (!actor || !actor->middleProcess)
	{
		ArLogger::LogP(ArLogger::Verbose, prefix, "Actor or middleProcess is null.");
		return false;
	}

	void* aiProcess = actor->middleProcess;

	// Loop through equip slot 0 (main hand) and 1 (off hand)
	for (UInt32 equipIndex = 0; equipIndex <= 1; ++equipIndex)
	{
		EquippedItem eqItem = { 0 };
		bool found = SafeAIProcessGetEquippedItem(aiProcess, equipIndex, &eqItem);

		// Fallback: check middleProcess->unk08->equipData if GetEquippedItem didn't return weapon
		if ((!found || !eqItem.item || eqItem.item->formType != kFormType_WEAP) && actor->middleProcess->unk08)
		{
			auto& equipArray = actor->middleProcess->unk08->equipData;
			if (equipArray.entries != nullptr && equipArray.count > 0)
			{
				for (UInt32 i = 0; i < equipArray.count; ++i)
				{
					if (equipArray.entries[i].item && equipArray.entries[i].item->formType == kFormType_WEAP)
					{
						eqItem.item = equipArray.entries[i].item;
						eqItem.instanceData = equipArray.entries[i].instanceData;
						eqItem.equipSlot = equipArray.entries[i].equipSlot;
						eqItem.equipIndex = equipArray.entries[i].unk18;
						eqItem.equippedData = equipArray.entries[i].equippedData;
						found = true;
						break;
					}
				}
			}
		}

		if (!found || !eqItem.item || eqItem.item->formType != kFormType_WEAP)
		{
			continue;
		}

		auto weapon = static_cast<TESObjectWEAP*>(eqItem.item);

		// Check 'Can't Drop' flag on the weapon form
		if (std::bitset<sizeof(UInt32) * CHAR_BIT>(weapon->flags).test(CANT_DROP_FLAG_POS))
		{
			ArLogger::LogP(ArLogger::Verbose, prefix, "%s (0x%08X) ignored due to 'Can't Drop' flag.",
				SafeGetFormName(weapon), weapon->formID);
			continue;
		}

		// Resolve ammo pointer
		TESAmmo* ammo = nullptr;
		if (eqItem.equippedData && eqItem.equippedData->ammo)
		{
			ammo = eqItem.equippedData->ammo;
		}
		if (!ammo)
		{
			ammo = SafeAIProcessGetCurrentAmmo(aiProcess, equipIndex);
		}
		if (!ammo)
		{
			ammo = weapon->weapData.ammo;
		}

		if (!ammo)
		{
			ArLogger::LogP(ArLogger::Verbose, prefix, "%s (0x%08X) has no ammo.",
				SafeGetFormName(weapon), weapon->formID);
			continue;
		}

		// Check for charged ammo (e.g. Fusion Core in Gatling Laser)
		const auto ammoCharge = ammo->data.health;
		if (ammoCharge > 0)
		{
			ArLogger::LogP(ArLogger::Verbose, prefix, "%s (0x%08X) ignored due to having a charge (%hu).",
				SafeGetFormName(ammo), ammo->formID, ammoCharge);
			continue;
		}

		// Get loaded ammo count in the magazine
		SInt32 ammoCount = static_cast<SInt32>(SafeAIProcessGetCurrentAmmoCount(aiProcess, equipIndex));
		if (ammoCount <= 0 && eqItem.equippedData)
		{
			ammoCount = static_cast<SInt32>(eqItem.equippedData->unk18 & LOWER_32_MASK);
		}

		ArLogger::LogP(ArLogger::Verbose, prefix, "Weapon %s (0x%08X) with Ammo %s (0x%08X), loaded count: %li",
			SafeGetFormName(weapon), weapon->formID, SafeGetFormName(ammo), ammo->formID, ammoCount);

		if (ammoCount < 0)
		{
			ArLogger::LogP(ArLogger::Verbose, prefix, "%s ignored due to negative ammo count (%li).",
				SafeGetFormName(ammo), ammoCount);
			continue;
		}

		if (unload)
		{
			if (ammoCount == 0)
			{
				ArLogger::LogP(ArLogger::Verbose, prefix, "%s (0x%08X) already has 0 loaded ammo, skipping.",
					SafeGetFormName(ammo), ammo->formID);
				continue;
			}

			ArLogger::LogP(ArLogger::Verbose, prefix, "Unloading %li %s (0x%08X).",
				ammoCount, SafeGetFormName(ammo), ammo->formID);

			// Unload: set live ammo count to 0
			SafeAIProcessSetCurrentAmmoCount(aiProcess, equipIndex, 0);
			if (eqItem.equippedData)
			{
				eqItem.equippedData->unk18 &= UPPER_32_MASK;
			}

			ammoStack.ammo = ammo;
			ammoStack.count = ammoCount;
			return true;
		}
		else
		{
			if (ammo != ammoStack.ammo)
			{
				ArLogger::LogP(ArLogger::Verbose, prefix, "%s skipped due to not matching desired ammo %s.",
					SafeGetFormName(ammo), ammoStack.ammo ? SafeGetFormName(ammoStack.ammo) : "null");
				continue;
			}

			ArLogger::LogP(ArLogger::Verbose, prefix, "Reloading %li %s (0x%08X).",
				ammoStack.count, SafeGetFormName(ammo), ammo->formID);

			SafeAIProcessSetCurrentAmmoCount(aiProcess, equipIndex, static_cast<UInt32>(ammoStack.count));
			if (eqItem.equippedData)
			{
				eqItem.equippedData->unk18 += ammoStack.count;
			}

			return true;
		}
	}

	ArLogger::LogP(ArLogger::Verbose, prefix, "Failed to find equipped weapon to %s.", action);
	return false;
}

bool TESDeathEventHandler::SetAmmo(TESObjectREFR* source, const Actor* actor, const AmmoStack ammoStack)
{
	ArLogger::LogP(ArLogger::Verbose, PREFIX_SA, "Attempting to set ammo for %s...",
		ammoStack.ammo ? SafeGetFormName(ammoStack.ammo) : "null");

	if (!actor || !source || !ammoStack.ammo)
	{
		ArLogger::LogP(ArLogger::Verbose, PREFIX_SA, "Invalid parameters.");
		return false;
	}

	const auto inventory = actor->inventoryList;
	// Direct traversal of inventoryList items without taking BSWriteLocker to prevent deadlocks
	// since TESDeathEvent runs synchronously on the engine's main thread
	if (inventory && inventory->items.entries != nullptr && inventory->items.count > 0)
	{
		for (UInt32 i = 0; i < inventory->items.count; ++i)
		{
			BGSInventoryItem item;
			if (!inventory->items.GetNthItem(i, item))
			{
				continue;
			}

			if (item.form == ammoStack.ammo)
			{
				if (!item.stack)
				{
					ArLogger::LogP(ArLogger::Verbose, PREFIX_SA, "Ammo item found but stack is null.");
					continue;
				}

				const SInt32 wepAmmo = ammoStack.count * wepAmmoPercent / 100;
				const SInt32 invAmmo = item.stack->count * invAmmoPercent / 100;
				const SInt32 total = wepAmmo + invAmmo;

				ArLogger::LogP(ArLogger::Verbose, PREFIX_SA, "Setting %s (0x%08X) to %li. %li (%li%% of %li) from weapon and %li (%li%% of %li) from inventory.",
					SafeGetFormName(ammoStack.ammo), ammoStack.ammo->formID, total,
					wepAmmo, wepAmmoPercent, ammoStack.count,
					invAmmo, invAmmoPercent, item.stack->count);

				if (total > 0)
				{
					item.stack->count = total;
				}
				else
				{
					item.stack->count = 0;
					const auto handle = PapyrusVM::GetHandleFromObject(static_cast<void*>(source), TESObjectREFR::kTypeID);
					bool silent = true;
					TESObjectREFR* target = nullptr;
					SendPapyrusEvent4<TESForm*, SInt32, bool, TESObjectREFR*>(handle, "ObjectReference", "RemoveItem", item.form, item.stack->count, silent, target);
				}

				return true;
			}
		}
	}

	// Ammo was not found in the corpse inventory!
	// Instead of failing and reloading the gun, add the unloaded ammo to the corpse!
	const SInt32 wepAmmo = ammoStack.count * wepAmmoPercent / 100;
	ArLogger::LogP(ArLogger::Verbose, PREFIX_SA, "Ammo not currently in inventory. Adding %li rounds of %s (0x%08X) to corpse.",
		wepAmmo, SafeGetFormName(ammoStack.ammo), ammoStack.ammo->formID);

	if (wepAmmo > 0)
	{
		const auto handle = PapyrusVM::GetHandleFromObject(static_cast<void*>(source), TESObjectREFR::kTypeID);
		bool silent = true;
		TESForm* ammoForm = ammoStack.ammo;
		SInt32 countToAdd = wepAmmo;
		SendPapyrusEvent3<TESForm*, SInt32, bool>(handle, "ObjectReference", "AddItem", ammoForm, countToAdd, silent);
	}

	return true;
}
