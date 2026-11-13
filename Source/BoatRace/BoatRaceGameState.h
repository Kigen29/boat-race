#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "RaceWeather.h"
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

	/** Elimination: last place is knocked out every time the leader completes a lap. */
	UPROPERTY(Replicated)
	bool bElimination = false;

	/** Server only: a line in everyone's feed ("Osprey eliminated"). */
	void Announce(const FString& Text);

	/** Time trial: alone against the ghost of your best lap. */
	UPROPERTY(Replicated)
	bool bTimeTrial = false;

	/** 0 no rivals, 1 easy, 2 normal, 3 hard. Shown in everyone's lobby. */
	UPROPERTY(Replicated)
	uint8 RivalLevel = 2;

	/** ERaceWeather as a byte; the host picks it in the lobby. */
	UPROPERTY(Replicated)
	uint8 WeatherIndex = 0;

	/** The second track: the bay driven clockwise. Changing it rebuilds the
	 *  bay on every machine, so only between races. */
	UPROPERTY(ReplicatedUsing = OnRep_Reverse)
	bool bReverse = false;

	/** Server only. */
	void SetReverse(bool bInReverse);

	ERaceWeather GetWeather() const { return RaceWeather::FromIndex(WeatherIndex); }

	/** Server only. */
	void SetWeather(ERaceWeather Weather) { WeatherIndex = static_cast<uint8>(Weather); }

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnRep_Reverse();

	void BuildBay();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastWreck(const FString& Victim, const FString& Attacker);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastNotice(const FString& Text);

private:
	TArray<FBoatKillFeedLine> KillFeed;
};
