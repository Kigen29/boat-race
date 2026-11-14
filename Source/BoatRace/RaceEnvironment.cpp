#include "RaceEnvironment.h"

#include "BayTerrain.h"
#include "BoatArt.h"
#include "BoatAudio.h"
#include "BoatPawn.h"
#include "BoatProjectile.h"
#include "BoatRace.h"
#include "BoatRaceGameState.h"
#include "BoatRacePlayerState.h"
#include "BoatTrack.h"
#include "OceanWaves.h"
#include "WaterSurface.h"

#include "Components/AudioComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"

namespace
{
	/** Buoy colours follow powerboat practice: turn marks on the inside. */
	const FLinearColor InnerBuoyColor(0.95f, 0.25f, 0.02f);  // orange turn mark
	const FLinearColor OuterBuoyColor(0.92f, 0.92f, 0.92f);  // white outer limit
	const FLinearColor StartLineColor(0.95f, 0.8f, 0.05f);   // yellow start line
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

	float YawOf(const FVector2D& Direction)
	{
		return static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(Direction.Y, Direction.X)));
	}

	double PointSegmentDistance(const FVector2D& P, const FVector2D& A, const FVector2D& B)
	{
		const FVector2D AB = B - A;
		const double T = FMath::Clamp(FVector2D::DotProduct(P - A, AB) / FMath::Max(AB.SizeSquared(), 1.0), 0.0, 1.0);
		return (P - (A + AB * T)).Size();
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

	Look = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Look"));
	Look->SetupAttachment(Body);
	Look->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ADebrisBarrel::BeginPlay()
{
	Super::BeginPlay();

	if (UStaticMesh* Barrel = BoatArt::Model(TEXT("Barrel")))
	{
		// The model is an oil drum 1.3 m tall; the body is a 0.9 x 1.3 m cylinder,
		// so undo the body's own scale and draw the drum at its real size.
		Look->SetStaticMesh(Barrel);
		const float Unit = BoatArt::UnitScale(Barrel, 1.3f);
		Look->SetRelativeScale3D(FVector(Unit / 0.9f, Unit / 0.9f, Unit / 1.3f));
		BoatArt::FLook Paint;
		// Red drums of fuel: everyone knows what those do.
		Paint.Paint = FLinearColor(0.75f, 0.06f, 0.03f);
		BoatArt::Paint(Look, Paint);
		Body->SetVisibility(false, false);
	}
	else if (UMaterialInstanceDynamic* Material = Body->CreateAndSetMaterialInstanceDynamic(0))
	{
		Material->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.35f, 0.16f, 0.05f));
	}

	if (HasAuthority())
	{
		Body->SetSimulatePhysics(true);
		Body->SetMassOverrideInKg(NAME_None, 160.f, true);
		Body->SetLinearDamping(0.9f);
		Body->SetAngularDamping(1.4f);
		Body->SetNotifyRigidBodyCollision(true);
		Body->OnComponentHit.AddDynamic(this, &ADebrisBarrel::OnBodyHit);
		Home = GetActorLocation();
	}
}

void ADebrisBarrel::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADebrisBarrel, bGone);
}

void ADebrisBarrel::OnRep_Gone()
{
	ApplyGone();
}

void ADebrisBarrel::ApplyGone()
{
	SetActorHiddenInGame(bGone);
	SetActorEnableCollision(!bGone);
}

void ADebrisBarrel::OnBodyHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent,
	FVector NormalImpulse, const FHitResult& Hit)
{
	// Rammed hard by a boat: the fuel inside goes up.
	const ABoatPawn* Boat = Cast<ABoatPawn>(OtherActor);
	if (Boat != nullptr && !bGone && ExplodeAt < 0.f && FMath::Abs(Boat->GetForwardSpeed()) > 900.f)
	{
		ExplodeAt = ServerSeconds(GetWorld()) + 0.05f;
	}
}

void ADebrisBarrel::Blow()
{
	ExplodeAt = -1.f;
	bGone = true;
	ApplyGone();
	Body->SetSimulatePhysics(false);
	ReturnAt = ServerSeconds(GetWorld()) + 20.f;
	// A fuel drum: a smaller blast than a torpedo, but it sets off its neighbours.
	BoatBlast::Detonate(GetWorld(), GetActorLocation(), 650.f, 25.f, 800000.f, nullptr);
}

void ADebrisBarrel::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!HasAuthority())
	{
		return; // Clients just watch the replicated motion.
	}

	const float Now = ServerSeconds(GetWorld());
	if (ExplodeAt > 0.f && Now >= ExplodeAt)
	{
		Blow();
		return;
	}
	if (bGone)
	{
		if (Now >= ReturnAt)
		{
			// Floats back in where it started.
			bGone = false;
			ApplyGone();
			SetActorLocationAndRotation(Home + FVector(0.f, 0.f, 80.f), FRotator::ZeroRotator, false, nullptr, ETeleportType::TeleportPhysics);
			Body->SetSimulatePhysics(true);
		}
		return;
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

	if (bGone)
	{
		return;
	}
	// Close to a blast, the drum goes up too, a moment later: chain reactions.
	if (Distance < Radius * 0.6f && Distance > 10.f && ExplodeAt < 0.f)
	{
		ExplodeAt = ServerSeconds(GetWorld()) + FMath::FRandRange(0.15f, 0.35f);
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
	Sun->SetAtmosphereSunLight(true);

	Moon = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Moon"));
	Moon->SetupAttachment(SceneRoot);
	Moon->Mobility = EComponentMobility::Movable;
	Moon->SetAtmosphereSunLight(false);
	Moon->SetIntensity(0.f);
	Moon->SetVisibility(false);

	Grade = CreateDefaultSubobject<UPostProcessComponent>(TEXT("Grade"));
	Grade->SetupAttachment(SceneRoot);
	Grade->bUnbound = true;

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

void ARaceEnvironment::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Only matters when the track is turned round mid-session: the next
	// environment builds its own sea, land and hazards.
	if (EndPlayReason == EEndPlayReason::Destroyed)
	{
		for (const TWeakObjectPtr<AActor>& Actor : Spawned)
		{
			if (Actor.IsValid()) { Actor->Destroy(); }
		}
		for (const TWeakObjectPtr<ABoatMine>& Mine : SeaMines)
		{
			if (Mine.IsValid()) { Mine->Destroy(); }
		}
		if (Water != nullptr) { Water->Destroy(); }
		if (Terrain != nullptr) { Terrain->Destroy(); }
	}
	Super::EndPlay(EndPlayReason);
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
		Terrain = World->SpawnActor<ABayTerrain>(ABayTerrain::StaticClass(), FTransform::Identity, Params);
	}

	BuildSky();
	BuildGates();
	BuildLaneRopes();
	BuildRocks();
	BuildRamps();
	BuildWhirlpools();
	BuildBarges();
	BuildLogs();
	BuildStartGantry();
	BuildIceFloes();
	BuildTraffic();
	SpawnServerActors();

	const ABoatRaceGameState* RaceState = World ? World->GetGameState<ABoatRaceGameState>() : nullptr;
	ApplyWeather(RaceState ? RaceState->GetWeather() : ERaceWeather::Sunny);

	UE_LOG(LogBoatRace, Log, TEXT("Bay built: %d gates, %d scenery pieces, %d rope floats, models %s."),
		BoatTrack::NumGates(), Pieces.Num(), RopeFloats[0].Num() + RopeFloats[1].Num(),
		BoatArt::Model(TEXT("Pylon")) ? TEXT("imported") : TEXT("MISSING (run Scripts/make_models.sh and import_assets.sh)"));
}

