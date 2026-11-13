#include "BoatRaceHUD.h"

#include "BoatAudio.h"
#include "BoatPawn.h"
#include "BoatPrefs.h"
#include "BoatRaceGameState.h"
#include "BoatRacePlayerController.h"
#include "BoatTrack.h"
#include "BoatRacePlayerState.h"
#include "BoatTrack.h"
#include "LanDiscovery.h"

#include "Algo/Sort.h"
#include "CanvasItem.h"
#include "Fonts/FontMeasure.h"
#include "Fonts/SlateFontInfo.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

namespace
{
	const FLinearColor Ink(1.f, 1.f, 1.f, 1.f);
	const FLinearColor Dim(0.72f, 0.78f, 0.84f, 1.f);
	const FLinearColor Faint(0.55f, 0.6f, 0.66f, 1.f);
	const FLinearColor Accent(1.f, 0.62f, 0.1f, 1.f);
	const FLinearColor Good(0.3f, 0.85f, 0.4f, 1.f);
	const FLinearColor Bad(1.f, 0.25f, 0.2f, 1.f);
	const FLinearColor Panel(0.f, 0.02f, 0.04f, 0.5f);
}

void ABoatRaceHUD::BeginPlay()
{
	Super::BeginPlay();
	TitleTexture = LoadObject<UTexture2D>(nullptr, TEXT("/Game/Higgsfield/T_Title.T_Title"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	RobotoFont = LoadObject<UFont>(nullptr, TEXT("/Engine/EngineFonts/Roboto.Roboto"), nullptr, LOAD_NoWarn | LOAD_Quiet);
}

// ---------------------------------------------------------------------------
// Small helpers
// ---------------------------------------------------------------------------
FString ABoatRaceHUD::FormatTime(float Seconds)
{
	const float Safe = FMath::Max(Seconds, 0.f);
	const int32 Minutes = FMath::FloorToInt(Safe / 60.f);
	return FString::Printf(TEXT("%d:%05.2f"), Minutes, Safe - Minutes * 60.f);
}

FString ABoatRaceHUD::FormatGap(float Seconds)
{
	if (Seconds <= 0.01f)
	{
		return FString(TEXT("leader"));
	}
	if (Seconds >= 100.f)
	{
		return FString(TEXT("+99.9"));
	}
	return FString::Printf(TEXT("+%.1f"), Seconds);
}

FString ABoatRaceHUD::Ordinal(int32 Number)
{
	switch (Number)
	{
	case 1:  return TEXT("1st");
	case 2:  return TEXT("2nd");
	case 3:  return TEXT("3rd");
	default: return FString::Printf(TEXT("%dth"), Number);
	}
}

namespace
{
	/**
	 * Size is the old engine-font scale (1 = body text). Roboto is a vector
	 * font rendered by Slate at the exact pixel size, so it stays sharp at any
	 * resolution instead of being a stretched bitmap.
	 */
	FSlateFontInfo LabelFont(const UFont* Font, float Size, bool bLarge)
	{
		// Calibrated so the old layout keeps its proportions.
		const float Points = FMath::Max(Size * (bLarge ? 9.f : 11.f), 6.f);
		FSlateFontInfo Info(Font, Points, bLarge ? FName(TEXT("Bold")) : FName(TEXT("Regular")));
		return Info;
	}

	bool CanUseSlateFonts(const UFont* Font)
	{
		return Font != nullptr && FSlateApplication::IsInitialized() && FSlateApplication::Get().GetRenderer() != nullptr;
	}
}

FVector2D ABoatRaceHUD::MeasureLabel(const FString& Text, float Size, bool bLarge) const
{
	if (CanUseSlateFonts(RobotoFont))
	{
		const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
		return Measure->Measure(Text, LabelFont(RobotoFont, Size, bLarge), 1.f);
	}
	UFont* Font = GEngine ? (bLarge ? GEngine->GetLargeFont() : GEngine->GetMediumFont()) : nullptr;
	float TextWidth = 0.f, TextHeight = 0.f;
	const_cast<ABoatRaceHUD*>(this)->GetTextSize(Text, TextWidth, TextHeight, Font, Size);
	return FVector2D(TextWidth, TextHeight);
}

void ABoatRaceHUD::DrawLabel(const FString& Text, float X, float Y, float Size, const FLinearColor& Color, float Align, bool bLarge)
{
	if (Canvas == nullptr || Text.IsEmpty())
	{
		return;
	}
	const FVector2D Extent = MeasureLabel(Text, Size, bLarge);
	const float Left = FMath::RoundToFloat(X - static_cast<float>(Extent.X) * Align);
	const float Top = FMath::RoundToFloat(Y);

	if (CanUseSlateFonts(RobotoFont))
	{
		FCanvasTextItem Item(FVector2D(Left, Top), FText::FromString(Text), LabelFont(RobotoFont, Size, bLarge), Color);
		// A soft drop shadow keeps text readable over bright water and sky.
		Item.EnableShadow(FLinearColor(0.f, 0.f, 0.f, 0.65f * Color.A), FVector2D(1.f, 1.f) * FMath::Clamp(Size * 1.1f, 1.f, 3.f));
		Canvas->DrawItem(Item);
		return;
	}
	UFont* Font = GEngine ? (bLarge ? GEngine->GetLargeFont() : GEngine->GetMediumFont()) : nullptr;
	const float Shadow = FMath::Max(1.f, Size * 0.6f);
	DrawText(Text, FLinearColor(0.f, 0.f, 0.f, 0.8f * Color.A), Left + Shadow, Top + Shadow, Font, Size, false);
	DrawText(Text, Color, Left, Top, Font, Size, false);
}

void ABoatRaceHUD::DrawPanel(float X, float Y, float W, float H, float Scale, float Alpha)
{
	DrawRect(FLinearColor(0.01f, 0.03f, 0.05f, Alpha), X, Y, W, H);
	DrawRect(FLinearColor(1.f, 0.62f, 0.1f, 0.95f), X, Y, W, FMath::Max(3.f * Scale, 2.f));
}

void ABoatRaceHUD::DrawTriangle(const FVector2D& A, const FVector2D& B, const FVector2D& C, const FLinearColor& Color)
{
	if (Canvas == nullptr)
	{
		return;
	}
	FCanvasTriangleItem Triangle(A, B, C, GWhiteTexture);
	Triangle.SetColor(Color);
	Triangle.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Triangle);
}

void ABoatRaceHUD::DrawBar(float X, float Y, float Width, float Height, float Fraction, const FLinearColor& Color)
{
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.55f), X, Y, Width, Height);
	DrawRect(Color, X, Y, Width * FMath::Clamp(Fraction, 0.f, 1.f), Height);
}

