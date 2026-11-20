#include "JetCombatComponent.h"
#include "JetTrainingTarget.h"
#include "JetHealthComponent.h"
#include "JetTerrain.h"
#include "JetPawn.h"
#include "JetAircraft.h"
#include "JetFighter.h"
#include "BoatArt.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Math/RotationMatrix.h"

void UJetCombatComponent::BeginPlay()
{
    Super::BeginPlay();
    Tracers=NewObject<UInstancedStaticMeshComponent>(GetOwner(),TEXT("GunTracers"));
    GetOwner()->AddInstanceComponent(Tracers);
    Tracers->SetupAttachment(GetOwner()->GetRootComponent());
    Tracers->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
    Tracers->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Tracers->SetCastShadow(false);
    Tracers->RegisterComponent();
    MuzzleFlash=NewObject<UStaticMeshComponent>(GetOwner(),TEXT("GunFlash"));
    GetOwner()->AddInstanceComponent(MuzzleFlash);
    MuzzleFlash->SetupAttachment(GetOwner()->GetRootComponent());
    MuzzleFlash->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere")));
    MuzzleFlash->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    MuzzleFlash->SetCastShadow(false);
    MuzzleFlash->SetRelativeScale3D(FVector(0.8,0.3,0.3));
    MuzzleFlash->SetVisibility(false);
    MuzzleFlash->RegisterComponent();
    for (UStaticMeshComponent* Part : {static_cast<UStaticMeshComponent*>(Tracers.Get()),MuzzleFlash.Get()})
        Part->SetMaterial(0,BoatArt::GlowMaterial(Part,FLinearColor(3,1.4,0.15)));
}

void UJetCombatComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    if (IsValid(Target)) Target->Destroy();
    Super::EndPlay(Reason);
}

void UJetCombatComponent::StartTraining()
{
    if (IsValid(Target)) Target->Destroy();
    Rounds.Reset();
    Heat=Cooldown=HitSeconds=FlashSeconds=0.f;
    Accumulator=0.;
    bOverheated=false;
    ShotsFired=Hits=0;
    const FVector Forward=GetOwner()->GetActorForwardVector();
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    Target=GetWorld()->SpawnActor<AJetTrainingTarget>(GetOwner()->GetActorLocation()+Forward*60000.,
        GetOwner()->GetActorRotation(),Params);
    RenderRounds();
    UE_LOG(LogJetFighter,Log,TEXT("Jet training: ready, stationary drone 600 m ahead"));
}

void UJetCombatComponent::Advance(float Dt, bool bTrigger, bool bCanFire, const FVector& InheritedVelocityMetres)
{
    if (!FMath::IsFinite(Dt) || Dt<=0.f) return;
    // Same hitch cap as flight: no catch-up burst after a long stall/pause.
    Accumulator+=FMath::Min(Dt,0.1f);
    constexpr double Fixed=1./120.;
    while (Accumulator+1.e-7>=Fixed)
    {
        Step(float(Fixed),bTrigger,bCanFire,InheritedVelocityMetres);
        Accumulator-=Fixed;
    }
    RenderRounds();
}

bool UJetCombatComponent::TraceSurface(const FVector& Start, const FVector& End, FVector& Contact) const
{
    const int32 Steps=FMath::Max(1,FMath::CeilToInt(FVector::Distance(Start,End)/500.f));
    auto Clearance=[this](const FVector& P)
    {
        return P.Z-25.-JetTerrain::SurfaceHeightCentimetres(FVector2D(P.X,P.Y),GetWorld()->GetTimeSeconds());
    };
    if (Clearance(Start)<=0.) { Contact=Start; return true; }
    FVector Previous=Start;
    for(int32 I=1; I<=Steps; ++I)
    {
        const FVector P=FMath::Lerp(Start,End,double(I)/Steps);
        if (Clearance(P)<=0.)
        {
            FVector Low=Previous,High=P;
            for(int32 J=0;J<10;++J)
            {
                const FVector Mid=(Low+High)*0.5;
                if(Clearance(Mid)>0.) Low=Mid; else High=Mid;
            }
            Contact=High;
            return true;
        }
        Previous=P;
    }
    return false;
}

