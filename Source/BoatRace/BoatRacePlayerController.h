#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "BoatRacePlayerController.generated.h"

/** Which menu is on screen. */
enum class EMenuScreen : uint8
{
	None,
	Main,
	Pause,
	Settings,
	Controls
};

enum class EMenuAction : uint8
{
	None,
	Solo,
	Host,
	JoinFound,
	JoinAddress,
	Weather,
	Laps,
	Rivals,
	Camera,
	Steering,
	Graphics,
	Rumble,
	Music,
	MusicVolume,
	EffectsVolume,
	Settings,
	Controls,
	Resume,
	StartRace,
	Restart,
	ResetBoat,
	Leave,
	BackToMenu,
	Quit,
	Back
};

/** One line of a menu: a label on the left, an optional value on the right. */
struct FMenuItem
{
	FString Label;
	FString Value;
	FString Hint;
	EMenuAction Action = EMenuAction::None;
	int32 Data = 0;
	bool bEnabled = true;
	/** Left / right changes the value (weather, volumes). */
	bool bAdjustable = false;
};

/**
 * Handles the keys that are not about driving: hosting, joining a race found
 * on the network or by address,
 * starting a race, resetting the boat and leaving.
 */
UCLASS()
class ABoatRacePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	virtual void PlayerTick(float DeltaTime) override;

	/** True while the player is typing a host address on the menu. */
	bool IsTypingAddress() const { return bTypingAddress; }
	const FString& GetTypedAddress() const { return TypedAddress; }

	/** Any menu is open: the boat ignores the driving keys meanwhile. */
	bool IsMenuOpen() const { return MenuScreen != EMenuScreen::None || bTypingAddress; }
	EMenuScreen GetMenuScreen() const { return MenuScreen; }
	int32 GetMenuSelection() const { return MenuSelection; }

	/** The lines of the open menu, rebuilt every frame from the game's state. */
	void BuildMenuItems(TArray<FMenuItem>& OutItems) const;

	/** Standalone and not racing: the title screen. */
	bool IsOnMainMenu() const;

protected:
	UFUNCTION(Server, Reliable)
	void ServerRequestStart();

	UFUNCTION(Server, Reliable)
	void ServerRequestReset();

	UFUNCTION(Server, Reliable)
	void ServerRequestWeather(int32 Step);

	UFUNCTION(Server, Reliable)
	void ServerRequestRestart();

	UFUNCTION(Server, Reliable)
	void ServerRaceOption(int32 Option, int32 Step);

private:
	void HandleAddressTyping();
	/** Number keys on the menu join the races listed under "on your Wi-Fi". */
	void JoinFoundRaceOnKeyPress();
	void HostRace();
	void JoinRace(const FString& Address);
	void LeaveRace();
	/** -PhotoTour: drives a scripted tour of every weather and takes screenshots. */
	void RunPhotoTour(float DeltaTime);

	void HandleMenuInput();
	void ActivateMenuItem(const FMenuItem& Item, int32 Step);
	void OpenMenu(EMenuScreen Screen);
	void SyncCursor();

	EMenuScreen MenuScreen = EMenuScreen::None;
	/** Where Back goes from Settings or Controls. */
	EMenuScreen MenuParent = EMenuScreen::Main;
	int32 MenuSelection = 0;
	FVector2D LastMouse = FVector2D(-1.f, -1.f);
	bool bCursorShown = false;

	float TourClock = -1.f;
	int32 TourStep = -1;

	bool bTypingAddress = false;
	FString TypedAddress;
};
