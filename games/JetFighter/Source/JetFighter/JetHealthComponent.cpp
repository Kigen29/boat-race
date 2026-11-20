#include "JetHealthComponent.h"

void UJetHealthComponent::ResetHull(float Maximum, int32 NewTeam)
{
    MaxHull = FMath::Max(1.f, Maximum);
    Hull = MaxHull;
    Team = NewTeam;
    LastShotId = 0;
}

bool UJetHealthComponent::ApplyHit(float Damage, int32 SourceTeam, uint64 ShotId)
{
    if (!FMath::IsFinite(Damage) || Damage <= 0.f || SourceTeam == Team || IsDestroyed()
        || ShotId == 0 || ShotId <= LastShotId) return false;
    LastShotId = ShotId;
    Hull = FMath::Max(0.f, Hull-Damage);
    return true;
}
