
#pragma once
#include "f4se\GameForms.h"
#include "Globals.h"

#pragma once
#include "f4se/GameForms.h"
#include "Globals.h"
#include "Utils.h"

inline bool GetForms() {
	_MESSAGE("[GetForms] Resolving BCR marker ActorValues...");

	// FormIDs in Fallout4.esm: 0x300 and 0x2FC
	checkAvif = reinterpret_cast<ActorValueInfo*>(LookupFormByID(0x300));
	checkAvif2 = reinterpret_cast<ActorValueInfo*>(LookupFormByID(0x2FC));

	if (!checkAvif) {
		checkAvif = reinterpret_cast<ActorValueInfo*>(GetFormFromIdentifier("Fallout4.esm|300"));
	}
	if (!checkAvif2) {
		checkAvif2 = reinterpret_cast<ActorValueInfo*>(GetFormFromIdentifier("Fallout4.esm|2FC"));
	}

	if (!checkAvif || !checkAvif2) {
		_MESSAGE("[GetForms] ERROR: Failed to resolve checkAvif (0x300) or checkAvif2 (0x2FC)!");
		return false;
	}

	_MESSAGE("[GetForms] Successfully resolved checkAvif (0x300): %p, checkAvif2 (0x2FC): %p", checkAvif, checkAvif2);
	return true;
}