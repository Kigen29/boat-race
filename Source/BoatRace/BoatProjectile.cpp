#include "BoatProjectile.h"

#include "BoatArt.h"
#include "BoatAudio.h"
#include "BoatPawn.h"
#include "BoatRace.h"
#include "BoatRaceGameState.h"
#include "BoatRacePlayerState.h"
#include "BoatTrack.h"
#include "OceanWaves.h"
#include "RaceEnvironment.h"

#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/PlayerController.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	/** Server clock, which every machine shares. */
	float ServerNow(const UWorld* World)
	{
		const ABoatRaceGameState* RaceState = World ? World->GetGameState<ABoatRaceGameState>() : nullptr;
		return RaceState ? RaceState->GetServerNow() : (World ? static_cast<float>(World->GetTimeSeconds()) : 0.f);
	}

	UStaticMesh* LoadBasicShape(const TCHAR* Path)
	{
		return LoadObject<UStaticMesh>(nullptr, Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

	void TintMesh(UStaticMeshComponent* Mesh, const FLinearColor& Color)
	{
		if (Mesh == nullptr)
		{
			return;
		}
		if (UMaterialInstanceDynamic* Material = Mesh->CreateAndSetMaterialInstanceDynamic(0))
		{
			Material->SetVectorParameterValue(TEXT("Color"), Color);
		}
	}

	/** Puts an actor on the wave surface at its anchor point. */
	void FloatAt(AActor* Actor, const FVector2D& Anchor, float Seconds, float Rise)
	{
		if (Actor == nullptr)
		{
			return;
		}
		const float SurfaceZ = OceanWaves::GetHeight(Anchor, Seconds);
		Actor->SetActorLocation(FVector(Anchor.X, Anchor.Y, static_cast<double>(SurfaceZ + Rise)));
	}
}

// ===========================================================================
// Blast
// ===========================================================================
void BoatBlast::Detonate(UWorld* World, const FVector& Origin, float Radius, float Damage,
	float Impulse, ABoatPawn* FiredBy)
{
	if (World == nullptr)
	{
		return;
	}

	// The visible flash, spawned by the server and replicated to everyone.
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (ABoatExplosion* Effect = World->SpawnActor<ABoatExplosion>(ABoatExplosion::StaticClass(), Origin, FRotator::ZeroRotator, Params))
	{
		Effect->SetBlastRadius(Radius);
	}

	ABoatRacePlayerState* Attacker = (FiredBy != nullptr) ? FiredBy->GetPlayerState<ABoatRacePlayerState>() : nullptr;
	ABoatRaceGameState* RaceState = World->GetGameState<ABoatRaceGameState>();

	for (TActorIterator<ABoatPawn> It(World); It; ++It)
	{
		ABoatPawn* Boat = *It;
		if (Boat == nullptr)
		{
			continue;
		}

		const float Distance = static_cast<float>(FVector::Dist(Boat->GetRaceLocation(), Origin));
		if (Distance > Radius)
		{
			continue;
		}

		// Linear falloff: a direct hit hurts, the edge of the blast only shoves.
		const float Falloff = FMath::Clamp(1.f - Distance / Radius, 0.f, 1.f);

		// The shove reaches everyone, including whoever fired: a close shot
		// knocks you off your own line too.
		Boat->ApplyExplosionImpulse(Origin, Impulse * Falloff);

		if (Boat == FiredBy)
		{
			continue; // Your own weapon cannot damage you.
		}

		const bool bWrecked = Boat->ApplyDamage(Damage * Falloff, Attacker);
		if (bWrecked && RaceState != nullptr)
		{
			const ABoatRacePlayerState* Victim = Boat->GetPlayerState<ABoatRacePlayerState>();
			RaceState->ReportWreck(
				Victim != nullptr ? Victim->GetPlayerName() : FString(TEXT("A boat")),
				Attacker != nullptr ? Attacker->GetPlayerName() : FString());
		}
	}

	// Scatter any floating debris caught in it.
	for (TActorIterator<ADebrisBarrel> It(World); It; ++It)
	{
		if (ADebrisBarrel* Barrel = *It)
		{
			Barrel->PushFromBlast(Origin, Radius, Impulse);
		}
	}
}

// ===========================================================================
// Explosion effect
// ===========================================================================
ABoatExplosion::ABoatExplosion()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicatingMovement(false);
	// Long enough for the smoke to clear; it also destroys itself.
	InitialLifeSpan = 5.f;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));

	Flash = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Flash"));
	SetRootComponent(Flash);
	Flash->SetStaticMesh(SphereFinder.Object);
	Flash->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Flash->SetCastShadow(false);

	Ring = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Ring"));
	Ring->SetupAttachment(Flash);
	Ring->SetStaticMesh(SphereFinder.Object);
	Ring->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Ring->SetCastShadow(false);
	Ring->SetUsingAbsoluteScale(true);

	Glare = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glare"));
	Glare->SetupAttachment(Flash);
	Glare->SetUsingAbsoluteScale(true);
	Glare->SetLightColor(FLinearColor(1.f, 0.6f, 0.25f));
	Glare->SetIntensity(0.f);
	Glare->SetCastShadows(false);
}

