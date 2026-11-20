#include "JetTerrain.h"
#include "OceanWaves.h"

namespace JetTerrain
{
    struct FIsland { double X, Y, RadiusX, RadiusY, Peak; };
    static constexpr FIsland Islands[] = {
        {-2300., -1200., 1700., 1100., 650.},
        {2400., 1200., 1600., 1200., 950.},
        {0., -2800., 1400., 950., 420.},
        {-600., 3100., 1300., 850., 680.},
        {900., -600., 300., 210., 160.},
        {-850., 600., 260., 200., 130.}
    };

    double VertexHeightMetres(double X, double Y)
    {
        double Height = -60.;
        for (const FIsland& Island : Islands)
        {
            const double R2 = FMath::Square((X-Island.X)/Island.RadiusX) + FMath::Square((Y-Island.Y)/Island.RadiusY);
            if (R2 < 1.) Height = FMath::Max(Height, -60. + (Island.Peak+60.)*FMath::Square(1.-R2));
        }
        // A flat airstrip, blended into the south island instead of floating
        // a runway above a rounded hill. One grid cell of padding keeps the
        // entire runway, including its ends, on level rendered triangles.
        const double DX = FMath::Max(FMath::Abs(X)-600., 0.);
        const double DY = FMath::Max(FMath::Abs(Y+2800.)-170., 0.);
        const double Blend = FMath::Clamp(FMath::Sqrt(DX*DX+DY*DY)/250., 0., 1.);
        const double Weight = 1. - Blend*Blend*(3.-2.*Blend);
        return FMath::Lerp(Height, 220., Weight);
    }

    double HeightMetres(double X, double Y)
    {
        // The exact same diagonal and planes as the rendered grid. Using the
        // smooth generating function directly can put invisible ground above
        // a coarse triangle, especially near mountain tops.
        const double X0 = FMath::FloorToDouble((X+HalfWidth)/Cell)*Cell-HalfWidth;
        const double Y0 = FMath::FloorToDouble((Y+HalfWidth)/Cell)*Cell-HalfWidth;
        const double U = (X-X0)/Cell, V = (Y-Y0)/Cell;
        const double H10 = VertexHeightMetres(X0+Cell, Y0);
        const double H01 = VertexHeightMetres(X0, Y0+Cell);
        if (U+V <= 1.)
        {
            const double H00 = VertexHeightMetres(X0, Y0);
            return H00 + U*(H10-H00) + V*(H01-H00);
        }
        const double H11 = VertexHeightMetres(X0+Cell, Y0+Cell);
        return H11 + (1.-U)*(H01-H11) + (1.-V)*(H10-H11);
    }

    double SurfaceHeightCentimetres(const FVector2D& XY, double Seconds)
    {
        return FMath::Max(HeightMetres(XY.X/100., XY.Y/100.)*100.,
            double(OceanWaves::GetHeight(XY, Seconds)));
    }

    const TArray<FLandmark>& Landmarks()
    {
        static const TArray<FLandmark> Points = {
            {TEXT("EAST SPIRE"), FVector(2400, 1200, HeightMetres(2400, 1200)+30.)},
            {TEXT("SOUTH AIRSTRIP"), FVector(0, -2800, 235)},
            {TEXT("WEST RIDGE"), FVector(-2300, -1200, HeightMetres(-2300, -1200)+30.)},
            {TEXT("NORTH ISLAND"), FVector(-600, 3100, HeightMetres(-600, 3100)+30.)},
            {TEXT("HOME BAY"), FVector(0, 0, 80)}
        };
        return Points;
    }
}
