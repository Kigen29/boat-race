#include "BoatPrefs.h"
#include "BoatTrack.h"

#include "Engine/Engine.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameUserSettings.h"

void UBoatPrefs::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	ApplyGraphics();
}

UBoatPrefs* UBoatPrefs::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const UGameInstance* Instance = World ? World->GetGameInstance() : nullptr;
	return Instance ? Instance->GetSubsystem<UBoatPrefs>() : nullptr;
}

const TCHAR* UBoatPrefs::CameraName(EBoatCamera Camera)
{
	switch (Camera)
	{
	case EBoatCamera::Far:    return TEXT("Far chase");
	case EBoatCamera::Bumper: return TEXT("Bow");
	default:                  return TEXT("Chase");
	}
}

const TCHAR* UBoatPrefs::GraphicsName(int32 Level)
{
	static const TCHAR* const Names[] = { TEXT("Auto"), TEXT("Low"), TEXT("Medium"), TEXT("High"), TEXT("Epic") };
	return Names[FMath::Clamp(Level, -1, 3) + 1];
}

void UBoatPrefs::CycleCamera(int32 Step)
{
	const int32 Count = static_cast<int32>(EBoatCamera::Count);
	CameraMode = ((CameraMode + Step) % Count + Count) % Count;
	Persist();
}

void UBoatPrefs::SetSteering(float Value)
{
	SteeringSensitivity = FMath::Clamp(FMath::RoundToFloat(Value * 10.f) / 10.f, 0.6f, 1.4f);
	Persist();
}

void UBoatPrefs::SetGraphics(int32 Level)
{
	GraphicsQuality = FMath::Clamp(Level, -1, 3);
	Persist();
	ApplyGraphics();
}

void UBoatPrefs::ApplyGraphics() const
{
	// The engine's own scalability groups do the heavy lifting (shadows,
	// effects, view distance); on top, the render resolution and motion blur.
	if (GEngine == nullptr)
	{
		return;
	}
	if (GraphicsQuality < 0)
	{
		// Auto: full resolution with a sharpening pass (the low-spec preset's
		// 70% made everything soft), paid for with fewer shadow cascades.
		static const TCHAR* const Auto[] =
		{
			TEXT("r.ScreenPercentage 100"), TEXT("r.Tonemapper.Sharpen 0.8"),
			TEXT("r.Shadow.CSM.MaxCascades 2"), TEXT("r.Shadow.DistanceScale 0.75"),
			TEXT("r.Shadow.MaxResolution 1024"),
		};
		for (const TCHAR* Command : Auto)
		{
			GEngine->Exec(nullptr, Command);
		}
		return;
	}
	if (UGameUserSettings* Settings = GEngine->GetGameUserSettings())
	{
		Settings->SetOverallScalabilityLevel(GraphicsQuality);
		Settings->ApplySettings(false);
	}
	static const int32 Percent[] = { 75, 90, 100, 100 };
	const FString Commands[] =
	{
		FString::Printf(TEXT("r.ScreenPercentage %d"), Percent[FMath::Clamp(GraphicsQuality, 0, 3)]),
		FString::Printf(TEXT("r.DefaultFeature.MotionBlur %d"), GraphicsQuality > 0 ? 1 : 0),
		TEXT("r.Tonemapper.Sharpen 0.6"),
	};
	for (const FString& Command : Commands)
	{
		GEngine->Exec(nullptr, *Command);
	}
}

bool UBoatPrefs::OfferLap(float Seconds)
{
	const float Best = GetBestLap();
	if (Seconds <= 0.f || (Best > 0.f && Seconds >= Best))
	{
		return false;
	}
	if (BoatTrack::IsReversed())
	{
		BestRace.Add(TEXT("RLap"), Seconds);
	}
	else
	{
		BestLap = Seconds;
	}
	Persist();
	return true;
}

float UBoatPrefs::GetBestLap() const
{
	if (!BoatTrack::IsReversed())
	{
		return BestLap;
	}
	const float* Best = BestRace.Find(TEXT("RLap"));
	return Best ? *Best : 0.f;
}

bool UBoatPrefs::OfferRace(int32 Laps, float Seconds)
{
	const FString Key = (BoatTrack::IsReversed() ? TEXT("R") : TEXT("")) + FString::FromInt(Laps);
	const float* Best = BestRace.Find(Key);
	if (Seconds <= 0.f || (Best != nullptr && Seconds >= *Best))
	{
		return false;
	}
	BestRace.Add(Key, Seconds);
	Persist();
	return true;
}

float UBoatPrefs::GetBestRace(int32 Laps) const
{
	const float* Best = BestRace.Find((BoatTrack::IsReversed() ? TEXT("R") : TEXT("")) + FString::FromInt(Laps));
	return Best ? *Best : 0.f;
}

// ---------------------------------------------------------------------------
// The garage
// ---------------------------------------------------------------------------
int32 UBoatPrefs::GetLevel(int32 Boat, EUpgrade Upgrade) const
{
	const int32 Index = Boat * 4 + static_cast<int32>(Upgrade);
	return Upgrades.IsValidIndex(Index) ? FMath::Clamp(Upgrades[Index], 0, BoatGarage::MaxLevel) : 0;
}

FBoatLoadout UBoatPrefs::GetLoadoutFor(int32 Boat, int32 Paint, int32 Flame) const
{
	FBoatLoadout Loadout;
	Loadout.Boat = static_cast<uint8>(Boat);
	Loadout.Paint = static_cast<uint8>(Paint);
	Loadout.Flame = static_cast<uint8>(Flame);
	for (int32 Index = 0; Index < 4; ++Index)
	{
		Loadout.Levels[Index] = static_cast<uint8>(GetLevel(Boat, static_cast<EUpgrade>(Index)));
	}
	return BoatGarage::Sanitise(Loadout);
}