// ---------------------------------------------------------------------------
// Order of the race
// ---------------------------------------------------------------------------
void ABoatRaceHUD::BuildStandings(TArray<FStanding>& OutStandings) const
{
	OutStandings.Reset();

	const UWorld* World = GetWorld();
	const ABoatRaceGameState* RaceState = World ? World->GetGameState<ABoatRaceGameState>() : nullptr;
	if (RaceState == nullptr)
	{
		return;
	}

	const TArray<FBoatGate>& Gates = BoatTrack::GetGates();
	const int32 GateCount = Gates.Num();
	const float LapLength = BoatTrack::GetLapLength();

	for (APlayerState* Player : RaceState->PlayerArray)
	{
		const ABoatRacePlayerState* RacePlayer = Cast<ABoatRacePlayerState>(Player);
		if (RacePlayer == nullptr || RacePlayer->SlotIndex < 0)
		{
			continue;
		}

		FStanding Standing;
		Standing.Player = RacePlayer;
		Standing.Boat = Cast<ABoatPawn>(RacePlayer->GetPawn());

		// How far round the circuit this boat has actually got: whole gates
		// already passed, less the distance still to run to the next one.
		const int32 NextGate = RacePlayer->GatesPassed % FMath::Max(GateCount, 1);
		const float AtNextGate = (RacePlayer->GatesPassed / FMath::Max(GateCount, 1)) * LapLength
			+ BoatTrack::GetDistanceToGate(NextGate);

		float ToNextGate = 0.f;
		if (Standing.Boat != nullptr && Gates.IsValidIndex(NextGate))
		{
			const FVector Location = Standing.Boat->GetActorLocation();
			ToNextGate = static_cast<float>((Gates[NextGate].Center - FVector2D(Location.X, Location.Y)).Size());
		}
		Standing.Progress = AtNextGate - ToNextGate;

		OutStandings.Add(Standing);
	}

	// Finishers in finishing order, then whoever has gone furthest.
	Algo::Sort(OutStandings, [](const FStanding& A, const FStanding& B)
	{
		if (A.Player->bFinished != B.Player->bFinished)
		{
			return A.Player->bFinished;
		}
		if (A.Player->bFinished)
		{
			return A.Player->FinishPosition < B.Player->FinishPosition;
		}
		if (A.Progress != B.Progress)
		{
			return A.Progress > B.Progress;
		}
		return A.Player->SlotIndex < B.Player->SlotIndex;
	});

	// Turn the distance behind the leader into a time, the way a real timing
	// screen does: how long it would take this boat to cover that ground.
	if (OutStandings.Num() > 0)
	{
		const float LeaderProgress = OutStandings[0].Progress;
		for (FStanding& Standing : OutStandings)
		{
			const float Behind = FMath::Max(LeaderProgress - Standing.Progress, 0.f);
			const float Speed = (Standing.Boat != nullptr)
				? FMath::Max(FMath::Abs(Standing.Boat->GetForwardSpeed()), 900.f)
				: 2000.f;
			Standing.GapSeconds = Behind / Speed;
		}
	}
}

// ---------------------------------------------------------------------------
// Frame
// ---------------------------------------------------------------------------
void ABoatRaceHUD::DrawHUD()
{
	Super::DrawHUD();

	const UWorld* World = GetWorld();
	if (Canvas == nullptr || World == nullptr)
	{
		return;
	}

	const float Width = static_cast<float>(Canvas->SizeX);
	const float Height = static_cast<float>(Canvas->SizeY);
	const float Scale = FMath::Clamp(Height / 1080.f, 0.45f, 3.f);

	const ABoatRaceGameState* RaceState = World->GetGameState<ABoatRaceGameState>();
	if (RaceState == nullptr)
	{
		DrawLabel(TEXT("Connecting..."), Width * 0.5f, Height * 0.45f, 2.f * Scale, Ink, 0.5f, true);
		return;
	}

	TArray<FStanding> Standings;
	BuildStandings(Standings);
	UpdateRaceCues(RaceState, Standings);

	const bool bOnMenu = GetNetMode() == NM_Standalone && RaceState->RaceState == ERaceState::Lobby;
	if (bOnMenu)
	{
		DrawMenu(Width, Height, Scale);
		return;
	}

	DrawNameTags(Scale);

	switch (RaceState->RaceState)
	{
	case ERaceState::Lobby:
		DrawLobby(RaceState, Standings, Width, Height, Scale);
		break;
	case ERaceState::Countdown:
		DrawCountdown(RaceState, Width, Height, Scale);
		DrawRaceHud(RaceState, Standings, Width, Height, Scale);
		break;
	case ERaceState::Racing:
		DrawRaceHud(RaceState, Standings, Width, Height, Scale);
		break;
	case ERaceState::Results:
		DrawResults(RaceState, Standings, Width, Height, Scale);
		break;
	default:
		break;
	}

	DrawKillFeed(RaceState, Width, Height, Scale);
	DrawBanner(Width, Height, Scale);

	// The pause menu over the race, which carries on underneath (it is a
	// multiplayer game: pausing cannot stop the others).
	const ABoatRacePlayerController* RaceController = Cast<ABoatRacePlayerController>(GetOwningPlayerController());
	if (RaceController != nullptr && RaceController->GetMenuScreen() != EMenuScreen::None)
	{
		DrawRect(FLinearColor(0.f, 0.f, 0.02f, 0.45f), 0.f, 0.f, Width, Height);
		DrawMenuScreen(Width, Height, Scale);
	}
	else
	{
		MenuRects.Reset();
		DrawLabel(TEXT("ESC  menu"), Width - 24.f * Scale, Height - 34.f * Scale, 0.95f * Scale, Faint, 1.f);
	}
}

// ---------------------------------------------------------------------------
// Menu and lobby
// ---------------------------------------------------------------------------
void ABoatRaceHUD::DrawMenu(float Width, float Height, float Scale)
{
	// The title screen: the art (or the live bay) behind a dark wash, the
	// name on the left, and the menu panel beside it.
	if (TitleTexture != nullptr)
	{
		DrawTexture(TitleTexture.Get(), 0.f, 0.f, Width, Height, 0.f, 0.f, 1.f, 1.f);
	}
	DrawRect(FLinearColor(0.f, 0.02f, 0.04f, 0.42f), 0.f, 0.f, Width, Height);

	const float Left = Width * 0.07f;
	const float TitleY = Height * 0.16f;
	DrawLabel(TEXT("BOAT RACE"), Left, TitleY, 8.f * Scale, Ink, 0.f, true);
	DrawRect(Accent, Left + 4.f * Scale, TitleY + 112.f * Scale, 140.f * Scale, 5.f * Scale);
	DrawLabel(TEXT("Offshore powerboat racing for up to four"), Left + 4.f * Scale, TitleY + 132.f * Scale, 1.6f * Scale, Dim);

	const ABoatRacePlayerController* RaceController = Cast<ABoatRacePlayerController>(GetOwningPlayerController());
	if (RaceController != nullptr && RaceController->IsTypingAddress())
	{
		DrawAddressEntry(Width, Height, Scale);
		return;
	}
	DrawMenuScreen(Width, Height, Scale);
}

void ABoatRaceHUD::DrawAddressEntry(float Width, float Height, float Scale)
{
	const ABoatRacePlayerController* RaceController = Cast<ABoatRacePlayerController>(GetOwningPlayerController());
	const float PanelW = 640.f * Scale, PanelH = 250.f * Scale;
	const float X = (Width - PanelW) * 0.5f, Y = (Height - PanelH) * 0.5f;
	MenuRects.Reset();
	DrawPanel(X, Y, PanelW, PanelH, Scale, 0.85f);
	DrawLabel(TEXT("JOIN BY ADDRESS"), X + 40.f * Scale, Y + 28.f * Scale, 2.6f * Scale, Ink, 0.f, true);
	DrawLabel(TEXT("Type the host's IP address (on the host: hostname -I)"), X + 40.f * Scale, Y + 82.f * Scale, 1.15f * Scale, Dim);
	DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.08f), X + 40.f * Scale, Y + 120.f * Scale, PanelW - 80.f * Scale, 52.f * Scale);
	const bool bCaretOn = FMath::Fmod(static_cast<float>(GetWorld()->GetRealTimeSeconds()), 1.f) < 0.5f;
	const FString Typed = RaceController ? RaceController->GetTypedAddress() : FString();
	DrawLabel(Typed + (bCaretOn ? TEXT("|") : TEXT(" ")), X + 56.f * Scale, Y + 128.f * Scale, 1.6f * Scale, Accent);
	DrawLabel(TEXT("ENTER  connect          ESC  cancel"), X + 40.f * Scale, Y + 196.f * Scale, 1.1f * Scale, Faint);
}