void ABoatExplosion::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABoatExplosion, BlastRadius);
}

void ABoatExplosion::SetBlastRadius(float Radius)
{
	BlastRadius = FMath::Max(Radius, 50.f);
}

void ABoatExplosion::BeginPlay()
{
	Super::BeginPlay();

	// The core flash: additive if import_assets.sh built M_Explosion.
	UMaterialInterface* BlastMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Higgsfield/M_Explosion.M_Explosion"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (BlastMaterial != nullptr)
	{
		Flash->SetMaterial(0, BlastMaterial);
	}
	TintMesh(Flash, FLinearColor(9.f, 3.5f, 0.6f));
	RingMaterial = BoatArt::SoftMaterial(Ring, FLinearColor(0.95f, 0.97f, 1.f), 0.7f);
	Ring->SetMaterial(0, RingMaterial);

	const UWorld* World = GetWorld();
	if (World == nullptr || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	BoatAudio::PlayAt(this, EBoatSound::Explosion, GetActorLocation(), 1.f, FMath::FRandRange(0.85f, 1.1f));
	Glare->SetAttenuationRadius(BlastRadius * 5.f);

	UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	UStaticMesh* Plank = BoatArt::Model(TEXT("Log"));
	const float R = BlastRadius;

	// Kinds: 0 flame, 1 water jet, 2 debris chunk, 3 smoke.
	auto AddBit = [&](int32 Kind, const FVector& Start, const FVector& Velocity, float Delay, float Life, float Size)
	{
		FBit Bit;
		Bit.Kind = Kind;
		Bit.Start = Start;
		Bit.Velocity = Velocity;
		Bit.Delay = Delay;
		Bit.Life = Life;
		Bit.Size = Size;
		Bit.Spin = FVector(FMath::FRandRange(-400.f, 400.f), FMath::FRandRange(-400.f, 400.f), FMath::FRandRange(-400.f, 400.f));

		UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(this);
		Mesh->SetStaticMesh(Kind == 2 ? (Plank ? Plank : Cube) : Sphere);
		Mesh->SetupAttachment(Flash);
		Mesh->SetUsingAbsoluteLocation(true);
		Mesh->SetUsingAbsoluteRotation(true);
		Mesh->SetUsingAbsoluteScale(true);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetCastShadow(Kind == 2);
		Mesh->SetVisibility(false);
		Mesh->RegisterComponent();
		UMaterialInstanceDynamic* Material = nullptr;
		if (Kind == 0)
		{
			Material = BoatArt::GlowMaterial(Mesh, FLinearColor(8.f, 2.6f, 0.3f));
		}
		else if (Kind == 2)
		{
			BoatArt::FLook Charred;
			Charred.Brightness = 0.25f;
			if (Plank)
			{
				BoatArt::Paint(Mesh, Charred);
			}
			else
			{
				Material = Mesh->CreateAndSetMaterialInstanceDynamic(0);
				if (Material)
				{
					Material->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.05f, 0.04f, 0.03f));
				}
			}
		}
		else
		{
			Material = BoatArt::SoftMaterial(Mesh, Kind == 1 ? FLinearColor(0.92f, 0.96f, 1.f) : FLinearColor(0.05f, 0.05f, 0.05f), 0.f);
		}
		if (Material != nullptr && Kind != 2)
		{
			Mesh->SetMaterial(0, Material);
		}
		Bits.Add(Bit);
		BitMeshes.Add(Mesh);
		BitMaterials.Add(Material);
	};

	const FVector Origin = GetActorLocation();
	for (int32 Index = 0; Index < 11; ++Index)
	{
		// Many smaller flames bursting outwards read as fire; one big one reads as a ball.
		const FVector Out = FMath::VRand();
		AddBit(0, Origin + FVector(Out.X, Out.Y, FMath::Abs(Out.Z)) * R * 0.15f,
			FVector(Out.X, Out.Y, FMath::Abs(Out.Z) + 0.4f) * FMath::FRandRange(400.f, 900.f),
			Index * 0.025f, FMath::FRandRange(0.45f, 0.85f), R * FMath::FRandRange(0.25f, 0.5f));
	}
	for (int32 Index = 0; Index < 7; ++Index)
	{
		const float Angle = Index * 0.9f + FMath::FRandRange(-0.3f, 0.3f);
		const FVector Out(FMath::Cos(Angle), FMath::Sin(Angle), 0.f);
		AddBit(1, Origin + Out * R * 0.15f, Out * FMath::FRandRange(200.f, 500.f) + FVector(0, 0, FMath::FRandRange(1600.f, 2600.f)),
			0.02f, FMath::FRandRange(1.5f, 2.1f), R * FMath::FRandRange(0.22f, 0.35f));
	}
	for (int32 Index = 0; Index < 9; ++Index)
	{
		const FVector Out = FMath::VRand();
		AddBit(2, Origin + FVector(0, 0, 80), FVector(Out.X, Out.Y, 0.f) * FMath::FRandRange(900.f, 2200.f) + FVector(0, 0, FMath::FRandRange(900.f, 2000.f)),
			0.f, FMath::FRandRange(1.4f, 2.4f), FMath::FRandRange(0.12f, 0.3f));
	}
	for (int32 Index = 0; Index < 12; ++Index)
	{
		const FVector Offset = FMath::VRand() * R * 0.35f;
		AddBit(3, Origin + FVector(Offset.X, Offset.Y, FMath::Abs(Offset.Z) + 150.f),
			FVector(Offset.X, Offset.Y, 0.f) * 0.6f + FVector(0, 0, FMath::FRandRange(300.f, 650.f)),
			0.25f + Index * 0.05f, FMath::FRandRange(2.4f, 3.6f), R * FMath::FRandRange(0.3f, 0.55f));
	}

	// Shake the camera of anyone close enough to feel it.
	if (const APlayerController* Controller = World->GetFirstPlayerController())
	{
		if (const APawn* Viewer = Controller->GetPawn())
		{
			const float Distance = static_cast<float>(FVector::Dist(Viewer->GetActorLocation(), Origin));
			if (Distance < R * 4.f)
			{
				// Through the boat's own hit feedback, which every boat already has.
				if (ABoatPawn* Boat = const_cast<ABoatPawn*>(Cast<ABoatPawn>(Viewer)))
				{
					Boat->ShakeCamera(FMath::Clamp(1.2f - Distance / (R * 4.f), 0.2f, 1.f));
				}
			}
		}
	}
}

