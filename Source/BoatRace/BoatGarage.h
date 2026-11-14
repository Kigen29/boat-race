#pragma once

#include "CoreMinimal.h"
#include "BoatGarage.generated.h"

/**
 * What a player races: the boat, its paint and nitrous flame, and the four
 * upgrade levels. Chosen in the garage, sent to the server once and
 * replicated on the player state, so every machine draws the same boat and
 * the server knows how much hull it has.
 */
USTRUCT()
struct FBoatLoadout
{
	GENERATED_BODY()

	UPROPERTY()
	uint8 Boat = 0;

	/** 0 is the seat's team colour; the rest index the paint list. */
	UPROPERTY()
	uint8 Paint = 0;

	UPROPERTY()
	uint8 Flame = 0;

	/** Engine, handling, armour, nitrous; 0 to BoatGarage::MaxLevel each. */
	UPROPERTY()
	uint8 Levels[4] = { 0, 0, 0, 0 };

	bool operator==(const FBoatLoadout& Other) const
	{
		return Boat == Other.Boat && Paint == Other.Paint && Flame == Other.Flame && FMemory::Memcmp(Levels, Other.Levels, sizeof(Levels)) == 0;
	}
	bool operator!=(const FBoatLoadout& Other) const { return !(*this == Other); }
};

/** One boat in the garage. Multipliers are on the configured handling. */
struct FBoatClass
{
	const TCHAR* Name;
	/** /Game/Models/SM_<Model>, made by Scripts/make_boats.py. */
	const TCHAR* Model;
	const TCHAR* Blurb;
	/** Length the model is drawn at, cm (the physics hull is the same for all). */
	float LengthCm;
	float TopSpeed;
	float Thrust;
	float Turn;
	float Armour;
	int32 Price;
};

/** The multipliers a loadout puts on the boat's configured handling. */
struct FBoatStats
{
	float TopSpeed = 1.f;
	float Thrust = 1.f;
	float Turn = 1.f;
	float Health = 1.f;
	float BoostSeconds = 1.f;
	float BoostThrust = 1.f;
};

enum class EUpgrade : uint8 { Engine, Handling, Armour, Nitro, Count };

namespace BoatGarage
{
	constexpr int32 MaxLevel = 3;
	constexpr int32 PaintPrice = 100;
	constexpr int32 FlamePrice = 150;
	constexpr int32 StartingCoins = 500;
	/** Each course coin is worth this many in the wallet. */
	constexpr int32 CoinValue = 2;

	int32 NumBoats();
	const FBoatClass& GetBoat(int32 Index);

	int32 NumPaints();
	const TCHAR* PaintName(int32 Index);
	/** The paint, or the seat's team colour for paint 0. */
	FLinearColor PaintColor(int32 Index, int32 Slot);

	int32 NumFlames();
	const TCHAR* FlameName(int32 Index);
	/** Bright (HDR) colour for the nitrous flame. */
	FLinearColor FlameColor(int32 Index);

	const TCHAR* UpgradeName(EUpgrade Upgrade);
	const TCHAR* UpgradeBlurb(EUpgrade Upgrade);
	/** Coins to go from Level - 1 to Level. */
	int32 UpgradeCost(int32 Level);

	FBoatStats GetStats(const FBoatLoadout& Loadout);

	/** 0..1 bars for the garage: speed, acceleration, handling, armour, nitrous. */
	void GetBars(const FBoatLoadout& Loadout, float OutBars[5]);

	/** A computer rival's boat: harder rivals bring better-tuned boats. */
	FBoatLoadout RivalLoadout(int32 RivalLevel, int32 Seed);

	/** Out-of-range values (an old save, a bad packet) become valid ones. */
	FBoatLoadout Sanitise(const FBoatLoadout& Loadout);

	/** Coins for finishing in Place (1-based) out of Racers. */
	int32 CoinsForPlace(int32 Place, int32 Racers);
}