void ABoatRaceHUD::DrawMenuScreen(float Width, float Height, float Scale)
{
	MenuRects.Reset();
	const ABoatRacePlayerController* RaceController = Cast<ABoatRacePlayerController>(GetOwningPlayerController());
	if (RaceController == nullptr)
	{
		return;
	}
	TArray<FMenuItem> Items;
	RaceController->BuildMenuItems(Items);
	const EMenuScreen Screen = RaceController->GetMenuScreen();
	if (Items.Num() == 0)
	{
		return;
	}

	const TCHAR* Title = Screen == EMenuScreen::Pause ? TEXT("PAUSED")
		: Screen == EMenuScreen::Settings ? TEXT("SETTINGS")
		: Screen == EMenuScreen::Controls ? TEXT("CONTROLS") : TEXT("MAIN MENU");

	// Rows of equal height; labels in one column, values right-aligned in another.
	const float RowH = 50.f * Scale;
	const float PanelW = (Screen == EMenuScreen::Controls ? 760.f : 620.f) * Scale;
	const float PanelH = (120.f + Items.Num() * 50.f + 70.f) * Scale;
	const bool bTitleScreen = RaceController->IsOnMainMenu();
	const float X = bTitleScreen ? Width * 0.94f - PanelW : (Width - PanelW) * 0.5f;
	const float Y = FMath::Max((Height - PanelH) * 0.5f, 20.f * Scale);
	DrawPanel(X, Y, PanelW, PanelH, Scale, 0.8f);
	DrawLabel(Title, X + 40.f * Scale, Y + 30.f * Scale, 3.f * Scale, Ink, 0.f, true);

	const float Pad = 40.f * Scale;
	float RowY = Y + 104.f * Scale;
	const int32 Selected = RaceController->GetMenuSelection();
	for (int32 Index = 0; Index < Items.Num(); ++Index)
	{
		const FMenuItem& Item = Items[Index];
		const bool bSelected = Index == Selected && Item.bEnabled;
		const FBox2D Row(FVector2D(X + 16.f * Scale, RowY), FVector2D(X + PanelW - 16.f * Scale, RowY + RowH - 4.f * Scale));
		MenuRects.Add(Row);
		if (bSelected)
		{
			DrawRect(FLinearColor(1.f, 0.62f, 0.1f, 0.22f), Row.Min.X, Row.Min.Y, Row.GetSize().X, Row.GetSize().Y);
			DrawRect(Accent, Row.Min.X, Row.Min.Y, 5.f * Scale, Row.GetSize().Y);
		}
		const bool bInfo = !Item.bEnabled && Item.Action == EMenuAction::None;
		const FLinearColor LabelColour = bSelected ? Ink : (Item.bEnabled ? Dim : (bInfo && Screen == EMenuScreen::Controls ? Ink : Faint));
		const float TextY = RowY + (RowH - 4.f * Scale) * 0.5f - 15.f * Scale;
		DrawLabel(Item.Label, X + Pad, TextY, 1.45f * Scale, LabelColour);
		if (!Item.Value.IsEmpty())
		{
			const FString Value = Item.bAdjustable ? FString::Printf(TEXT("<   %s   >"), *Item.Value) : Item.Value;
			DrawLabel(Value, X + PanelW - Pad, TextY, 1.45f * Scale,
				Item.bAdjustable ? (bSelected ? Accent : Ink) : (Screen == EMenuScreen::Controls ? Accent : Dim), 1.f);
		}
		RowY += RowH;
	}

	// The selected line's explanation, and how to drive the menu.
	const FString Hint = Items.IsValidIndex(Selected) ? Items[Selected].Hint : FString();
	DrawLabel(Hint, X + Pad, Y + PanelH - 58.f * Scale, 1.1f * Scale, Faint);
	DrawLabel(Screen == EMenuScreen::Main ? TEXT("Arrows or mouse to choose     ENTER select")
		: TEXT("Arrows or mouse to choose     ENTER select     ESC back"),
		X + Pad, Y + PanelH - 32.f * Scale, 1.f * Scale, Faint);
}

int32 ABoatRaceHUD::HitTestMenu(const FVector2D& Point) const
{
	for (int32 Index = 0; Index < MenuRects.Num(); ++Index)
	{
		if (MenuRects[Index].IsInside(Point))
		{
			return Index;
		}
	}
	return -1;
}

void ABoatRaceHUD::DrawLobby(const ABoatRaceGameState* RaceState, const TArray<FStanding>& Standings, float Width, float Height, float Scale)
{
	// A panel at the top: who is in, the race settings, and what happens next.
	const APlayerController* Controller = GetOwningPlayerController();
	const bool bHost = Controller != nullptr && Controller->HasAuthority();
	static const TCHAR* const RivalNames[] = { TEXT("No rivals"), TEXT("Easy rivals"), TEXT("Normal rivals"), TEXT("Hard rivals") };
	const float PanelW = 620.f * Scale;
	const float PanelH = (190.f + BoatTrack::MaxPlayers * 40.f) * Scale;
	const float X = (Width - PanelW) * 0.5f, Y = 70.f * Scale;
	DrawPanel(X, Y, PanelW, PanelH, Scale, 0.7f);
	DrawLabel(TEXT("LOBBY"), X + 32.f * Scale, Y + 22.f * Scale, 2.6f * Scale, Ink, 0.f, true);
	DrawLabel(FString::Printf(TEXT("%d / %d"), Standings.Num(), BoatTrack::MaxPlayers), X + PanelW - 32.f * Scale, Y + 30.f * Scale, 1.6f * Scale, Dim, 1.f);
	DrawLabel(FString::Printf(TEXT("%s   ·   %d laps   ·   %s"), RaceWeather::Get(RaceState->GetWeather()).Name, RaceState->TotalLaps,
		RivalNames[FMath::Clamp<int32>(RaceState->RivalLevel, 0, 3)]), X + 32.f * Scale, Y + 76.f * Scale, 1.2f * Scale, Accent);

	float RowY = Y + 116.f * Scale;
	for (int32 Slot = 0; Slot < BoatTrack::MaxPlayers; ++Slot)
	{
		const FStanding* Found = Standings.FindByPredicate([Slot](const FStanding& S) { return S.Player && S.Player->SlotIndex == Slot; });
		DrawRect(BoatTrack::GetPlayerColor(Slot), X + 32.f * Scale, RowY + 6.f * Scale, 6.f * Scale, 22.f * Scale);
		DrawLabel(Found ? Found->Player->GetPlayerName() : FString(TEXT("open seat")), X + 50.f * Scale, RowY, 1.35f * Scale,
			Found ? Ink : Faint);
		if (!Found && RaceState->RivalLevel > 0)
		{
			DrawLabel(TEXT("rival at the start"), X + PanelW - 32.f * Scale, RowY + 3.f * Scale, 1.05f * Scale, Faint, 1.f);
		}
		RowY += 40.f * Scale;
	}
	DrawLabel(bHost ? TEXT("ENTER start the race      ESC menu: laps, rivals, weather") : TEXT("Waiting for the host to start...      ESC menu"),
		X + 32.f * Scale, Y + PanelH - 40.f * Scale, 1.1f * Scale, Dim);

	if (const ABoatPawn* Boat = Cast<ABoatPawn>(Controller ? Controller->GetPawn() : nullptr))
	{
		DrawSpeed(Boat, Width, Height, Scale);
		DrawStatus(Boat, Width, Height, Scale);
	}
	DrawMiniMap(Standings, Controller ? Controller->GetPlayerState<ABoatRacePlayerState>() : nullptr, Width, Height, Scale);
}

void ABoatRaceHUD::DrawCountdown(const ABoatRaceGameState* RaceState, float Width, float Height, float Scale)
{
	DrawStartLights(RaceState, Width, Height, Scale);
	DrawLabel(FString::Printf(TEXT("%d LAPS   %s"), RaceState->TotalLaps, RaceWeather::Get(RaceState->GetWeather()).Name),
		Width * 0.5f, Height * 0.2f + 150.f * Scale, 1.7f * Scale, Ink, 0.5f);
}

