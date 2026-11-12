#include "RaceEnvironment.h"

#include "BoatProjectile.h"
#include "BoatRace.h"
#include "BoatRaceGameState.h"
#include "BoatRacePlayerState.h"
#include "BoatTrack.h"
#include "OceanWaves.h"
#include "WaterSurface.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	/** Buoy colours follow powerboat practice: turn marks on the inside. */
	const FLinearColor InnerBuoyColor(0.95f, 0.25f, 0.02f);  // orange turn mark
	const FLinearColor OuterBuoyColor(0.92f, 0.92f, 0.92f);  // white outer limit
	const FLinearColor StartLineColor(0.95f, 0.9f, 0.1f);    // yellow start line
	const FLinearColor NextGateColor(1.4f, 1.1f, 0.05f);

	UStaticMesh* Shape(const TCHAR* Path)
	{
		return LoadObject<UStaticMesh>(nullptr, Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

	float ServerSeconds(const UWorld* World)
	{
		const ABoatRaceGameState* RaceState = World ? World->GetGameState<ABoatRaceGameState>() : nullptr;
		return RaceState ? RaceState->GetServerNow() : (World ? static_cast<float>(World->GetTimeSeconds()) : 0.f);
	}

	/** Repeatable pseudo-random so every machine builds identical scenery. */
	float Wobble(int32 Seed, int32 Salt)
	{
		const uint32 Hash = static_cast<uint32>(Seed * 73856093) ^ static_cast<uint32>(Salt * 19349663);
		return static_cast<float>((Hash % 2000u)) / 1000.f - 1.f; // -1 .. 1
	}
}

// ===========================================================================
// Floating debris
// ===========================================================================
ADebrisBarrel::ADebrisBarrel()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	// The server simulates it and the engine replicates the physics state,
	// so a barrel knocked flying goes the same way on every screen.
	SetReplicateMovement(true);

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	SetRootComponent(Body);
	Body->SetStaticMesh(Shape(TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
	Body->SetRelativeScale3D(FVector(0.9f, 0.9f, 1.3f));
	Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Body->SetCollisionObjectType(ECC_PhysicsBody);
	Body->SetCollisionResponseToAllChannels(ECR_Block);
	Body->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Body->SetCastShadow(false);
}

void ADebrisBarrel::BeginPlay()
{
	Super::BeginPlay();

	if (UMaterialInstanceDynamic* Material = Body->CreateAndSetMaterialInstanceDynamic(0))
	{
		Material->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.35f, 0.16f, 0.05f));
	}

	if (HasAuthority())
	{
		Body->SetSimulatePhysics(true);
		Body->SetMassOverrideInKg(NAME_None, 160.f, true);
		Body->SetLinearDamping(0.9f);
		Body->SetAngularDamping(1.4f);
	}
}

void ADebrisBarrel::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!HasAuthority())
	{
		return; // Clients just watch the replicated motion.
	}

	// A light, crude buoyancy: enough to make barrels ride the swell and come
	// back up after being knocked under, without a second physics model.
	const FVector Location = GetActorLocation();
	const float Surface = OceanWaves::GetHeight(FVector2D(Location.X, Location.Y), ServerSeconds(GetWorld()));
	const float Depth = Surface - static_cast<float>(Location.Z);
	if (Depth > -80.f)
	{
		const float Submersion = FMath::Clamp(Depth / 80.f, 0.f, 1.f);
		const FVector Velocity = Body->GetPhysicsLinearVelocity();
		const float Lift = 160.f * OceanWaves::Gravity * 2.2f * Submersion;
		const float Damping = static_cast<float>(Velocity.Z) * 900.f * Submersion;
		Body->AddForce(FVector(0.f, 0.f, Lift - Damping));
	}
}

void ADebrisBarrel::PushFromBlast(const FVector& Origin, float Radius, float Impulse)
{
	if (!HasAuthority())
	{
		return;
	}

	const float Distance = static_cast<float>(FVector::Dist(GetActorLocation(), Origin));
	if (Distance > Radius)
	{
		return;
	}

	const float Falloff = FMath::Clamp(1.f - Distance / Radius, 0.f, 1.f);
	FVector Away = GetActorLocation() - Origin;
	Away.Z = FMath::Abs(Away.Z) + 200.0;
	// Barrels are light, so the same blast sends them much further than a boat.
	Body->AddImpulse(Away.GetSafeNormal() * (Impulse * Falloff * 0.35f));
}

