#include "BoatRaceHUD.h"

#include "BoatPawn.h"
#include "BoatRaceGameState.h"
#include "BoatRacePlayerController.h"
#include "BoatRacePlayerState.h"
#include "BoatTrack.h"
#include "LanDiscovery.h"

#include "Algo/Sort.h"
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

void ABoatRaceHUD::DrawLabel(const FString& Text, float X, float Y, float Size, const FLinearColor& Color, float Align, bool bLarge)
{
	UFont* Font = nullptr;
	if (GEngine != nullptr)
	{
		Font = bLarge ? GEngine->GetLargeFont() : GEngine->GetMediumFont();
	}

	float TextWidth = 0.f;
	float TextHeight = 0.f;
	GetTextSize(Text, TextWidth, TextHeight, Font, Size);

	const float Left = X - TextWidth * Align;
	const float Shadow = FMath::Max(1.f, Size * 0.6f);
	DrawText(Text, FLinearColor(0.f, 0.f, 0.f, 0.8f * Color.A), Left + Shadow, Y + Shadow, Font, Size, false);
	DrawText(Text, Color, Left, Y, Font, Size, false);
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
}

// ---------------------------------------------------------------------------
// Menu and lobby
// ---------------------------------------------------------------------------
void ABoatRaceHUD::DrawMenu(float Width, float Height, float Scale)
{
	if (TitleTexture != nullptr)
	{
		DrawTexture(TitleTexture.Get(), 0.f, 0.f, Width, Height, 0.f, 0.f, 1.f, 1.f);
	}

	const ULanDiscovery* Discovery = GetGameInstance() ? GetGameInstance()->GetSubsystem<ULanDiscovery>() : nullptr;
	const TArray<FFoundRace> NoRaces;
	const TArray<FFoundRace>& Races = Discovery ? Discovery->GetFoundRaces() : NoRaces;
	const int32 RaceRows = FMath::Clamp(Races.Num(), 1, BoatTrack::MaxPlayers);

	const float PanelWidth = 820.f * Scale;
	const float PanelHeight = (520.f + 60.f + 44.f * RaceRows) * Scale;
	const float PanelX = (Width - PanelWidth) * 0.5f;
	const float PanelY = Height * 0.5f - PanelHeight * 0.5f;
	DrawRect(Panel, PanelX, PanelY, PanelWidth, PanelHeight);

	const float CentreX = Width * 0.5f;
	float Y = PanelY + 28.f * Scale;
	DrawLabel(TEXT("BOAT RACE"), CentreX, Y, 3.4f * Scale, Ink, 0.5f, true);
	Y += 105.f * Scale;
	DrawLabel(TEXT("Four-player powerboat racing, armed"), CentreX, Y, 1.4f * Scale, Dim, 0.5f);
	Y += 60.f * Scale;

	const ABoatRacePlayerController* RaceController = Cast<ABoatRacePlayerController>(GetOwningPlayerController());
	if (RaceController != nullptr && RaceController->IsTypingAddress())
	{
		DrawLabel(TEXT("Host address (IP or name):"), CentreX, Y, 1.6f * Scale, Ink, 0.5f);
		Y += 52.f * Scale;
		const bool bCaretOn = FMath::Fmod(static_cast<float>(GetWorld()->GetRealTimeSeconds()), 1.f) < 0.5f;
		DrawLabel(RaceController->GetTypedAddress() + (bCaretOn ? TEXT("_") : TEXT(" ")), CentreX, Y, 2.2f * Scale, Accent, 0.5f, true);
		Y += 82.f * Scale;
		DrawLabel(TEXT("ENTER  connect        ESC  cancel"), CentreX, Y, 1.3f * Scale, Dim, 0.5f);
		return;
	}

	// Races on the same network come first: for most players joining is just
	// pressing the number next to a friend's name.
	if (Races.Num() == 0)
	{
		DrawLabel(TEXT("Looking for races on your Wi-Fi..."), CentreX, Y, 1.4f * Scale, Dim, 0.5f);
		Y += 44.f * Scale;
	}
	else
	{
		DrawLabel(TEXT("RACES ON YOUR WI-FI"), CentreX, Y, 1.4f * Scale, Dim, 0.5f);
		Y += 44.f * Scale;
		for (int32 Index = 0; Index < RaceRows; ++Index)
		{
			const FFoundRace& Race = Races[Index];
			const bool bFull = Race.Players >= Race.MaxPlayers;
			const FString Status = bFull ? TEXT("full") : Race.bRacing ? TEXT("racing now") : FString::Printf(TEXT("%d/%d players"), Race.Players, Race.MaxPlayers);
			DrawLabel(FString::Printf(TEXT("%d        Join %s   (%s)"), Index + 1, *Race.Name, *Status),
				CentreX, Y, 1.6f * Scale, bFull ? Dim : Accent, 0.5f);
			Y += 44.f * Scale;
		}
	}
	Y += 16.f * Scale;

	DrawLabel(TEXT("H        Host a race (you and up to 3 others)"), CentreX, Y, 1.6f * Scale, Ink, 0.5f);
	Y += 48.f * Scale;
	DrawLabel(TEXT("J        Join by typing an address"), CentreX, Y, 1.6f * Scale, Ink, 0.5f);
	Y += 48.f * Scale;
	DrawLabel(TEXT("ENTER    Solo practice race"), CentreX, Y, 1.6f * Scale, Ink, 0.5f);
	Y += 72.f * Scale;
	DrawLabel(TEXT("W / S throttle     A / D steer     SPACE torpedo     Q mine"), CentreX, Y, 1.2f * Scale, Dim, 0.5f);
	Y += 34.f * Scale;
	DrawLabel(TEXT("R  back to the last gate          Gamepad: triggers, stick, A, X"), CentreX, Y, 1.2f * Scale, Dim, 0.5f);
}