void ABoatRaceHUD::DrawStartLights(const ABoatRaceGameState* RaceState, float Width, float Height, float Scale)
{
	// Five red lights come on one a second; when they all go out, go.
	const float Remaining = RaceState->StateEndTime - RaceState->GetServerNow();
	const int32 Lit = FMath::Clamp(5 - FMath::CeilToInt(Remaining) + 1, 0, 5);
	const float Lamp = 64.f * Scale;
	const float Gap = 22.f * Scale;
	const float Total = 5.f * Lamp + 4.f * Gap;
	const float X0 = (Width - Total) * 0.5f;
	const float Y0 = Height * 0.2f;
	DrawRect(FLinearColor(0.02f, 0.02f, 0.025f, 0.9f), X0 - 18.f * Scale, Y0 - 18.f * Scale, Total + 36.f * Scale, Lamp * 2.f + Gap + 36.f * Scale);
	for (int32 Index = 0; Index < 5; ++Index)
	{
		const float X = X0 + Index * (Lamp + Gap);
		for (int32 Row = 0; Row < 2; ++Row)
		{
			const FLinearColor Colour = Index < Lit ? FLinearColor(1.f, 0.06f, 0.04f, 1.f) : FLinearColor(0.16f, 0.03f, 0.03f, 1.f);
			DrawRect(Colour, X + 6.f * Scale, Y0 + Row * (Lamp + Gap) + 6.f * Scale, Lamp - 12.f * Scale, Lamp - 12.f * Scale);
			if (Index < Lit)
			{
				// A soft halo so a lit lamp glows rather than just being red.
				DrawRect(FLinearColor(1.f, 0.2f, 0.1f, 0.25f), X - 4.f * Scale, Y0 + Row * (Lamp + Gap) - 4.f * Scale, Lamp + 8.f * Scale, Lamp + 8.f * Scale);
			}
		}
	}
}

void ABoatRaceHUD::ShowBanner(const FString& Text, const FLinearColor& Color, float Seconds)
{
	BannerText = Text;
	BannerColor = Color;
	BannerUntil = static_cast<float>(GetWorld()->GetRealTimeSeconds()) + Seconds;
}

void ABoatRaceHUD::DrawBanner(float Width, float Height, float Scale)
{
	const float Now = static_cast<float>(GetWorld()->GetRealTimeSeconds());
	if (BannerText.IsEmpty() || Now > BannerUntil)
	{
		return;
	}
	const float Fade = FMath::Clamp((BannerUntil - Now) / 0.4f, 0.f, 1.f);
	const float Y = Height * 0.32f;
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.45f * Fade), 0.f, Y - 12.f * Scale, Width, 110.f * Scale);
	FLinearColor Colour = BannerColor;
	Colour.A = Fade;
	DrawLabel(BannerText, Width * 0.5f, Y, 3.f * Scale, Colour, 0.5f, true);
}

void ABoatRaceHUD::DrawWrongWay(const ABoatPawn* Boat, const ABoatRacePlayerState* Me, float Width, float Height, float Scale)
{
	if (Boat == nullptr || Me == nullptr || Me->bFinished)
	{
		return;
	}
	const TArray<FBoatGate>& Gates = BoatTrack::GetGates();
	if (!Gates.IsValidIndex(Me->NextGate))
	{
		return;
	}
	// Heading away from the next gate at speed for more than a moment.
	const FVector Location = Boat->GetActorLocation();
	const FVector2D ToGate = (Gates[Me->NextGate].Center - FVector2D(Location.X, Location.Y)).GetSafeNormal();
	const FVector Forward = Boat->GetActorForwardVector();
	const bool bWrong = FVector2D::DotProduct(ToGate, FVector2D(Forward.X, Forward.Y).GetSafeNormal()) < -0.35f && Boat->GetSpeedKmh() > 15.f;
	WrongWayTime = bWrong ? WrongWayTime + GetWorld()->GetDeltaSeconds() : 0.f;
	if (WrongWayTime > 1.2f && FMath::Fmod(WrongWayTime, 0.8f) < 0.55f)
	{
		DrawLabel(TEXT("WRONG WAY"), Width * 0.5f, Height * 0.42f, 3.4f * Scale, Bad, 0.5f, true);
	}
}

void ABoatRaceHUD::UpdateRaceCues(const ABoatRaceGameState* RaceState, const TArray<FStanding>& Standings)
{
	const APlayerController* Controller = GetOwningPlayerController();
	const ABoatRacePlayerState* Me = Controller ? Controller->GetPlayerState<ABoatRacePlayerState>() : nullptr;
	const int32 State = static_cast<int32>(RaceState->RaceState);

	if (State != SeenState)
	{
		if (RaceState->RaceState == ERaceState::Racing)
		{
			BoatAudio::Play2D(this, EBoatSound::Go, 0.8f);
			ShowBanner(TEXT("GO!  GO!  GO!"), Good, 1.4f);
		}
		else if (RaceState->RaceState == ERaceState::Results)
		{
			BoatAudio::Play2D(this, EBoatSound::Fanfare, 0.7f);
		}
		else if (RaceState->RaceState == ERaceState::Countdown)
		{
			SeenLights = 0;
			SeenLaps = -1;
			SeenPosition = 0;
			bSeenFinish = false;
		}
		SeenState = State;
	}

	if (RaceState->RaceState == ERaceState::Countdown)
	{
		const int32 Lit = FMath::Clamp(5 - FMath::CeilToInt(RaceState->StateEndTime - RaceState->GetServerNow()) + 1, 0, 5);
		if (Lit > SeenLights)
		{
			BoatAudio::Play2D(this, EBoatSound::Beep, 0.7f);
			SeenLights = Lit;
		}
		return;
	}
	if (RaceState->RaceState != ERaceState::Racing || Me == nullptr)
	{
		return;
	}

	if (Me->LapsCompleted != SeenLaps)
	{
		if (SeenLaps >= 0 && Me->LapsCompleted > SeenLaps && !Me->bFinished)
		{
			const bool bFinal = Me->LapsCompleted == RaceState->TotalLaps - 1;
			BoatAudio::Play2D(this, bFinal ? EBoatSound::FinalLap : EBoatSound::LapChime, 0.75f);
			ShowBanner(bFinal ? FString(TEXT("FINAL LAP")) : FString::Printf(TEXT("LAP %d / %d"), Me->LapsCompleted + 1, RaceState->TotalLaps),
				bFinal ? Accent : Ink);
			// The lap just finished is the newest best lap if it beat it.
			UBoatPrefs* Prefs = UBoatPrefs::Get(this);
			if (Prefs != nullptr && Me->BestLapTime > 0.f && Prefs->OfferLap(Me->BestLapTime))
			{
				ShowBanner(FString::Printf(TEXT("NEW LAP RECORD  %s"), *FormatTime(Me->BestLapTime)), FLinearColor(0.3f, 1.f, 1.f, 1.f), 3.f);
			}
		}
		SeenLaps = Me->LapsCompleted;
	}
	if (Me->bFinished && !bSeenFinish)
	{
		bSeenFinish = true;
		if (UBoatPrefs* Prefs = UBoatPrefs::Get(this))
		{
			Prefs->OfferLap(Me->BestLapTime);
			Prefs->OfferRace(RaceState->TotalLaps, Me->FinishTime);
		}
		BoatAudio::Play2D(this, Me->FinishPosition == 1 ? EBoatSound::Fanfare : EBoatSound::LapChime, 0.8f);
		ShowBanner(Me->FinishPosition == 1 ? FString(TEXT("YOU WIN!")) : FString::Printf(TEXT("%s PLACE"), *Ordinal(Me->FinishPosition).ToUpper()),
			Me->FinishPosition == 1 ? Accent : Ink, 4.f);
	}

	int32 Position = 0;
	for (int32 Index = 0; Index < Standings.Num(); ++Index)
	{
		if (Standings[Index].Player == Me)
		{
			Position = Index + 1;
		}
	}
	if (SeenPosition > 0 && Position > 0 && Position != SeenPosition && RaceState->GetRaceTime() > 4.f && !Me->bFinished)
	{
		ShowBanner(FString::Printf(TEXT("P%d  %s"), Position, Position < SeenPosition ? TEXT("UP") : TEXT("DOWN")),
			Position < SeenPosition ? Good : Bad, 1.6f);
	}
	SeenPosition = Position;
}

