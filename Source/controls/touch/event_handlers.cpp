#include "controls/touch/event_handlers.h"

#include "control.h"
#include "controls/plrctrls.h"
#include "cursor.h"
#include "diablo.h"
#include "engine.h"
#include "engine/render/scrollrt.h"
#include "gmenu.h"
#include "inv.h"
#include "options.h"
#include "panels/spell_book.hpp"
#include "qol/itemlabels.h"
#include "qol/stash.h"
#include "stores.h"
#include "utils/ui_fwd.h"

namespace devilution {

namespace {

VirtualGamepadEventHandler Handler(&VirtualGamepadState);

Point ScaleToScreenCoordinates(float x, float y)
{
	return Point {
		(int)round(x * gnScreenWidth),
		(int)round(y * gnScreenHeight)
	};
}

bool SimulateMouseMovement(const SDL_Event &event)
{
	Point position = ScaleToScreenCoordinates(event.tfinger.x, event.tfinger.y);

	bool isInMainPanel = GetMainPanel().contains(position);
	bool isInLeftPanel = GetLeftPanel().contains(position);
	bool isInRightPanel = GetRightPanel().contains(position);
	if (IsStashOpen) {
		if (!spselflag && !isInMainPanel && !isInLeftPanel && !isInRightPanel)
			return false;
	} else if (invflag) {
		if (!spselflag && !isInMainPanel && !isInRightPanel)
			return false;
	}

	MousePosition = position;

	SetPointAndClick(true);

	InvalidateInventorySlot();
	if (gbRunGame) {
#ifdef __IPHONEOS__
		// A label highlighted on the prior frame must not pin the next tap to the
		// old item before the label renderer has a chance to clear its state.
		ResetItemlabelHighlighted();
#endif
		CheckCursMove(event.type != SDL_FINGERMOTION);
	}
	return true;
}

#ifdef __IPHONEOS__
struct InventoryTouchGestureState {
	SDL_TouchID touchId = 0;
	SDL_FingerID primaryFinger = 0;
	SDL_FingerID secondaryFinger = 0;
	Uint32 startedAt = 0;
	Point primaryPosition {};
	bool primaryDown = false;
	bool secondaryDown = false;
	bool usedItem = false;

	void Reset()
	{
		*this = {};
	}
};

InventoryTouchGestureState InventoryTouchGesture;

struct StoreTouchGestureState {
	SDL_TouchID touchId = 0;
	SDL_FingerID fingerId = 0;
	int lastY = 0;
	bool scrolled = false;