void ABoatExplosion::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	Age += DeltaSeconds;
	const float Alpha = FMath::Clamp(Age / Lifetime, 0.f, 1.f);
	const float Seconds = ServerNow(GetWorld());

	// The core flash: swells for a tenth of a second, then collapses.
	const float FlashAlpha = FMath::Clamp(Age / 0.55f, 0.f, 1.f);
	const float Swell = FMath::Sin(FMath::Clamp(FlashAlpha * 3.f, 0.f, 1.f) * HALF_PI);
	const float Collapse = 1.f - FMath::Clamp((FlashAlpha - 0.25f) / 0.75f, 0.f, 1.f);
	Flash->SetWorldScale3D(FVector(FMath::Max((BlastRadius / 50.f) * 0.8f * Swell * Collapse, 0.01f)));
	Flash->SetVisibility(FlashAlpha < 1.f);
	Glare->SetIntensity(FMath::Max(0.f, 1.f - Age / 0.6f) * 400000.f);

	// The shock ring runs out across the water and thins away.
	const float RingAge = FMath::Clamp(Age / 1.4f, 0.f, 1.f);
	const FVector Origin = GetActorLocation();
	const float RingSize = (BlastRadius / 50.f) * (0.3f + 2.4f * FMath::Sqrt(RingAge));
	Ring->SetWorldScale3D(FVector(RingSize, RingSize, 0.05f));
	Ring->SetWorldLocation(FVector(Origin.X, Origin.Y, OceanWaves::GetHeight(FVector2D(Origin.X, Origin.Y), Seconds) + 10.0));
	BoatArt::SetPuff(RingMaterial, FLinearColor(0.95f, 0.97f, 1.f), 0.7f * (1.f - RingAge));
	Ring->SetVisibility(RingAge < 1.f);

	for (int32 Index = 0; Index < Bits.Num(); ++Index)
	{
		const FBit& Bit = Bits[Index];
		UStaticMeshComponent* Mesh = BitMeshes[Index];
		const float Local = Age - Bit.Delay;
		if (Mesh == nullptr)
		{
			continue;
		}
		if (Local < 0.f || Local > Bit.Life)
		{
			Mesh->SetVisibility(false);
			continue;
		}
		Mesh->SetVisibility(true);
		const float U = Local / Bit.Life;
		switch (Bit.Kind)
		{
		case 0: // flame: bursts out, rises, turns from yellow to deep red and shrinks
		{
			const float Size = Bit.Size / 50.f * FMath::Sin(FMath::Min(U * 2.5f, 1.f) * HALF_PI) * (1.f - U * 0.6f);
			Mesh->SetWorldLocation(Bit.Start + Bit.Velocity * Local);
			Mesh->SetWorldScale3D(FVector(FMath::Max(Size, 0.01f)));
			if (BitMaterials[Index])
			{
				BitMaterials[Index]->SetVectorParameterValue(TEXT("Color"),
					FMath::Lerp(FLinearColor(14.f, 5.f, 0.6f), FLinearColor(4.f, 0.5f, 0.03f), FMath::Sqrt(U)) * (1.f - U * U));
			}
			break;
		}
		case 1: // water jet: shoots up as a tall column, falls back and spreads
		{
			const FVector Position = Bit.Start + Bit.Velocity * Local + FVector(0, 0, -490.f * Local * Local);
			const float Rise = FMath::Max(static_cast<float>(Bit.Velocity.Z) - 980.f * Local, -400.f);
			const float Stretch = 1.f + FMath::Clamp(Rise / 900.f, 0.f, 2.5f);
			Mesh->SetWorldLocation(FVector(Position.X, Position.Y, FMath::Max(Position.Z, -50.0)));
			Mesh->SetWorldScale3D(FVector(Bit.Size / 50.f * (0.6f + U), Bit.Size / 50.f * (0.6f + U), Bit.Size / 50.f * Stretch));
			BoatArt::SetPuff(BitMaterials[Index], FLinearColor(0.92f, 0.96f, 1.f), 0.8f * (1.f - U));
			break;
		}
		case 2: // debris: ballistic, tumbling, gone when it hits the water
		{
			const FVector Position = Bit.Start + Bit.Velocity * Local + FVector(0, 0, -490.f * Local * Local);
			const float Surface = OceanWaves::GetHeight(FVector2D(Position.X, Position.Y), Seconds);
			if (Position.Z < Surface && Local > 0.3f)
			{
				Mesh->SetVisibility(false);
				break;
			}
			Mesh->SetWorldLocationAndRotation(Position, FRotator(Bit.Spin.X * Local, Bit.Spin.Y * Local, Bit.Spin.Z * Local));
			Mesh->SetWorldScale3D(FVector(Bit.Size));
			break;
		}
		default: // smoke: rolls upwards, growing and fading
		{
			const FVector Position = Bit.Start + Bit.Velocity * Local * (1.f - U * 0.4f);
			const float Size = Bit.Size / 50.f * (0.5f + 1.3f * FMath::Sqrt(U));
			Mesh->SetWorldLocation(Position);
			Mesh->SetWorldScale3D(FVector(Size, Size, Size * 0.85f));
			const float Grey = FMath::Lerp(0.03f, 0.18f, U);
			BoatArt::SetPuff(BitMaterials[Index], FLinearColor(Grey, Grey, Grey), 0.6f * FMath::Min(U * 6.f, 1.f) * (1.f - U));
			break;
		}
		}
	}

	if (Alpha >= 1.f)
	{
		Destroy();
	}
}