// ---------------------------------------------------------------------------
// Weather
// ---------------------------------------------------------------------------
void ARaceEnvironment::ApplyWeather(ERaceWeather NewWeather)
{
	AppliedWeather = static_cast<int32>(NewWeather);
	const FWeatherLook& Look = RaceWeather::Get(NewWeather);
	bNight = Look.bNight;

	OceanWaves::SetRoughness(WaveRoughness * Look.Waves);

	if (Look.bNight)
	{
		// The sun just below the horizon gives the atmosphere a deep blue
		// night sky; a separate, cold moon does the lighting.
		Sun->SetRelativeRotation(FRotator(12.f, Look.SunYaw + 180.f, 0.f));
		Sun->SetIntensity(0.02f);
		BaseSunIntensity = 0.02f;
		Moon->SetRelativeRotation(FRotator(Look.SunPitch, Look.SunYaw, 0.f));
		Moon->SetLightColor(Look.SunColor);
		Moon->SetIntensity(Look.SunIntensity);
		Moon->SetVisibility(true);
		// Only the moon throws shadows: two shadowed lights halve the frame rate.
		Sun->SetCastShadows(false);
		// And the moon is the light the water and glass reflect.
		Moon->ForwardShadingPriority = 1;
		Sun->ForwardShadingPriority = 0;
	}
	else
	{
		Sun->SetRelativeRotation(FRotator(Look.SunPitch, Look.SunYaw, 0.f));
		Sun->SetIntensity(Look.SunIntensity);
		BaseSunIntensity = Look.SunIntensity;
		Moon->SetIntensity(0.f);
		Moon->SetVisibility(false);
		Sun->SetCastShadows(true);
		Moon->ForwardShadingPriority = 0;
		Sun->ForwardShadingPriority = 1;
	}
	Moon->MarkRenderStateDirty();
	Sun->MarkRenderStateDirty();
	BoatArt::SetAmbientLight(Look.bNight ? 0.12f : FMath::Lerp(1.f, 0.55f, Look.Overcast));
	Sun->SetLightColor(Look.SunColor);
	ApplyGrade(Look);
	SkyLight->SetIntensity(Look.SkyIntensity);
	BaseSkyIntensity = Look.SkyIntensity;
	Haze->SetFogDensity(Look.FogDensity);
	Haze->SetFogHeightFalloff(Look.FogDensity > 0.1f ? 0.05f : 0.12f);
	Haze->SetFogInscatteringColor(Look.FogColor);
	Haze->SetVolumetricFog(false);

	if (Water != nullptr)
	{
		Water->SetWaterTint(Look.WaterTint);
		Water->SetSkyTint(Look.bNight ? Look.FogColor * 0.6f : Look.FogColor * 1.1f);
	}
	if (Ripples != nullptr)
	{
		Ripples->SetVisibility(Look.Precipitation == EPrecipitation::Rain);
		if (UMaterialInstanceDynamic* Foam = Cast<UMaterialInstanceDynamic>(Ripples->GetMaterial(0)))
		{
			Foam->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.85f, 0.9f, 0.95f) * FMath::Max(Look.FoamBrightness * 1.6f, 0.3f));
			Foam->SetScalarParameterValue(TEXT("Opacity"), 0.28f);
		}
	}
	if (Terrain != nullptr)
	{
		Terrain->SetSnow(Look.Snow);
		Terrain->SetNight(Look.bNight);
	}

	if (CloudDeck != nullptr)
	{
		CloudDeck->SetVisibility(Look.Overcast > 0.01f);
		// Unlit, so it has to be as bright as the sky behind it to read as cloud.
		BoatArt::SetPuff(CloudDeckMaterial, Look.CloudColor * (Look.bNight ? 0.4f : 2.5f), Look.Overcast);
	}
	if (Cumulus != nullptr)
	{
		Cumulus->SetVisibility(Look.Cumulus > 0.01f);
		BoatArt::SetPuff(CumulusMaterial, Look.bNight ? Look.CloudColor : FLinearColor(Look.CloudColor) * 4.f, 0.92f);
	}
	for (UStaticMeshComponent* Floe : IceFloes)
	{
		if (Floe != nullptr)
		{
			Floe->SetVisibility(Look.Snow > 0.5f);
		}
	}

	// Rain, snow or a gale of spray around the camera.
	if (Precipitation != nullptr)
	{
		ShownPrecipitation = Look.Precipitation;
		UStaticMesh* Mesh = Shape(Look.Precipitation == EPrecipitation::Snow
			? TEXT("/Engine/BasicShapes/Sphere.Sphere") : TEXT("/Engine/BasicShapes/Cube.Cube"));
		Precipitation->SetStaticMesh(Mesh);
		Precipitation->SetMaterial(0, PrecipitationMaterial);
		Precipitation->SetVisibility(Look.Precipitation != EPrecipitation::None);
		const FLinearColor Tint = Look.Precipitation == EPrecipitation::Snow ? FLinearColor(3.f, 3.f, 3.2f)
			: (Look.Precipitation == EPrecipitation::Gale ? FLinearColor(2.f, 2.2f, 2.3f) : FLinearColor(1.2f, 1.3f, 1.5f));
		BoatArt::SetPuff(PrecipitationMaterial, Tint, Look.Precipitation == EPrecipitation::Snow ? 0.95f : 0.22f);
	}

	RepaintGates();
	for (UStaticMeshComponent* Piece : Pieces)
	{
		// Models (not plain shapes) get dusted with snow and light their lamps.
		if (Piece != nullptr && Piece->ComponentTags.Contains(TEXT("Model")) && !GatePylons.Contains(Piece))
		{
			BoatArt::FLook Paint;
			Paint.Snow = Look.Snow;
			Paint.bNight = false;
			BoatArt::Paint(Piece, Paint);
		}
	}
	StartLamps.Reset();
	for (UStaticMeshComponent* Piece : Pieces)
	{
		if (Piece == nullptr || !Piece->ComponentTags.Contains(TEXT("Gantry")))
		{
			continue;
		}
		const TArray<FName> Slots = Piece->GetMaterialSlotNames();
		for (int32 Lamp = 1; Lamp <= 5; ++Lamp)
		{
			for (int32 Slot = 0; Slot < Slots.Num(); ++Slot)
			{
				if (Slots[Slot].ToString().Contains(FString::Printf(TEXT("Lamp%d"), Lamp)))
				{
					StartLamps.Add(Cast<UMaterialInstanceDynamic>(Piece->GetMaterial(Slot)));
				}
			}
		}
	}

	UE_LOG(LogBoatRace, Log, TEXT("Weather now %s."), Look.Name);
}

void ARaceEnvironment::BuildSky()
{
	UStaticMesh* Sphere = Shape(TEXT("/Engine/BasicShapes/Sphere.Sphere"));

	// A dome of grey cloud over everything, for overcast, rain and storm.
	CloudDeck = NewObject<UStaticMeshComponent>(this);
	CloudDeck->SetStaticMesh(Sphere);
	CloudDeck->SetupAttachment(SceneRoot);
	CloudDeck->SetUsingAbsoluteLocation(true);
	CloudDeck->SetWorldScale3D(FVector(1300.f, 1300.f, 130.f));
	CloudDeck->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CloudDeck->SetCastShadow(false);
	CloudDeck->RegisterComponent();
	CloudDeckMaterial = BoatArt::SoftMaterial(CloudDeck, FLinearColor::Gray, 0.f);
	CloudDeck->SetMaterial(0, CloudDeckMaterial);

	// Storybook cumulus: clusters of soft puffs drifting high over the hills.
	Cumulus = NewObject<UInstancedStaticMeshComponent>(this);
	Cumulus->SetStaticMesh(Sphere);
	Cumulus->SetupAttachment(SceneRoot);
	Cumulus->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Cumulus->SetCastShadow(false);
	Cumulus->RegisterComponent();
	CumulusMaterial = BoatArt::SoftMaterial(Cumulus, FLinearColor::White, 0.9f);
	Cumulus->SetMaterial(0, CumulusMaterial);
	for (int32 Cloud = 0; Cloud < 34; ++Cloud)
	{
		const float Angle = Cloud * 2.39996f; // golden angle: an even spread
		const float Distance = 60000.f + 150000.f * FMath::Abs(Wobble(Cloud, 1));
		const FVector Centre(3000.f + FMath::Cos(Angle) * Distance, -7500.f + FMath::Sin(Angle) * Distance, 55000.f + 25000.f * Wobble(Cloud, 2));
		for (int32 Puff = 0; Puff < 5; ++Puff)
		{
			const float Size = 9000.f + 7000.f * FMath::Abs(Wobble(Cloud * 7 + Puff, 3));
			const FVector Offset(Puff * 7000.f - 14000.f + 3000.f * Wobble(Cloud, Puff + 4), 4000.f * Wobble(Cloud, Puff + 9), 2500.f * FMath::Abs(Wobble(Cloud, Puff + 13)));
			CumulusSeeds.Add(Centre + Offset);
			Cumulus->AddInstance(FTransform(FRotator::ZeroRotator, Centre + Offset, FVector(Size / 100.f, Size / 100.f, Size * 0.6f / 100.f)), true);
		}
	}

	// Rain, snow and gale particles, moved as a block around the camera.
	Precipitation = NewObject<UInstancedStaticMeshComponent>(this);
	Precipitation->SetStaticMesh(Shape(TEXT("/Engine/BasicShapes/Cube.Cube")));
	Precipitation->SetupAttachment(SceneRoot);
	Precipitation->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Precipitation->SetCastShadow(false);
	Precipitation->RegisterComponent();
	PrecipitationMaterial = BoatArt::SoftMaterial(Precipitation, FLinearColor::White, 0.5f);
	Precipitation->SetMaterial(0, PrecipitationMaterial);
	const int32 Count = FMath::Clamp(PrecipitationCount, 100, 5000);
	FRandomStream Random(99);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		PrecipitationSeeds.Add(FVector(Random.FRandRange(0.f, 1.f), Random.FRandRange(0.f, 1.f), Random.FRandRange(0.f, 1.f)));
		Precipitation->AddInstance(FTransform::Identity, true);
	}
	PrecipitationTransforms.SetNum(Count);
	Precipitation->SetVisibility(false);

	// Raindrop rings on the water around the camera.
	if (UStaticMesh* Ring = BoatArt::Model(TEXT("Ripple")))
	{
		Ripples = NewObject<UInstancedStaticMeshComponent>(this);
		Ripples->SetStaticMesh(Ring);
		Ripples->SetupAttachment(SceneRoot);
		Ripples->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Ripples->SetCastShadow(false);
		Ripples->RegisterComponent();
		BoatArt::Paint(Ripples);
		for (int32 Index = 0; Index < 260; ++Index)
		{
			Ripples->AddInstance(FTransform::Identity, true);
		}
		RippleTransforms.SetNum(260);
		Ripples->SetVisibility(false);
	}

	// A lightning bolt: a jagged chain of glowing rods, rebuilt for each strike.
	UStaticMesh* Cube = Shape(TEXT("/Engine/BasicShapes/Cube.Cube"));
	for (int32 Index = 0; Index < 14; ++Index)
	{
		UStaticMeshComponent* Piece = NewObject<UStaticMeshComponent>(this);
		Piece->SetStaticMesh(Cube);
		Piece->SetupAttachment(SceneRoot);
		Piece->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Piece->SetCastShadow(false);
		Piece->RegisterComponent();
		Piece->SetMaterial(0, BoatArt::GlowMaterial(Piece, FLinearColor(14.f, 14.f, 22.f)));
		Piece->SetVisibility(false);
		BoltPieces.Add(Piece);
	}
}

