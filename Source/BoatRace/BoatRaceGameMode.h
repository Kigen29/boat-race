#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "BoatRaceGameMode.generated.h"

class ABoatRaceGameState;
class APlayerController;

/**
 * The rules, run only on the server: admit up to four players, give each a
 * grid slot and a boat, run the countdown, count gates and laps, rebuild
 * wrecked boats and publish the results.
 */
UCLASS()
class ABoatRaceGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ABoatRaceGameMode();

	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual void Tick(float DeltaSeconds) override;

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

private:
	int32 FindFreeSlot() const;
	void StartCountdown(ABoatRaceGameState* RaceState);
	void StartRacing(ABoatRaceGameState* RaceState);
	void UpdateRacing(ABoatRaceGameState* RaceState);
	void UpdateWrecks(ABoatRaceGameState* RaceState);
	void EndRace(ABoatRaceGameState* RaceState);

	int32 FinishedCount = 0;
	float FirstFinishServerTime = -1.f;
};