// ===========================================================================
// Torpedo
// ===========================================================================
ABoatTorpedo::ABoatTorpedo()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	// Its position is worked out from the launch details rather than sent.
	SetReplicatingMovement(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(45.f);
	Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetRootComponent(Collision);

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Collision);
	Body->SetStaticMesh(CylinderFinder.Object);
	// Pitch 90 lays the cylinder along the direction of travel.
	Body->SetRelativeRotation(FRotator(90.f, 0.f, 0.f));
	Body->SetRelativeScale3D(FVector(0.35f, 0.35f, 1.6f));
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetCastShadow(false);

	Wake = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Wake"));
	Wake->SetupAttachment(Collision);
	Wake->SetStaticMesh(SphereFinder.Object);
	Wake->SetRelativeLocation(FVector(-90.f, 0.f, -10.f));
	Wake->SetRelativeScale3D(FVector(1.1f, 0.5f, 0.12f));
	Wake->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Wake->SetCastShadow(false);
}

void ABoatTorpedo::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABoatTorpedo, LaunchLocation);
	DOREPLIFETIME(ABoatTorpedo, LaunchDirection);
	DOREPLIFETIME(ABoatTorpedo, LaunchTime);
}

void ABoatTorpedo::InitProjectile(ABoatPawn* Shooter)
{
	FiredBy = Shooter;
	LaunchLocation = GetActorLocation();
	LaunchDirection = GetActorForwardVector().GetSafeNormal2D();
	LaunchTime = ServerNow(GetWorld());
	PreviousLocation = LaunchLocation;
}

