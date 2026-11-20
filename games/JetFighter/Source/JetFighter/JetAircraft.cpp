#include "JetAircraft.h"

const TArray<JetAircraft::FAirframe>& JetAircraft::Roster()
{
    static const TArray<FAirframe> Aircraft = {
        {TEXT("AF-01 KITE"), TEXT("LIGHT FIGHTER"), TEXT("/Game/Aircraft/SM_JetKite.SM_JetKite"), 13.4f, 9.8f},
        {TEXT("AF-02 ROOK"), TEXT("INTERCEPTOR"), TEXT("/Game/Aircraft/SM_JetRook.SM_JetRook"), 17.2f, 11.8f},
        {TEXT("AF-03 MANTA"), TEXT("MULTIROLE"), TEXT("/Game/Aircraft/SM_JetManta.SM_JetManta"), 18.f, 14.f}
    };
    return Aircraft;
}
