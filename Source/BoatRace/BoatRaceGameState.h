#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "BoatRaceGameState.generated.h"

UENUM()
enum class ERaceState : uint8
{
	/** Free roaming warm-up while players join. */
	Lobby,
	/** Boats held on the grid. */
	Countdown,
	Racing,
	/** Final standings on screen. */
	Results
};

/** One line of the kill feed, built on each machine from a multicast. */
USTRUCT()
struct FBoatKillFeedLine
{
	GENERATED_BODY()

	UPROPERTY()
	FString Text;

	/** Local time it arrived, used to fade it out. */
	UPROPERTY()
	float ArrivedAt = 0.f;
};

/** Race phase, clocks and the kill feed, shared with every player. */
UCLASS()
class ABoatRaceGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** The server's clock as seen from this machine, seconds. */
	float GetServerNow() const;

	/** Seconds since the green light, 0 outside a race. */
	float GetRaceTime() const;

	/** Server only: tell everyone a boat was wrecked. */
	void ReportWreck(const FString& Victim, const FString& Attacker);

	/** Recent kill-feed lines, newest last. */
	const TArray<FBoatKillFeedLine>& GetKillFeed() const { return KillFeed; }

	UPROPERTY(Replicated)
	ERaceState RaceState = ERaceState::Lobby;

	/** Server time at which Countdown or Results ends. */
	UPROPERTY(Replicated)
	float StateEndTime = 0.f;

	/** Server time of the green light. */
	UPROPERTY(Replicated)
	float RaceStartTime = 0.f;

	/** Race time when the Results phase began, which freezes the clock. */
	UPROPERTY(Replicated)
	float RaceEndTime = 0.f;

	UPROPERTY(Replicated)
	int32 TotalLaps = 2;

protected:
	virtual void BeginPlay() override;

	UFUNCTION(NetMulticast, Reliable)
	void MulticastWreck(const FString& Victim, const FString& Attacker);

private:
	TArray<FBoatKillFeedLine> KillFeed;
};