void ABoatTorpedo::BeginPlay()
{
	Super::BeginPlay();
	if (UStaticMesh* Model = BoatArt::Model(TEXT("Torpedo")))
	{
		// The model lies along X already; drawn half as big again so it reads at speed.
		Body->SetStaticMesh(Model);
		Body->SetRelativeRotation(FRotator::ZeroRotator);
		Body->SetRelativeScale3D(FVector(BoatArt::UnitScale(Model, 0.5f) * 1.5f));
		BoatArt::Paint(Body);
	}
	else
	{
		TintMesh(Body, FLinearColor(0.75f, 0.75f, 0.8f));
	}
	Wake->SetMaterial(0, BoatArt::SoftMaterial(Wake, FLinearColor(0.92f, 0.96f, 1.f), 0.7f));
	Wake->SetRelativeScale3D(FVector(2.4f, 0.7f, 0.18f));
	Wake->SetRelativeLocation(FVector(-260.f, 0.f, -15.f));
	BoatAudio::PlayAt(this, EBoatSound::Torpedo, GetActorLocation(), 1.f, FMath::FRandRange(0.95f, 1.1f));
	if (PreviousLocation.IsNearlyZero())
	{
		PreviousLocation = GetActorLocation();
	}
}

void ABoatTorpedo::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UWorld* World = GetWorld();
	if (World == nullptr || bExploded)
	{
		return;
	}

	const float Now = ServerNow(World);
	const float Elapsed = Now - LaunchTime;
	if (Elapsed < 0.f)
	{
		return; // Clocks not yet in step on a client that just joined.
	}

	// Same arithmetic on every machine, so it looks identical everywhere
	// without a single position update being sent.
	FVector Target = LaunchLocation + LaunchDirection * (Speed * Elapsed);
	Target.Z = OceanWaves::GetHeight(FVector2D(Target.X, Target.Y), Now) + 25.f;
	SetActorLocation(Target, false, nullptr, ETeleportType::TeleportPhysics);
	SetActorRotation(LaunchDirection.Rotation());

	if (!HasAuthority())
	{
		return; // Only the server decides what was hit.
	}

	// Sweep the gap since the last check, so nothing is missed at 55 m/s.
	FHitResult Hit;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);
	if (FiredBy != nullptr)
	{
		QueryParams.AddIgnoredActor(FiredBy); // Cannot hit the boat that fired it.
	}
	const bool bHit = World->SweepSingleByChannel(Hit, PreviousLocation, Target, FQuat::Identity,
		ECC_Pawn, FCollisionShape::MakeSphere(45.f), QueryParams);
	PreviousLocation = Target;

	if (bHit)
	{
		Explode(Hit.ImpactPoint);
		return;
	}

	if (Elapsed > RangeSeconds
		|| FMath::Abs(Target.X) > BoatTrack::BoundsX
		|| FMath::Abs(Target.Y) > BoatTrack::BoundsY)
	{
		Explode(Target); // Runs out of fuel and goes off harmlessly far away.
	}
}