	void Reset()
	{
		*this = {};
	}
};

StoreTouchGestureState StoreTouchGesture;

bool IsInventoryTouchGestureActive()
{
	return InventoryTouchGesture.primaryDown || InventoryTouchGesture.secondaryDown;
}

SDL_Event MakeTouchMouseButtonEvent(const SDL_TouchFingerEvent &fingerEvent, Uint32 type, Uint8 button, Point position)
{
	SDL_Event mouseEvent {};
	mouseEvent.button.type = type;
	mouseEvent.button.timestamp = fingerEvent.timestamp;
	mouseEvent.button.windowID = fingerEvent.windowID;
	mouseEvent.button.which = DevilutionTouchMouseId;
	mouseEvent.button.button = button;
	mouseEvent.button.state = type == SDL_MOUSEBUTTONDOWN ? SDL_PRESSED : SDL_RELEASED;
	mouseEvent.button.clicks = 1;
	mouseEvent.button.x = position.x;
	mouseEvent.button.y = position.y;
	return mouseEvent;
}

void ConvertTouchToMouseEvent(SDL_Event &event)
{
	SDL_Event mouseEvent {};
	if (event.type == SDL_FINGERMOTION) {
		mouseEvent.motion.type = SDL_MOUSEMOTION;
		mouseEvent.motion.timestamp = event.tfinger.timestamp;
		mouseEvent.motion.windowID = event.tfinger.windowID;
		mouseEvent.motion.which = DevilutionTouchMouseId;
		mouseEvent.motion.x = MousePosition.x;
		mouseEvent.motion.y = MousePosition.y;
	} else
		mouseEvent = MakeTouchMouseButtonEvent(event.tfinger, event.type == SDL_FINGERDOWN ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP, SDL_BUTTON_LEFT, MousePosition);
	event = mouseEvent;
}

void ConvertTouchTapToMouseClick(SDL_Event &event, Point position)
{
	const SDL_TouchFingerEvent fingerEvent = event.tfinger;
	event = MakeTouchMouseButtonEvent(fingerEvent, SDL_MOUSEBUTTONDOWN, SDL_BUTTON_LEFT, position);
	SDL_Event mouseUp = MakeTouchMouseButtonEvent(fingerEvent, SDL_MOUSEBUTTONUP, SDL_BUTTON_LEFT, position);
	SDL_PushEvent(&mouseUp);
}

void RestoreInventoryTouchTarget()
{
	MousePosition = InventoryTouchGesture.primaryPosition;
	SetPointAndClick(true);
	InvalidateInventorySlot();
	CheckCursMove(true);
}

bool HandleInventoryTouchGesture(SDL_Event &event)
{
	constexpr Uint32 TwoFingerTapWindowMs = 450;

	// Preserve only a genuinely new second finger in the current timing window.
	// Any other down starts a clean gesture, recovering from a missing finger-up.
	if (IsInventoryTouchGestureActive() && event.type == SDL_FINGERDOWN) {
		const bool isNewSecondFinger = InventoryTouchGesture.primaryDown
		    && !InventoryTouchGesture.secondaryDown
		    && event.tfinger.fingerId != InventoryTouchGesture.primaryFinger
		    && event.tfinger.timestamp - InventoryTouchGesture.startedAt <= TwoFingerTapWindowMs;
		if (!isNewSecondFinger)
			InventoryTouchGesture.Reset();
	}

	if (!InventoryTouchGesture.primaryDown) {
		if (event.type != SDL_FINGERDOWN
		    || (pcursinvitem == -1 && pcursstashitem == StashStruct::EmptyCell))
			return false;

		InventoryTouchGesture.touchId = event.tfinger.touchId;
		InventoryTouchGesture.primaryFinger = event.tfinger.fingerId;
		InventoryTouchGesture.startedAt = event.tfinger.timestamp;
		InventoryTouchGesture.primaryPosition = MousePosition;
		InventoryTouchGesture.primaryDown = true;
		return true;
	}

	if (event.tfinger.touchId != InventoryTouchGesture.touchId)
		return false;

	if (event.tfinger.fingerId == InventoryTouchGesture.primaryFinger) {
		if (event.type == SDL_FINGERMOTION)
			return false;
		if (event.type != SDL_FINGERUP)
			return true;

		if (InventoryTouchGesture.usedItem) {
			InventoryTouchGesture.Reset();
			return true;
		}
		InventoryTouchGesture.primaryDown = false;
		RestoreInventoryTouchTarget();
		ConvertTouchTapToMouseClick(event, InventoryTouchGesture.primaryPosition);
		if (!InventoryTouchGesture.secondaryDown)
			InventoryTouchGesture.Reset();
		return true;
	}

	if (event.type == SDL_FINGERDOWN && !InventoryTouchGesture.secondaryDown) {
		InventoryTouchGesture.secondaryFinger = event.tfinger.fingerId;
		InventoryTouchGesture.secondaryDown = true;

		const Uint32 elapsed = event.tfinger.timestamp - InventoryTouchGesture.startedAt;
		if (elapsed <= TwoFingerTapWindowMs) {
			RestoreInventoryTouchTarget();
			if (invflag)
				PerformSecondaryAction();
			else
				UseInvItem(pcursinvitem);
			InventoryTouchGesture.usedItem = true;
		}
		return true;
	}

	if (event.tfinger.fingerId != InventoryTouchGesture.secondaryFinger)
		return true;
	if (event.type == SDL_FINGERUP) {
		if (InventoryTouchGesture.usedItem) {
			InventoryTouchGesture.Reset();
			return true;
		}
		InventoryTouchGesture.secondaryDown = false;
		if (!InventoryTouchGesture.primaryDown)
			InventoryTouchGesture.Reset();
	}
	return true;
}
#endif

bool HandleGameMenuInteraction(const SDL_Event &event)
{
	if (!gmenu_is_active())
		return false;
	if (event.type == SDL_FINGERDOWN && gmenu_left_mouse(true, true))
		return true;
	if (event.type == SDL_FINGERMOTION && gmenu_on_mouse_move())
		return true;
	return event.type == SDL_FINGERUP && gmenu_left_mouse(false);
}

bool HandleStoreInteraction(const SDL_Event &event)
{
	if (stextflag == TalkID::None) {
#ifdef __IPHONEOS__
		StoreTouchGesture.Reset();
#endif
		return false;
	}

#ifdef __IPHONEOS__
	constexpr int ScrollStep = 32;
	if (event.type == SDL_FINGERDOWN) {
		StoreTouchGesture.touchId = event.tfinger.touchId;
		StoreTouchGesture.fingerId = event.tfinger.fingerId;
		StoreTouchGesture.lastY = MousePosition.y;
		StoreTouchGesture.scrolled = false;
		return true;
	}

	if (event.tfinger.touchId != StoreTouchGesture.touchId
	    || event.tfinger.fingerId != StoreTouchGesture.fingerId)
		return true;

	if (event.type == SDL_FINGERMOTION) {
		while (MousePosition.y <= StoreTouchGesture.lastY - ScrollStep) {
			StoreDown();
			StoreTouchGesture.lastY -= ScrollStep;
			StoreTouchGesture.scrolled = true;
		}
		while (MousePosition.y >= StoreTouchGesture.lastY + ScrollStep) {
			StoreUp();
			StoreTouchGesture.lastY += ScrollStep;
			StoreTouchGesture.scrolled = true;
		}
		return true;
	}

	if (event.type == SDL_FINGERUP) {
		if (!StoreTouchGesture.scrolled)
			CheckStoreBtn(true);
		ReleaseStoreBtn();
		StoreTouchGesture.Reset();
	}
#else
	if (event.type == SDL_FINGERDOWN)
		CheckStoreBtn(true);
#endif
	return true;
}

void HandleSpellBookInteraction(const SDL_Event &event)
{
	if (!sbookflag)
		return;

	if (event.type == SDL_FINGERUP)
		CheckSBook();
}

bool HandleSpeedBookInteraction(const SDL_Event &event)
{
	if (!spselflag)
		return false;
	if (event.type == SDL_FINGERUP)
		SetSpell();
	return true;
}

void HandleBottomPanelInteraction(const SDL_Event &event)
{
	if (!gbRunGame || !MyPlayer->HoldItem.isEmpty())
		return;

	ClearPanBtn();

	if (event.type != SDL_FINGERUP) {
		spselflag = true;
		DoPanBtn();
		spselflag = false;
	} else {
		DoPanBtn();
		if (panbtndown)
			CheckBtnUp();
	}
}

void HandleCharacterPanelInteraction(const SDL_Event &event)
{
	if (!chrflag)
		return;

	if (event.type == SDL_FINGERDOWN)
		CheckChrBtns();
	else if (event.type == SDL_FINGERUP && chrbtnactive)
		ReleaseChrBtns(false);
}

void HandleStashPanelInteraction(const SDL_Event &event)
{
	if (!IsStashOpen || !MyPlayer->HoldItem.isEmpty())
		return;

	if (event.type != SDL_FINGERUP) {
		CheckStashButtonPress(MousePosition);
	} else {
		CheckStashButtonRelease(MousePosition);
	}
}

} // namespace

bool IsDirectTouchEvent(const SDL_TouchFingerEvent &event)
{
	const SDL_TouchDeviceType deviceType = SDL_GetTouchDeviceType(event.touchId);
	return deviceType != SDL_TOUCH_DEVICE_INDIRECT_ABSOLUTE
	    && deviceType != SDL_TOUCH_DEVICE_INDIRECT_RELATIVE;
}

void HandleTouchEvent(SDL_Event &event)
{
#ifdef __IPHONEOS__
	// A real pointer action supersedes any incomplete direct-touch gesture. SDL's
	// synthetic touch-mouse stream is excluded so a normal tap stays intact.
	const bool isRealPointerButton = event.type == SDL_MOUSEBUTTONDOWN
	    && event.button.which != SDL_TOUCH_MOUSEID && event.button.which != DevilutionTouchMouseId;
	if (IsInventoryTouchGestureActive() && isRealPointerButton) {
		InventoryTouchGesture.Reset();
	}

	// Trackpads already arrive through SDL's mouse path. Ignore their optional
	// companion finger stream before it can change any direct-touch state.
	if (IsAnyOf(event.type, SDL_FINGERDOWN, SDL_FINGERUP, SDL_FINGERMOTION)
	    && !IsDirectTouchEvent(event.tfinger)) {
		event.type = SDL_FIRSTEVENT;
		return;
	}
#endif

	SetPointAndClick(false);

	if (Handler.Handle(event)) {
		return;
	}

	if (!IsAnyOf(event.type, SDL_FINGERDOWN, SDL_FINGERUP, SDL_FINGERMOTION)) {
		return;
	}

	if (!SimulateMouseMovement(event)) {
#ifdef __IPHONEOS__
		if (IsInventoryTouchGestureActive()) {
			if (event.type == SDL_FINGERUP && event.tfinger.fingerId == InventoryTouchGesture.primaryFinger)
				InventoryTouchGesture.Reset();
			else
				HandleInventoryTouchGesture(event);
		}
		event.type = SDL_FIRSTEVENT;
#endif
		return;
	}

#ifdef __IPHONEOS__
	if (HandleGameMenuInteraction(event))
		return;

	if (HandleStoreInteraction(event))
		return;

	if (HandleInventoryTouchGesture(event)) {
		if (IsAnyOf(event.type, SDL_FINGERDOWN, SDL_FINGERUP, SDL_FINGERMOTION))
			event.type = SDL_FIRSTEVENT;
		return;
	}

	// Preserve Diablo's native pointer behavior on iPad: a touch selects the
	// current target, then the normal left-click path walks, interacts, attacks,
	// or uses the tapped panel item.
	ConvertTouchToMouseEvent(event);
	return;
#endif

	if (HandleGameMenuInteraction(event))
		return;

	if (HandleStoreInteraction(event))
		return;

	if (HandleSpeedBookInteraction(event))
		return;

	HandleSpellBookInteraction(event);
	HandleBottomPanelInteraction(event);
	HandleCharacterPanelInteraction(event);
	HandleStashPanelInteraction(event);
}

bool VirtualGamepadEventHandler::Handle(const SDL_Event &event)
{
#ifdef __IPHONEOS__
	if (!*sgOptions.Controller.showTouchControls || IsLeftPanelOpen() || IsRightPanelOpen()) {
		VirtualGamepadState.Deactivate();
		VirtualGamepadState.isActive = true;
		return false;
	}
#endif

	if (!VirtualGamepadState.isActive || !IsAnyOf(event.type, SDL_FINGERDOWN, SDL_FINGERUP, SDL_FINGERMOTION)) {
		VirtualGamepadState.primaryActionButton.didStateChange = false;
		VirtualGamepadState.secondaryActionButton.didStateChange = false;
		VirtualGamepadState.spellActionButton.didStateChange = false;
		VirtualGamepadState.cancelButton.didStateChange = false;
		return false;
	}

#ifndef __IPHONEOS__
	if (charMenuButtonEventHandler.Handle(event))
		return true;

	if (questsMenuButtonEventHandler.Handle(event))
		return true;

	if (inventoryMenuButtonEventHandler.Handle(event))
		return true;

	if (mapMenuButtonEventHandler.Handle(event))
		return true;
#endif

	if (directionPadEventHandler.Handle(event))
		return true;

	if (leveltype != DTYPE_TOWN && standButtonEventHandler.Handle(event))
		return true;

	if (primaryActionButtonEventHandler.Handle(event))
		return true;

	if (secondaryActionButtonEventHandler.Handle(event))
		return true;

	if (spellActionButtonEventHandler.Handle(event))
		return true;

	if (cancelButtonEventHandler.Handle(event))
		return true;

#ifndef __IPHONEOS__
	if (healthButtonEventHandler.Handle(event))
		return true;

	if (manaButtonEventHandler.Handle(event))
		return true;
#endif

	return false;
}

bool VirtualDirectionPadEventHandler::Handle(const SDL_Event &event)
{
	switch (event.type) {
	case SDL_FINGERDOWN:
		return HandleFingerDown(event.tfinger);

	case SDL_FINGERUP:
		return HandleFingerUp(event.tfinger);

	case SDL_FINGERMOTION:
		return HandleFingerMotion(event.tfinger);

	default:
		return false;
	}
}

bool VirtualDirectionPadEventHandler::HandleFingerDown(const SDL_TouchFingerEvent &event)
{
	if (isActive)
		return false;

	float x = event.x;
	float y = event.y;

	Point touchCoordinates = ScaleToScreenCoordinates(x, y);
	if (!virtualDirectionPad->area.contains(touchCoordinates))
		return false;

	virtualDirectionPad->UpdatePosition(touchCoordinates);
	activeFinger = event.fingerId;
	isActive = true;
	return true;
}

bool VirtualDirectionPadEventHandler::HandleFingerUp(const SDL_TouchFingerEvent &event)
{
	if (!isActive || event.fingerId != activeFinger)
		return false;

	Point position = virtualDirectionPad->area.position;
	virtualDirectionPad->UpdatePosition(position);
	isActive = false;
	return true;
}

bool VirtualDirectionPadEventHandler::HandleFingerMotion(const SDL_TouchFingerEvent &event)
{
	if (!isActive || event.fingerId != activeFinger)
		return false;

	float x = event.x;
	float y = event.y;

	Point touchCoordinates = ScaleToScreenCoordinates(x, y);
	virtualDirectionPad->UpdatePosition(touchCoordinates);
	return true;
}

bool VirtualButtonEventHandler::Handle(const SDL_Event &event)
{
	if (!virtualButton->isUsable()) {
		virtualButton->didStateChange = virtualButton->isHeld;
		virtualButton->isHeld = false;
		return false;
	}

	virtualButton->didStateChange = false;

	switch (event.type) {
	case SDL_FINGERDOWN:
		return HandleFingerDown(event.tfinger);

	case SDL_FINGERUP:
		return HandleFingerUp(event.tfinger);

	case SDL_FINGERMOTION:
		return HandleFingerMotion(event.tfinger);

	default:
		return false;
	}
}

bool VirtualButtonEventHandler::HandleFingerDown(const SDL_TouchFingerEvent &event)
{
	if (isActive)
		return false;

	float x = event.x;
	float y = event.y;

	Point touchCoordinates = ScaleToScreenCoordinates(x, y);
	if (!virtualButton->contains(touchCoordinates))
		return false;

	if (toggles)
		virtualButton->isHeld = !virtualButton->isHeld;
	else
		virtualButton->isHeld = true;

	virtualButton->didStateChange = true;
	activeFinger = event.fingerId;
	isActive = true;
	return true;
}

bool VirtualButtonEventHandler::HandleFingerUp(const SDL_TouchFingerEvent &event)
{
	if (!isActive || event.fingerId != activeFinger)
		return false;

	if (!toggles) {
		if (virtualButton->isHeld)
			virtualButton->didStateChange = true;
		virtualButton->isHeld = false;
	}

	isActive = false;
	return true;
}

bool VirtualButtonEventHandler::HandleFingerMotion(const SDL_TouchFingerEvent &event)
{
	if (!isActive || event.fingerId != activeFinger)
		return false;

	if (toggles)
		return true;

	float x = event.x;
	float y = event.y;
	Point touchCoordinates = ScaleToScreenCoordinates(x, y);

	bool wasHeld = virtualButton->isHeld;
	virtualButton->isHeld = virtualButton->contains(touchCoordinates);
	virtualButton->didStateChange = virtualButton->isHeld != wasHeld;

	return true;
}

} // namespace devilution
