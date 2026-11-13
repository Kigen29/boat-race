#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "BoatRaceGameMode.generated.h"

class ABoatPawn;
class ABoatRaceGameState;
class ABoatRivalController;
class APlayerController;

/**
 * The rules, run only on the server: admit up to four players, give each a
 * grid slot and a boat, run the countdown, count gates and laps, rebuild
 * wrecked boats and publish the results.
 */
UCLASS(Config = Game)
class ABoatRaceGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ABoatRaceGameMode();

	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void BeginPlay() override;
	virtual void Logout(AController* Exiting) override;

	/** From the host's TAB key in the lobby: the next weather (or the one before). */
	void RequestWeatherChange(APlayerController* Requester, int32 Step);

	/** Puts a boat back just past the last gate it passed (a rival that is stuck). */
	void ResetBoatToLastGate(ABoatPawn* Boat);

	/** From a player's ENTER key. Only the host may start a race. */
	void RequestStartRace(APlayerController* Requester);

	/** From a player's R key: back to the last gate passed. */
	void RequestResetBoat(APlayerController* Requester);

	UPROPERTY(Config)
	int32 TotalLaps = 2;

	UPROPERTY(Config)
	float CountdownSeconds = 4.f;

	/** How long the standings stay up before returning to the lobby. */
	UPROPERTY(Config)
	float ResultsSeconds = 25.f;

	/** Once the winner is home, the rest have this long to finish. */
	UPROPERTY(Config)
	float FinishGraceSeconds = 60.f;

	/** How long a wrecked boat drifts before it is rebuilt. */
	UPROPERTY(Config)
	float WreckSeconds = 4.f;

	/** Protection from damage after being rebuilt. */
	UPROPERTY(Config)
	float RespawnInvulnerableSeconds = 3.f;

	/** Fill empty grid slots with computer rivals when a race starts. */
	UPROPERTY(Config)
	bool bFillWithRivals = true;

	/** 0.6 gentle .. 1 flat out. */
	UPROPERTY(Config)
	float RivalSkill = 0.9f;

	/** Weather for a new race, by name (Sunny, Cloudy, Rainy, Stormy, Windy,
	 *  Snowy, Foggy, Sunset, Night). -Weather=Snowy on the command line wins. */
	UPROPERTY(Config)
	FString Weather = TEXT("Sunny");

private:
	int32 FindFreeSlot() const;
	void SpawnRivals(ABoatRaceGameState* RaceState);
	void RemoveRivals(int32 Keep);

	UPROPERTY()
	TArray<TObjectPtr<ABoatRivalController>> Rivals;
	void StartCountdown(ABoatRaceGameState* RaceState);
	void StartRacing(ABoatRaceGameState* RaceState);
	void UpdateRacing(ABoatRaceGameState* RaceState);
	void UpdateWrecks(ABoatRaceGameState* RaceState);
	void EndRace(ABoatRaceGameState* RaceState);

	int32 FinishedCount = 0;
	float FirstFinishServerTime = -1.f;
};