// ---------------------------------------------------------------------------
// Racing
// ---------------------------------------------------------------------------
void ABoatRaceHUD::DrawRaceHud(const ABoatRaceGameState* RaceState, const TArray<FStanding>& Standings, float Width, float Height, float Scale)
{
	const APlayerController* Controller = GetOwningPlayerController();
	const ABoatRacePlayerState* Me = Controller ? Controller->GetPlayerState<ABoatRacePlayerState>() : nullptr;
	const ABoatPawn* MyBoat = Controller ? Cast<ABoatPawn>(Controller->GetPawn()) : nullptr;

	DrawStandingsBlock(RaceState, Standings, Me, 44.f * Scale, 40.f * Scale, Scale);
	DrawSpeed(MyBoat, Width, Height, Scale);
	DrawStatus(MyBoat, Width, Height, Scale);
	DrawMiniMap(Standings, Me, Width, Height, Scale);
	DrawDamageEdge(MyBoat, Width, Height);


	if (Me == nullptr)
	{
		return;
	}

	if (Me->bFinished)
	{
		DrawLabel(FString::Printf(TEXT("FINISHED  %s"), *Ordinal(Me->FinishPosition)),
			Width * 0.5f, Height * 0.18f, 4.f * Scale, Accent, 0.5f, true);
		DrawLabel(TEXT("Waiting for the other boats..."), Width * 0.5f, Height * 0.18f + 115.f * Scale, 1.3f * Scale, Dim, 0.5f);
	}
	else if (MyBoat != nullptr && MyBoat->IsWrecked())
	{
		DrawLabel(TEXT("WRECKED"), Width * 0.5f, Height * 0.2f, 5.f * Scale, Bad, 0.5f, true);
		DrawLabel(TEXT("Rebuilding at your last gate"), Width * 0.5f, Height * 0.2f + 130.f * Scale, 1.4f * Scale, Dim, 0.5f);
	}
	else if (MyBoat != nullptr)
	{
		DrawHeadingStrip(MyBoat, Me, Width, Scale);
		DrawGateMarker(MyBoat, Me, Width, Height, Scale);
		DrawWrongWay(MyBoat, Me, Width, Height, Scale);
	}
}

void ABoatRaceHUD::DrawStandingsBlock(const ABoatRaceGameState* RaceState, const TArray<FStanding>& Standings,
	const ABoatRacePlayerState* Me, float X, float Y, float Scale)
{
	int32 MyPosition = 0;
	for (int32 Index = 0; Index < Standings.Num(); ++Index)
	{
		if (Standings[Index].Player == Me)
		{
			MyPosition = Index + 1;
		}
	}

	// Place and field size, the biggest thing on the screen after speed.
	if (MyPosition > 0)
	{
		DrawLabel(FString::FromInt(MyPosition), X, Y, 5.2f * Scale, Ink, 0.f, true);
		DrawLabel(FString::Printf(TEXT("/%d"), Standings.Num()), X + 62.f * Scale, Y + 62.f * Scale, 1.6f * Scale, Dim);
	}

	float Row = Y + 118.f * Scale;
	const float LabelX = X + 2.f * Scale;
	const float ValueX = X + 110.f * Scale;
	const int32 Lap = Me ? FMath::Clamp(Me->LapsCompleted + 1, 1, RaceState->TotalLaps) : 1;

	DrawLabel(TEXT("LAP"), LabelX, Row, 1.15f * Scale, Faint);
	DrawLabel(FString::Printf(TEXT("%d/%d"), Lap, RaceState->TotalLaps), ValueX, Row, 1.15f * Scale, Ink);
	Row += 26.f * Scale;

	DrawLabel(TEXT("TIME"), LabelX, Row, 1.15f * Scale, Faint);
	DrawLabel(FormatTime(Me && Me->bFinished ? Me->FinishTime : RaceState->GetRaceTime()), ValueX, Row, 1.15f * Scale, Ink);
	Row += 26.f * Scale;

	DrawLabel(TEXT("BEST"), LabelX, Row, 1.15f * Scale, Faint);
	DrawLabel((Me && Me->BestLapTime > 0.f) ? FormatTime(Me->BestLapTime) : FString(TEXT("--")), ValueX, Row, 1.15f * Scale, Ink);
	if (const UBoatPrefs* Prefs = UBoatPrefs::Get(this))
	{
		if (Prefs->BestLap > 0.f)
		{
			Row += 22.f * Scale;
			DrawLabel(TEXT("RECORD"), LabelX, Row, 1.15f * Scale, Faint);
			DrawLabel(FormatTime(Prefs->BestLap), ValueX, Row, 1.15f * Scale, FLinearColor(0.3f, 1.f, 1.f, 1.f));
		}
	}
	Row += 42.f * Scale;

	// The order, with the gap to the leader in seconds.
	for (int32 Index = 0; Index < Standings.Num(); ++Index)
	{
		const FStanding& Standing = Standings[Index];
		const bool bIsMe = (Standing.Player == Me);
		const FLinearColor RowColor = bIsMe ? Accent : Dim;

		DrawRect(BoatTrack::GetPlayerColor(Standing.Player->SlotIndex), LabelX, Row + 4.f * Scale, 6.f * Scale, 16.f * Scale);

		FString Name = Standing.Player->GetPlayerName().ToUpper();
		if (Name.Len() > 9)
		{
			Name = Name.Left(9);
		}
		DrawLabel(FString::Printf(TEXT("%d  %s"), Index + 1, bIsMe ? TEXT("YOU") : *Name), LabelX + 14.f * Scale, Row, 1.1f * Scale, RowColor);

		FString Right;
		if (Standing.Player->bFinished)
		{
			Right = FormatTime(Standing.Player->FinishTime);
		}
		else if (Standing.Boat != nullptr && Standing.Boat->IsWrecked())
		{
			Right = TEXT("wreck");
		}
		else if (Index > 0)
		{
			Right = FormatGap(Standing.GapSeconds);
		}
		DrawLabel(Right, X + 215.f * Scale, Row, 1.1f * Scale, Index > 0 ? Faint : Good, 1.f);
		Row += 24.f * Scale;
	}
}

void ABoatRaceHUD::DrawSpeed(const ABoatPawn* Boat, float Width, float Height, float Scale)
{
	if (Boat == nullptr)
	{
		return;
	}

	const float Kmh = Boat->GetSpeedKmh();
	const float BottomY = Height - 150.f * Scale;
	DrawLabel(FString::Printf(TEXT("%d"), FMath::RoundToInt(Kmh)), 44.f * Scale, BottomY, 5.5f * Scale, Ink, 0.f, true);
	DrawLabel(TEXT("KM/H"), 48.f * Scale, BottomY + 92.f * Scale, 1.2f * Scale, Faint);
	// A thin bar under the number, as a rev counter would be.
	DrawBar(46.f * Scale, BottomY + 126.f * Scale, 210.f * Scale, 7.f * Scale, Kmh / 115.f, Accent);
}

