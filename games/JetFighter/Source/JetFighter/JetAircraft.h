#pragma once
#include "CoreMinimal.h"

/** Art roster. Flight specialisations are separate, planned tuning work. */
namespace JetAircraft
{
    struct FAirframe
    {
        const TCHAR* Name;
        const TCHAR* Role;
        const TCHAR* Asset;
        float LengthMetres;
        float SpanMetres;
    };
    const TArray<FAirframe>& Roster();
}
