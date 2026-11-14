#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "BoatGarage.h"
#include "BoatPrefs.generated.h"

/** The chase camera choices (C cycles them). */
enum class EBoatCamera : uint8
{
	Chase,
	Far,
	Bumper,
	Count
};

/**
 * This player's own settings and records, kept between sessions in the
 * user's Game.ini: camera, steering feel, graphics quality and the best lap
 * and race times this machine has set. Nothing here is sent to the others.
 */
UCLASS(Config = Game)
class UBoatPrefs : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UPROPERTY(Config)
	int32 CameraMode = 0;

	/** 0.6 gentle .. 1.4 twitchy. */
	UPROPERTY(Config)
	float SteeringSensitivity = 1.f;

	/** -1 leaves the engine's own choice; 0 low, 1 medium, 2 high, 3 epic. */
	UPROPERTY(Config)
	int32 GraphicsQuality = -1;

	UPROPERTY(Config)
	bool bRumble = true;

	/** Best single lap ever, seconds; 0 until one is set. */
	UPROPERTY(Config)
	float BestLap = 0.f;

	/** Best whole race, per lap count ("2" -> seconds; "R2" reversed, and
	 *  "RLap" the reverse circuit's best lap). */
	UPROPERTY(Config)
	TMap<FString, float> BestRace;

	EBoatCamera GetCamera() const { return static_cast<EBoatCamera>(FMath::Clamp(CameraMode, 0, static_cast<int32>(EBoatCamera::Count) - 1)); }
	void CycleCamera(int32 Step);
	void SetSteering(float Value);
	void SetGraphics(int32 Level);
	void ApplyGraphics() const;

	/** Returns true when this is a new record (and saves it). */
	bool OfferLap(float Seconds);
	bool OfferRace(int32 Laps, float Seconds);
	float GetBestRace(int32 Laps) const;
	/** Best lap on the circuit being raced now (each direction has its own). */
	float GetBestLap() const;

	// --- The garage: wallet, what is owned, and what is fitted -------------

	UPROPERTY(Config)
	int32 Coins = BoatGarage::StartingCoins;

	/** Bit per boat class, paint and flame owned; the first of each is free. */
	UPROPERTY(Config)
	int32 OwnedBoats = 1;

	UPROPERTY(Config)
	int32 OwnedPaints = 1;

	UPROPERTY(Config)
	int32 OwnedFlames = 1;

	UPROPERTY(Config)
	int32 SelectedBoat = 0;

	UPROPERTY(Config)
	int32 SelectedPaint = 0;

	UPROPERTY(Config)
	int32 SelectedFlame = 0;

	/** Upgrade levels, four per boat class, in class order. */
	UPROPERTY(Config)
	TArray<int32> Upgrades;

	/** Career totals for the leaderboard screen. */
	UPROPERTY(Config)
	int32 RacesRun = 0;

	UPROPERTY(Config)
	int32 RacesWon = 0;

	UPROPERTY(Config)
	int32 CoinsEarned = 0;

	/** Best ten races, "score|name|boat|place|racers|seconds|track|date". */
	UPROPERTY(Config)
	TArray<FString> Leaderboard;

	bool OwnsBoat(int32 Boat) const { return Boat == 0 || (OwnedBoats & (1 << Boat)) != 0; }
	bool OwnsPaint(int32 Paint) const { return Paint == 0 || (OwnedPaints & (1 << Paint)) != 0; }
	bool OwnsFlame(int32 Flame) const { return Flame == 0 || (OwnedFlames & (1 << Flame)) != 0; }
	int32 GetLevel(int32 Boat, EUpgrade Upgrade) const;

	/** The fitted loadout: the selected boat with its own upgrades. */
	FBoatLoadout GetLoadout() const;
	/** The same, for another boat (the garage shows it before you buy). */
	FBoatLoadout GetLoadoutFor(int32 Boat, int32 Paint, int32 Flame) const;

	/** Buy (if need be) and fit. False when the wallet is short. */
	bool BuyBoat(int32 Boat);
	bool BuyPaint(int32 Paint);
	bool BuyFlame(int32 Flame);
	bool BuyUpgrade(int32 Boat, EUpgrade Upgrade);

	/** Pays the coins and files the race; returns its rank on the board (0-based), or -1. */
	int32 RecordRace(int32 Score, const FString& Name, int32 Boat, int32 Place, int32 Racers, float Seconds, int32 CoinsWon);

	struct FBoardRow
	{
		int32 Score = 0;
		FString Name;
		int32 Boat = 0;
		int32 Place = 0;
		int32 Racers = 0;
		float Seconds = 0.f;
		bool bReverse = false;
		FString Date;
	};
	TArray<FBoardRow> GetBoard() const;

	/** Saves and writes the file now, so a crash cannot lose coins. Test runs
	 *  (-Autopilot, -PhotoTour) keep their changes in memory only. */
	void Persist();

	static const TCHAR* CameraName(EBoatCamera Camera);
	static const TCHAR* GraphicsName(int32 Level);
	static UBoatPrefs* Get(const UObject* WorldContext);
};