void ARaceEnvironment::UpdateSky(float Seconds, float DeltaSeconds)
{
	const FVector2D Gusting = RaceWeather::GetWind(RaceWeather::FromIndex(AppliedWeather), Seconds);
	SmoothedWind = FMath::Vector2DInterpTo(SmoothedWind, Gusting, DeltaSeconds, 0.5f);
	const FWeatherLook& Look = RaceWeather::Get(RaceWeather::FromIndex(AppliedWeather));
	FVector Camera = FVector::ZeroVector;
	if (const APlayerController* Controller = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		FRotator Ignored;
		Controller->GetPlayerViewPoint(Camera, Ignored);
	}
	if (CloudDeck != nullptr)
	{
		CloudDeck->SetWorldLocation(FVector(Camera.X, Camera.Y, -2000.0));
	}

	// Cumulus drift with the wind, wrapping round a 4 km square.
	if (Cumulus != nullptr && Cumulus->IsVisible())
	{
		const FVector2D Wind = SmoothedWind;
		CloudDrift += FVector(300.f + Wind.X * 0.6f, 120.f + Wind.Y * 0.6f, 0.f) * DeltaSeconds;
		const FVector Drift = CloudDrift;
		TArray<FTransform> Transforms;
		Transforms.Reserve(CumulusSeeds.Num());
		for (int32 Index = 0; Index < CumulusSeeds.Num(); ++Index)
		{
			FTransform Transform;
			Cumulus->GetInstanceTransform(Index, Transform, true);
			FVector Position = CumulusSeeds[Index] + Drift;
			Position.X = FMath::Fmod(Position.X - 3000.0 + 200000.0 * 1000.0, 400000.0) - 200000.0 + 3000.0;
			Position.Y = FMath::Fmod(Position.Y + 7500.0 + 200000.0 * 1000.0, 400000.0) - 200000.0 - 7500.0;
			Transform.SetLocation(Position);
			Transforms.Add(Transform);
		}
		Cumulus->BatchUpdateInstancesTransforms(0, Transforms, true, true, true);
	}

	// Lightning: a flash that lights the whole bay, a bolt, and thunder later.
	FVector2D Strike;
	const float Age = RaceWeather::GetLightningAge(RaceWeather::FromIndex(AppliedWeather), Seconds, &Strike);
	const bool bFlash = Age >= 0.f && Age < 0.45f;
	if (bFlash && (LastLightningAge < 0.f || LastLightningAge > Age))
	{
		// A new strike: shape the bolt and send the thunder after it.
		const FVector Ground(Strike.X * 100.0, Strike.Y * 100.0, 0.0);
		FVector Point = Ground + FVector(0.0, 0.0, 70000.0);
		const int32 Seed = FMath::FloorToInt(Seconds);
		for (int32 Index = 0; Index < BoltPieces.Num(); ++Index)
		{
			const float Fraction = static_cast<float>(Index + 1) / BoltPieces.Num();
			FVector Next = FMath::Lerp(Ground + FVector(0.0, 0.0, 70000.0), Ground, Fraction)
				+ FVector(4000.f * Wobble(Seed, Index * 2), 4000.f * Wobble(Seed, Index * 2 + 1), 0.f) * (1.f - Fraction * 0.6f);
			if (Index == BoltPieces.Num() - 1)
			{
				Next = Ground;
			}
			const FVector Segment = Next - Point;
			BoltPieces[Index]->SetWorldLocationAndRotation((Point + Next) * 0.5, FRotationMatrix::MakeFromZ(Segment).Rotator());
			BoltPieces[Index]->SetWorldScale3D(FVector(1.6f, 1.6f, static_cast<float>(Segment.Size()) / 100.f));
			Point = Next;
		}
		if (UBoatAudioSubsystem* Audio = UBoatAudioSubsystem::Get(this))
		{
			const float Distance = static_cast<float>(FVector::Dist2D(Ground, Camera)) / 100.f;
			Audio->QueueThunder(FMath::Clamp(Distance / 340.f, 0.4f, 4.5f), FMath::Clamp(1.4f - Distance / 2500.f, 0.35f, 1.f));
		}
	}
	LastLightningAge = Age;
	float Flash = 0.f;
	if (bFlash)
	{
		Flash = Age < 0.07f ? 1.f : (Age < 0.13f ? 0.25f : (Age < 0.2f ? 0.85f : FMath::Max(0.f, 1.f - (Age - 0.2f) / 0.25f) * 0.5f));
	}
	for (UStaticMeshComponent* Piece : BoltPieces)
	{
		Piece->SetVisibility(Flash > 0.2f);
	}
	Sun->SetIntensity(BaseSunIntensity + Flash * 14.f);
	SkyLight->SetIntensity(BaseSkyIntensity + Flash * 4.f);
}

void ARaceEnvironment::UpdatePrecipitation(float Seconds, float DeltaSeconds)
{
	if (Precipitation == nullptr || ShownPrecipitation == EPrecipitation::None)
	{
		return;
	}
	FVector Camera = FVector::ZeroVector;
	if (const APlayerController* Controller = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		FRotator Ignored;
		Controller->GetPlayerViewPoint(Camera, Ignored);
	}

	// The wind, smoothed over a couple of seconds: raindrops and flakes feel
	// the mean wind, not every flicker of a gust.
	const FVector2D Wind = SmoothedWind; // smoothed in UpdateSky, every frame
	FVector Velocity;
	FVector Box;
	FVector Scale;
	switch (ShownPrecipitation)
	{
	case EPrecipitation::Snow:
		// Flakes sink slowly and drift a little with the breeze.
		Velocity = FVector(Wind.X * 0.15, Wind.Y * 0.15, -120.0);
		Box = FVector(6000.0, 6000.0, 3000.0);
		Scale = FVector(0.07f);
		break;
	case EPrecipitation::Gale:
		Velocity = FVector(Wind.X * 1.6, Wind.Y * 1.6, -80.0);
		Box = FVector(9000.0, 9000.0, 1200.0);
		Scale = FVector(0.4f, 0.03f, 0.03f);
		break;
	default:
		// Rain falls at about 9 m/s, leaning only slightly with the wind.
		Velocity = FVector(Wind.X * 0.18, Wind.Y * 0.18, -900.0);
		Box = FVector(5000.0, 5000.0, 3000.0);
		// Wide enough to cover a whole pixel; thinner streaks break into dashes.
		Scale = FVector(0.02f, 0.02f, 1.0f);
		break;
	}
	PrecipitationDrift += Velocity * DeltaSeconds;
	// Keep the sum small; the wrap below makes any whole number of boxes invisible.
	PrecipitationDrift = FVector(FMath::Fmod(PrecipitationDrift.X, Box.X * 64.0), FMath::Fmod(PrecipitationDrift.Y, Box.Y * 64.0),
		FMath::Fmod(PrecipitationDrift.Z, Box.Z * 64.0));
	const FRotator Along = ShownPrecipitation == EPrecipitation::Gale
		? FRotationMatrix::MakeFromX(Velocity).Rotator() : FRotationMatrix::MakeFromZ(-Velocity).Rotator();
	const FVector Corner = Camera - Box * 0.5 + FVector(0.0, 0.0, Box.Z * 0.15);

	for (int32 Index = 0; Index < PrecipitationSeeds.Num(); ++Index)
	{
		const FVector& Seed = PrecipitationSeeds[Index];
		FVector Position = Seed * Box + PrecipitationDrift;
		if (ShownPrecipitation == EPrecipitation::Snow)
		{
			// Each flake flutters on its own slow, small circle as it falls.
			Position.X += 35.0 * FMath::Sin(Seconds * (0.7 + Seed.X * 0.6) + Index);
			Position.Y += 35.0 * FMath::Cos(Seconds * (0.6 + Seed.Y * 0.5) + Index * 0.7);
		}
		// Wrap into the box around the camera, so the particles never run out.
		const FVector Relative = Position - (Corner - FVector(0.0));
		FVector Wrapped(
			FMath::Fmod(FMath::Fmod(Relative.X, Box.X) + Box.X, Box.X),
			FMath::Fmod(FMath::Fmod(Relative.Y, Box.Y) + Box.Y, Box.Y),
			FMath::Fmod(FMath::Fmod(Relative.Z, Box.Z) + Box.Z, Box.Z));
		PrecipitationTransforms[Index] = FTransform(Along, Corner + Wrapped, Scale);
	}
	Precipitation->BatchUpdateInstancesTransforms(0, PrecipitationTransforms, true, true, true);
}

