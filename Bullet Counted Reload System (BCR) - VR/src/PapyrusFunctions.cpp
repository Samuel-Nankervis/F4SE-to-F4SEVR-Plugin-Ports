#include "PapyrusFunctions.h"

#include <functional>
#include <chrono>
#include <future>
#include <cstdio>



class later
{
public:
	template <class callable, class... arguments>
	later(int after, bool async, callable&& f, arguments&&... args)
	{
		std::function<std::invoke_result_t<callable, arguments...>()> task(std::bind(std::forward<callable>(f), std::forward<arguments>(args)...));

		if (async)
		{
			std::thread([after, task]() {
				std::this_thread::sleep_for(std::chrono::milliseconds(after));
				task();
			}).detach();
		}
		else
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(after));
			task();
		}
	}

};




bool SendAnimEventOnPlayer(BSFixedString name) {
	return (*g_player)->animGraphHolder.NotifyAnimationGraphImpl(name);
}


//Play reload end idles when needed
bool SendStopSignal(bool hasPostReload) {
	static BSFixedString reloadEndAnimEvent("reloadEnd");
	static BSFixedString reloadEndAnimEvent2("reloadEndSlave");

	static BSFixedString sprintEnd("SprintStop");

	static BSFixedString initiateStart("initiateStart");
	static BSFixedString reloadEndAnimEventPostReload("reloadToIdleStart");
	//static BSFixedString reloadEndAnimEvent3("boltChargeStart");
	bool resp = false;
	bool vv = false;


	//bool v1 = playAnimEvent(actor, idlePA, sprintEnd);
	if (hasPostReload) {
		bool v1 = SendAnimEventOnPlayer(sprintEnd);
		bool v2 = SendAnimEventOnPlayer(initiateStart);
		resp = SendAnimEventOnPlayer(reloadEndAnimEventPostReload);

	}
	else {
		resp = SendAnimEventOnPlayer(reloadEndAnimEvent);

	}


	//bool v2 = playAnimEvent(actor, idlePA, initiateStart);


	//resp = playAnimEvent(actor, idlePA, hasPostReload ? reloadEndAnimEventPostReload : reloadEndAnimEvent);


	if (hasPostReload && !resp) {
		//resp = playAnimEvent(actor, idlePA, reloadEndAnimEvent);
		resp = SendAnimEventOnPlayer(reloadEndAnimEvent);

		//playAnimEvent(actor, idlePA, reloadEndAnimEvent2);
		SendAnimEventOnPlayer(reloadEndAnimEvent2);

	}
	if (!hasPostReload) {
		vv = SendAnimEventOnPlayer(reloadEndAnimEvent2);
	}

	/*isBoltAction = true;
	if (isBoltAction) {
	bool t = false;
	t = playAnimEvent2(actor, idlePA, reloadEndAnimEvent3);
	}*/



	logIfNeeded("is animation stopped:");
	logIfNeeded(std::to_string(resp).c_str());
	return resp;
	/*VMArray<VMVariable> varr;
	VMVariable b1;
	b1.Set(&idle);
	varr.Push(&b1);
	CallFunctionNoWait(actor, "PlayIdle", varr);
	return true;*/
}



void ShowNotification(std::string asNotificationText) {
	CallGlobalFunctionNoWait1<BSFixedString>("Debug", "Notification", BSFixedString(asNotificationText.c_str()));
}

void ShowMessagebox(std::string asText) {
	CallGlobalFunctionNoWait1<BSFixedString>("Debug", "Messagebox", BSFixedString(asText.c_str()));
}


//------------------------------------------------------------------------------------------------------------------------
//Non papyrus

//Set weapon capacity to needed amount to be sure reloadComplete fills needed amount of ammo
void SetWeapAmmoCapacity(int amount) {
	if (weapInstance) {
		int capLimit = maxAmmoCapacity > 0 ? maxAmmoCapacity : ammoCapacity;
		if (amount > capLimit) {
			weapInstance->ammoCapacity = capLimit;
		}
		else {
			weapInstance->ammoCapacity = amount;
		}
	}
	else {
		_MESSAGE("weapon instance is nullptr");
	}
}

//clear needed stuff when reload ends
void reloadEndHandle(bool threaded) {
	reloadEnd = true;
	reloadStarted = false;
	incrementor = 0;
	toAdd = 0;
	stopPressed = false;
	animationGoingToPlay = true;
	uncullbone = false;
	readyForStopPress = false;
	ClearInterruptDebounce();
	int restoreCap = maxAmmoCapacity > 0 ? maxAmmoCapacity : ammoCapacity;
	if (threaded) {

		later a(500, true, &SetWeapAmmoCapacity, restoreCap);
	}
	else {
		SetWeapAmmoCapacity(restoreCap);
	}

}



//ready needed stuff when reload is started
void reloadStartHandle() {
	incrementor = 0;
	int fullCap = maxAmmoCapacity > 0 ? maxAmmoCapacity : ammoCapacity;
	ammoCapacity = fullCap;
	toAdd = fullCap - ammoCount;
	reloadStarted = true;
	reloadEnd = false;
	animationGoingToPlay = true;
	stopPressed = false;
	uncullbone = false;
	readyForStopPress = false;
	SetWeapAmmoCapacity(ammoCount);
}