// ===========================================================================
// The bay
// ===========================================================================
ARaceEnvironment::ARaceEnvironment()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false; // Every machine builds its own copy.

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	Sun = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Sun"));
	Sun->SetupAttachment(SceneRoot);
	Sun->Mobility = EComponentMobility::Movable;

	Atmosphere = CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("Atmosphere"));
	Atmosphere->SetupAttachment(SceneRoot);

	Clouds = CreateDefaultSubobject<UVolumetricCloudComponent>(TEXT("Clouds"));
	Clouds->SetupAttachment(SceneRoot);

	SkyLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("SkyLight"));
	SkyLight->SetupAttachment(SceneRoot);
	SkyLight->Mobility = EComponentMobility::Movable;
	SkyLight->bRealTimeCapture = true;

	Haze = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("Haze"));
	Haze->SetupAttachment(SceneRoot);
	Haze->SetRelativeLocation(FVector(0.f, 0.f, 200.f));
}

void ARaceEnvironment::BeginPlay()
{
	Super::BeginPlay();

	UWorld* World = GetWorld();
	if (World != nullptr)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Water = World->SpawnActor<AWaterSurface>(AWaterSurface::StaticClass(), FTransform::Identity, Params);
	}

	ApplyWeather();

	BuildGates();
	BuildRocks();
	BuildRamps();
	BuildWhirlpools();
	BuildBarges();
	SpawnServerActors();

	UE_LOG(LogBoatRace, Log, TEXT("Bay built: %d gates, %d scenery pieces, weather %d."),
		BoatTrack::NumGates(), Pieces.Num(), Weather);
}

void ARaceEnvironment::ApplyWeather()
{
	// Three settings for the sky, the light and the sea, chosen by the Weather
	// line in DefaultGame.ini. Storm is the default: dark, hazy and loud.
	OceanWaves::SetRoughness(WaveRoughness);

	FLinearColor WaterTint(0.006f, 0.02f, 0.025f);
	switch (Weather)
	{
	case 2: // Clear, low sun
		Sun->SetRelativeRotation(FRotator(-18.f, 205.f, 0.f));
		Sun->SetIntensity(9.f);
		Sun->SetLightColor(FLinearColor(1.f, 0.93f, 0.84f));
		SkyLight->SetIntensity(1.f);
		Haze->SetFogDensity(0.012f);
		Haze->SetFogInscatteringColor(FLinearColor(0.42f, 0.56f, 0.72f));
		WaterTint = FLinearColor(0.008f, 0.05f, 0.09f);
		OceanWaves::SetRoughness(FMath::Min(OceanWaves::Roughness, 0.8f));
		break;

	case 1: // Broken cloud
		Sun->SetRelativeRotation(FRotator(-32.f, 160.f, 0.f));
		Sun->SetIntensity(4.5f);
		Sun->SetLightColor(FLinearColor(0.95f, 0.95f, 0.98f));
		SkyLight->SetIntensity(1.2f);
		Haze->SetFogDensity(0.03f);
		Haze->SetFogInscatteringColor(FLinearColor(0.4f, 0.46f, 0.52f));
		WaterTint = FLinearColor(0.007f, 0.03f, 0.045f);
		break;

	default: // Storm
		// A low, weak sun behind thick cloud, strong grey haze and a dark
		// green-grey sea: the horizon all but disappears.
		Sun->SetRelativeRotation(FRotator(-11.f, 140.f, 0.f));
		Sun->SetIntensity(2.2f);
		Sun->SetLightColor(FLinearColor(0.78f, 0.84f, 0.95f));
		SkyLight->SetIntensity(1.45f);
		Haze->SetFogDensity(0.055f);
		Haze->SetFogHeightFalloff(0.12f);
		Haze->SetFogInscatteringColor(FLinearColor(0.33f, 0.37f, 0.4f));
		Haze->SetVolumetricFog(false);
		WaterTint = FLinearColor(0.006f, 0.018f, 0.02f);
		OceanWaves::SetRoughness(FMath::Max(OceanWaves::Roughness, 1.f));
		break;
	}

	if (Water != nullptr)
	{
		Water->SetWaterTint(WaterTint);
	}
}

