#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "BoatGarage.h"
#include "BoatRacePlayerController.generated.h"

/** Which menu is on screen. */
enum class EMenuScreen : uint8
{
	None,
	Main,
	Pause,
	Settings,
	Controls,
	Garage,
	Leaderboard
};

enum class EMenuAction : uint8
{
	None,
	Solo,
	TimeTrial,
	Host,
	JoinFound,
	JoinAddress,
	Weather,
	Laps,
	Rivals,
	Mode,
	Track,
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
	PhotoMode,
	StartRace,
	Restart,
	ResetBoat,
	Leave,
	BackToMenu,
	Garage,
	Leaderboard,
	GarageBoat,
	GaragePaint,
	GarageFlame,
	GarageUpgrade,
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
	bool IsMenuOpen() const { return MenuScreen != EMenuScreen::None || bTypingAddress || bPhotoMode; }

	/** Free camera for screenshots: no HUD, game paused when playing solo. */
	bool IsInPhotoMode() const { return bPhotoMode; }
	/** True while the finish replay is on screen. */
	bool IsReplaying() const;
	/** Seconds since the last photo was saved (for the HUD's note). */
	float GetSecondsSincePhoto() const { return PhotoClock; }
	EMenuScreen GetMenuScreen() const { return MenuScreen; }
	int32 GetMenuSelection() const { return MenuSelection; }

	/** The race's personal tally, shown under the results. */
	struct FScoreCard
	{
		bool bValid = false;
		/** What each line paid, in coins. */
		TArray<TPair<FString, int32>> Lines;
		int32 Coins = 0;
		int32 Score = 0;
		/** Place on this machine's leaderboard, 0-based; -1 off it. */
		int32 Rank = -1;
		int32 Wallet = 0;
	};
	const FScoreCard& GetScoreCard() const { return ScoreCard; }

	/** Coins picked up on the course since the green light. */
	int32 GetRaceCoins() const;

	/** The boat the garage is showing (owned or not). */
	FBoatLoadout GetGarageLoadout() const;
	int32 GetGarageBoat() const { return GarageBoat; }
	/** The menus are looking into the boathouse rather than at the race. */
	bool IsShowroomView() const;
	/** A line under the garage ("Not enough coins"), and its age. */
	const FString& GetGarageMessage(float& OutAge) const;

	/** The lines of the open menu, rebuilt every frame from the game's state. */
	void BuildMenuItems(TArray<FMenuItem>& OutItems) const;

	/** Standalone and not racing: the title screen. */
	bool IsOnMainMenu() const;

	/** Time trial: seconds ahead (negative) or behind the ghost at the last gate. */
	bool GetGhostDelta(float& OutDelta, float& OutAge) const;
	float GetGhostLapTime() const;

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
	void ServerRequestTimeTrial();

	UFUNCTION(Server, Reliable)
	void ServerRaceOption(int32 Option, int32 Step);

	/** The garage's choice; taken at once in the lobby, else from the next race. */
	UFUNCTION(Server, Reliable)
	void ServerSetLoadout(FBoatLoadout Loadout);

private:
	void HandleAddressTyping();
	/** Number keys on the menu join the races listed under "on your Wi-Fi". */
	void JoinFoundRaceOnKeyPress();
	void HostRace();
	void JoinRace(const FString& Address);
	void LeaveRace();
	/** -PhotoTour: drives a scripted tour of every weather and takes screenshots. */
	void RunPhotoTour(float DeltaTime);

	void UpdateTimeTrial(float DeltaTime);
	void EnterPhotoMode();
	void ExitPhotoMode();
	void UpdatePhotoMode(float DeltaTime);

	bool bPhotoMode = false;
	float PhotoClock = 100.f;
	int32 PhotoCount = 0;
	UPROPERTY()
	TObjectPtr<class ACameraActor> PhotoCamera;

	UPROPERTY()
	TObjectPtr<class AFinishReplay> Replay;

	// The garage: which boat, paint and flame are being looked at, the
	// loadout last sent to the server, and the turntable camera.
	bool bGraphicsApplied = false;
	int32 GarageBoat = 0;
	int32 GaragePaint = 0;
	int32 GarageFlame = 0;
	FString GarageMessage;
	float GarageMessageAt = -100.f;
	bool bLoadoutSent = false;
	FBoatLoadout SentLoadout;
	bool bGarageView = false;
	UPROPERTY()
	TObjectPtr<class ACameraActor> GarageCamera;
	UPROPERTY()
	TObjectPtr<class ACourseCoins> CourseCoins;
	UPROPERTY()
	TObjectPtr<class AGarageShowroom> Showroom;
	FScoreCard ScoreCard;

	void SendLoadout(const FBoatLoadout& Loadout);
	void UpdateGarage(float DeltaTime);
	void GarageSay(const FString& Text, bool bGood);
	void BuildScoreCard();

	/** Last race phase seen, so the replay starts on the change to Results. */
	uint8 ReplaySeenState = 0;
	void UpdateReplay(float DeltaTime);
	void HandleMenuInput();
	void ActivateMenuItem(const FMenuItem& Item, int32 Step);
	void OpenMenu(EMenuScreen Screen);
	void SyncCursor();

	// Time trial: the lap being recorded, the best lap's ghost.
	TSharedPtr<struct FGhostLap> BestGhost;
	TSharedPtr<struct FGhostLap> Recording;
	UPROPERTY()
	TObjectPtr<class AGhostBoat> Ghost;
	int32 GhostSeenLaps = -1;
	int32 GhostSeenGate = -1;
	float GhostLapStart = 0.f;
	float GhostRecordClock = 0.f;
	float GhostDelta = 0.f;
	float GhostDeltaAt = -100.f;
	bool bGhostDeltaValid = false;

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