void ABoatRaceHUD::DrawStatus(const ABoatPawn* Boat, float Width, float Height, float Scale)
{
	if (Boat == nullptr)
	{
		return;
	}

	const float RightX = Width - 44.f * Scale;
	float Y = Height - 184.f * Scale;

	const float HealthFraction = Boat->GetHealth() / FMath::Max(Boat->GetMaxHealth(), 1.f);
	const FLinearColor HullColor = (HealthFraction > 0.55f) ? Good : (HealthFraction > 0.25f ? Accent : Bad);
	DrawLabel(TEXT("HULL"), RightX - 230.f * Scale, Y, 1.1f * Scale, Faint);
	DrawLabel(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(HealthFraction * 100.f)), RightX, Y, 1.1f * Scale, HullColor, 1.f);
	Y += 24.f * Scale;
	DrawBar(RightX - 230.f * Scale, Y, 230.f * Scale, 9.f * Scale, HealthFraction, HullColor);
	Y += 28.f * Scale;

	if (Boat->IsInvulnerable())
	{
		DrawLabel(TEXT("SHIELDED"), RightX, Y, 1.1f * Scale, Good, 1.f);
		Y += 24.f * Scale;
	}

	// Boost tank: cyan, flashing while it fires.
	const float Boost = Boat->GetBoost();
	const bool bFiring = Boat->IsBoosting();
	const FLinearColor BoostColour = bFiring ? FLinearColor(0.6f, 1.f, 1.f, 1.f) : FLinearColor(0.1f, 0.8f, 1.f, 1.f);
	DrawLabel(TEXT("BOOST   SHIFT"), RightX - 230.f * Scale, Y, 1.1f * Scale, Boost > 0.15f ? Dim : Faint);
	DrawLabel(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Boost * 100.f)), RightX, Y, 1.1f * Scale, BoostColour, 1.f);
	Y += 24.f * Scale;
	DrawBar(RightX - 230.f * Scale, Y, 230.f * Scale, 9.f * Scale, Boost, BoostColour);
	Y += 28.f * Scale;

	const int32 Torpedoes = Boat->GetTorpedoAmmo();
	const int32 Mines = Boat->GetMineAmmo();
	DrawLabel(TEXT("TORPEDO   SPACE"), RightX - 230.f * Scale, Y, 1.1f * Scale, Torpedoes > 0 ? Dim : Faint);
	DrawLabel(FString::FromInt(Torpedoes), RightX, Y, 1.4f * Scale, Torpedoes > 0 ? Ink : Faint, 1.f);
	Y += 26.f * Scale;
	DrawLabel(TEXT("MINE      Q"), RightX - 230.f * Scale, Y, 1.1f * Scale, Mines > 0 ? Dim : Faint);
	DrawLabel(FString::FromInt(Mines), RightX, Y, 1.4f * Scale, Mines > 0 ? Ink : Faint, 1.f);
}

void ABoatRaceHUD::DrawMiniMap(const TArray<FStanding>& Standings, const ABoatRacePlayerState* Me, float Width, float Height, float Scale)
{
	if (!bShowMiniMap)
	{
		return;
	}

	const TArray<FBoatGate>& Gates = BoatTrack::GetGates();
	if (Gates.Num() < 2)
	{
		return;
	}

	if (!bMapBoundsReady)
	{
		MapMin = FVector2D(BIG_NUMBER, BIG_NUMBER);
		MapMax = FVector2D(-BIG_NUMBER, -BIG_NUMBER);
		for (const FBoatGate& Gate : Gates)
		{
			MapMin.X = FMath::Min(MapMin.X, Gate.Center.X);
			MapMin.Y = FMath::Min(MapMin.Y, Gate.Center.Y);
			MapMax.X = FMath::Max(MapMax.X, Gate.Center.X);
			MapMax.Y = FMath::Max(MapMax.Y, Gate.Center.Y);
		}
		const FVector2D Pad = (MapMax - MapMin) * 0.08;
		MapMin -= Pad;
		MapMax += Pad;
		bMapBoundsReady = true;
	}

	const float BoxSize = 200.f * Scale;
	const float BoxX = Width - BoxSize - 40.f * Scale;
	const float BoxY = 40.f * Scale;
	DrawRect(Panel, BoxX, BoxY, BoxSize, BoxSize);

	// Keep the circuit's proportions inside the square.
	const FVector2D Span = MapMax - MapMin;
	const double Biggest = FMath::Max(Span.X, Span.Y);
	auto ToScreen = [&](const FVector2D& World)
	{
		const double U = (World.X - MapMin.X) / Biggest;
		const double V = (World.Y - MapMin.Y) / Biggest;
		// North up: world +X draws upwards, world +Y to the right.
		return FVector2D(BoxX + V * BoxSize, BoxY + (1.0 - U) * BoxSize);
	};

	for (int32 Index = 0; Index < Gates.Num(); ++Index)
	{
		const FVector2D A = ToScreen(Gates[Index].Center);
		const FVector2D B = ToScreen(Gates[(Index + 1) % Gates.Num()].Center);
		const bool bStart = (Index == 0);
		DrawLine(static_cast<float>(A.X), static_cast<float>(A.Y), static_cast<float>(B.X), static_cast<float>(B.Y),
			bStart ? Accent : FLinearColor(0.45f, 0.5f, 0.55f, 0.9f), bStart ? 3.f : 2.f);
	}

	for (const FStanding& Standing : Standings)
	{
		if (Standing.Boat == nullptr)
		{
			continue;
		}
		const FVector Location = Standing.Boat->GetActorLocation();
		const FVector2D Point = ToScreen(FVector2D(Location.X, Location.Y));
		const bool bIsMe = (Standing.Player == Me);
		const float Dot = (bIsMe ? 9.f : 7.f) * Scale;
		DrawRect(BoatTrack::GetPlayerColor(Standing.Player->SlotIndex),
			static_cast<float>(Point.X) - Dot * 0.5f, static_cast<float>(Point.Y) - Dot * 0.5f, Dot, Dot);
		if (bIsMe)
		{
			DrawRect(Ink, static_cast<float>(Point.X) - Dot * 0.5f - 2.f, static_cast<float>(Point.Y) - Dot * 0.5f - 2.f, Dot + 4.f, 2.f);
		}
	}
}

void ABoatRaceHUD::DrawKillFeed(const ABoatRaceGameState* RaceState, float Width, float Height, float Scale)
{
	const UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	const float Now = static_cast<float>(World->GetTimeSeconds());
	const TArray<FBoatKillFeedLine>& Feed = RaceState->GetKillFeed();
	float Y = Height * 0.42f;

	for (int32 Index = FMath::Max(0, Feed.Num() - 4); Index < Feed.Num(); ++Index)
	{
		const float Age = Now - Feed[Index].ArrivedAt;
		if (Age > 6.f)
		{
			continue;
		}
		// Fades out over its last second and a half.
		const float Alpha = FMath::Clamp((6.f - Age) / 1.5f, 0.f, 1.f);
		FLinearColor Color = Bad;
		Color.A = Alpha;
		DrawLabel(Feed[Index].Text, Width - 44.f * Scale, Y, 1.2f * Scale, Color, 1.f);
		Y += 26.f * Scale;
	}
}