void ABoatTorpedo::Explode(const FVector& At)
{
	if (bExploded || !HasAuthority())
	{
		return;
	}
	bExploded = true;
	BoatBlast::Detonate(GetWorld(), At, BlastRadius, Damage, BlastImpulse, FiredBy);
	Destroy();
}

// ===========================================================================
// Mine
// ===========================================================================
ABoatMine::ABoatMine()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicatingMovement(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	SetRootComponent(Body);
	Body->SetStaticMesh(SphereFinder.Object);
	Body->SetRelativeScale3D(FVector(0.9f));
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetCastShadow(false);

	Light = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Light"));
	Light->SetupAttachment(Body);
	Light->SetStaticMesh(SphereFinder.Object);
	Light->SetRelativeLocation(FVector(0.f, 0.f, 55.f));
	Light->SetRelativeScale3D(FVector(0.3f));
	Light->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Light->SetCastShadow(false);
}

void ABoatMine::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABoatMine, Anchor);
	DOREPLIFETIME(ABoatMine, DropTime);
}

void ABoatMine::InitProjectile(ABoatPawn* Shooter)
{
	FiredBy = Shooter;
	const FVector Location = GetActorLocation();
	Anchor = FVector2D(Location.X, Location.Y);
	DropTime = ServerNow(GetWorld());
}

void ABoatMine::InitSeaMine(const FVector2D& InHome, float Phase)
{
	FiredBy = nullptr;
	Home = InHome;
	Anchor = InHome;
	DriftPhase = Phase;
	bSeaMine = true;
	bArmSounded = true; // no beep for these
	DropTime = ServerNow(GetWorld()) - ArmSeconds;
}