void ABoatRaceHUD::DrawLobby(const ABoatRaceGameState* RaceState, const TArray<FStanding>& Standings, float Width, float Height, float Scale)
{
	const float CentreX = Width * 0.5f;
	float Y = 55.f * Scale;

	DrawLabel(FString::Printf(TEXT("LOBBY   %d / %d"), Standings.Num(), BoatTrack::MaxPlayers),
		CentreX, Y, 2.2f * Scale, Ink, 0.5f, true);
	Y += 74.f * Scale;

	const APlayerController* Controller = GetOwningPlayerController();
	const bool bHost = Controller != nullptr && Controller->HasAuthority();
	DrawLabel(bHost ? TEXT("You are the host. Press ENTER to start.") : TEXT("Waiting for the host to start the race..."),
		CentreX, Y, 1.6f * Scale, Accent, 0.5f);
	Y += 42.f * Scale;
	DrawLabel(TEXT("Warm up on the course.   L  leave"), CentreX, Y, 1.2f * Scale, Dim, 0.5f);

	float ListY = 55.f * Scale;
	const float ListX = 44.f * Scale;
	for (const FStanding& Standing : Standings)
	{
		DrawRect(BoatTrack::GetPlayerColor(Standing.Player->SlotIndex), ListX, ListY + 6.f * Scale, 18.f * Scale, 18.f * Scale);
		DrawLabel(FString::Printf(TEXT("P%d  %s"), Standing.Player->SlotIndex + 1, *Standing.Player->GetPlayerName()),
			ListX + 32.f * Scale, ListY, 1.4f * Scale, Ink);
		ListY += 38.f * Scale;
	}

	if (const ABoatPawn* Boat = Cast<ABoatPawn>(Controller ? Controller->GetPawn() : nullptr))
	{
		DrawSpeed(Boat, Width, Height, Scale);
		DrawStatus(Boat, Width, Height, Scale);
	}
	DrawMiniMap(Standings, Controller ? Controller->GetPlayerState<ABoatRacePlayerState>() : nullptr, Width, Height, Scale);
}