UStaticMeshComponent* ARaceEnvironment::AddPiece(UStaticMesh* Mesh, const FVector& Location, const FRotator& Rotation,
	const FVector& Scale, const FLinearColor& Color, bool bBlocking, UMaterialInstanceDynamic** OutMaterial)
{
	UStaticMeshComponent* Piece = NewObject<UStaticMeshComponent>(this);
	Piece->SetMobility(EComponentMobility::Movable);
	Piece->SetStaticMesh(Mesh);
	Piece->SetupAttachment(SceneRoot);
	Piece->SetRelativeLocation(Location);
	Piece->SetRelativeRotation(Rotation);
	Piece->SetRelativeScale3D(Scale);
	if (bBlocking)
	{
		Piece->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Piece->SetCollisionObjectType(ECC_WorldDynamic);
		Piece->SetCollisionResponseToAllChannels(ECR_Block);
		Piece->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	}
	else
	{
		Piece->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	Piece->SetCanEverAffectNavigation(false);
	Piece->RegisterComponent();
	Pieces.Add(Piece);

	UMaterialInstanceDynamic* Material = Piece->CreateAndSetMaterialInstanceDynamic(0);
	if (Material != nullptr)
	{
		Material->SetVectorParameterValue(TEXT("Color"), Color);
	}
	if (OutMaterial != nullptr)
	{
		*OutMaterial = Material;
	}
	return Piece;
}

void ARaceEnvironment::BuildGates()
{
	UStaticMesh* Sphere = Shape(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	UStaticMesh* Cylinder = Shape(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	UStaticMesh* Cone = Shape(TEXT("/Engine/BasicShapes/Cone.Cone"));

	const TArray<FBoatGate>& Gates = BoatTrack::GetGates();
	GateMaterials.Reset();
	GateBaseColors.Reset();
	MaterialsPerGate = 4; // buoy and pole at each of the two ends

	for (int32 Index = 0; Index < Gates.Num(); ++Index)
	{
		const FBoatGate& Gate = Gates[Index];
		const bool bStartLine = (Index == 0);
		const float PoleHeight = bStartLine ? 2000.f : 1100.f;

		const FVector2D Ends[2] = { Gate.Inner, Gate.Outer };
		const FLinearColor Colors[2] =
		{
			bStartLine ? StartLineColor : InnerBuoyColor,
			bStartLine ? StartLineColor : OuterBuoyColor
		};

		for (int32 End = 0; End < 2; ++End)
		{
			const FVector2D Anchor = Ends[End];
			const FVector Base(Anchor.X, Anchor.Y, 0.0);

			// The float: a 3.5 m ball, the part a boat actually clips.
			UMaterialInstanceDynamic* BuoyMaterial = nullptr;
			UStaticMeshComponent* Buoy = AddPiece(Sphere, Base, FRotator::ZeroRotator,
				FVector(3.5f), Colors[End], true, &BuoyMaterial);
			FFloatingPiece Float;
			Float.Component = Buoy;
			Float.Anchor = Anchor;
			Float.Rise = 10.f;
			FloatingPieces.Add(Float);

			// A tall mast so the next gate can be picked out through the haze.
			UMaterialInstanceDynamic* PoleMaterial = nullptr;
			UStaticMeshComponent* Pole = AddPiece(Cylinder, Base + FVector(0.0, 0.0, PoleHeight * 0.5),
				FRotator::ZeroRotator, FVector(0.42f, 0.42f, PoleHeight / 100.f), Colors[End], false, &PoleMaterial);
			FFloatingPiece PoleFloat;
			PoleFloat.Component = Pole;
			PoleFloat.Anchor = Anchor;
			PoleFloat.Rise = PoleHeight * 0.5f;
			PoleFloat.bTilts = true;
			FloatingPieces.Add(PoleFloat);

			// Different top marks, so the two sides are still telling apart in
			// bad light or for anyone who cannot separate the colours.
			UStaticMeshComponent* Top = AddPiece(End == 0 ? Cone : Sphere,
				Base + FVector(0.0, 0.0, PoleHeight + 90.0), FRotator::ZeroRotator,
				End == 0 ? FVector(2.4f, 2.4f, 2.4f) : FVector(2.1f), Colors[End], false);
			FFloatingPiece TopFloat;
			TopFloat.Component = Top;
			TopFloat.Anchor = Anchor;
			TopFloat.Rise = PoleHeight + 90.f;
			TopFloat.bTilts = true;
			FloatingPieces.Add(TopFloat);

			GateMaterials.Add(BuoyMaterial);
			GateMaterials.Add(PoleMaterial);
			GateBaseColors.Add(Colors[End]);
			GateBaseColors.Add(Colors[End]);
		}
	}
}

void ARaceEnvironment::BuildRocks()
{
	UStaticMesh* Cylinder = Shape(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	UStaticMesh* Cone = Shape(TEXT("/Engine/BasicShapes/Cone.Cone"));

	const FLinearColor RockDark(0.055f, 0.06f, 0.062f);
	const FLinearColor RockLight(0.1f, 0.105f, 0.1f);

	int32 Seed = 0;
	for (const FBoatCircle& Rock : BoatTrack::GetRocks())
	{
		++Seed;
		const FVector Base(Rock.Center.X, Rock.Center.Y, 0.0);
		const float Diameter = Rock.Radius * 2.f;
		// Tall sea stacks rather than low skerries: they give the bay a
		// skyline and something to aim between.
		const float Height = Rock.Radius * 6.5f;

		// The blocking part is one plain column; the rest is silhouette.
		AddPiece(Cylinder, Base + FVector(0.0, 0.0, Height * 0.35),
			FRotator::ZeroRotator, FVector(Diameter / 100.f, Diameter / 100.f, Height * 0.7f / 100.f),
			RockDark, true);

		// Three leaning slabs, tapering, so no two stacks look alike.
		for (int32 Tier = 0; Tier < 3; ++Tier)
		{
			const float Fraction = 0.45f + Tier * 0.28f;
			const float TierWidth = Diameter * (1.f - Tier * 0.22f) * (1.f + 0.12f * Wobble(Seed, Tier));
			const float Lean = 5.f * Wobble(Seed, Tier + 40);
			AddPiece(Tier == 2 ? Cone : Cylinder,
				Base + FVector(Rock.Radius * 0.1 * Wobble(Seed, Tier + 7), Rock.Radius * 0.1 * Wobble(Seed, Tier + 11), Height * Fraction),
				FRotator(Lean, 40.f * Wobble(Seed, Tier + 3), Lean * 0.6f),
				FVector(TierWidth / 100.f, TierWidth / 100.f, Height * 0.5f / 100.f),
				Tier == 1 ? RockLight : RockDark, false);
		}

		// A ring of wash where it meets the water.
		AddPiece(Cylinder, Base + FVector(0.0, 0.0, 20.0), FRotator::ZeroRotator,
			FVector(Diameter * 1.25f / 100.f, Diameter * 1.25f / 100.f, 0.3f),
			FLinearColor(0.5f, 0.55f, 0.56f), false);
	}
}

void ARaceEnvironment::BuildRamps()
{
	UStaticMesh* Cube = Shape(TEXT("/Engine/BasicShapes/Cube.Cube"));

	for (const FBoatRamp& Ramp : BoatTrack::GetRamps())
	{
		const FVector Base(Ramp.Center.X, Ramp.Center.Y, 0.0);
		// A pontoon wedge: 24 m wide, 20 m long, tilted nose-up. Hit it with
		// the throttle open and the boat leaves the water.
		UStaticMeshComponent* Wedge = AddPiece(Cube, Base + FVector(0.0, 0.0, 120.0),
			FRotator(-13.f, Ramp.Yaw, 0.f), FVector(20.f, 24.f, 1.1f),
			FLinearColor(0.16f, 0.17f, 0.18f), true);

		FFloatingPiece Float;
		Float.Component = Wedge;
		Float.Anchor = Ramp.Center;
		Float.Rise = 120.f;
		FloatingPieces.Add(Float);

		// Hazard stripes so it is obvious at a distance.
		UStaticMeshComponent* Stripe = AddPiece(Cube, Base + FVector(0.0, 0.0, 240.0),
			FRotator(-13.f, Ramp.Yaw, 0.f), FVector(3.f, 24.f, 0.4f),
			FLinearColor(1.3f, 0.75f, 0.03f), false);
		FFloatingPiece StripeFloat;
		StripeFloat.Component = Stripe;
		StripeFloat.Anchor = Ramp.Center;
		StripeFloat.Rise = 240.f;
		FloatingPieces.Add(StripeFloat);
	}
}

void ARaceEnvironment::BuildWhirlpools()
{
	UStaticMesh* Cylinder = Shape(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	UStaticMesh* Sphere = Shape(TEXT("/Engine/BasicShapes/Sphere.Sphere"));

	for (const FBoatWhirlpool& Pool : BoatTrack::GetWhirlpools())
	{
		const FVector Base(Pool.Center.X, Pool.Center.Y, 0.0);
		// A dark disc for the throat, just above the surface so it always reads.
		AddPiece(Cylinder, Base + FVector(0.0, 0.0, 30.0), FRotator::ZeroRotator,
			FVector(Pool.Radius * 2.f / 100.f, Pool.Radius * 2.f / 100.f, 0.12f),
			FLinearColor(0.004f, 0.012f, 0.016f), false);

		// Foam caught in the spin, placed around the rim.
		for (int32 Index = 0; Index < 7; ++Index)
		{
			const float Angle = (2.f * PI * Index) / 7.f;
			const float Reach = Pool.Radius * 0.72f;
			AddPiece(Sphere, Base + FVector(FMath::Cos(Angle) * Reach, FMath::Sin(Angle) * Reach, 45.0),
				FRotator::ZeroRotator, FVector(Pool.Radius * 0.22f / 100.f, Pool.Radius * 0.22f / 100.f, 0.3f),
				FLinearColor(0.75f, 0.8f, 0.82f), false);
		}
	}
}

void ARaceEnvironment::BuildBarges()
{
	UStaticMesh* Cube = Shape(TEXT("/Engine/BasicShapes/Cube.Cube"));
	BargePieces.Reset();

	for (const FBoatBarge& Barge : BoatTrack::GetBarges())
	{
		const FVector2D Start = BoatTrack::GetBargeLocation(Barge, 0.f);
		const FVector2D Heading = (Barge.To - Barge.From).GetSafeNormal();
		const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Heading.Y, Heading.X));

		// 40 m by 12 m of solid steel crossing the racing line.
		UStaticMeshComponent* Hull = AddPiece(Cube, FVector(Start.X, Start.Y, 60.0),
			FRotator(0.f, Yaw, 0.f), FVector(40.f, 12.f, 2.2f),
			FLinearColor(0.09f, 0.1f, 0.11f), true);
		BargePieces.Add(Hull);

		UStaticMeshComponent* Cabin = AddPiece(Cube, FVector(Start.X, Start.Y, 300.0),
			FRotator(0.f, Yaw, 0.f), FVector(7.f, 8.f, 2.4f),
			FLinearColor(0.5f, 0.12f, 0.05f), false);
		BargePieces.Add(Cabin);
	}
}

void ARaceEnvironment::SpawnServerActors()
{
	UWorld* World = GetWorld();
	if (World == nullptr || !HasAuthority())
	{
		return; // Only the server owns the things the rules depend on.
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	for (const FVector2D& Point : BoatTrack::GetCratePoints())
	{
		const FVector Location(Point.X, Point.Y, 50.0);
		if (AWeaponCrate* Crate = World->SpawnActor<AWeaponCrate>(AWeaponCrate::StaticClass(), Location, FRotator::ZeroRotator, Params))
		{
			Crate->SetUpCrate(Point, CrateTorpedoes, CrateMines, CrateRespawnSeconds);
		}
	}

	for (const FVector2D& Point : BoatTrack::GetDebrisPoints())
	{
		const FVector Location(Point.X, Point.Y, 80.0);
		World->SpawnActor<ADebrisBarrel>(ADebrisBarrel::StaticClass(), Location, FRotator::ZeroRotator, Params);
	}
}

void ARaceEnvironment::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const float Seconds = ServerSeconds(GetWorld());
	UpdateFloatingPieces(Seconds);
	HighlightNextGate(Seconds);

	// Barges run on the shared clock, so they are in the same place on every
	// machine without anything being sent over the network.
	const TArray<FBoatBarge>& Barges = BoatTrack::GetBarges();
	for (int32 Index = 0; Index < Barges.Num(); ++Index)
	{
		const FVector2D Position = BoatTrack::GetBargeLocation(Barges[Index], Seconds);
		const float Surface = OceanWaves::GetHeight(Position, Seconds);
		FVector2D Slope;
		OceanWaves::GetHeightAndSlope(Position, Seconds, Slope);

		for (int32 Part = 0; Part < 2; ++Part)
		{
			const int32 PieceIndex = Index * 2 + Part;
			if (!BargePieces.IsValidIndex(PieceIndex) || BargePieces[PieceIndex] == nullptr)
			{
				continue;
			}
			const float Rise = (Part == 0) ? 60.f : 300.f;
			BargePieces[PieceIndex]->SetRelativeLocation(FVector(Position.X, Position.Y, Surface * 0.6f + Rise));
		}
	}
}

void ARaceEnvironment::UpdateFloatingPieces(float Seconds)
{
	for (const FFloatingPiece& Piece : FloatingPieces)
	{
		if (Piece.Component == nullptr)
		{
			continue;
		}

		FVector2D Slope;
		const float Surface = OceanWaves::GetHeightAndSlope(Piece.Anchor, Seconds, Slope);
		Piece.Component->SetRelativeLocation(FVector(Piece.Anchor.X, Piece.Anchor.Y, Surface + Piece.Rise));

		if (Piece.bTilts)
		{
			// Masts lean with the face of the wave. Without this the buoys look
			// like posts driven into the seabed rather than things afloat.
			const FRotator Lean(
				FMath::RadiansToDegrees(FMath::Atan(Slope.X)) * 0.6f,
				0.f,
				FMath::RadiansToDegrees(FMath::Atan(Slope.Y)) * 0.6f);
			Piece.Component->SetRelativeRotation(Lean);
		}
	}
}

void ARaceEnvironment::HighlightNextGate(float Seconds)
{
	// Each player sees only their own next gate lit, which matters when four
	// boats are on different laps.
	int32 NextGate = -1;

	const UWorld* World = GetWorld();
	const ABoatRaceGameState* RaceState = World ? World->GetGameState<ABoatRaceGameState>() : nullptr;
	const APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
	const ABoatRacePlayerState* RacePlayer = Controller ? Controller->GetPlayerState<ABoatRacePlayerState>() : nullptr;
	if (RaceState != nullptr && RacePlayer != nullptr && !RacePlayer->bFinished
		&& (RaceState->RaceState == ERaceState::Racing || RaceState->RaceState == ERaceState::Countdown))
	{
		NextGate = RacePlayer->NextGate;
	}

	auto PaintGate = [this](int32 Gate, const FLinearColor* Override)
	{
		for (int32 Offset = 0; Offset < MaterialsPerGate; ++Offset)
		{
			const int32 MaterialIndex = Gate * MaterialsPerGate + Offset;
			if (GateMaterials.IsValidIndex(MaterialIndex) && GateMaterials[MaterialIndex] != nullptr)
			{
				GateMaterials[MaterialIndex]->SetVectorParameterValue(TEXT("Color"),
					Override ? *Override : GateBaseColors[MaterialIndex]);
			}
		}
	};

	if (HighlightedGate != NextGate && HighlightedGate >= 0)
	{
		PaintGate(HighlightedGate, nullptr);
	}
	HighlightedGate = NextGate;

	if (NextGate >= 0)
	{
		const float Pulse = 0.5f + 0.5f * FMath::Sin(Seconds * 6.f);
		const FLinearColor Lit = FMath::Lerp(NextGateColor, FLinearColor(2.2f, 2.2f, 1.6f), Pulse * 0.6f);
		PaintGate(NextGate, &Lit);
	}
}