void ABoatMine::BeginPlay()
{
	Super::BeginPlay();
	if (UStaticMesh* Model = BoatArt::Model(TEXT("Mine")))
	{
		Body->SetStaticMesh(Model);
		Body->SetRelativeScale3D(FVector(BoatArt::UnitScale(Model, 1.5f) * 1.2f));
		BoatArt::Paint(Body);
		Light->SetRelativeLocation(FVector(0.f, 0.f, 80.f));
		Light->SetRelativeScale3D(FVector(0.2f));
	}
	else
	{
		TintMesh(Body, FLinearColor(0.08f, 0.08f, 0.09f));
	}
	Light->SetMaterial(0, BoatArt::GlowMaterial(Light, FLinearColor(4.f, 0.2f, 0.2f)));
	BoatAudio::PlayAt(this, EBoatSound::MineDrop, GetActorLocation(), 0.9f);
}

void ABoatMine::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UWorld* World = GetWorld();
	if (World == nullptr || bExploded)
	{
		return;
	}

	const float Now = ServerNow(World);
	const float Age = Now - DropTime;
	if (bSeaMine && HasAuthority())
	{
		// Drifting slowly on a six-metre circle.
		Anchor = Home + FVector2D(FMath::Sin(Now * 0.07f + DriftPhase), FMath::Cos(Now * 0.05f + DriftPhase)) * 600.0;
	}
	FloatAt(this, Anchor, Now, 30.f);

	// The lamp blinks slowly while it arms, then fast once it is live.
	const bool bArmed = Age >= ArmSeconds;
	const float BlinkRate = bArmed ? 7.f : 2.f;
	const float Blink = 0.5f + 0.5f * FMath::Sin(Now * BlinkRate);
	if (UMaterialInstanceDynamic* Lamp = Cast<UMaterialInstanceDynamic>(Light->GetMaterial(0)))
	{
		Lamp->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.4f + 8.f * Blink, 0.15f, 0.1f));
	}
	if (bArmed && !bArmSounded)
	{
		bArmSounded = true;
		BoatAudio::PlayAt(this, EBoatSound::MineArm, GetActorLocation(), 0.6f);
	}

	if (!HasAuthority() || !bArmed)
	{
		return;
	}

	for (TActorIterator<ABoatPawn> It(World); It; ++It)
	{
		ABoatPawn* Boat = *It;
		if (Boat == nullptr || Boat->IsWrecked())
		{
			continue;
		}
		// The boat that laid it gets a moment to clear the area; after that it
		// is as dangerous to them as to anyone.
		if (Boat == FiredBy && Age < ArmSeconds + 2.f)
		{
			continue;
		}
		if (FVector::Dist(Boat->GetRaceLocation(), GetActorLocation()) <= TriggerRadius)
		{
			bExploded = true;
			BoatBlast::Detonate(World, GetActorLocation(), BlastRadius, Damage, BlastImpulse, FiredBy);
			Destroy();
			return;
		}
	}

	if (!bSeaMine && Age > LifetimeSeconds)
	{
		Destroy(); // Sinks harmlessly rather than littering the course all race.
	}
}

// ===========================================================================
// Ammo crate
// ===========================================================================
AWeaponCrate::AWeaponCrate()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicatingMovement(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeFinder(TEXT("/Engine/BasicShapes/Cone.Cone"));

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	SetRootComponent(Body);
	Body->SetStaticMesh(CubeFinder.Object);
	Body->SetRelativeScale3D(FVector(1.3f, 1.3f, 1.f));
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// A floating marker, so a crate can be spotted from the last gate.
	Marker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Marker"));
	Marker->SetupAttachment(Body);
	Marker->SetStaticMesh(ConeFinder.Object);
	Marker->SetRelativeLocation(FVector(0.f, 0.f, 180.f));
	Marker->SetRelativeScale3D(FVector(0.9f, 0.9f, 1.4f));
	Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Marker->SetCastShadow(false);
}

