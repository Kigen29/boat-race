#pragma once
#include "CoreMinimal.h"

/** Authored airspace geometry, in metres. Independent of the boat circuits. */
namespace JetTerrain
{
    constexpr double HalfWidth = 8000.;
    constexpr double Cell = 80.;
    constexpr int32 Cells = 200;

    struct FLandmark
    {
        const TCHAR* Name;
        FVector PositionMetres;
    };

    /** Heights at mesh vertices; collision uses the triangles between them. */
    double VertexHeightMetres(double X, double Y);
    double HeightMetres(double X, double Y);
    double SurfaceHeightCentimetres(const FVector2D& XY, double Seconds);
    const TArray<FLandmark>& Landmarks();
}
