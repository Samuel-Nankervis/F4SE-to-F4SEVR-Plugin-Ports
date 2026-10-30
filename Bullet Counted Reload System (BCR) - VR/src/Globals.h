#pragma once
#include "f4se\GameForms.h"
#include "f4se\GameExtraData.h"
#include "f4se\GameObjects.h"
#include <vector>



extern bool reloadStarted;
extern bool reloadEnd;
extern bool cullBone;
extern bool uncullbone;
extern bool canBeInterrupted;
extern bool animationGoingToPlay;
extern bool stopPressed;
extern bool readyForStopPress;

extern int ammoCount;
extern int ammoCapacity;
extern int maxAmmoCapacity;
extern int incrementor;
extern int toAdd;

extern ActorValueInfo* checkAvif;
extern ActorValueInfo* checkAvif2;

extern bool logEnabled;
extern bool g_bVerboseLogging;

extern TESObjectWEAP::InstanceData* weapInstance;

void ResetInterruptDebounce();
void ClearInterruptDebounce();
void InitVRInput();


