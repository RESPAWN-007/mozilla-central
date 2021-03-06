/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Hal.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/StaticPtr.h"

#include "GamepadService.h"
#include "nsAutoPtr.h"
#include "nsIDOMEvent.h"
#include "nsIDOMDocument.h"
#include "nsIDOMEventTarget.h"
#include "nsDOMGamepad.h"
#include "nsIDOMGamepadButtonEvent.h"
#include "nsIDOMGamepadAxisMoveEvent.h"
#include "nsIDOMGamepadEvent.h"
#include "GeneratedEvents.h"
#include "nsIDOMWindow.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsIServiceManager.h"
#include "nsITimer.h"
#include "nsThreadUtils.h"
#include "mozilla/Services.h"

#include <cstddef>

namespace mozilla {
namespace dom {

namespace {
// Amount of time to wait before cleaning up gamepad resources
// when no pages are listening for events.
const int kCleanupDelayMS = 2000;
const nsTArray<nsRefPtr<nsGlobalWindow> >::index_type NoIndex =
    nsTArray<nsRefPtr<nsGlobalWindow> >::NoIndex;

StaticRefPtr<GamepadService> gGamepadServiceSingleton;

} // namespace

bool GamepadService::sShutdown = false;

NS_IMPL_ISUPPORTS0(GamepadService)

GamepadService::GamepadService()
  : mStarted(false),
    mShuttingDown(false)
{
  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();
  observerService->AddObserver(this,
                               NS_XPCOM_WILL_SHUTDOWN_OBSERVER_ID,
                               false);
}

NS_IMETHODIMP
GamepadService::Observe(nsISupports* aSubject,
                        const char* aTopic,
                        const PRUnichar* aData)
{
  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();
  nsCOMPtr<nsIObserver> observer = do_QueryInterface(this);
  observerService->RemoveObserver(observer, NS_XPCOM_WILL_SHUTDOWN_OBSERVER_ID);

  BeginShutdown();
  return NS_OK;
}

void
GamepadService::BeginShutdown()
{
  mShuttingDown = true;
  if (mTimer) {
    mTimer->Cancel();
  }
  mozilla::hal::StopMonitoringGamepadStatus();
  mStarted = false;
  // Don't let windows call back to unregister during shutdown
  for (uint32_t i = 0; i < mListeners.Length(); i++) {
    mListeners[i]->SetHasGamepadEventListener(false);
  }
  mListeners.Clear();
  mGamepads.Clear();
  sShutdown = true;
}

void
GamepadService::AddListener(nsGlobalWindow* aWindow)
{
  if (mShuttingDown) {
    return;
  }

  if (mListeners.IndexOf(aWindow) != NoIndex) {
    return; // already exists
  }

  if (!mStarted) {
    mozilla::hal::StartMonitoringGamepadStatus();
    mStarted = true;
  }

  mListeners.AppendElement(aWindow);
}

void
GamepadService::RemoveListener(nsGlobalWindow* aWindow)
{
  if (mShuttingDown) {
    // Doesn't matter at this point. It's possible we're being called
    // as a result of our own destructor here, so just bail out.
    return;
  }

  if (mListeners.IndexOf(aWindow) == NoIndex) {
    return; // doesn't exist
  }

  mListeners.RemoveElement(aWindow);

  if (mListeners.Length() == 0 && !mShuttingDown) {
    StartCleanupTimer();
  }
}

uint32_t
GamepadService::AddGamepad(const char* aId,
                           uint32_t aNumButtons,
                           uint32_t aNumAxes)
{
  //TODO: bug 852258: get initial button/axis state
  nsRefPtr<nsDOMGamepad> gamepad =
    new nsDOMGamepad(nullptr,
                     NS_ConvertUTF8toUTF16(nsDependentCString(aId)),
                     0,
                     aNumButtons,
                     aNumAxes);
  int index = -1;
  for (uint32_t i = 0; i < mGamepads.Length(); i++) {
    if (!mGamepads[i]) {
      mGamepads[i] = gamepad;
      index = i;
      break;
    }
  }
  if (index == -1) {
    mGamepads.AppendElement(gamepad);
    index = mGamepads.Length() - 1;
  }

  gamepad->SetIndex(index);
  NewConnectionEvent(index, true);

  return index;
}

void
GamepadService::RemoveGamepad(uint32_t aIndex)
{
  if (aIndex < mGamepads.Length()) {
    mGamepads[aIndex]->SetConnected(false);
    NewConnectionEvent(aIndex, false);
    // If this is the last entry in the list, just remove it.
    if (aIndex == mGamepads.Length() - 1) {
      mGamepads.RemoveElementAt(aIndex);
    } else {
      // Otherwise just null it out and leave it, so the
      // indices of the following entries remain valid.
      mGamepads[aIndex] = nullptr;
    }
  }
}

void
GamepadService::NewButtonEvent(uint32_t aIndex, uint32_t aButton, bool aPressed)
{
  if (mShuttingDown || aIndex >= mGamepads.Length()) {
    return;
  }

  double value = aPressed ? 1.0L : 0.0L;
  mGamepads[aIndex]->SetButton(aButton, value);

  // Hold on to listeners in a separate array because firing events
  // can mutate the mListeners array.
  nsTArray<nsRefPtr<nsGlobalWindow> > listeners(mListeners);

  for (uint32_t i = listeners.Length(); i > 0 ; ) {
    --i;

    // Only send events to non-background windows
    if (!listeners[i]->GetOuterWindow() ||
        listeners[i]->GetOuterWindow()->IsBackground()) {
      continue;
    }

    if (!WindowHasSeenGamepad(listeners[i], aIndex)) {
      SetWindowHasSeenGamepad(listeners[i], aIndex);
      // This window hasn't seen this gamepad before, so
      // send a connection event first.
      NewConnectionEvent(aIndex, true);
    }

    nsRefPtr<nsDOMGamepad> gamepad = listeners[i]->GetGamepad(aIndex);
    if (gamepad) {
      gamepad->SetButton(aButton, value);
      // Fire event
      FireButtonEvent(listeners[i], gamepad, aButton, value);
    }
  }
}

void
GamepadService::FireButtonEvent(EventTarget* aTarget,
                                nsDOMGamepad* aGamepad,
                                uint32_t aButton,
                                double aValue)
{
  nsCOMPtr<nsIDOMEvent> event;
  bool defaultActionEnabled = true;
  NS_NewDOMGamepadButtonEvent(getter_AddRefs(event), aTarget, nullptr, nullptr);
  nsCOMPtr<nsIDOMGamepadButtonEvent> je = do_QueryInterface(event);
  MOZ_ASSERT(je, "QI should not fail");


  nsString name = aValue == 1.0L ? NS_LITERAL_STRING("gamepadbuttondown") :
                                   NS_LITERAL_STRING("gamepadbuttonup");
  je->InitGamepadButtonEvent(name, false, false, aGamepad, aButton);
  je->SetTrusted(true);

  aTarget->DispatchEvent(event, &defaultActionEnabled);
}

void
GamepadService::NewAxisMoveEvent(uint32_t aIndex, uint32_t aAxis, double aValue)
{
  if (mShuttingDown || aIndex >= mGamepads.Length()) {
    return;
  }
  mGamepads[aIndex]->SetAxis(aAxis, aValue);

  // Hold on to listeners in a separate array because firing events
  // can mutate the mListeners array.
  nsTArray<nsRefPtr<nsGlobalWindow> > listeners(mListeners);

  for (uint32_t i = listeners.Length(); i > 0 ; ) {
    --i;

    // Only send events to non-background windows
    if (!listeners[i]->GetOuterWindow() ||
        listeners[i]->GetOuterWindow()->IsBackground()) {
      continue;
    }

    if (!WindowHasSeenGamepad(listeners[i], aIndex)) {
      SetWindowHasSeenGamepad(listeners[i], aIndex);
      // This window hasn't seen this gamepad before, so
      // send a connection event first.
      NewConnectionEvent(aIndex, true);
    }

    nsRefPtr<nsDOMGamepad> gamepad = listeners[i]->GetGamepad(aIndex);
    if (gamepad) {
      gamepad->SetAxis(aAxis, aValue);
      // Fire event
      FireAxisMoveEvent(listeners[i], gamepad, aAxis, aValue);
    }
  }
}

void
GamepadService::FireAxisMoveEvent(EventTarget* aTarget,
                                  nsDOMGamepad* aGamepad,
                                  uint32_t aAxis,
                                  double aValue)
{
  nsCOMPtr<nsIDOMEvent> event;
  bool defaultActionEnabled = true;
  NS_NewDOMGamepadAxisMoveEvent(getter_AddRefs(event), aTarget, nullptr,
                                nullptr);
  nsCOMPtr<nsIDOMGamepadAxisMoveEvent> je = do_QueryInterface(event);
  MOZ_ASSERT(je, "QI should not fail");

  je->InitGamepadAxisMoveEvent(NS_LITERAL_STRING("gamepadaxismove"),
                               false, false, aGamepad, aAxis, aValue);
  je->SetTrusted(true);

  aTarget->DispatchEvent(event, &defaultActionEnabled);
}

void
GamepadService::NewConnectionEvent(uint32_t aIndex, bool aConnected)
{
  if (mShuttingDown || aIndex >= mGamepads.Length()) {
    return;
  }

  // Hold on to listeners in a separate array because firing events
  // can mutate the mListeners array.
  nsTArray<nsRefPtr<nsGlobalWindow> > listeners(mListeners);

  if (aConnected) {
    for (uint32_t i = listeners.Length(); i > 0 ; ) {
      --i;

      // Only send events to non-background windows
      if (!listeners[i]->GetOuterWindow() ||
          listeners[i]->GetOuterWindow()->IsBackground())
        continue;

      // We don't fire a connected event here unless the window
      // has seen input from at least one device.
      if (aConnected && !listeners[i]->HasSeenGamepadInput()) {
        return;
      }

      SetWindowHasSeenGamepad(listeners[i], aIndex);

      nsRefPtr<nsDOMGamepad> gamepad = listeners[i]->GetGamepad(aIndex);
      if (gamepad) {
        // Fire event
        FireConnectionEvent(listeners[i], gamepad, aConnected);
      }
    }
  } else {
    // For disconnection events, fire one at every window that has received
    // data from this gamepad.
    for (uint32_t i = listeners.Length(); i > 0 ; ) {
      --i;

      // Even background windows get these events, so we don't have to
      // deal with the hassle of syncing the state of removed gamepads.

      if (WindowHasSeenGamepad(listeners[i], aIndex)) {
        nsRefPtr<nsDOMGamepad> gamepad = listeners[i]->GetGamepad(aIndex);
        if (gamepad) {
          gamepad->SetConnected(false);
          // Fire event
          FireConnectionEvent(listeners[i], gamepad, false);
        }

        if (gamepad) {
          listeners[i]->RemoveGamepad(aIndex);
        }
      }
    }
  }
}

void
GamepadService::FireConnectionEvent(EventTarget* aTarget,
                                    nsDOMGamepad* aGamepad,
                                    bool aConnected)
{
  nsCOMPtr<nsIDOMEvent> event;
  bool defaultActionEnabled = true;
  NS_NewDOMGamepadEvent(getter_AddRefs(event), aTarget, nullptr, nullptr);
  nsCOMPtr<nsIDOMGamepadEvent> je = do_QueryInterface(event);
  MOZ_ASSERT(je, "QI should not fail");

  nsString name = aConnected ? NS_LITERAL_STRING("gamepadconnected") :
                               NS_LITERAL_STRING("gamepaddisconnected");
  je->InitGamepadEvent(name, false, false, aGamepad);
  je->SetTrusted(true);

  aTarget->DispatchEvent(event, &defaultActionEnabled);
}

void
GamepadService::SyncGamepadState(uint32_t aIndex, nsDOMGamepad* aGamepad)
{
  if (mShuttingDown || aIndex > mGamepads.Length()) {
    return;
  }

  aGamepad->SyncState(mGamepads[aIndex]);
}

// static
already_AddRefed<GamepadService>
GamepadService::GetService()
{
  if (sShutdown) {
    return nullptr;
  }

  if (!gGamepadServiceSingleton) {
    gGamepadServiceSingleton = new GamepadService();
    ClearOnShutdown(&gGamepadServiceSingleton);
  }
  nsRefPtr<GamepadService> service(gGamepadServiceSingleton);
  return service.forget();
}

bool
GamepadService::WindowHasSeenGamepad(nsGlobalWindow* aWindow, uint32_t aIndex)
{
  nsRefPtr<nsDOMGamepad> gamepad = aWindow->GetGamepad(aIndex);
  return gamepad != nullptr;
}

void
GamepadService::SetWindowHasSeenGamepad(nsGlobalWindow* aWindow,
                                        uint32_t aIndex,
                                        bool aHasSeen)
{
  if (mListeners.IndexOf(aWindow) == NoIndex) {
    // This window isn't even listening for gamepad events.
    return;
  }

  if (aHasSeen) {
    aWindow->SetHasSeenGamepadInput(true);
    nsCOMPtr<nsISupports> window = nsGlobalWindow::ToSupports(aWindow);
    nsRefPtr<nsDOMGamepad> gamepad = mGamepads[aIndex]->Clone(window);
    aWindow->AddGamepad(aIndex, gamepad);
  } else {
    aWindow->RemoveGamepad(aIndex);
  }
}

// static
void
GamepadService::TimeoutHandler(nsITimer* aTimer, void* aClosure)
{
  // the reason that we use self, instead of just using nsITimerCallback or nsIObserver
  // is so that subclasses are free to use timers without worry about the base classes's
  // usage.
  GamepadService* self = reinterpret_cast<GamepadService*>(aClosure);
  if (!self) {
    NS_ERROR("no self");
    return;
  }

  if (self->mShuttingDown) {
    return;
  }

  if (self->mListeners.Length() == 0) {
    mozilla::hal::StopMonitoringGamepadStatus();
    self->mStarted = false;
    if (!self->mGamepads.IsEmpty()) {
      self->mGamepads.Clear();
    }
  }
}

void
GamepadService::StartCleanupTimer()
{
  if (mTimer) {
    mTimer->Cancel();
  }

  mTimer = do_CreateInstance("@mozilla.org/timer;1");
  if (mTimer) {
    mTimer->InitWithFuncCallback(TimeoutHandler,
                                 this,
                                 kCleanupDelayMS,
                                 nsITimer::TYPE_ONE_SHOT);
  }
}

/*
 * Implementation of the test service. This is just to provide a simple binding
 * of the GamepadService to JavaScript via XPCOM so that we can write Mochitests
 * that add and remove fake gamepads, avoiding the platform-specific backends.
 */
NS_IMPL_ISUPPORTS1(GamepadServiceTest, nsIGamepadServiceTest)

GamepadServiceTest* GamepadServiceTest::sSingleton = nullptr;

// static
already_AddRefed<GamepadServiceTest>
GamepadServiceTest::CreateService()
{
  if (sSingleton == nullptr) {
    sSingleton = new GamepadServiceTest();
  }
  nsRefPtr<GamepadServiceTest> service = sSingleton;
  return service.forget();
}

GamepadServiceTest::GamepadServiceTest()
{
  /* member initializers and constructor code */
}

/* uint32_t addGamepad (in string id, in uint32_t numButtons, in uint32_t numAxes); */
NS_IMETHODIMP GamepadServiceTest::AddGamepad(const char* aID,
                                             uint32_t aNumButtons,
                                             uint32_t aNumAxes,
                                             uint32_t* aRetval)
{
  *aRetval = gGamepadServiceSingleton->AddGamepad(aID,
                                                  aNumButtons,
                                                  aNumAxes);
  return NS_OK;
}

/* void removeGamepad (in uint32_t index); */
NS_IMETHODIMP GamepadServiceTest::RemoveGamepad(uint32_t aIndex)
{
  gGamepadServiceSingleton->RemoveGamepad(aIndex);
  return NS_OK;
}

/* void newButtonEvent (in uint32_t index, in uint32_t button,
   in boolean pressed); */
NS_IMETHODIMP GamepadServiceTest::NewButtonEvent(uint32_t aIndex,
                                                 uint32_t aButton,
                                                 bool aPressed)
{
  gGamepadServiceSingleton->NewButtonEvent(aIndex, aButton, aPressed);
  return NS_OK;
}

/* void newAxisMoveEvent (in uint32_t index, in uint32_t axis,
   in double value); */
NS_IMETHODIMP GamepadServiceTest::NewAxisMoveEvent(uint32_t aIndex,
                                                   uint32_t aAxis,
                                                   double aValue)
{
  gGamepadServiceSingleton->NewAxisMoveEvent(aIndex, aAxis, aValue);
  return NS_OK;
}

} // namespace dom
} // namespace mozilla
