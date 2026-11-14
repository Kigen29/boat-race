#pragma once

#include "CoreMinimal.h"
#include "Sound/SoundWaveProcedural.h"
#include "Subsystems/WorldSubsystem.h"
#include "BoatAudio.generated.h"

class UAudioComponent;
class USceneComponent;
class USoundAttenuation;

/** Every sound in the game. All are synthesised in code; there are no audio files. */
enum class EBoatSound : uint8
{
	Engine,      // loop: outboard at 60 Hz firing, pitched by revs
	WaterRush,   // loop: hull through water, faded by speed
	Explosion,
	Torpedo,
	MineDrop,
	MineArm,
	Splash,
	Crash,
	Pickup,
	Beep,        // start light
	Go,
	LapChime,
	FinalLap,    // air horn
	Fanfare,
	Thunder,
	Rain,        // loop
	Wind,        // loop
	Sea,         // loop
	Gull,
	Music,       // loop: the race
	MenuMusic,   // loop: menus and lobby, calmer
	Crowd,       // a grandstand cheering
	Nitro,       // loop: nitrous hiss and roar while boosting
	MissileLaunch,
	MissileLock, // the beeping when something has you locked
	Flare,
	Grenade,
	Coin,        // a bright two-note chime
	Purchase,    // the till: a coin rattle and a ding
	Count
};

/**
 * Plays a synthesised buffer. A procedural wave is the one kind of sound an
 * engine build can make at run time without an asset, and each playing copy
 * needs its own cursor, so every play gets a fresh one.
 */
UCLASS()
class UBoatSynthWave : public USoundWaveProcedural
{
	GENERATED_BODY()

public:
	void Start(TSharedPtr<const TArray<int16>, ESPMode::ThreadSafe> InSamples, bool bInLoop);

	/** Called on the audio render thread; reads only the shared, never-changing buffer. */
	virtual int32 OnGeneratePCMAudio(TArray<uint8>& OutAudio, int32 NumSamples) override;

private:
	TSharedPtr<const TArray<int16>, ESPMode::ThreadSafe> Samples;
	int32 Cursor = 0;
	bool bLoop = false;
};

/**
 * Owns the music, the weather ambience, and the one-shot sounds of a world,
 * and stops each one-shot when its buffer runs out (a procedural sound would
 * otherwise play silence for ever).
 */
UCLASS(Config = Game)
class UBoatAudioSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	/** 0..1. Music is per machine; M toggles it in game. */
	UPROPERTY(Config)
	float MusicVolume = 0.45f;

	UPROPERTY(Config)
	float EffectsVolume = 1.f;

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	/** A sound at a place in the world. */
	UAudioComponent* PlayAt(EBoatSound Sound, const FVector& Location, float Volume = 1.f, float Pitch = 1.f);

	/** A sound with no position: start lights, the lap chime, the horn. */
	UAudioComponent* Play2D(EBoatSound Sound, float Volume = 1.f, float Pitch = 1.f);

	/** A loop that follows a component (an engine), stopped and started by its owner. */
	UAudioComponent* MakeLoop(USceneComponent* AttachTo, EBoatSound Sound, bool bPositional);

	void ToggleMusic();
	bool IsMusicOn() const { return bMusicOn; }

	/** 0..1, remembered for next time. */
	void SetMusicVolume(float Volume);
	void SetEffectsVolume(float Volume);

	/** Thunder arrives a moment after the flash, like the real thing. */
	void QueueThunder(float DelaySeconds, float Volume);

	static UBoatAudioSubsystem* Get(const UObject* WorldContext);

private:
	UBoatSynthWave* MakeWave(EBoatSound Sound, bool bLoop) const;
	void StartBeds();
	void UpdateBeds(float DeltaTime);

	struct FOneShot
	{
		TWeakObjectPtr<UAudioComponent> Component;
		double StopAt = 0.0;
	};
	TArray<FOneShot> OneShots;
	TArray<TPair<double, float>> PendingThunder;

	UPROPERTY()
	TObjectPtr<USoundAttenuation> NearAttenuation;

	UPROPERTY()
	TObjectPtr<USoundAttenuation> FarAttenuation;

	UPROPERTY()
	TObjectPtr<UAudioComponent> MusicLoop;

	UPROPERTY()
	TObjectPtr<UAudioComponent> MenuLoop;

	float RaceMix = 0.f;

	UPROPERTY()
	TObjectPtr<UAudioComponent> RainLoop;

	UPROPERTY()
	TObjectPtr<UAudioComponent> WindLoop;

	UPROPERTY()
	TObjectPtr<UAudioComponent> SeaLoop;

	bool bBedsStarted = false;
	bool bMusicOn = true;
	float RainLevel = 0.f;
	float WindLevel = 0.f;
	float NextGullTime = 8.f;
};

namespace BoatAudio
{
	/** Convenience wrappers that do nothing on a dedicated server or in a world without audio. */
	void PlayAt(const UObject* WorldContext, EBoatSound Sound, const FVector& Location, float Volume = 1.f, float Pitch = 1.f);
	void Play2D(const UObject* WorldContext, EBoatSound Sound, float Volume = 1.f, float Pitch = 1.f);

	/** Length of a sound in seconds. */
	float GetDuration(EBoatSound Sound);
}