FBoatLoadout UBoatPrefs::GetLoadout() const
{
	// Only what is owned is ever raced, whatever an edited ini says.
	return GetLoadoutFor(OwnsBoat(SelectedBoat) ? SelectedBoat : 0, OwnsPaint(SelectedPaint) ? SelectedPaint : 0,
		OwnsFlame(SelectedFlame) ? SelectedFlame : 0);
}

namespace
{
	bool Spend(int32& Wallet, int32 Price)
	{
		if (Price > Wallet)
		{
			return false;
		}
		Wallet -= Price;
		return true;
	}
}

bool UBoatPrefs::BuyBoat(int32 Boat)
{
	if (Boat < 0 || Boat >= BoatGarage::NumBoats())
	{
		return false;
	}
	if (!OwnsBoat(Boat))
	{
		if (!Spend(Coins, BoatGarage::GetBoat(Boat).Price))
		{
			return false;
		}
		OwnedBoats |= 1 << Boat;
	}
	SelectedBoat = Boat;
	Persist();
	return true;
}

bool UBoatPrefs::BuyPaint(int32 Paint)
{
	if (Paint < 0 || Paint >= BoatGarage::NumPaints())
	{
		return false;
	}
	if (!OwnsPaint(Paint))
	{
		if (!Spend(Coins, BoatGarage::PaintPrice))
		{
			return false;
		}
		OwnedPaints |= 1 << Paint;
	}
	SelectedPaint = Paint;
	Persist();
	return true;
}

bool UBoatPrefs::BuyFlame(int32 Flame)
{
	if (Flame < 0 || Flame >= BoatGarage::NumFlames())
	{
		return false;
	}
	if (!OwnsFlame(Flame))
	{
		if (!Spend(Coins, BoatGarage::FlamePrice))
		{
			return false;
		}
		OwnedFlames |= 1 << Flame;
	}
	SelectedFlame = Flame;
	Persist();
	return true;
}

bool UBoatPrefs::BuyUpgrade(int32 Boat, EUpgrade Upgrade)
{
	const int32 Next = GetLevel(Boat, Upgrade) + 1;
	if (!OwnsBoat(Boat) || Next > BoatGarage::MaxLevel || !Spend(Coins, BoatGarage::UpgradeCost(Next)))
	{
		return false;
	}
	if (Upgrades.Num() < BoatGarage::NumBoats() * 4)
	{
		Upgrades.SetNumZeroed(BoatGarage::NumBoats() * 4);
	}
	Upgrades[Boat * 4 + static_cast<int32>(Upgrade)] = Next;
	Persist();
	return true;
}

TArray<UBoatPrefs::FBoardRow> UBoatPrefs::GetBoard() const
{
	TArray<FBoardRow> Rows;
	for (const FString& Line : Leaderboard)
	{
		TArray<FString> Parts;
		Line.ParseIntoArray(Parts, TEXT("|"), false);
		if (Parts.Num() < 8)
		{
			continue;
		}
		FBoardRow& Row = Rows.AddDefaulted_GetRef();
		Row.Score = FCString::Atoi(*Parts[0]);
		Row.Name = Parts[1];
		Row.Boat = FCString::Atoi(*Parts[2]);
		Row.Place = FCString::Atoi(*Parts[3]);
		Row.Racers = FCString::Atoi(*Parts[4]);
		Row.Seconds = FCString::Atof(*Parts[5]);
		Row.bReverse = Parts[6] == TEXT("R");
		Row.Date = Parts[7];
	}
	Rows.StableSort([](const FBoardRow& A, const FBoardRow& B) { return A.Score > B.Score; });
	return Rows;
}

int32 UBoatPrefs::RecordRace(int32 Score, const FString& Name, int32 Boat, int32 Place, int32 Racers, float Seconds, int32 CoinsWon)
{
	Coins += FMath::Max(CoinsWon, 0);
	CoinsEarned += FMath::Max(CoinsWon, 0);
	++RacesRun;
	if (Place == 1 && Racers > 1)
	{
		++RacesWon;
	}

	// The name cannot carry the separator.
	const FString CleanName = Name.Replace(TEXT("|"), TEXT("/")).Left(20);
	const FString Line = FString::Printf(TEXT("%d|%s|%d|%d|%d|%.2f|%s|%s"), Score, *CleanName, Boat, Place, Racers, Seconds,
		BoatTrack::IsReversed() ? TEXT("R") : TEXT("F"), *FDateTime::Now().ToString(TEXT("%d %b %Y")));
	Leaderboard.Add(Line);

	// Keep the best ten, and say where the new one landed.
	TArray<FString> Kept = Leaderboard;
	Kept.StableSort([](const FString& A, const FString& B) { return FCString::Atoi(*A) > FCString::Atoi(*B); });
	Kept.SetNum(FMath::Min(Kept.Num(), 10));
	const int32 Rank = Kept.IndexOfByKey(Line);
	Leaderboard = Kept;
	Persist();
	return Rank;
}

void UBoatPrefs::Persist()
{
	if (FParse::Param(FCommandLine::Get(), TEXT("Autopilot")) || FParse::Param(FCommandLine::Get(), TEXT("PhotoTour")))
	{
		return;
	}
	SaveConfig();
	if (GConfig != nullptr)
	{
		GConfig->Flush(false, GGameIni);
	}
}
