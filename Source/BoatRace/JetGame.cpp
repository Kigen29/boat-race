#include "JetGame.h"
#include "JetPawn.h"

#include "BoatRace.h"
#include "RaceWeather.h"
#include "WaterSurface.h"

#include "Camera/CameraComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "UnrealClient.h"

namespace JetGameLocal
{
	/** Where -JetCheck stops to measure, metres above the sea. The boat game
	 *  never went above 200 m, so everything past the first is new ground. */
	const float CheckHeights[] = { 30.f, 300.f, 1500.f, 4000.f };
	constexpr int32 NumCheckHeights = UE_ARRAY_COUNT(CheckHeights);

	/** Let shaders compile and the water settle before the first stop. */
	constexpr float CheckWarmupSeconds = 10.f;
	constexpr float CheckSettleSeconds = 3.f;
	constexpr float CheckMeasureSeconds = 4.f;

	/** The scout's cruising and fast speeds, cm/s (about 320 and 1,200 km/h). */
	constexpr float ScoutCruise = 9000.f;
	constexpr float ScoutFast = 34000.f;
}

// ---- Game mode and state ----------------------------------------------------

AJetGameMode::AJetGameMode()
{
	DefaultPawnClass = FParse::Param(FCommandLine::Get(), TEXT("JetCheck"))
        ? AJetScoutPawn::StaticClass() : AJetPawn::StaticClass();
	PlayerControllerClass = AJetPlayerController::StaticClass();
	GameStateClass = AJetGameState::StaticClass();
	HUDClass = AJetHUD::StaticClass();
	bStartPlayersAsSpectators = false;
}

void AJetGameState::BeginPlay()
{
	Super::BeginPlay();
	// Identical on every machine, so each builds its own, as the boat bay does.
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	GetWorld()->SpawnActor<AJetWorld>(AJetWorld::StaticClass(), FTransform::Identity, Params);
	UE_LOG(LogBoatRace, Log, TEXT("Jet game: world built."));
}

// ---- World ------------------------------------------------------------------

AJetWorld::AJetWorld()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	Sun = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Sun"));
	Sun->SetupAttachment(SceneRoot);
	Sun->Mobility = EComponentMobility::Movable;
	Sun->SetAtmosphereSunLight(true);

	Atmosphere = CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("Atmosphere"));
	Atmosphere->SetupAttachment(SceneRoot);

	SkyLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("SkyLight"));
	SkyLight->SetupAttachment(SceneRoot);
	SkyLight->Mobility = EComponentMobility::Movable;
	SkyLight->bRealTimeCapture = true;

	Haze = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("Haze"));
	Haze->SetupAttachment(SceneRoot);
}

void AJetWorld::BeginPlay()
{
	Super::BeginPlay();

	// The boat game's sunny day, so the two games share a palette.
	const FWeatherLook& Look = RaceWeather::Get(ERaceWeather::Sunny);
	Sun->SetWorldRotation(FRotator(Look.SunPitch, Look.SunYaw, 0.f));
	Sun->SetIntensity(Look.SunIntensity);
	Sun->SetLightColor(Look.SunColor);
	SkyLight->SetIntensity(Look.SkyIntensity);
	// Much thinner than the boat game's haze: from 4 km up the sea is 30 km
	// away, and the boat game's fog swallows it whole. The atmosphere's own
	// aerial perspective does the rest of the fading.
	Haze->SetFogDensity(Look.FogDensity * 0.25f);
	Haze->SetFogHeightFalloff(0.2f);
	Haze->SetFogInscatteringColor(Look.FogColor);
	Haze->SetVolumetricFog(false);

	// The same moving sea the boats float on (one wave function everywhere),
	// without the bay's shore foam.
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Water = GetWorld()->SpawnActorDeferred<AWaterSurface>(AWaterSurface::StaticClass(), FTransform::Identity);
	if (Water != nullptr)
	{
		Water->bOpenSea = true;
		Water->FinishSpawning(FTransform::Identity);
		Water->SetFarWaterSize(500000.f);
		Water->SetWaterTint(Look.WaterTint);
		Water->SetSkyTint(Look.FogColor * 1.1f);
	}
}

void AJetWorld::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Water != nullptr)
	{
		Water->Destroy();
	}
	Super::EndPlay(EndPlayReason);
}

// ---- Scout camera -----------------------------------------------------------

AJetScoutPawn::AJetScoutPawn()
{
	PrimaryActorTick.bCanEverTick = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(GetRootComponent());
	Camera->SetFieldOfView(80.f);
}

void AJetScoutPawn::SetAutopilot(bool bOn, const FVector& Target, float Yaw)
{
	bAutopilot = bOn;
	if (bOn)
	{
		SetActorLocation(Target);
		View = FRotator(-8.f, Yaw, 0.f);
		Velocity = FVector::ZeroVector;
	}
}

void AJetScoutPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (PC == nullptr || !PC->IsLocalController())
	{
		return;
	}
	if (!bPlaced)
	{
		// The empty map has no start spots, so begin 80 m up over the middle.
		bPlaced = true;
		SetActorLocation(FVector(0.f, 0.f, 8000.f));
	}

	FVector Wish = FVector::ZeroVector;
	bool bFast = false;
	if (!bAutopilot)
	{
		// Keys are polled, as the boat does, rather than bound through input assets.
		float MouseX = 0.f;
		float MouseY = 0.f;
		PC->GetInputMouseDelta(MouseX, MouseY);
		View.Yaw += MouseX * 0.5f + PC->GetInputAnalogKeyState(EKeys::Gamepad_RightX) * 110.f * DeltaSeconds;
		View.Pitch = FMath::Clamp(View.Pitch + MouseY * 0.5f + PC->GetInputAnalogKeyState(EKeys::Gamepad_RightY) * 80.f * DeltaSeconds, -89.f, 89.f);

		auto Key = [PC](const FKey& K) { return PC->IsInputKeyDown(K) ? 1.f : 0.f; };
		const float Forward = Key(EKeys::W) - Key(EKeys::S) + Key(EKeys::Up) - Key(EKeys::Down) + PC->GetInputAnalogKeyState(EKeys::Gamepad_LeftY);
		const float Right = Key(EKeys::D) - Key(EKeys::A) + PC->GetInputAnalogKeyState(EKeys::Gamepad_LeftX);
		const float Up = Key(EKeys::E) + Key(EKeys::SpaceBar) - Key(EKeys::Q) - Key(EKeys::LeftControl)
			+ PC->GetInputAnalogKeyState(EKeys::Gamepad_RightTriggerAxis) - PC->GetInputAnalogKeyState(EKeys::Gamepad_LeftTriggerAxis);
		bFast = PC->IsInputKeyDown(EKeys::LeftShift) || PC->IsInputKeyDown(EKeys::Gamepad_LeftShoulder);

		Wish = View.Vector() * Forward + FRotator(0.f, View.Yaw, 0.f).RotateVector(FVector::RightVector) * Right + FVector::UpVector * Up;
		Wish = Wish.GetClampedToMaxSize(1.f);
	}
	const float Speed = bFast ? JetGameLocal::ScoutFast : JetGameLocal::ScoutCruise;
	Velocity = FMath::VInterpTo(Velocity, Wish * Speed, DeltaSeconds, 2.5f);

	FVector Location = GetActorLocation() + Velocity * DeltaSeconds;
	// Stay above the waves and inside 30 km of the middle.
	Location.Z = FMath::Clamp(Location.Z, 300.0, 1200000.0);
	const FVector2D Flat(Location.X, Location.Y);
	if (Flat.Size() > 3000000.0)
	{
		const FVector2D Edge = Flat.GetSafeNormal() * 3000000.0;
		Location.X = Edge.X;
		Location.Y = Edge.Y;
	}
	SetActorLocation(Location);
	Camera->SetWorldRotation(View);
}

// ---- Controller -------------------------------------------------------------

void AJetPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (IsLocalController())
	{
		bShowMouseCursor = false;
		bJetCheck = FParse::Param(FCommandLine::Get(), TEXT("JetCheck"));
	}
}

void AJetPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	if (DeltaTime > 0.f)
	{
		SmoothedFps = FMath::Lerp(SmoothedFps, 1.f / DeltaTime, 0.05f);
	}
	if (!IsLocalController())
	{
		return;
	}
	// BeginPlay can run before the local input object exists in UE 5.9.
	if (!bInputReady && PlayerInput)
	{
		SetInputMode(FInputModeGameOnly());
		bInputReady = true;
	}
	if (WasInputKeyJustPressed(EKeys::Escape) || WasInputKeyJustPressed(EKeys::Gamepad_Special_Right))
	{
		// Back to the boat game: the plain map loads the default (boat) game mode.
		UE_LOG(LogBoatRace, Log, TEXT("Jet game: back to the boats."));
		ClientTravel(TEXT("/Engine/Maps/Entry"), TRAVEL_Absolute);
		return;
	}
	if (bJetCheck)
	{
		RunJetCheck(DeltaTime);
	}
}

void AJetPlayerController::RunJetCheck(float DeltaTime)
{
	using namespace JetGameLocal;
	AJetScoutPawn* Scout = Cast<AJetScoutPawn>(GetPawn());
	if (Scout == nullptr)
	{
		return;
	}
	CheckClock += DeltaTime;
	if (CheckStop < 0)
	{
		if (CheckClock < CheckWarmupSeconds)
		{
			return;
		}
		CheckStop = 0;
		CheckClock = 0.f;
		Scout->SetAutopilot(true, FVector(0.f, 0.f, CheckHeights[0] * 100.f), 30.f);
		return;
	}
	if (bShotPending)
	{
		// The screenshot is taken at the end of the frame it was asked for,
		// so only move on once that frame has been drawn where it was asked.
		bShotPending = false;
		if (CheckStop >= NumCheckHeights)
		{
			UE_LOG(LogBoatRace, Log, TEXT("Jet check finished."));
			ConsoleCommand(TEXT("quit"));
			bJetCheck = false;
			return;
		}
		CheckClock = 0.f;
		Scout->SetAutopilot(true, FVector(0.f, 0.f, CheckHeights[CheckStop] * 100.f), 30.f);
		return;
	}
	if (CheckClock < CheckSettleSeconds)
	{
		return;
	}
	CheckFrames += 1.f;
	CheckSeconds += DeltaTime;
	if (CheckClock < CheckSettleSeconds + CheckMeasureSeconds)
	{
		return;
	}

	const int32 Height = FMath::RoundToInt(CheckHeights[CheckStop]);
	const int32 Fps = FMath::RoundToInt(CheckFrames / FMath::Max(CheckSeconds, 0.001f));
	FScreenshotRequest::RequestScreenshot(FString::Printf(TEXT("Jet_%04dm"), Height), true, false);
	UE_LOG(LogBoatRace, Log, TEXT("Jet check: %d m, %d fps"), Height, Fps);

	CheckFrames = 0.f;
	CheckSeconds = 0.f;
	++CheckStop;
	bShotPending = true;
}
