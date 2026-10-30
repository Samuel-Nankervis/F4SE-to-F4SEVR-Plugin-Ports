#pragma once
#include "f4se\GameForms.h"
#include "f4se/GameTypes.h"
#include "f4se\PapyrusArgs.h"
#include "f4se\PapyrusEvents.h"
#include "f4se\GameObjects.h"
#include "f4se\GameReferences.h"
#include "f4se\GameRTTI.h"
#include "f4se/GameCamera.h"

#include "Globals.h"
#include "Utils.h"



void ShowNotification(std::string asNotificationText);

void ShowMessagebox(std::string asText);

void PlaySubgraphAnimation(Actor* target, BSFixedString asEventName);

bool SendStopSignal(bool hasPostReload);

void reloadStartHandle();

void reloadEndHandle(bool threaded = false);

void SetWeapAmmoCapacity(int amount);