// ---------------------------------------------------------------------------
// Building
// ---------------------------------------------------------------------------
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

UStaticMeshComponent* ARaceEnvironment::AddModel(const TCHAR* Name, float ModelHeightMetres, const FVector& Location,
	const FRotator& Rotation, float Size, const FLinearColor& Paint)
{
	UStaticMesh* Mesh = BoatArt::Model(Name);
	if (Mesh == nullptr)
	{
		return nullptr;
	}
	UStaticMeshComponent* Piece = NewObject<UStaticMeshComponent>(this);
	Piece->SetMobility(EComponentMobility::Movable);
	Piece->SetStaticMesh(Mesh);
	Piece->SetupAttachment(SceneRoot);
	Piece->SetRelativeLocation(Location);
	Piece->SetRelativeRotation(Rotation);
	Piece->SetRelativeScale3D(FVector(BoatArt::UnitScale(Mesh, ModelHeightMetres) * Size));
	// Looks only: every model that blocks has a plain, invisible collision shape
	// of the right size, which is cheaper and more predictable than its mesh.
	Piece->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Piece->SetCanEverAffectNavigation(false);
	Piece->ComponentTags.Add(TEXT("Model"));
	Piece->RegisterComponent();
	Pieces.Add(Piece);
	BoatArt::FLook Look;
	Look.Paint = Paint;
	BoatArt::Paint(Piece, Look);
	return Piece;
}

void ARaceEnvironment::Float(UStaticMeshComponent* Component, const FVector2D& Anchor, float Rise, float Yaw, bool bTilts)
{
	if (Component == nullptr)
	{
		return;
	}
	FFloatingPiece Piece;
	Piece.Component = Component;
	Piece.Anchor = Anchor;
	Piece.Rise = Rise;
	Piece.Yaw = Yaw;
	Piece.bTilts = bTilts;
	FloatingPieces.Add(Piece);
}