void UJetCombatComponent::Step(float Dt,bool bTrigger,bool bCanFire,const FVector& InheritedVelocityMetres)
{
    Cooldown=FMath::Max(0.f,Cooldown-Dt);
    HitSeconds=FMath::Max(0.f,HitSeconds-Dt);
    FlashSeconds=FMath::Max(0.f,FlashSeconds-Dt);
    // Heat is per shot; cool when released, disabled or locked out.
    if (!bTrigger || !bCanFire || bOverheated) Heat=FMath::Max(0.f,Heat-0.25f*Dt);
    if (bOverheated && Heat<=0.35f) bOverheated=false;
    if (bTrigger && bCanFire && !bOverheated && Cooldown<=1.e-5f && Rounds.Num()<64)
    {
        const auto* Jet=Cast<AJetPawn>(GetOwner());
        const float Nose=Jet ? JetAircraft::Roster()[Jet->GetAirframeIndex()].LengthMetres*50.f+40.f : 700.f;
        const FVector Muzzle=GetOwner()->GetActorLocation()+GetOwner()->GetActorForwardVector()*Nose;
        FRound Round;
        Round.Position=Muzzle;
        Round.Velocity=(GetOwner()->GetActorForwardVector()*1200.+InheritedVelocityMetres)*100.;
        Round.Id=NextShotId++;
        Rounds.Add(Round);
        ++ShotsFired;
        Cooldown+=1.f/15.f;
        Heat=FMath::Min(1.f,Heat+1.f/38.f);
        bOverheated=Heat>=0.9999f;
        FlashSeconds=0.035f;
        if(MuzzleFlash) MuzzleFlash->SetWorldLocation(Muzzle);
    }
    FCollisionQueryParams Query(SCENE_QUERY_STAT(JetGun),false,GetOwner());
    for(int32 I=Rounds.Num()-1;I>=0;--I)
    {
        auto& Round=Rounds[I];
        const float Remaining=RangeMetres*100.f-Round.Travelled;
        const FVector Delta=Round.Velocity*Dt;
        const FVector End=Round.Position+Delta.GetClampedToMaxSize(Remaining);
        FHitResult Hit;
        const bool bActor=GetWorld()->SweepSingleByChannel(Hit,Round.Position,End,FQuat::Identity,
            ECC_Visibility,FCollisionShape::MakeSphere(25.f),Query);
        FVector Ground;
        const bool bGround=TraceSurface(Round.Position,End,Ground);
        const bool bGroundFirst=bGround && (!bActor || FVector::DistSquared(Round.Position,Ground)<=FVector::DistSquared(Round.Position,Hit.Location));
        if (bActor && !bGroundFirst)
        {
            if(auto* Drone=Cast<AJetTrainingTarget>(Hit.GetActor()))
                if(Drone->ReceiveShot(12.f,0,Round.Id,Hit.ImpactPoint))
                {
                    ++Hits;
                    HitSeconds=0.18f;
                }
        }
        Round.Travelled+=FVector::Distance(Round.Position,End);
        Round.Position=End;
        if(bActor || bGround || Round.Travelled>=RangeMetres*100.f-1.f) Rounds.RemoveAtSwap(I);
    }
}

void UJetCombatComponent::RenderRounds()
{
    if(MuzzleFlash) MuzzleFlash->SetVisibility(FlashSeconds>0.f);
    if(!Tracers) return;
    Tracers->ClearInstances();
    for(const auto& Round:Rounds)
    {
        const FVector Direction=Round.Velocity.GetSafeNormal();
        const float Length=FMath::Min(4500.f,Round.Travelled);
        const FTransform Pose(FRotationMatrix::MakeFromZ(Direction).ToQuat(),Round.Position-Direction*Length*0.5,
            FVector(0.12,0.12,Length/100.f));
        Tracers->AddInstance(Pose,true);
    }
}
