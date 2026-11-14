#include "BoatGarage.h"

#include "BoatTrack.h"

namespace
{
	// Balanced so each boat wins somewhere: the Bullet on the straights, the
	// Hornet in the narrows, the Bulldog when the weapons come out.
	const FBoatClass Boats[] =
	{
		{ TEXT("Interceptor"), TEXT("RaceBoat"),  TEXT("7 m twin-step offshore racer. Good at everything."),
			770.f, 1.00f, 1.00f, 1.00f, 1.00f, 0 },
		{ TEXT("Bullet"),      TEXT("Cigarette"), TEXT("9 m cigarette boat, triple outboards. Fastest flat out; wide in the turns."),
			900.f, 1.08f, 0.94f, 0.92f, 1.00f, 800 },
		{ TEXT("Hornet"),      TEXT("Tunnel"),    TEXT("F1 tunnel catamaran. Quickest off the line and the sharpest turner; fragile."),
			640.f, 1.03f, 1.12f, 1.16f, 0.75f, 1200 },
		{ TEXT("Bulldog"),     TEXT("Patrol"),    TEXT("Armoured patrol RIB. Shrugs off missiles and mines; a little slower."),
			780.f, 0.95f, 0.98f, 0.95f, 1.55f, 1000 },
	};

	struct FNamedColour { const TCHAR* Name; FLinearColor Colour; };
	const FNamedColour Paints[] =
	{
		{ TEXT("Team colour"),   FLinearColor::Black },
		{ TEXT("Racing red"),    FLinearColor(0.75f, 0.03f, 0.03f) },
		{ TEXT("Signal orange"), FLinearColor(0.95f, 0.28f, 0.02f) },
		{ TEXT("Sun yellow"),    FLinearColor(0.95f, 0.7f, 0.02f) },
		{ TEXT("Acid lime"),     FLinearColor(0.35f, 0.85f, 0.05f) },
		{ TEXT("Ocean teal"),    FLinearColor(0.02f, 0.5f, 0.5f) },
		{ TEXT("Royal blue"),    FLinearColor(0.03f, 0.12f, 0.75f) },
		{ TEXT("Violet"),        FLinearColor(0.35f, 0.06f, 0.7f) },
		{ TEXT("Hot pink"),      FLinearColor(0.9f, 0.08f, 0.4f) },
		{ TEXT("Gunmetal"),      FLinearColor(0.08f, 0.09f, 0.1f) },
		{ TEXT("Pearl white"),   FLinearColor(0.85f, 0.86f, 0.88f) },
		{ TEXT("Gloss black"),   FLinearColor(0.012f, 0.012f, 0.014f) },
		{ TEXT("Gold"),          FLinearColor(0.75f, 0.5f, 0.12f) },
	};
	const FNamedColour Flames[] =
	{
		{ TEXT("Nitrous blue"),  FLinearColor(1.5f, 4.f, 14.f) },
		{ TEXT("Toxic green"),   FLinearColor(2.f, 12.f, 1.5f) },
		{ TEXT("Purple haze"),   FLinearColor(8.f, 1.5f, 14.f) },
		{ TEXT("Afterburner"),   FLinearColor(14.f, 5.f, 0.6f) },
		{ TEXT("Pink neon"),     FLinearColor(14.f, 1.5f, 6.f) },
		{ TEXT("White hot"),     FLinearColor(10.f, 10.f, 12.f) },
	};
}

int32 BoatGarage::NumBoats() { return UE_ARRAY_COUNT(Boats); }
const FBoatClass& BoatGarage::GetBoat(int32 Index) { return Boats[FMath::Clamp(Index, 0, NumBoats() - 1)]; }

int32 BoatGarage::NumPaints() { return UE_ARRAY_COUNT(Paints); }
const TCHAR* BoatGarage::PaintName(int32 Index) { return Paints[FMath::Clamp(Index, 0, NumPaints() - 1)].Name; }
FLinearColor BoatGarage::PaintColor(int32 Index, int32 Slot)
{
	return Index <= 0 || Index >= NumPaints() ? BoatTrack::GetPlayerColor(FMath::Max(Slot, 0)) : Paints[Index].Colour;
}

int32 BoatGarage::NumFlames() { return UE_ARRAY_COUNT(Flames); }
const TCHAR* BoatGarage::FlameName(int32 Index) { return Flames[FMath::Clamp(Index, 0, NumFlames() - 1)].Name; }
FLinearColor BoatGarage::FlameColor(int32 Index) { return Flames[FMath::Clamp(Index, 0, NumFlames() - 1)].Colour; }

const TCHAR* BoatGarage::UpgradeName(EUpgrade Upgrade)
{
	static const TCHAR* const Names[] = { TEXT("Engine"), TEXT("Handling"), TEXT("Armour"), TEXT("Nitrous") };
	return Names[FMath::Clamp(static_cast<int32>(Upgrade), 0, 3)];
}

