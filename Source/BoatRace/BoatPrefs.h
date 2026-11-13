#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
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

	static const TCHAR* CameraName(EBoatCamera Camera);
	static const TCHAR* GraphicsName(int32 Level);
	static UBoatPrefs* Get(const UObject* WorldContext);
};