void ABoatRaceHUD::DrawHeadingStrip(const ABoatPawn* Boat, const ABoatRacePlayerState* RacePlayer, float Width, float Scale)
{
	// A compass tape across the top of the screen, as on a boat's
	// instruments: the heading scrolls past and a pip shows the next gate.
	const float StripW = 520.f * Scale, StripH = 34.f * Scale;
	const float X0 = (Width - StripW) * 0.5f, Y0 = 18.f * Scale;
	DrawRect(FLinearColor(0.01f, 0.03f, 0.05f, 0.55f), X0, Y0, StripW, StripH);
	const float Heading = static_cast<float>(FRotator::NormalizeAxis(Boat->GetActorRotation().Yaw));
	const float Span = 120.f; // degrees shown across the tape
	const float PixelsPerDegree = StripW / Span;
	for (int32 Mark = -180; Mark < 540; Mark += 15)
	{
		const float Delta = FRotator::NormalizeAxis(static_cast<float>(Mark) - Heading);
		if (FMath::Abs(Delta) > Span * 0.5f || Mark >= 180 + 360 || Mark < -180)
		{
			continue;
		}
		const float Px = X0 + StripW * 0.5f + Delta * PixelsPerDegree;
		const bool bMajor = (Mark % 45) == 0;
		DrawRect(FLinearColor(1.f, 1.f, 1.f, bMajor ? 0.8f : 0.35f), Px - 1.f, Y0 + StripH - (bMajor ? 12.f : 7.f) * Scale, 2.f, (bMajor ? 12.f : 7.f) * Scale);
		if (bMajor && Mark >= -180 && Mark < 180)
		{
			// Unreal's +X is east here and yaw grows clockwise from above.
			static const TCHAR* const Names[] = { TEXT("W"), TEXT("SW"), TEXT("S"), TEXT("SE"), TEXT("E"), TEXT("NE"), TEXT("N"), TEXT("NW") };
			const int32 Slot = ((Mark + 180) / 45) % 8;
			DrawLabel(Names[Slot], Px, Y0 + 2.f * Scale, 0.95f * Scale, Dim, 0.5f);
		}
	}
	DrawRect(Ink, X0 + StripW * 0.5f - 1.f, Y0 + StripH - 4.f * Scale, 2.f, 8.f * Scale);

	const TArray<FBoatGate>& Gates = BoatTrack::GetGates();
	if (!Gates.IsValidIndex(RacePlayer->NextGate))
	{
		return;
	}
	const FVector Location = Boat->GetActorLocation();
	const FVector2D ToGate = Gates[RacePlayer->NextGate].Center - FVector2D(Location.X, Location.Y);
	const float Bearing = FMath::RadiansToDegrees(static_cast<float>(FMath::Atan2(ToGate.Y, ToGate.X)));
	const float Relative = FRotator::NormalizeAxis(Bearing - Heading);
	const float Clamped = FMath::Clamp(Relative, -Span * 0.5f, Span * 0.5f);
	const float Px = X0 + StripW * 0.5f + Clamped * PixelsPerDegree;
	const bool bBehind = FMath::Abs(Relative) > 110.f;
	const FLinearColor Pip = bBehind ? Bad : Accent;
	// A downward pointer on the tape; a side arrow when the gate is off it.
	if (FMath::Abs(Relative) <= Span * 0.5f)
	{
		DrawTriangle(FVector2D(Px - 9.f * Scale, Y0 + StripH + 2.f * Scale), FVector2D(Px + 9.f * Scale, Y0 + StripH + 2.f * Scale),
			FVector2D(Px, Y0 + StripH + 14.f * Scale), Pip);
	}
	else
	{
		const float Dir = Relative > 0.f ? 1.f : -1.f;
		const float Ex = X0 + StripW * 0.5f + Dir * (StripW * 0.5f + 16.f * Scale);
		const float Ey = Y0 + StripH * 0.5f;
		DrawTriangle(FVector2D(Ex - Dir * 10.f * Scale, Ey - 12.f * Scale), FVector2D(Ex - Dir * 10.f * Scale, Ey + 12.f * Scale),
			FVector2D(Ex + Dir * 10.f * Scale, Ey), Pip);
	}
	if (bBehind)
	{
		DrawLabel(TEXT("TURN AROUND"), Width * 0.5f, Y0 + StripH + 18.f * Scale, 2.2f * Scale, Bad, 0.5f, true);
	}
}

void ABoatRaceHUD::DrawGateMarker(const ABoatPawn* Boat, const ABoatRacePlayerState* RacePlayer, float Width, float Height, float Scale)
{
	// A marker hanging in the world over the next gate, like a waypoint in an
	// open-world game: a chevron pointing down at the gap, with the gate
	// number and distance. When the gate is off screen it sits on the edge
	// and points towards it.
	const TArray<FBoatGate>& Gates = BoatTrack::GetGates();
	if (!Gates.IsValidIndex(RacePlayer->NextGate))
	{
		return;
	}
	const FBoatGate& Gate = Gates[RacePlayer->NextGate];
	const FVector Target(Gate.Center.X, Gate.Center.Y, 1100.0);
	const FVector Location = Boat->GetActorLocation();
	const float Distance = static_cast<float>(FVector::Dist2D(Location, Target)) / 100.f;

	FVector ViewPoint;
	FRotator ViewRotation;
	GetOwningPlayerController()->GetPlayerViewPoint(ViewPoint, ViewRotation);
	const bool bInFront = FVector::DotProduct(ViewRotation.Vector(), Target - ViewPoint) > 0.0;
	const FVector Projected = Project(Target, false);
	FVector2D Screen(Projected.X, Projected.Y);
	const float Margin = 60.f * Scale;
	const bool bOnScreen = bInFront && Screen.X > Margin && Screen.X < Width - Margin && Screen.Y > Margin && Screen.Y < Height - Margin;

	const float Pulse = 0.85f + 0.15f * FMath::Sin(static_cast<float>(GetWorld()->GetRealTimeSeconds()) * 5.f);
	const FLinearColor Colour(1.f, 0.62f, 0.1f, 0.95f);
	const float S = 16.f * Scale * Pulse;
	if (bOnScreen)
	{
		// Chevron: two stacked arrowheads pointing down at the gate.
		for (int32 Layer = 0; Layer < 2; ++Layer)
		{
			const float Oy = static_cast<float>(Screen.Y) - Layer * S * 1.1f;
			DrawTriangle(FVector2D(Screen.X - S * 1.3f, Oy - S), FVector2D(Screen.X + S * 1.3f, Oy - S), FVector2D(Screen.X, Oy + S * 0.4f),
				FLinearColor(0.f, 0.f, 0.f, 0.35f));
			DrawTriangle(FVector2D(Screen.X - S * 1.15f, Oy - S * 1.05f), FVector2D(Screen.X + S * 1.15f, Oy - S * 1.05f),
				FVector2D(Screen.X, Oy + S * 0.2f), Layer == 0 ? Colour : FLinearColor(1.f, 0.85f, 0.4f, 0.7f));
		}
		DrawLabel(FString::Printf(TEXT("GATE %d"), RacePlayer->NextGate + 1), Screen.X, Screen.Y + S * 0.8f, 1.9f * Scale, Ink, 0.5f, true);
		DrawLabel(FString::Printf(TEXT("%d m"), FMath::RoundToInt(Distance)), Screen.X, Screen.Y + S * 0.8f + 26.f * Scale, 1.f * Scale, Dim, 0.5f);
		return;
	}

	// Off screen: pin to the nearest edge in the gate's direction.
	FVector2D Centre(Width * 0.5f, Height * 0.5f);
	FVector2D Dir = Screen - Centre;
	if (!bInFront)
	{
		Dir = -Dir;
	}
	if (Dir.IsNearlyZero())
	{
		Dir = FVector2D(0.f, -1.f);
	}
	Dir.Normalize();
	const float Reach = FMath::Min((Width * 0.5f - Margin) / FMath::Max(FMath::Abs(static_cast<float>(Dir.X)), 0.001f),
		(Height * 0.5f - Margin) / FMath::Max(FMath::Abs(static_cast<float>(Dir.Y)), 0.001f));
	const FVector2D Edge = Centre + Dir * Reach;
	const FVector2D Side(-Dir.Y, Dir.X);
	DrawTriangle(Edge + Dir * S * 1.3f, Edge - Dir * S * 0.6f + Side * S, Edge - Dir * S * 0.6f - Side * S, Colour);
	DrawLabel(FString::Printf(TEXT("%d m"), FMath::RoundToInt(Distance)), Edge.X - Dir.X * S * 2.4f, Edge.Y - Dir.Y * S * 2.4f - 12.f * Scale,
		1.f * Scale, Ink, 0.5f);
}

void ABoatRaceHUD::DrawDamageEdge(const ABoatPawn* Boat, float Width, float Height)
{
	if (Boat == nullptr)
	{
		return;
	}

	// A red wash around the edge of the screen for a moment after a hit.
	const float SinceHit = Boat->GetTimeSinceHit();
	if (SinceHit < 0.f || SinceHit > 0.6f)
	{
		return;
	}

	const float Strength = (1.f - SinceHit / 0.6f) * 0.45f;
	FLinearColor Flash = Bad;
	Flash.A = Strength;
	const float Band = Height * 0.09f;
	DrawRect(Flash, 0.f, 0.f, Width, Band);
	DrawRect(Flash, 0.f, Height - Band, Width, Band);
	DrawRect(Flash, 0.f, 0.f, Band, Height);
	DrawRect(Flash, Width - Band, 0.f, Band, Height);
}