const TCHAR* BoatGarage::UpgradeBlurb(EUpgrade Upgrade)
{
	static const TCHAR* const Blurbs[] =
	{
		TEXT("+3% top speed, +4% acceleration a level"),
		TEXT("+5% turn rate a level"),
		TEXT("+12% hull strength a level"),
		TEXT("+20% boost tank, +4% boost push a level"),
	};
	return Blurbs[FMath::Clamp(static_cast<int32>(Upgrade), 0, 3)];
}

int32 BoatGarage::UpgradeCost(int32 Level)
{
	static const int32 Costs[] = { 0, 200, 400, 800 };
	return Costs[FMath::Clamp(Level, 0, MaxLevel)];
}

FBoatStats BoatGarage::GetStats(const FBoatLoadout& Loadout)
{
	const FBoatClass& Boat = GetBoat(Loadout.Boat);
	const float Engine = FMath::Min<int32>(Loadout.Levels[0], MaxLevel);
	const float Handling = FMath::Min<int32>(Loadout.Levels[1], MaxLevel);
	const float Armour = FMath::Min<int32>(Loadout.Levels[2], MaxLevel);
	const float Nitro = FMath::Min<int32>(Loadout.Levels[3], MaxLevel);
	FBoatStats Stats;
	Stats.TopSpeed = Boat.TopSpeed * (1.f + 0.03f * Engine);
	Stats.Thrust = Boat.Thrust * (1.f + 0.04f * Engine);
	Stats.Turn = Boat.Turn * (1.f + 0.05f * Handling);
	Stats.Health = Boat.Armour * (1.f + 0.12f * Armour);
	Stats.BoostSeconds = 1.f + 0.2f * Nitro;
	Stats.BoostThrust = 1.f + 0.04f * Nitro;
	return Stats;
}

void BoatGarage::GetBars(const FBoatLoadout& Loadout, float OutBars[5])
{
	// Scaled so the stock Interceptor sits a little under half way and a
	// fully tuned specialist nearly fills its bar.
	const FBoatStats S = GetStats(Loadout);
	auto Bar = [](float Value, float Low, float High) { return FMath::Clamp((Value - Low) / (High - Low), 0.05f, 1.f); };
	OutBars[0] = Bar(S.TopSpeed, 0.85f, 1.2f);
	OutBars[1] = Bar(S.Thrust, 0.85f, 1.3f);
	OutBars[2] = Bar(S.Turn, 0.75f, 1.35f);
	OutBars[3] = Bar(S.Health, 0.5f, 2.1f);
	OutBars[4] = Bar(S.BoostSeconds * S.BoostThrust, 0.8f, 1.8f);
}

FBoatLoadout BoatGarage::RivalLoadout(int32 RivalLevel, int32 Seed)
{
	// Neighbouring seeds give neighbouring first draws, so scramble it.
	FRandomStream Random(static_cast<int32>(FCrc::MemCrc32(&Seed, sizeof(Seed)) & 0x7fffffff));
	FBoatLoadout Loadout;
	Loadout.Boat = static_cast<uint8>(Random.RandRange(0, NumBoats() - 1));
	Loadout.Flame = static_cast<uint8>(Random.RandRange(0, NumFlames() - 1));
	const int32 Top = FMath::Clamp(RivalLevel - 1, 0, MaxLevel);
	for (uint8& Level : Loadout.Levels)
	{
		Level = static_cast<uint8>(Random.RandRange(FMath::Max(Top - 1, 0), Top));
	}
	return Loadout;
}

FBoatLoadout BoatGarage::Sanitise(const FBoatLoadout& In)
{
	FBoatLoadout Out = In;
	Out.Boat = static_cast<uint8>(FMath::Clamp<int32>(In.Boat, 0, NumBoats() - 1));
	Out.Paint = static_cast<uint8>(FMath::Clamp<int32>(In.Paint, 0, NumPaints() - 1));
	Out.Flame = static_cast<uint8>(FMath::Clamp<int32>(In.Flame, 0, NumFlames() - 1));
	for (uint8& Level : Out.Levels)
	{
		Level = static_cast<uint8>(FMath::Min<int32>(Level, MaxLevel));
	}
	return Out;
}

int32 BoatGarage::CoinsForPlace(int32 Place, int32 Racers)
{
	static const int32 Prizes[] = { 150, 100, 70, 50 };
	if (Place < 1)
	{
		return 0;
	}
	// Alone on the water (time trial) there is no podium to pay for.
	return Racers <= 1 ? 40 : Prizes[FMath::Clamp(Place - 1, 0, 3)];
}
