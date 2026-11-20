#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/HUD.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "JetGame.generated.h"

class AWaterSurface;
class UCameraComponent;
class UDirectionalLightComponent;
class UExponentialHeightFogComponent;
class USkyAtmosphereComponent;
class USkyLightComponent;

/**
 * Phase two: jet fighters, a second game in the same program.
 *
 * It shares the engine, the menus' look, the sea maths and (later) the
 * networking and weapons with the boat game, but none of the boat game's
 * world: the boat title screen opens it with "?game=Jets", which the alias
 * in DefaultEngine.ini maps to this game mode, and Escape goes back.
 * Clients learn which game they joined from the game state class, so a jet
 * host can never hand a joining machine the boat bay.
 */
UCLASS(Config = Game)
class AJetGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AJetGameMode();
};

/** The jet game's shared state. Builds its own world on every machine. */
UCLASS()
class AJetGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	/** The server's clock in seconds. Double, unlike the boat game's float:
	 *  at jet speeds a millisecond is a quarter of a metre. */
	double GetServerNow() const { return GetServerWorldTimeSeconds(); }

protected:
	virtual void BeginPlay() override;
};

/**
 * The jet game's world. For now open sea under a clear sky; land, clouds and
 * an airbase come with the jet world stage. Every machine builds its own.
 */
UCLASS()
class AJetWorld : public AActor
{
	GENERATED_BODY()

public:
	AJetWorld();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Jet")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Jet")
	TObjectPtr<UDirectionalLightComponent> Sun;

	UPROPERTY(VisibleAnywhere, Category = "Jet")
	TObjectPtr<USkyAtmosphereComponent> Atmosphere;

	UPROPERTY(VisibleAnywhere, Category = "Jet")
	TObjectPtr<USkyLightComponent> SkyLight;

	UPROPERTY(VisibleAnywhere, Category = "Jet")
	TObjectPtr<UExponentialHeightFogComponent> Haze;

	UPROPERTY()
	TObjectPtr<AWaterSurface> Water;
};

/**
 * The altitude-measurement camera used by -JetCheck. Normal play uses
 * AJetPawn; keeping this separate makes performance measurements repeatable.
 */
UCLASS()
class AJetScoutPawn : public APawn
{
	GENERATED_BODY()

public:
	AJetScoutPawn();

	virtual void Tick(float DeltaSeconds) override;

	float GetSpeedMetres() const { return Velocity.Size() / 100.f; }

	/** -JetCheck: the controller steers the camera to set heights. */
	void SetAutopilot(bool bOn, const FVector& Target, float Yaw);

protected:
	UPROPERTY(VisibleAnywhere, Category = "Jet")
	TObjectPtr<UCameraComponent> Camera;

private:
	FVector Velocity = FVector::ZeroVector;
	FRotator View = FRotator(-5.f, 0.f, 0.f);
	bool bAutopilot = false;
	bool bPlaced = false;
};

/** Keys that are not about flying: Escape back to the boat game, -JetCheck. */
UCLASS()
class AJetPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void PlayerTick(float DeltaTime) override;

	/** Smoothed frames per second, for the HUD and -JetCheck. */
	float GetFps() const { return SmoothedFps; }

private:
	void RunJetCheck(float DeltaTime);

	float SmoothedFps = 60.f;
	bool bJetCheck = false;
	bool bInputReady = false;
	bool bShotPending = false;
	int32 CheckStop = -1;
	float CheckClock = 0.f;
	float CheckFrames = 0.f;
	float CheckSeconds = 0.f;
};

/** Height, speed and frame rate, and how to get back to the boats. */
UCLASS()
class AJetHUD : public AHUD
{
	GENERATED_BODY()

public:
    virtual void BeginPlay() override;
	virtual void DrawHUD() override;
private:
    void Label(const FString& Text, float X, float Y, float Size, const FLinearColor& Color, float Align = 0.f);
    void Circle(const FVector2D& Centre, float Radius, const FLinearColor& Color);
    UPROPERTY() TObjectPtr<UFont> FlightFont;
};