void AWeaponCrate::BeginPlay()
{
	Super::BeginPlay();
	if (UStaticMesh* Model = BoatArt::Model(TEXT("Crate")))
	{
		// Undo the placeholder's flattening and draw the 1.8 m float-collared crate.
		Body->SetStaticMesh(Model);
		Body->SetRelativeScale3D(FVector(BoatArt::UnitScale(Model, 1.8f) * 1.15f));
		BoatArt::Paint(Body);
		Marker->SetRelativeLocation(FVector(0.f, 0.f, 260.f));
	}
	else
	{
		TintMesh(Body, FLinearColor(0.5f, 0.35f, 0.12f));
	}
	// A glowing yellow arrow above it, visible from the gate before.
	Marker->SetRelativeRotation(FRotator(180.f, 0.f, 0.f));
	Marker->SetMaterial(0, BoatArt::GlowMaterial(Marker, FLinearColor(4.f, 2.8f, 0.2f)));
}

void AWeaponCrate::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWeaponCrate, bAvailable);
	DOREPLIFETIME(AWeaponCrate, Anchor);
}

void AWeaponCrate::SetUpCrate(const FVector2D& Position, int32 InTorpedoes, int32 InMines, float InRespawnSeconds)
{
	Anchor = Position;
	Torpedoes = InTorpedoes;
	Mines = InMines;
	RespawnSeconds = FMath::Max(InRespawnSeconds, 1.f);
	bAvailable = true;
}

void AWeaponCrate::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	const float Now = ServerNow(World);
	FloatAt(this, Anchor, Now, 45.f);
	// Turning slowly so it catches the light and is easier to see.
	SetActorRotation(FRotator(0.f, Now * 45.f, 0.f));

	Body->SetVisibility(bAvailable);
	Marker->SetVisibility(bAvailable);
	Marker->SetRelativeLocation(FVector(0.f, 0.f, 260.f + 40.f * FMath::Sin(Now * 3.f)));
	if (bWasAvailable && !bAvailable)
	{
		// Every machine hears the pickup when the replicated flag flips.
		BoatAudio::PlayAt(this, EBoatSound::Pickup, GetActorLocation(), 0.8f);
	}
	bWasAvailable = bAvailable;

	if (!HasAuthority())
	{
		return;
	}

	if (!bAvailable)
	{
		if (Now >= RespawnAtServerTime)
		{
			bAvailable = true;
		}
		return;
	}

	for (TActorIterator<ABoatPawn> It(World); It; ++It)
	{
		ABoatPawn* Boat = *It;
		if (Boat == nullptr || Boat->IsWrecked())
		{
			continue;
		}
		if (FVector::Dist(Boat->GetRaceLocation(), GetActorLocation()) <= PickupRadius)
		{
			// What is inside: a shield, a missile with flares, grenades, or
			// the usual torpedoes and mines.
			const float Roll = FMath::FRand();
			if (Roll < ShieldChance)
			{
				Boat->GrantShield(ShieldSeconds);
				Boat->NotifyPickup(1, 0, 0);
			}
			else if (Roll < ShieldChance + 0.25f)
			{
				Boat->GiveSpecials(1, 1, 0);
				Boat->NotifyPickup(2, 0, 0);
			}
			else if (Roll < ShieldChance + 0.45f)
			{
				Boat->GiveSpecials(0, 0, 3);
				Boat->NotifyPickup(3, 0, 0);
			}
			else
			{
				Boat->GiveAmmo(Torpedoes, Mines);
				Boat->NotifyPickup(0, static_cast<uint8>(Torpedoes), static_cast<uint8>(Mines));
			}
			bAvailable = false;
			RespawnAtServerTime = Now + RespawnSeconds;
			return;
		}
	}
}
