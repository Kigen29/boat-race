#include "JetGame.h"
#include "JetPawn.h"
#include "JetTerrain.h"
#include "JetAircraft.h"
#include "JetCombatComponent.h"
#include "JetTrainingTarget.h"
#include "JetHealthComponent.h"
#include "Fonts/CompositeFont.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Fonts/FontMeasure.h"
#include "Fonts/SlateFontInfo.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"

namespace JetHudLocal
{
    const FLinearColor Ink(0.88f, 0.98f, 1.f);
    const FLinearColor Dim(0.5f, 0.7f, 0.75f);
    const FLinearColor Cyan(0.3f, 0.95f, 0.95f);
    const FLinearColor Amber(1.f, 0.7f, 0.25f);
    const FLinearColor Red(1.f, 0.3f, 0.2f);
    const FLinearColor Panel(0.005f, 0.018f, 0.03f, 0.78f);

    float Heading(float Yaw) { return FMath::Fmod(Yaw + 360.f, 360.f); }
}

void AJetHUD::BeginPlay()
{
    Super::BeginPlay();
    FlightFont = LoadObject<UFont>(nullptr, TEXT("/Engine/EngineFonts/Roboto.Roboto"));
    const FString Folder = FPaths::ProjectContentDir()/TEXT("Fonts");
    const FString Regular = Folder/TEXT("Rajdhani-Regular.ttf");
    const FString Bold = Folder/TEXT("Rajdhani-SemiBold.ttf");
    if (IFileManager::Get().FileExists(*Regular) && IFileManager::Get().FileExists(*Bold))
    {
        // Canvas requires a UFont carrier even when Slate can measure the
        // raw composite font. Without it, valid glyphs never reach DrawItem.
        auto* Font = NewObject<UFont>(this, TEXT("JetAviationFont"));
        Font->FontCacheType = EFontCacheType::Runtime;
        auto& Faces = Font->GetMutableInternalCompositeFont().DefaultTypeface.Fonts;
        Faces.Emplace(FName(TEXT("Regular")), Regular, EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
        Faces.Emplace(FName(TEXT("Bold")), Bold, EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
        FlightFont = Font;
        bTechnicalTypeface = true;
    }
}

void AJetHUD::Label(const FString& Text, float X, float Y, float Size, const FLinearColor& Color, float Align)
{
    if (FlightFont && FSlateApplication::IsInitialized() && FSlateApplication::Get().GetRenderer())
    {
        const FSlateFontInfo Font = bTechnicalTypeface
            ? FSlateFontInfo(FlightFont, Size*1.12f, FName(Size >= 20.f ? TEXT("Bold") : TEXT("Regular")))
            : FSlateFontInfo(FlightFont, Size, FName(TEXT("Regular")));
        const auto Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
        const FVector2D Extent = Measure->Measure(Text, Font);
        FCanvasTextItem Item(FVector2D(FMath::RoundToFloat(X-Extent.X*Align), Y), FText::FromString(Text), Font, Color);
        Item.EnableShadow(FLinearColor(0, 0, 0, 0.8f), FVector2D(1, 1));
        Canvas->DrawItem(Item);
    }
    else DrawText(Text, Color, X, Y, GEngine->GetMediumFont(), Size/16.f);
}

void AJetHUD::Circle(const FVector2D& Centre, float Radius, const FLinearColor& Color)
{
    for (int32 I = 0; I < 32; ++I)
    {
        const float A = I * 2.f*PI/32.f, B = (I+1)*2.f*PI/32.f;
        DrawLine(Centre.X+FMath::Cos(A)*Radius, Centre.Y+FMath::Sin(A)*Radius,
            Centre.X+FMath::Cos(B)*Radius, Centre.Y+FMath::Sin(B)*Radius, Color, 1.5f);
    }
}

void AJetHUD::DrawHUD()
{
    Super::DrawHUD();
    if (!Canvas || !GEngine) return;
    using namespace JetHudLocal;
    const auto* PC = Cast<AJetPlayerController>(GetOwningPlayerController());
    const auto* Jet = PC ? Cast<AJetPawn>(PC->GetPawn()) : nullptr;
    const auto* Scout = PC ? Cast<AJetScoutPawn>(PC->GetPawn()) : nullptr;
    if (!Jet && !Scout) return;
    const float W = Canvas->SizeX, H = Canvas->SizeY;
    const float S = FMath::Clamp(H/1080.f, 0.7f, 1.5f);
    const float Mid = W*0.5f;
    DrawRect(Panel, 20*S, 20*S, 245*S, 74*S);
    Label(TEXT("JET FIGHTERS"), 36*S, 29*S, 23*S, Ink);
    Label(Scout ? TEXT("ALTITUDE SURVEY") : JetAircraft::Roster()[Jet->GetAirframeIndex()].Name, 36*S, 61*S, 12*S, Cyan);
    Label(FString::Printf(TEXT("%d FPS"), FMath::RoundToInt(PC->GetFps())), W-30*S, 30*S, 13*S, Dim, 1.f);

    if (Scout)
    {
        Label(FString::Printf(TEXT("%d m   /   %d km/h"), FMath::RoundToInt(float(Scout->GetActorLocation().Z/100.)),
            FMath::RoundToInt(Scout->GetSpeedMetres()*3.6f)), Mid, H-110*S, 22*S, Ink, 0.5f);
        Label(TEXT("Mouse look    W/S forward    A/D sideways    E/Q up/down    Shift fast    Esc pause"),
            Mid, H-52*S, 14*S, Ink, 0.5f);
        return;
    }

    const float Yaw = Heading(Jet->GetActorRotation().Yaw);
    // A short compass tape stays useful even when the sea offers no landmarks.
    DrawRect(Panel, Mid-220*S, 20*S, 440*S, 65*S);
    for (int32 Bearing = 0; Bearing < 360; Bearing += 15)
    {
        const float Difference = FMath::FindDeltaAngleDegrees(Yaw, float(Bearing));
        if (FMath::Abs(Difference) > 55.f) continue;
        const float X = Mid + Difference*3.6f*S;
        DrawLine(X, 60*S, X, 69*S, Dim, 1.f);
        if (Bearing % 30 == 0)
        {
            const FString Name = Bearing == 0 ? TEXT("N") : Bearing == 90 ? TEXT("E")
                : Bearing == 180 ? TEXT("S") : Bearing == 270 ? TEXT("W") : FString::FromInt(Bearing);
            Label(Name, X, 33*S, 12*S, Ink, 0.5f);
        }
    }
    DrawLine(Mid, 65*S, Mid-5*S, 76*S, Amber, 2.f);
    DrawLine(Mid, 65*S, Mid+5*S, 76*S, Amber, 2.f);

    const float CentreY = H*0.48f;
    const float Left = FMath::Max(30*S, Mid-385*S), Right = FMath::Min(W-30*S, Mid+385*S);
    DrawRect(Panel, Left-12*S, CentreY-28*S, 150*S, 100*S);
    Label(TEXT("AIRSPEED"), Left, CentreY-18*S, 12*S, Dim);
    Label(FString::Printf(TEXT("%d"), FMath::RoundToInt(Jet->SpeedKmh())), Left, CentreY+2*S, 30*S, Ink);
    Label(TEXT("km/h"), Left+90*S, CentreY+20*S, 12*S, Dim);
    DrawRect(Panel, Right-138*S, CentreY-28*S, 150*S, 100*S);
    Label(TEXT("ALTITUDE"), Right, CentreY-18*S, 12*S, Dim, 1.f);
    Label(FString::Printf(TEXT("%d m"), FMath::RoundToInt(float(Jet->GetActorLocation().Z/100.))), Right, CentreY+2*S, 27*S, Ink, 1.f);
    Label(FString::Printf(TEXT("%+.0f m/s"), Jet->GetFlightVelocity().Z), Right, CentreY+48*S, 12*S, Cyan, 1.f);

    auto Project = [PC, W, H](const FVector& World, FVector2D& Screen)
    {
        return PC->ProjectWorldLocationToScreen(World, Screen) && Screen.X > 15 && Screen.X < W-15
            && Screen.Y > 100 && Screen.Y < H-140;
    };
    const FVector Origin = Jet->GetActorLocation();
    const auto& Points = JetTerrain::Landmarks();
    const auto& Destination = Points[PC->GetNavigationIndex() % Points.Num()];
    const FVector ToLandmark = Destination.PositionMetres*100.-Origin;
    const float DistanceKm = ToLandmark.Size()/100000.f;
    Label(FString::Printf(TEXT("NAV  %s  /  %.1f km  /  %03d  /  N next"), Destination.Name, DistanceKm,
        FMath::RoundToInt(Heading(ToLandmark.Rotation().Yaw)) % 360), Mid, 96*S, 12*S, Cyan, 0.5f);
    FVector2D Nav;
    if (Project(Destination.PositionMetres*100., Nav))
    {
        const float R = 8*S;
        DrawLine(Nav.X-R, Nav.Y, Nav.X, Nav.Y-R, Cyan, 1.5f);
        DrawLine(Nav.X, Nav.Y-R, Nav.X+R, Nav.Y, Cyan, 1.5f);
        DrawLine(Nav.X+R, Nav.Y, Nav.X, Nav.Y+R, Cyan, 1.5f);
        DrawLine(Nav.X, Nav.Y+R, Nav.X-R, Nav.Y, Cyan, 1.5f);
        Label(Destination.Name, Nav.X, Nav.Y+12*S, 11*S, Cyan, 0.5f);
    }
    FVector2D Nose, Path, Aim;
    if (Project(Origin + Jet->GetActorForwardVector()*500000., Nose))
    {
        DrawLine(Nose.X-22*S, Nose.Y, Nose.X-7*S, Nose.Y, Ink, 1.5f);
        DrawLine(Nose.X+7*S, Nose.Y, Nose.X+22*S, Nose.Y, Ink, 1.5f);
        DrawLine(Nose.X, Nose.Y-10*S, Nose.X, Nose.Y-3*S, Ink, 1.5f);
    }
    if (Project(Origin + Jet->GetFlightVelocity().GetSafeNormal()*500000., Path))
    {
        Circle(Path, 6*S, Cyan);
        DrawLine(Path.X-15*S, Path.Y, Path.X-6*S, Path.Y, Cyan, 1.5f);
        DrawLine(Path.X+6*S, Path.Y, Path.X+15*S, Path.Y, Cyan, 1.5f);
        DrawLine(Path.X, Path.Y-6*S, Path.X, Path.Y-12*S, Cyan, 1.5f);
    }
    if (Jet->IsMouseAim())
    {
        if (Project(Origin + Jet->GetAimDirection()*500000., Aim)) Circle(Aim, 13*S, Amber);
        else
        {
            // Keep the selected direction visible during a steep bank or
            // reversal, when the world-space aim circle leaves the screen.
            FVector Eye;
            FRotator View;
            PC->GetPlayerViewPoint(Eye, View);
            const FVector Local = View.UnrotateVector(Jet->GetAimDirection());
            FVector2D Direction(Local.Y, -Local.Z);
            Direction = Direction.IsNearlyZero() ? FVector2D(1, 0) : Direction.GetSafeNormal();
            const float Reach = FMath::Min((W*0.5f-40*S)/FMath::Max(FMath::Abs(Direction.X), 0.001),
                (H*0.5f-125*S)/FMath::Max(FMath::Abs(Direction.Y), 0.001));
            const FVector2D Edge = FVector2D(Mid, H*0.5f) + Direction*Reach;
            const FVector2D Side(-Direction.Y, Direction.X);
            const FVector2D A = Edge-Direction*14*S+Side*7*S, B = Edge-Direction*14*S-Side*7*S;
            DrawLine(Edge.X, Edge.Y, A.X, A.Y, Amber, 2.f);
            DrawLine(Edge.X, Edge.Y, B.X, B.Y, Amber, 2.f);
            Label(TEXT("AIM"), Edge.X-Direction.X*27*S, Edge.Y-Direction.Y*27*S-6*S, 11*S, Amber, 0.5f);
        }
    }

    if (const auto* Gun=Jet->GetCombat(); Gun && Gun->GetTarget())
    {
        const auto* Target=Gun->GetTarget();
        const auto* Hull=Target->GetHealth();
        const float Distance=FVector::Distance(Origin,Target->GetActorLocation())/100.f;
        DrawRect(Panel,Mid-220*S,140*S,440*S,64*S);
        Label(Hull->IsDestroyed() ? TEXT("TRAINING COMPLETE / T TO RESTART") : TEXT("TRAINING / DESTROY THE STATIONARY DRONE"),
            Mid,146*S,14*S,Hull->IsDestroyed()?Cyan:Ink,0.5f);
        Label(FString::Printf(TEXT("GUN %s / HEAT %d%% / HITS %d"),Gun->IsOverheated()?TEXT("COOLING"):TEXT("READY"),
            FMath::RoundToInt(Gun->GetHeat()*100.f),Gun->GetHits()),Mid,175*S,13*S,Gun->IsOverheated()?Red:Amber,0.5f);
        FVector2D Screen;
        if (!Hull->IsDestroyed() && Project(Target->GetActorLocation(),Screen))
        {
            const float R=24*S;
            DrawLine(Screen.X-R,Screen.Y-R,Screen.X-R,Screen.Y+R,Amber,2.f);
            DrawLine(Screen.X+R,Screen.Y-R,Screen.X+R,Screen.Y+R,Amber,2.f);
            DrawRect(Panel,Screen.X-R,Screen.Y-R-9*S,2*R,5*S);
            DrawRect(Amber,Screen.X-R,Screen.Y-R-9*S,2*R*Hull->GetHull()/Hull->GetMaxHull(),5*S);
            Label(FString::Printf(TEXT("DRONE / %.0f m / HULL %.0f"),Distance,Hull->GetHull()),
                Screen.X,Screen.Y+R+6*S,12*S,Distance>UJetCombatComponent::RangeMetres?Dim:Amber,0.5f);
        }
        if (Gun->GetHitSeconds()>0.f)
        {
            DrawLine(Mid-10*S,H*0.5f-10*S,Mid-4*S,H*0.5f-4*S,Cyan,2.f);
            DrawLine(Mid+10*S,H*0.5f-10*S,Mid+4*S,H*0.5f-4*S,Cyan,2.f);
            DrawLine(Mid-10*S,H*0.5f+10*S,Mid-4*S,H*0.5f+4*S,Cyan,2.f);
            DrawLine(Mid+10*S,H*0.5f+10*S,Mid+4*S,H*0.5f+4*S,Cyan,2.f);
        }
    }

    // Small attitude instrument: actual bank and pitch, independent of camera lag.
    const FVector Up = Jet->GetActorUpVector(), Wing = Jet->GetActorRightVector();
    const float Bank = FMath::Atan2(-Wing.Z, Up.Z);
    const FVector2D Att(Mid, H-215*S);
    Circle(Att, 42*S, Dim);
    const FVector2D Along(FMath::Cos(Bank), -FMath::Sin(Bank));
    const FVector2D Down(FMath::Sin(Bank), FMath::Cos(Bank));
    const FVector2D Level = Att + Down*FMath::Clamp(Jet->GetActorRotation().Pitch*0.7, -30., 30.)*S;
    const FVector2D A = Level-Along*32*S, B = Level+Along*32*S;
    DrawLine(A.X, A.Y, B.X, B.Y, Cyan, 2.f);
    DrawLine(Att.X-13*S, Att.Y, Att.X+13*S, Att.Y, Amber, 2.f);
    Label(FString::Printf(TEXT("%+.1f G"), Jet->LoadG), Mid, H-159*S, 14*S, Ink, 0.5f);

    DrawRect(Panel, 0, H-94*S, W, 94*S);
    Label(Jet->bAssisted ? TEXT("ASSISTED") : TEXT("EXPERT"), 28*S, H-83*S, 14*S, Cyan);
    Label(Jet->IsMouseAim() ? TEXT("MOUSE AIM") : TEXT("DIRECT STICK"), 162*S, H-83*S, 14*S, Ink);
    Label(FString::Printf(TEXT("THROTTLE %d%% %s"), FMath::RoundToInt(Jet->GetThrottle()*100.f), Jet->IsBoosting() ? TEXT("/ BOOST") : TEXT("")),
        W-28*S, H-83*S, 14*S, Jet->IsBoosting() ? Amber : Ink, 1.f);
    Label(TEXT("Mouse aim  /  M direct stick    Up/Down pitch    A/D roll    Q/E rudder    W/S throttle    Shift boost    Space brake"),
        Mid, H-53*S, 12*S, Dim, 0.5f);
    Label(TEXT("LMB / Pad A gun    T / D-pad down restart training    V airframe    F assists    N landmark    R flight reset    Esc pause"),
        Mid, H-29*S, 11*S, Dim, 0.5f);

    FString Warning, Detail;
    if (Jet->bCrashed)
    {
        Warning = TEXT("AIRCRAFT LOST");
        Detail = FString::Printf(TEXT("Returning to flight in %.1f s"), FMath::Max(0.f, Jet->GetRespawnSeconds()));
    }
    else if (Jet->IsOutsideAirspace())
    {
        Warning = TEXT("RETURN TO AIRSPACE");
        Detail = FString::Printf(TEXT("%.1f s remaining / centre bearing %03d / below %.0f m"), FMath::Max(0.f, Jet->GetAirspaceSeconds()),
            FMath::RoundToInt(Heading((-Origin).Rotation().Yaw)) % 360, Jet->AirspaceCeilingMetres);
    }
    else if (Jet->NeedsPullUp()) Warning = TEXT("PULL UP");
    else if (Jet->IsStalling()) { Warning = TEXT("STALL"); Detail = TEXT("Ease the pull and add throttle"); }
    else if (Jet->SpeedKmh() < 220.f) { Warning = TEXT("LOW AIRSPEED"); Detail = TEXT("Add throttle / Shift for afterburner"); }
    if (!Warning.IsEmpty())
    {
        DrawRect(Panel, Mid-240*S, H*0.23f, 480*S, 83*S);
        Label(Warning, Mid, H*0.23f+10*S, 24*S, Red, 0.5f);
        Label(Detail, Mid, H*0.23f+50*S, 12*S, Ink, 0.5f);
    }
    if (PC->IsFlightPaused())
    {
        DrawRect(FLinearColor(0.f, 0.01f, 0.025f, 0.7f), 0, 0, W, H);
        Label(TEXT("FLIGHT PAUSED"), Mid, H*0.38f, 30*S, Ink, 0.5f);
        Label(TEXT("Esc / Enter / A: resume"), Mid, H*0.48f, 18*S, Cyan, 0.5f);
        Label(TEXT("Q / B: quit JetFighter"), Mid, H*0.55f, 16*S, Dim, 0.5f);
    }
}