// ---------------------------------------------------------------------------
// Results and name tags
// ---------------------------------------------------------------------------
void ABoatRaceHUD::DrawResults(const ABoatRaceGameState* RaceState, const TArray<FStanding>& Standings, float Width, float Height, float Scale)
{
	DrawRect(FLinearColor(0.f, 0.01f, 0.03f, 0.35f), 0.f, 0.f, Width, Height);

	// The podium: first in the middle and highest, in each boat's colour.
	const float Base = Height * 0.52f;
	const float BlockW = 200.f * Scale;
	const int32 Order[] = { 1, 0, 2 }; // left: 2nd, middle: 1st, right: 3rd
	const float Heights[] = { 150.f, 210.f, 110.f };
	for (int32 Column = 0; Column < 3; ++Column)
	{
		const int32 Place = Order[Column];
		if (!Standings.IsValidIndex(Place))
		{
			continue;
		}
		const ABoatRacePlayerState* Player = Standings[Place].Player;
		const float X = Width * 0.5f + (Column - 1) * (BlockW + 16.f * Scale) - BlockW * 0.5f;
		const float H = Heights[Place] * Scale;
		FLinearColor Colour = BoatTrack::GetPlayerColor(Player->SlotIndex);
		Colour.A = 0.85f;
		DrawRect(Colour, X, Base - H, BlockW, H);
		DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.18f), X, Base - H, BlockW, 4.f * Scale);
		DrawLabel(FString::FromInt(Place + 1), X + BlockW * 0.5f, Base - H + 14.f * Scale, 3.4f * Scale, Ink, 0.5f, true);
		DrawLabel(Player->GetPlayerName(), X + BlockW * 0.5f, Base - H - 44.f * Scale, 1.5f * Scale, Ink, 0.5f, true);
		DrawLabel(Player->bFinished ? FormatTime(Player->FinishTime) : FString(TEXT("DNF")), X + BlockW * 0.5f, Base - 40.f * Scale,
			1.3f * Scale, Ink, 0.5f);
	}
	DrawLabel(TEXT("RESULTS"), Width * 0.5f, Height * 0.08f, 3.4f * Scale, Ink, 0.5f, true);

	// The full table under the podium.
	const float PanelW = 760.f * Scale;
	const float X = (Width - PanelW) * 0.5f;
	float Y = Base + 24.f * Scale;
	DrawPanel(X, Y, PanelW, (70.f + Standings.Num() * 40.f) * Scale, Scale, 0.72f);
	Y += 18.f * Scale;
	DrawLabel(TEXT("TIME"), X + PanelW - 300.f * Scale, Y, 1.f * Scale, Faint, 1.f);
	DrawLabel(TEXT("BEST LAP"), X + PanelW - 160.f * Scale, Y, 1.f * Scale, Faint, 1.f);
	DrawLabel(TEXT("WRECKED"), X + PanelW - 32.f * Scale, Y, 1.f * Scale, Faint, 1.f);
	Y += 34.f * Scale;
	for (int32 Index = 0; Index < Standings.Num(); ++Index)
	{
		const ABoatRacePlayerState* Player = Standings[Index].Player;
		DrawRect(BoatTrack::GetPlayerColor(Player->SlotIndex), X + 32.f * Scale, Y + 5.f * Scale, 6.f * Scale, 20.f * Scale);
		DrawLabel(FString::Printf(TEXT("%s   %s"), *Ordinal(Index + 1), *Player->GetPlayerName()), X + 50.f * Scale, Y, 1.3f * Scale,
			Index == 0 ? Accent : Ink);
		DrawLabel(Player->bFinished ? FormatTime(Player->FinishTime) : FString(TEXT("DNF")), X + PanelW - 300.f * Scale, Y, 1.3f * Scale, Ink, 1.f);
		DrawLabel(Player->BestLapTime > 0.f ? FormatTime(Player->BestLapTime) : FString(TEXT("--")), X + PanelW - 160.f * Scale, Y, 1.3f * Scale, Dim, 1.f);
		DrawLabel(FString::FromInt(Player->Hits), X + PanelW - 32.f * Scale, Y, 1.3f * Scale, Dim, 1.f);
		Y += 40.f * Scale;
	}

	const int32 SecondsLeft = FMath::Max(FMath::CeilToInt(RaceState->StateEndTime - RaceState->GetServerNow()), 0);
	const APlayerController* Controller = GetOwningPlayerController();
	const bool bHost = Controller != nullptr && Controller->HasAuthority();
	DrawLabel(bHost
		? FString::Printf(TEXT("ENTER race again      ESC menu      back to the lobby in %d s"), SecondsLeft)
		: FString::Printf(TEXT("The host can start another race      back to the lobby in %d s"), SecondsLeft),
		Width * 0.5f, Y + 30.f * Scale, 1.1f * Scale, Dim, 0.5f);
}

void ABoatRaceHUD::DrawNameTags(float Scale)
{
	const UWorld* World = GetWorld();
	const ABoatRaceGameState* RaceState = World ? World->GetGameState<ABoatRaceGameState>() : nullptr;
	const APlayerController* Controller = GetOwningPlayerController();
	if (RaceState == nullptr || Controller == nullptr)
	{
		return;
	}

	const APawn* MyBoat = Controller->GetPawn();
	const FVector Me = MyBoat ? MyBoat->GetActorLocation() : FVector::ZeroVector;
	// Tags fade with distance and step up out of each other's way.
	TArray<FBox2D> Placed;
	for (APlayerState* Player : RaceState->PlayerArray)
	{
		const ABoatRacePlayerState* RacePlayer = Cast<ABoatRacePlayerState>(Player);
		const ABoatPawn* Boat = RacePlayer ? Cast<ABoatPawn>(RacePlayer->GetPawn()) : nullptr;
		if (Boat == nullptr || Boat == MyBoat || RacePlayer->SlotIndex < 0)
		{
			continue;
		}
		const float Distance = static_cast<float>(FVector::Dist(Me, Boat->GetActorLocation())) / 100.f;
		const float Fade = 1.f - FMath::Clamp((Distance - 250.f) / 350.f, 0.f, 1.f);
		if (Fade <= 0.02f)
		{
			continue;
		}
		const FVector Screen = Project(Boat->GetActorLocation() + FVector(0.f, 0.f, 420.f));
		if (Screen.Z <= 0.f)
		{
			continue; // Behind the camera.
		}
		FLinearColor Color = BoatTrack::GetPlayerColor(RacePlayer->SlotIndex);
		FString Tag = RacePlayer->GetPlayerName();
		if (Boat->IsWrecked())
		{
			Tag += TEXT("  wrecked");
			Color = Bad;
		}
		Color = FMath::Lerp(Color, FLinearColor::White, 0.35f);
		Color.A = Fade;
		const float Size = FMath::Lerp(0.9f, 1.15f, Fade) * Scale;
		const FVector2D Extent = MeasureLabel(Tag, Size, true);
		FBox2D Box(FVector2D(Screen.X - Extent.X * 0.5f, Screen.Y), FVector2D(Screen.X + Extent.X * 0.5f, Screen.Y + Extent.Y));
		for (int32 Tries = 0; Tries < 4; ++Tries)
		{
			const bool bHit = Placed.ContainsByPredicate([&Box](const FBox2D& Other) { return Other.Intersect(Box); });
			if (!bHit)
			{
				break;
			}
			Box = Box.ShiftBy(FVector2D(0.f, -(Extent.Y + 4.f)));
		}
		Placed.Add(Box);
		DrawLabel(Tag, static_cast<float>(Screen.X), static_cast<float>(Box.Min.Y), Size, Color, 0.5f, true);
	}
}