void ABoatRaceHUD::DrawCountdown(const ABoatRaceGameState* RaceState, float Width, float Height, float Scale)
{
	const float Remaining = RaceState->StateEndTime - RaceState->GetServerNow();
	const int32 Number = FMath::Max(FMath::CeilToInt(Remaining), 1);
	DrawLabel(FString::FromInt(Number), Width * 0.5f, Height * 0.24f, 9.f * Scale, Accent, 0.5f, true);
	DrawLabel(FString::Printf(TEXT("%d LAPS"), RaceState->TotalLaps),
		Width * 0.5f, Height * 0.24f + 230.f * Scale, 1.7f * Scale, Ink, 0.5f);
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

	if (RaceState->RaceState == ERaceState::Racing && RaceState->GetRaceTime() < 1.3f)
	{
		DrawLabel(TEXT("GO!"), Width * 0.5f, Height * 0.24f, 9.f * Scale, Good, 0.5f, true);
	}

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
		DrawGateCompass(MyBoat, Me, Width, Scale);
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
	float Y = Height - 132.f * Scale;

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

void ABoatRaceHUD::DrawGateCompass(const ABoatPawn* Boat, const ABoatRacePlayerState* RacePlayer, float Width, float Scale)
{
	const TArray<FBoatGate>& Gates = BoatTrack::GetGates();
	if (!Gates.IsValidIndex(RacePlayer->NextGate))
	{
		return;
	}

	// Bearing of the next gate relative to the bow: screen up is dead ahead.
	const FVector Location = Boat->GetActorLocation();
	const FVector2D ToGate = Gates[RacePlayer->NextGate].Center - FVector2D(Location.X, Location.Y);
	const float Distance = static_cast<float>(ToGate.Size());
	const float Bearing = FMath::RadiansToDegrees(static_cast<float>(FMath::Atan2(ToGate.Y, ToGate.X)));
	const float Relative = static_cast<float>(FRotator::NormalizeAxis(Bearing - Boat->GetActorRotation().Yaw));

	const float CentreX = Width * 0.5f;
	const float CentreY = 96.f * Scale;
	const float Length = 44.f * Scale;
	const float Angle = FMath::DegreesToRadians(Relative);
	const FVector2D Direction(FMath::Sin(Angle), -FMath::Cos(Angle));
	const FVector2D Side(-Direction.Y, Direction.X);
	const FVector2D Tip = FVector2D(CentreX, CentreY) + Direction * Length;
	const FVector2D Tail = FVector2D(CentreX, CentreY) - Direction * Length;
	const FVector2D BarbA = Tip - Direction * (Length * 0.6f) + Side * (Length * 0.4f);
	const FVector2D BarbB = Tip - Direction * (Length * 0.6f) - Side * (Length * 0.4f);

	const bool bWrongWay = FMath::Abs(Relative) > 110.f;
	const FLinearColor Color = bWrongWay ? Bad : Accent;
	const float Thickness = 5.f * Scale;
	DrawLine(static_cast<float>(Tail.X), static_cast<float>(Tail.Y), static_cast<float>(Tip.X), static_cast<float>(Tip.Y), Color, Thickness);
	DrawLine(static_cast<float>(Tip.X), static_cast<float>(Tip.Y), static_cast<float>(BarbA.X), static_cast<float>(BarbA.Y), Color, Thickness);
	DrawLine(static_cast<float>(Tip.X), static_cast<float>(Tip.Y), static_cast<float>(BarbB.X), static_cast<float>(BarbB.Y), Color, Thickness);

	DrawLabel(FString::Printf(TEXT("GATE %d   %d m"), RacePlayer->NextGate + 1, FMath::RoundToInt(Distance / 100.f)),
		CentreX, CentreY + Length + 10.f * Scale, 1.15f * Scale, Color, 0.5f);
	if (bWrongWay)
	{
		DrawLabel(TEXT("TURN AROUND"), CentreX, CentreY + Length + 36.f * Scale, 1.3f * Scale, Bad, 0.5f);
	}
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
	const float PanelWidth = 900.f * Scale;
	const float PanelHeight = (250.f + Standings.Num() * 56.f) * Scale;
	const float PanelX = (Width - PanelWidth) * 0.5f;
	const float PanelY = Height * 0.15f;
	DrawRect(Panel, PanelX, PanelY, PanelWidth, PanelHeight);

	const float CentreX = Width * 0.5f;
	float Y = PanelY + 26.f * Scale;
	DrawLabel(TEXT("RESULTS"), CentreX, Y, 3.f * Scale, Ink, 0.5f, true);
	Y += 104.f * Scale;

	DrawLabel(TEXT("TIME"), PanelX + PanelWidth - 300.f * Scale, Y - 26.f * Scale, 1.f * Scale, Faint, 1.f);
	DrawLabel(TEXT("BEST LAP"), PanelX + PanelWidth - 170.f * Scale, Y - 26.f * Scale, 1.f * Scale, Faint, 1.f);
	DrawLabel(TEXT("HITS"), PanelX + PanelWidth - 50.f * Scale, Y - 26.f * Scale, 1.f * Scale, Faint, 1.f);

	for (int32 Index = 0; Index < Standings.Num(); ++Index)
	{
		const ABoatRacePlayerState* Player = Standings[Index].Player;
		const float RowX = PanelX + 46.f * Scale;
		DrawRect(BoatTrack::GetPlayerColor(Player->SlotIndex), RowX, Y + 8.f * Scale, 20.f * Scale, 20.f * Scale);
		DrawLabel(FString::Printf(TEXT("%s   %s"), *Ordinal(Index + 1), *Player->GetPlayerName()),
			RowX + 34.f * Scale, Y, 1.6f * Scale, Index == 0 ? Accent : Ink);

		DrawLabel(Player->bFinished ? FormatTime(Player->FinishTime) : FString(TEXT("DNF")),
			PanelX + PanelWidth - 300.f * Scale, Y, 1.4f * Scale, Ink, 1.f);
		DrawLabel(Player->BestLapTime > 0.f ? FormatTime(Player->BestLapTime) : FString(TEXT("--")),
			PanelX + PanelWidth - 170.f * Scale, Y, 1.4f * Scale, Dim, 1.f);
		DrawLabel(FString::Printf(TEXT("%d"), Player->Hits),
			PanelX + PanelWidth - 50.f * Scale, Y, 1.4f * Scale, Dim, 1.f);
		Y += 56.f * Scale;
	}

	Y += 24.f * Scale;
	const int32 SecondsLeft = FMath::Max(FMath::CeilToInt(RaceState->StateEndTime - RaceState->GetServerNow()), 0);
	const APlayerController* Controller = GetOwningPlayerController();
	const bool bHost = Controller != nullptr && Controller->HasAuthority();
	DrawLabel(bHost
		? FString::Printf(TEXT("ENTER  race again        back to lobby in %d s"), SecondsLeft)
		: FString::Printf(TEXT("The host can start another race.   Back to lobby in %d s"), SecondsLeft),
		CentreX, Y, 1.3f * Scale, Dim, 0.5f);
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
	for (APlayerState* Player : RaceState->PlayerArray)
	{
		const ABoatRacePlayerState* RacePlayer = Cast<ABoatRacePlayerState>(Player);
		const ABoatPawn* Boat = RacePlayer ? Cast<ABoatPawn>(RacePlayer->GetPawn()) : nullptr;
		if (Boat == nullptr || Boat == MyBoat || RacePlayer->SlotIndex < 0)
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
			Tag += TEXT("  [wrecked]");
			Color = Bad;
		}
		DrawLabel(Tag, static_cast<float>(Screen.X), static_cast<float>(Screen.Y), 1.05f * Scale, Color, 0.5f);
	}
}
