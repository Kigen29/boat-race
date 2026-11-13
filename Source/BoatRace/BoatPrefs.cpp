#include "BoatPrefs.h"

#include "Engine/Engine.h"
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
	SaveConfig();
}

void UBoatPrefs::SetSteering(float Value)
{
	SteeringSensitivity = FMath::Clamp(FMath::RoundToFloat(Value * 10.f) / 10.f, 0.6f, 1.4f);
	SaveConfig();
}

void UBoatPrefs::SetGraphics(int32 Level)
{
	GraphicsQuality = FMath::Clamp(Level, -1, 3);
	SaveConfig();
	ApplyGraphics();
}

void UBoatPrefs::ApplyGraphics() const
{
	// The engine's own scalability groups do the heavy lifting (shadows,
	// effects, view distance); on top, the render resolution and motion blur.
	if (GEngine == nullptr || GraphicsQuality < 0)
	{
		return; // Auto: whatever the engine and the low-spec preset chose
	}
	if (UGameUserSettings* Settings = GEngine->GetGameUserSettings())
	{
		Settings->SetOverallScalabilityLevel(GraphicsQuality);
		Settings->ApplySettings(false);
	}
	static const int32 Percent[] = { 70, 85, 100, 100 };
	const FString Commands[] =
	{
		FString::Printf(TEXT("r.ScreenPercentage %d"), Percent[FMath::Clamp(GraphicsQuality, 0, 3)]),
		FString::Printf(TEXT("r.DefaultFeature.MotionBlur %d"), GraphicsQuality > 0 ? 1 : 0),
	};
	for (const FString& Command : Commands)
	{
		GEngine->Exec(nullptr, *Command);
	}
}

bool UBoatPrefs::OfferLap(float Seconds)
{
	if (Seconds <= 0.f || (BestLap > 0.f && Seconds >= BestLap))
	{
		return false;
	}
	BestLap = Seconds;
	SaveConfig();
	return true;
}

bool UBoatPrefs::OfferRace(int32 Laps, float Seconds)
{
	const FString Key = FString::FromInt(Laps);
	const float* Best = BestRace.Find(Key);
	if (Seconds <= 0.f || (Best != nullptr && Seconds >= *Best))
	{
		return false;
	}
	BestRace.Add(Key, Seconds);
	SaveConfig();
	return true;
}

float UBoatPrefs::GetBestRace(int32 Laps) const
{
	const float* Best = BestRace.Find(FString::FromInt(Laps));
	return Best ? *Best : 0.f;
}