void ARaceEnvironment::BuildGates()
{
	UStaticMesh* Sphere = Shape(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	UStaticMesh* Cylinder = Shape(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	const bool bModels = BoatArt::Model(TEXT("Pylon")) != nullptr;

	const TArray<FBoatGate>& Gates = BoatTrack::GetGates();
	for (int32 Index = 0; Index < Gates.Num(); ++Index)
	{
		const FBoatGate& Gate = Gates[Index];
		const bool bStartLine = (Index == 0);
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

			// The part a boat actually clips: a 3.5 m ball, hidden under the pylon.
			UStaticMeshComponent* Buoy = AddPiece(Sphere, Base, FRotator::ZeroRotator, FVector(3.5f), Colors[End], true);
			Float(Buoy, Anchor, 10.f, 0.f, false);

			if (bModels)
			{
				Buoy->SetVisibility(false);
				// An inflatable turn-mark pylon, 7 m tall, in the gate's colour.
				UStaticMeshComponent* Pylon = AddModel(TEXT("Pylon"), 4.4f, Base, FRotator::ZeroRotator, 1.6f, Colors[End]);
				Float(Pylon, Anchor, -20.f, 0.f, true);
				GatePylons.Add(Pylon);
				PylonColors.Add(Colors[End]);
			}
			else
			{
				UMaterialInstanceDynamic* Material = nullptr;
				const float PoleHeight = bStartLine ? 2000.f : 1100.f;
				UStaticMeshComponent* Pole = AddPiece(Cylinder, Base + FVector(0.0, 0.0, PoleHeight * 0.5),
					FRotator::ZeroRotator, FVector(0.42f, 0.42f, PoleHeight / 100.f), Colors[End], false, &Material);
				Float(Pole, Anchor, PoleHeight * 0.5f, 0.f, true);
				GatePylons.Add(Pole);
				PylonColors.Add(Colors[End]);
			}
		}
	}
	RepaintGates();
}

void ARaceEnvironment::RepaintGates()
{
	GateMaterials.Reset();
	GateOfMaterial.Reset();
	GateBaseColors.Reset();
	HighlightedGate = -1;
	for (int32 Index = 0; Index < GatePylons.Num(); ++Index)
	{
		UStaticMeshComponent* Pylon = GatePylons[Index];
		if (Pylon == nullptr)
		{
			continue;
		}
		TArray<UMaterialInstanceDynamic*> Paints;
		if (Pylon->ComponentTags.Contains(TEXT("Model")))
		{
			BoatArt::FLook Look;
			Look.Paint = PylonColors[Index];
			Look.bNight = bNight;
			Paints = BoatArt::Paint(Pylon, Look);
		}
		else if (UMaterialInstanceDynamic* Material = Cast<UMaterialInstanceDynamic>(Pylon->GetMaterial(0)))
		{
			Paints.Add(Material);
		}
		for (UMaterialInstanceDynamic* Material : Paints)
		{
			GateMaterials.Add(Material);
			GateOfMaterial.Add(Index / 2);
			GateBaseColors.Add(bNight ? PylonColors[Index] * 0.9f : PylonColors[Index]);
		}
	}
}

void ARaceEnvironment::BuildLaneRopes()
{
	// Rope lines of floats along both edges of the course, like the lanes in
	// the reference video: red on the inside, white on the outside.
	UStaticMesh* FloatMesh = BoatArt::Model(TEXT("Float"));
	UStaticMesh* Mesh = FloatMesh ? FloatMesh : Shape(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	const float Unit = FloatMesh ? BoatArt::UnitScale(FloatMesh, 0.6f) * 1.6f : 0.6f;
	const FLinearColor Colours[2] = { FLinearColor(0.85f, 0.06f, 0.04f), FLinearColor(0.95f, 0.95f, 0.95f) };

	for (int32 Side = 0; Side < 2; ++Side)
	{
		UInstancedStaticMeshComponent* Instances = NewObject<UInstancedStaticMeshComponent>(this);
		Instances->SetStaticMesh(Mesh);
		Instances->SetupAttachment(SceneRoot);
		Instances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Instances->SetCastShadow(false);
		Instances->SetCullDistances(0, 60000);
		Instances->RegisterComponent();
		BoatArt::FLook Look;
		Look.Paint = Colours[Side];
		if (FloatMesh != nullptr)
		{
			BoatArt::Paint(Instances, Look);
		}
		else if (UMaterialInstanceDynamic* Material = Instances->CreateAndSetMaterialInstanceDynamic(0))
		{
			Material->SetVectorParameterValue(TEXT("Color"), Colours[Side]);
		}
		RopeInstances.Add(Instances);
	}

	const TArray<FBoatGate>& Gates = BoatTrack::GetGates();
	const TArray<FBoatBarge>& Barges = BoatTrack::GetBarges();
	for (int32 Index = 0; Index < Gates.Num(); ++Index)
	{
		const FBoatGate& A = Gates[Index];
		const FBoatGate& B = Gates[(Index + 1) % Gates.Num()];
		for (int32 Side = 0; Side < 2; ++Side)
		{
			const FVector2D From = Side == 0 ? A.Inner : A.Outer;
			const FVector2D To = Side == 0 ? B.Inner : B.Outer;
			const double Length = (To - From).Size();
			const float Yaw = YawOf(To - From);
			for (double Along = 600.0; Along < Length - 600.0; Along += 450.0)
			{
				const FVector2D Point = From + (To - From) * (Along / Length);
				// Leave a gap where a barge crosses, and anywhere the bank is.
				bool bClear = BayTerrain::GetHeight(Point) < -120.f;
				for (const FBoatBarge& Barge : Barges)
				{
					bClear &= PointSegmentDistance(Point, Barge.From, Barge.To) > 3200.0;
				}
				if (bClear)
				{
					RopeFloats[Side].Add({ Point, Yaw });
					RopeInstances[Side]->AddInstance(FTransform(FRotator(0.f, Yaw, 0.f), FVector(Point.X, Point.Y, 0.0), FVector(Unit)), true);
				}
			}
		}
	}
}

void ARaceEnvironment::BuildRocks()
{
	UStaticMesh* Cylinder = Shape(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	UStaticMesh* Cone = Shape(TEXT("/Engine/BasicShapes/Cone.Cone"));
	const FLinearColor RockDark(0.055f, 0.06f, 0.062f);

	int32 Seed = 0;
	for (const FBoatCircle& Rock : BoatTrack::GetRocks())
	{
		++Seed;
		const FVector Base(Rock.Center.X, Rock.Center.Y, 0.0);
		const float Diameter = Rock.Radius * 2.f;
		const float Height = Rock.Radius * 6.5f;

		// The blocking part is one plain column.
		UStaticMeshComponent* Column = AddPiece(Cylinder, Base + FVector(0.0, 0.0, Height * 0.35),
			FRotator::ZeroRotator, FVector(Diameter / 100.f, Diameter / 100.f, Height * 0.7f / 100.f), RockDark, true);

		// A weathered sea stack, its base the same width as the column.
		if (UStaticMeshComponent* Stack = AddModel(TEXT("SeaStack"), 7.f, Base - FVector(0, 0, 150),
			FRotator(0.f, 360.f * Wobble(Seed, 5), 0.f), Rock.Radius / 100.f * 1.0f))
		{
			Column->SetVisibility(false);
			continue;
		}
		AddPiece(Cone, Base + FVector(0.0, 0.0, Height * 0.85), FRotator::ZeroRotator,
			FVector(Diameter * 0.7f / 100.f, Diameter * 0.7f / 100.f, Height * 0.4f / 100.f), RockDark, false);
	}
}

void ARaceEnvironment::BuildRamps()
{
	UStaticMesh* Cube = Shape(TEXT("/Engine/BasicShapes/Cube.Cube"));

	for (const FBoatRamp& Ramp : BoatTrack::GetRamps())
	{
		const FVector Base(Ramp.Center.X, Ramp.Center.Y, 0.0);
		// The solid part: a 24 m x 20 m pontoon wedge, tilted nose-up (positive
		// pitch) so its top runs from just under the water at the back to 4 m
		// at the lip, exactly under the model's deck. Hit it with the throttle
		// open and the boat leaves the water.
		UStaticMeshComponent* Wedge = AddPiece(Cube, Base + FVector(0.0, 0.0, 120.0),
			FRotator(13.f, Ramp.Yaw, 0.f), FVector(20.f, 24.f, 1.1f), FLinearColor(0.16f, 0.17f, 0.18f), true);
		Float(Wedge, Ramp.Center, 120.f, Ramp.Yaw, false);
		// The pitch has to survive the bobbing.
		FloatingPieces.Last().bTilts = false;
		FloatingPieces.Last().Yaw = Ramp.Yaw;

		// The model's deck follows the same 13 degree slope.
		if (UStaticMeshComponent* Model = AddModel(TEXT("Ramp"), 8.9f, Base, FRotator(0.f, Ramp.Yaw, 0.f), 1.f))
		{
			Wedge->SetVisibility(false);
			Float(Model, Ramp.Center, 0.f, Ramp.Yaw, false);
		}
	}
}

void ARaceEnvironment::BuildWhirlpools()
{
	UStaticMesh* Cylinder = Shape(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	UStaticMesh* Sphere = Shape(TEXT("/Engine/BasicShapes/Sphere.Sphere"));

	int32 PoolIndex = 0;
	for (const FBoatWhirlpool& Pool : BoatTrack::GetWhirlpools())
	{
		const FVector Base(Pool.Center.X, Pool.Center.Y, 0.0);
		// The dark throat, just above the surface so it always reads.
		AddPiece(Cylinder, Base + FVector(0.0, 0.0, 30.0), FRotator::ZeroRotator,
			FVector(Pool.Radius * 1.4f / 100.f, Pool.Radius * 1.4f / 100.f, 0.12f), FLinearColor(0.004f, 0.012f, 0.016f), false);

		// Foam caught in the spin, circling faster towards the middle.
		for (int32 Index = 0; Index < 14; ++Index)
		{
			FSwirl Swirl;
			Swirl.Pool = PoolIndex;
			Swirl.Radius = Pool.Radius * (0.3f + 0.65f * (Index % 7) / 6.f);
			Swirl.Angle = Index * 0.9f;
			Swirl.Speed = 2.2f * (1.2f - Swirl.Radius / Pool.Radius);
			UStaticMeshComponent* Foam = NewObject<UStaticMeshComponent>(this);
			Foam->SetStaticMesh(Sphere);
			Foam->SetupAttachment(SceneRoot);
			Foam->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Foam->SetCastShadow(false);
			Foam->SetRelativeScale3D(FVector(Pool.Radius * 0.28f / 100.f, Pool.Radius * 0.1f / 100.f, 0.25f));
			Foam->RegisterComponent();
			Foam->SetMaterial(0, BoatArt::SoftMaterial(Foam, FLinearColor(0.9f, 0.95f, 1.f), 0.75f));
			SwirlPieces.Add(Foam);
			Swirls.Add(Swirl);
		}
		++PoolIndex;
	}
}

void ARaceEnvironment::UpdateWhirlpools(float Seconds)
{
	const TArray<FBoatWhirlpool>& Pools = BoatTrack::GetWhirlpools();
	for (int32 Index = 0; Index < Swirls.Num(); ++Index)
	{
		const FSwirl& Swirl = Swirls[Index];
		if (!Pools.IsValidIndex(Swirl.Pool) || SwirlPieces[Index] == nullptr)
		{
			continue;
		}
		const float Angle = Swirl.Angle + Seconds * Swirl.Speed;
		const FVector2D Centre = Pools[Swirl.Pool].Center;
		const FVector2D Position = Centre + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Swirl.Radius;
		const float Surface = OceanWaves::GetHeight(Position, Seconds);
		SwirlPieces[Index]->SetRelativeLocationAndRotation(FVector(Position.X, Position.Y, Surface + 15.f - (1.f - Swirl.Radius / Pools[Swirl.Pool].Radius) * 80.f),
			FRotator(0.f, FMath::RadiansToDegrees(Angle) + 90.f, 0.f));
	}
}

void ARaceEnvironment::BuildBarges()
{
	UStaticMesh* Cube = Shape(TEXT("/Engine/BasicShapes/Cube.Cube"));
	BargePieces.Reset();

	int32 Index = 0;
	for (const FBoatBarge& Barge : BoatTrack::GetBarges())
	{
		const FVector2D Start = BoatTrack::GetBargeLocation(Barge, 0.f);
		const float Yaw = YawOf(Barge.To - Barge.From);

		// 40 m by 12 m of solid steel crossing the racing line.
		UStaticMeshComponent* Hull = AddPiece(Cube, FVector(Start.X, Start.Y, 60.0),
			FRotator(0.f, Yaw, 0.f), FVector(40.f, 12.f, 2.2f), FLinearColor(0.09f, 0.1f, 0.11f), true);
		BargePieces.Add(Hull);
		BargeOfPiece.Add(Index);
		BargePieceRise.Add(60.f);

		if (UStaticMeshComponent* Model = AddModel(TEXT("Barge"), 11.8f, FVector(Start.X, Start.Y, 0.0), FRotator(0.f, Yaw, 0.f), 1.f))
		{
			Hull->SetVisibility(false);
			BargePieces.Add(Model);
			BargeOfPiece.Add(Index);
			BargePieceRise.Add(-80.f);
		}
		++Index;
	}
}

void ARaceEnvironment::BuildLogs()
{
	UStaticMesh* Cylinder = Shape(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	for (const FBoatLog& Log : BoatTrack::GetLogs())
	{
		// A solid 5 m log lying across the water: a capsule-ish cylinder on its side.
		UStaticMeshComponent* Solid = AddPiece(Cylinder, FVector(Log.Center.X, Log.Center.Y, 0.0),
			FRotator(90.f, Log.Yaw, 0.f), FVector(0.75f, 0.75f, 5.f), FLinearColor(0.25f, 0.15f, 0.08f), true);
		Float(Solid, Log.Center, 5.f, Log.Yaw, false);
		FloatingPieces.Last().bTilts = false;
		if (UStaticMeshComponent* Model = AddModel(TEXT("Log"), 1.1f, FVector(Log.Center.X, Log.Center.Y, 0.0), FRotator(0.f, Log.Yaw, 0.f), 1.f))
		{
			Solid->SetVisibility(false);
			Float(Model, Log.Center, -10.f, Log.Yaw, true);
		}
	}
}

void ARaceEnvironment::BuildStartGantry()
{
	const FBoatGate& Start = BoatTrack::GetGates()[0];
	const FVector Base(Start.Center.X, Start.Center.Y, 0.0);
	const float Yaw = YawOf(Start.Forward);
	if (UStaticMeshComponent* Gantry = AddModel(TEXT("StartGantry"), 23.f, Base - FVector(0, 0, 60), FRotator(0.f, Yaw, 0.f), 1.f))
	{
		Gantry->ComponentTags.Add(TEXT("Gantry"));
		// The towers stand on pontoons 50 m either side of the line: solid.
		UStaticMesh* Cube = Shape(TEXT("/Engine/BasicShapes/Cube.Cube"));
		const FVector2D Across(-Start.Forward.Y, Start.Forward.X);
		for (const double Side : { -1.0, 1.0 })
		{
			const FVector2D Tower = Start.Center + Across * (Side * 5000.0);
			UStaticMeshComponent* Solid = AddPiece(Cube, FVector(Tower.X, Tower.Y, 900.0), FRotator(0.f, Yaw, 0.f),
				FVector(5.f, 5.f, 20.f), FLinearColor::Gray, true);
			Solid->SetVisibility(false);
		}
	}
}

void ARaceEnvironment::BuildIceFloes()
{
	// Only seen in the snow: plates of ice drifting outside the course.
	int32 Seed = 0;
	const TArray<FBoatGate>& Gates = BoatTrack::GetGates();
	for (int32 Index = 0; Index < Gates.Num() * 3; ++Index)
	{
		++Seed;
		const FBoatGate& Gate = Gates[Index % Gates.Num()];
		const double Side = (Index % 2) ? 1.0 : -1.0;
		const FVector2D Point = Gate.Center + FVector2D(-Gate.Forward.Y, Gate.Forward.X) * (Side * (6000.0 + 2500.0 * FMath::Abs(Wobble(Seed, 1))))
			+ Gate.Forward * (8000.0 * Wobble(Seed, 2));
		if (BayTerrain::GetHeight(Point) > -150.f)
		{
			continue;
		}
		if (UStaticMeshComponent* Floe = AddModel(TEXT("IceFloe"), 0.53f, FVector(Point.X, Point.Y, 0.0),
			FRotator(0.f, 360.f * Wobble(Seed, 3), 0.f), 2.f + 2.5f * FMath::Abs(Wobble(Seed, 4))))
		{
			Floe->SetVisibility(false);
			Float(Floe, Point, -5.f, 360.f * Wobble(Seed, 3), true);
			IceFloes.Add(Floe);
		}
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
			Spawned.Add(Crate);
			Crate->SetUpCrate(Point, CrateTorpedoes, CrateMines, CrateRespawnSeconds);
		}
	}

	for (const FVector2D& Point : BoatTrack::GetDebrisPoints())
	{
		const FVector Location(Point.X, Point.Y, 80.0);
		Spawned.Add(World->SpawnActor<ADebrisBarrel>(ADebrisBarrel::StaticClass(), Location, FRotator::ZeroRotator, Params));
	}

	// A field of sea mines: in the course but off the racing line, clear of
	// every other obstacle, so they can be dodged by anyone watching.
	const TArray<FBoatGate>& Gates = BoatTrack::GetGates();
	for (int32 Index = 1; Index < Gates.Num(); Index += 2)
	{
		const FBoatGate& A = Gates[Index];
		const FBoatGate& B = Gates[(Index + 1) % Gates.Num()];
		const FVector2D Along = (B.Center - A.Center).GetSafeNormal();
		const FVector2D Point = (A.Center + B.Center) * 0.5 + FVector2D(-Along.Y, Along.X) * ((Index % 4 == 1) ? 1900.0 : -1900.0);
		bool bClear = BayTerrain::GetClearanceMetres(Point.X / 100.0, Point.Y / 100.0) > 15.f;
		for (const FBoatCircle& Rock : BoatTrack::GetRocks()) { bClear &= FVector2D::Distance(Point, Rock.Center) > Rock.Radius + 1500.f; }
		for (const FBoatRamp& Ramp : BoatTrack::GetRamps()) { bClear &= FVector2D::Distance(Point, Ramp.Center) > 2500.f; }
		for (const FBoatWhirlpool& Pool : BoatTrack::GetWhirlpools()) { bClear &= FVector2D::Distance(Point, Pool.Center) > Pool.Radius + 1000.f; }
		for (const FBoatLog& Log : BoatTrack::GetLogs()) { bClear &= FVector2D::Distance(Point, Log.Center) > 1000.f; }
		for (const FVector2D& Crate : BoatTrack::GetCratePoints()) { bClear &= FVector2D::Distance(Point, Crate) > 1500.f; }
		for (const FBoatBarge& Barge : BoatTrack::GetBarges()) { bClear &= PointSegmentDistance(Point, Barge.From, Barge.To) > 3000.0; }
		if (bClear)
		{
			SeaMineHomes.Add(Point);
			SeaMines.Add(nullptr);
			SeaMineReturn.Add(0.f);
		}
	}
	UE_LOG(LogBoatRace, Log, TEXT("%d sea mines laid on the course."), SeaMineHomes.Num());
}

// ---------------------------------------------------------------------------
// Every frame
// ---------------------------------------------------------------------------
void ARaceEnvironment::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const UWorld* World = GetWorld();
	const ABoatRaceGameState* RaceState = World ? World->GetGameState<ABoatRaceGameState>() : nullptr;
	if (RaceState != nullptr && static_cast<int32>(RaceState->GetWeather()) != AppliedWeather)
	{
		ApplyWeather(RaceState->GetWeather());
	}

	const float Seconds = ServerSeconds(World);
	UpdateFloatingPieces(Seconds);
	UpdateRopes(Seconds);
	UpdateWhirlpools(Seconds);
	HighlightNextGate(Seconds);
	UpdateStartLights();
	UpdateSky(Seconds, DeltaSeconds);
	UpdatePrecipitation(Seconds, DeltaSeconds);
	UpdateRipples(Seconds);
	UpdateTraffic(Seconds, DeltaSeconds);
	if (HasAuthority())
	{
		UpdateSeaMines(Seconds);
	}

	// Barges run on the shared clock, so they are in the same place on every
	// machine without anything being sent over the network.
	const TArray<FBoatBarge>& Barges = BoatTrack::GetBarges();
	for (int32 Piece = 0; Piece < BargePieces.Num(); ++Piece)
	{
		if (BargePieces[Piece] == nullptr || !Barges.IsValidIndex(BargeOfPiece[Piece]))
		{
			continue;
		}
		const FVector2D Position = BoatTrack::GetBargeLocation(Barges[BargeOfPiece[Piece]], Seconds);
		const float Surface = OceanWaves::GetHeight(Position, Seconds);
		BargePieces[Piece]->SetRelativeLocation(FVector(Position.X, Position.Y, Surface * 0.6f + BargePieceRise[Piece]));
	}
}

void ARaceEnvironment::UpdateRopes(float Seconds)
{
	// Fifteen times a second is plenty for floats bobbing on a swell.
	RopeRefresh -= GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.f;
	if (RopeRefresh > 0.f)
	{
		return;
	}
	RopeRefresh = 1.f / 15.f;
	for (int32 Side = 0; Side < 2 && Side < RopeInstances.Num(); ++Side)
	{
		UInstancedStaticMeshComponent* Instances = RopeInstances[Side];
		if (Instances == nullptr || RopeFloats[Side].Num() == 0)
		{
			continue;
		}
		const float Scale = Instances->GetInstanceCount() > 0 ? [&]()
		{
			FTransform First;
			Instances->GetInstanceTransform(0, First, false);
			return static_cast<float>(First.GetScale3D().X);
		}() : 1.f;
		TArray<FTransform> Transforms;
		Transforms.Reserve(RopeFloats[Side].Num());
		for (const FRopeFloat& Rope : RopeFloats[Side])
		{
			const float Surface = OceanWaves::GetHeight(Rope.Anchor, Seconds);
			Transforms.Add(FTransform(FRotator(0.f, Rope.Yaw, 0.f), FVector(Rope.Anchor.X, Rope.Anchor.Y, Surface), FVector(Scale)));
		}
		Instances->BatchUpdateInstancesTransforms(0, Transforms, false, true, true);
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
			// Buoys lean with the face of the wave. Without this they look like
			// posts driven into the seabed rather than things afloat.
			const FRotator Lean(
				FMath::RadiansToDegrees(FMath::Atan(Slope.X)) * 0.6f,
				Piece.Yaw,
				FMath::RadiansToDegrees(FMath::Atan(Slope.Y)) * 0.6f);
			Piece.Component->SetRelativeRotation(Lean);
		}
	}
}

void ARaceEnvironment::UpdateStartLights()
{
	if (StartLamps.Num() == 0)
	{
		return;
	}
	const UWorld* World = GetWorld();
	const ABoatRaceGameState* RaceState = World ? World->GetGameState<ABoatRaceGameState>() : nullptr;
	if (RaceState == nullptr)
	{
		return;
	}
	// Five reds come on a second apart; all go out (green) for the start.
	int32 Lit = 0;
	bool bGreen = false;
	const float Now = RaceState->GetServerNow();
	if (RaceState->RaceState == ERaceState::Countdown)
	{
		Lit = FMath::Clamp(5 - FMath::CeilToInt(RaceState->StateEndTime - Now) + 1, 0, 5);
	}
	else if (RaceState->RaceState == ERaceState::Racing && Now - RaceState->RaceStartTime < 3.f)
	{
		bGreen = true;
	}
	for (int32 Index = 0; Index < StartLamps.Num(); ++Index)
	{
		if (StartLamps[Index] == nullptr)
		{
			continue;
		}
		const FLinearColor Colour = bGreen ? FLinearColor(0.3f, 9.f, 0.5f)
			: (Index < Lit ? FLinearColor(10.f, 0.15f, 0.05f) : FLinearColor(0.15f, 0.f, 0.f));
		StartLamps[Index]->SetVectorParameterValue(TEXT("Color"), Colour);
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

	const float Pulse = 0.5f + 0.5f * FMath::Sin(Seconds * 6.f);
	const FLinearColor Lit = FMath::Lerp(NextGateColor, FLinearColor(2.2f, 2.2f, 1.6f), Pulse * 0.6f);
	for (int32 Index = 0; Index < GateMaterials.Num(); ++Index)
	{
		UMaterialInstanceDynamic* Material = GateMaterials[Index];
		if (Material == nullptr)
		{
			continue;
		}
		const bool bThis = GateOfMaterial[Index] == NextGate;
		if (bThis || GateOfMaterial[Index] == HighlightedGate)
		{
			Material->SetVectorParameterValue(TEXT("Color"), bThis ? Lit * (bNight ? 2.f : 1.f) : GateBaseColors[Index]);
		}
	}
	HighlightedGate = NextGate;
}

// ---------------------------------------------------------------------------
// The camera's grade: what makes it look filmed rather than rendered
// ---------------------------------------------------------------------------
void ARaceEnvironment::ApplyGrade(const FWeatherLook& Look)
{
	FPostProcessSettings& Post = Grade->Settings;
	// Auto-exposure would brighten a night scene back to day; pin it down.
	Post.bOverride_AutoExposureBias = true;
	Post.AutoExposureBias = Look.bNight ? -1.4f : (Look.FogDensity > 0.1f ? 0.3f : 0.f);
	// Highlights on wet paint, chrome and sun glitter bloom softly.
	Post.bOverride_BloomIntensity = true;
	Post.BloomIntensity = Look.bNight ? 1.1f : 0.8f;
	Post.bOverride_BloomThreshold = true;
	Post.BloomThreshold = 1.2f;
	// A light vignette; no chromatic fringe, which only smears the edges.
	Post.bOverride_VignetteIntensity = true;
	Post.VignetteIntensity = 0.3f;
	Post.bOverride_SceneFringeIntensity = true;
	Post.SceneFringeIntensity = 0.f;
	// Sunny Florida colour: slightly richer and warmer in good weather,
	// flatter and cooler when it rains.
	const bool bBright = Look.Precipitation == EPrecipitation::None && !Look.bNight && Look.FogDensity < 0.1f;
	Post.bOverride_ColorSaturation = true;
	Post.ColorSaturation = bBright ? FVector4(1.12, 1.12, 1.12, 1.0) : FVector4(0.92, 0.92, 0.92, 1.0);
	Post.bOverride_ColorContrast = true;
	Post.ColorContrast = FVector4(1.1, 1.1, 1.1, 1.0);
	// A racing-game split tone: highlights a touch warm, shadows a touch
	// cool and blue, so sun on paint pops against the water.
	Post.bOverride_ColorGainHighlights = true;
	Post.ColorGainHighlights = Look.bNight ? FVector4(1.0, 1.0, 1.0, 1.0) : FVector4(1.03, 1.0, 0.96, 1.0);
	Post.bOverride_ColorGainShadows = true;
	Post.ColorGainShadows = FVector4(0.96, 1.0, 1.05, 1.0);
	Post.bOverride_WhiteTemp = true;
	Post.WhiteTemp = bBright ? 6200.f : (Look.bNight ? 7600.f : 6900.f);
	// No motion blur: it read as a soft, blurry picture.
	Post.bOverride_MotionBlurAmount = true;
	Post.MotionBlurAmount = 0.f;
	// No film grain: on top of the lower resolution it read as blur.
	Post.bOverride_FilmGrainIntensity = true;
	Post.FilmGrainIntensity = 0.f;
}

// ---------------------------------------------------------------------------
// Boat traffic off the course
// ---------------------------------------------------------------------------
void ARaceEnvironment::BuildTraffic()
{
	struct FKind { const TCHAR* Name; float Height; float Length; float SpeedMs; FLinearColor Paint; };
	const FKind Kinds[] =
	{
		{ TEXT("CenterConsole"), 3.2f, 1080.f, 13.f, FLinearColor(0.02f, 0.12f, 0.35f) },
		{ TEXT("JetSki"),        1.3f,  340.f, 16.f, FLinearColor(0.9f, 0.25f, 0.02f) },
		{ TEXT("Yacht"),         6.2f, 1750.f,  6.f, FLinearColor(0.85f, 0.86f, 0.88f) },
		{ TEXT("JetSki"),        1.3f,  340.f, 14.f, FLinearColor(0.05f, 0.55f, 0.3f) },
		{ TEXT("CenterConsole"), 3.2f, 1080.f, 11.f, FLinearColor(0.75f, 0.05f, 0.08f) },
	};
	const TArray<FBoatGate>& Gates = BoatTrack::GetGates();
	UStaticMesh* Cube = Shape(TEXT("/Engine/BasicShapes/Cube.Cube"));

	// Look for stretches of open water beside the course, clear of the
	// course itself, the shore, rocks and the barge channels.
	int32 Next = 0;
	for (int32 Gate = 0; Gate < Gates.Num() && Next < UE_ARRAY_COUNT(Kinds); ++Gate)
	{
		for (const double Side : { -1.0, 1.0 })
		{
			if (Next >= UE_ARRAY_COUNT(Kinds))
			{
				break;
			}
			const FKind& Kind = Kinds[Next];
			const FVector2D A = Gates[Gate].Center, B = Gates[(Gate + 1) % Gates.Num()].Center;
			const FVector2D Along = (B - A).GetSafeNormal();
			const FVector2D Out(-Along.Y * Side, Along.X * Side);
			const double LegLength = (B - A).Size();
			const float Radius = 1300.f;
			const FVector2D Centre = (A + B) * 0.5 + Out * 8200.0;
			const float Straight = static_cast<float>(LegLength * 0.28);

			bool bClear = Straight > 2500.f;
			for (int32 Sample = 0; Sample < 48 && bClear; ++Sample)
			{
				const float Angle = Sample / 48.f * 2.f * PI;
				const FVector2D Point = Centre + Along * (FMath::Cos(Angle) * (Straight + Radius))
					+ Out * (FMath::Sin(Angle) * Radius * 1.4f);
				bClear &= BayTerrain::GetClearanceMetres(Point.X / 100.0, Point.Y / 100.0) > 14.f;
				double Course = 1.e12;
				for (int32 Leg = 0; Leg < Gates.Num(); ++Leg)
				{
					Course = FMath::Min(Course, PointSegmentDistance(Point, Gates[Leg].Center, Gates[(Leg + 1) % Gates.Num()].Center));
				}
				bClear &= Course > 6200.0;
				for (const FBoatBarge& Barge : BoatTrack::GetBarges())
				{
					bClear &= PointSegmentDistance(Point, Barge.From, Barge.To) > 4500.0;
				}
				for (const FBoatCircle& Rock : BoatTrack::GetRocks())
				{
					bClear &= FVector2D::Distance(Point, Rock.Center) > Rock.Radius + 2000.f;
				}
			}
			if (!bClear)
			{
				continue;
			}

			UStaticMeshComponent* Look = AddModel(Kind.Name, Kind.Height, FVector(Centre.X, Centre.Y, 0.0), FRotator::ZeroRotator, 1.f, Kind.Paint);
			if (Look == nullptr)
			{
				return; // models not imported
			}
			FTrafficBoat& Boat = Traffic.AddDefaulted_GetRef();
			Boat.Centre = Centre;
			Boat.Along = Along;
			Boat.Straight = Straight;
			Boat.Radius = Radius;
			Boat.Speed = Kind.SpeedMs * 100.f;
			Boat.Phase = 5000.f * Next;
			Boat.Look = Look;
			Boat.Length = Kind.Length;
			// A plain solid hull so a racer who strays out here hits something.
			Boat.Solid = AddPiece(Cube, FVector(Centre.X, Centre.Y, 0.0), FRotator::ZeroRotator,
				FVector(Kind.Length * 0.9f / 100.f, Kind.Length * 0.3f / 100.f, 1.6f), FLinearColor::Gray, true);
			Boat.Solid->SetVisibility(false);
			Boat.Wake.Init(this);
			if (UBoatAudioSubsystem* Audio = UBoatAudioSubsystem::Get(this))
			{
				Boat.Engine = Audio->MakeLoop(Look, EBoatSound::Engine, true);
				if (Boat.Engine != nullptr)
				{
					Boat.Engine->SetVolumeMultiplier(Kind.Length > 1500.f ? 0.25f : 0.45f);
					Boat.Engine->SetPitchMultiplier(Kind.Length < 500.f ? 1.6f : (Kind.Length > 1500.f ? 0.55f : 0.9f));
					Boat.Engine->Play();
					TrafficRefs.Add(Boat.Engine);
				}
			}
			++Next;
		}
	}
	UE_LOG(LogBoatRace, Log, TEXT("%d boats out on the water off the course."), Traffic.Num());
}

void ARaceEnvironment::UpdateTraffic(float Seconds, float DeltaSeconds)
{
	const FWeatherLook& Look = RaceWeather::Get(RaceWeather::FromIndex(AppliedWeather));
	const float Foam = Look.FoamBrightness;
	const float Now = GetWorld() ? static_cast<float>(GetWorld()->GetTimeSeconds()) : 0.f;

	for (FTrafficBoat& Boat : Traffic)
	{
		if (Boat.Look == nullptr)
		{
			continue;
		}
		// Position along a stadium: two straights joined by half circles.
		const float Lap = 4.f * Boat.Straight + 2.f * PI * Boat.Radius;
		float S = FMath::Fmod(Boat.Phase + Seconds * Boat.Speed, Lap);
		const FVector2D Side(-Boat.Along.Y, Boat.Along.X);
		FVector2D Position, Heading;
		if (S < 2.f * Boat.Straight)
		{
			Position = Boat.Centre + Boat.Along * (S - Boat.Straight) - Side * Boat.Radius;
			Heading = Boat.Along;
		}
		else if ((S -= 2.f * Boat.Straight) < PI * Boat.Radius)
		{
			const float Angle = S / Boat.Radius - HALF_PI;
			const FVector2D End = Boat.Centre + Boat.Along * Boat.Straight;
			Position = End + Boat.Along * (FMath::Cos(Angle) * Boat.Radius) + Side * (FMath::Sin(Angle) * Boat.Radius);
			Heading = -Boat.Along * FMath::Sin(Angle) + Side * FMath::Cos(Angle);
		}
		else if ((S -= PI * Boat.Radius) < 2.f * Boat.Straight)
		{
			Position = Boat.Centre + Boat.Along * (Boat.Straight - S) + Side * Boat.Radius;
			Heading = -Boat.Along;
		}
		else
		{
			S -= 2.f * Boat.Straight;
			const float Angle = S / Boat.Radius + HALF_PI;
			const FVector2D End = Boat.Centre - Boat.Along * Boat.Straight;
			Position = End + Boat.Along * (FMath::Cos(Angle) * Boat.Radius) + Side * (FMath::Sin(Angle) * Boat.Radius);
			Heading = -Boat.Along * FMath::Sin(Angle) + Side * FMath::Cos(Angle);
		}
		Heading = Heading.GetSafeNormal();

		// Ride the waves: sit on the surface, pitch and roll with its slope,
		// lean into the turns and lift the bow with speed.
		FVector2D Slope;
		const float Surface = OceanWaves::GetHeightAndSlope(Position, Seconds, Slope);
		const FVector2D Right(-Heading.Y, Heading.X);
		const float Pitch = -FMath::RadiansToDegrees(FMath::Atan(FVector2D::DotProduct(Slope, Heading))) * 0.7f - 2.5f;
		const bool bTurning = !Heading.Equals(Boat.Along, 0.01) && !Heading.Equals(-Boat.Along, 0.01);
		const float Roll = FMath::RadiansToDegrees(FMath::Atan(FVector2D::DotProduct(Slope, Right))) * 0.7f + (bTurning ? -8.f : 0.f);
		const FRotator Rotation(Pitch, FMath::RadiansToDegrees(FMath::Atan2(Heading.Y, Heading.X)), Roll);
		Boat.Look->SetRelativeLocationAndRotation(FVector(Position.X, Position.Y, Surface - 25.f), Rotation);
		Boat.Solid->SetRelativeLocationAndRotation(FVector(Position.X, Position.Y, Surface + 20.f), FRotator(0.f, Rotation.Yaw, 0.f));

		const FVector Forward(Heading.X, Heading.Y, 0.f);
		Boat.Wake.Update(FVector(Position.X, Position.Y, Surface) - Forward * (Boat.Length * 0.45f), Forward, Boat.Speed,
			true, Seconds, Now, Foam, Boat.Length / 750.f);
	}
}

void ARaceEnvironment::UpdateRipples(float Seconds)
{
	if (Ripples == nullptr || !Ripples->IsVisible())
	{
		return;
	}
	FVector Camera = FVector::ZeroVector;
	if (const APlayerController* Controller = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		FRotator Ignored;
		Controller->GetPlayerViewPoint(Camera, Ignored);
	}
	// Each ring grows for half a second and then pops up somewhere else
	// nearby, so the surface looks pelted everywhere you look.
	// The ring model is a metre in radius; scale it to the ring's size in metres.
	const float PerMetre = 100.f / FMath::Max(static_cast<float>(Ripples->GetStaticMesh()->GetBounds().BoxExtent.X), 1.f);
	for (int32 Index = 0; Index < RippleTransforms.Num(); ++Index)
	{
		const float Cycle = Seconds * 1.8f + Index * 0.6180339f;
		const int32 Round = FMath::FloorToInt(Cycle);
		const float Age = Cycle - Round;
		const FVector2D Offset(Wobble(Index * 31 + Round, 1) * 2600.f, Wobble(Index * 17 + Round, 2) * 2600.f + 900.f);
		const FVector2D Point(Camera.X + Offset.X, Camera.Y + Offset.Y);
		const float Size = 0.05f + 0.3f * Age;
		const float Surface = OceanWaves::GetHeight(Point, Seconds);
		RippleTransforms[Index] = FTransform(FRotator::ZeroRotator, FVector(Point.X, Point.Y, Surface + 4.f),
			FVector(Size * PerMetre));
	}
	Ripples->BatchUpdateInstancesTransforms(0, RippleTransforms, true, true, true);
}

void ARaceEnvironment::UpdateSeaMines(float Seconds)
{
	if (Seconds < SeaMineCheck)
	{
		return;
	}
	SeaMineCheck = Seconds + 1.f;
	UWorld* World = GetWorld();
	for (int32 Index = 0; Index < SeaMineHomes.Num(); ++Index)
	{
		if (SeaMines[Index].IsValid())
		{
			continue;
		}
		// Gone (or never laid): lay a fresh one twenty-five seconds later.
		if (SeaMineReturn[Index] <= 0.f)
		{
			SeaMineReturn[Index] = Seconds + (Seconds < 5.f ? 0.f : 25.f);
		}
		if (Seconds < SeaMineReturn[Index])
		{
			continue;
		}
		SeaMineReturn[Index] = 0.f;
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		const FVector2D Home = SeaMineHomes[Index];
		if (ABoatMine* Mine = World->SpawnActor<ABoatMine>(ABoatMine::StaticClass(), FVector(Home.X, Home.Y, 0.0), FRotator::ZeroRotator, Params))
		{
			Mine->InitSeaMine(Home, Index * 1.7f);
			SeaMines[Index] = Mine;
		}
	}
}
