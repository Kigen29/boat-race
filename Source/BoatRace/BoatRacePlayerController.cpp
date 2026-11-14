#include "BoatRacePlayerController.h"

#include "BayTerrain.h"
#include "BoatAudio.h"
#include "BoatCoins.h"
#include "BoatShowroom.h"
#include "BoatGhost.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "BoatRacePlayerState.h"
#include "BoatPawn.h"
#include "BoatPrefs.h"
#include "BoatProjectile.h"
#include "BoatRace.h"
#include "BoatRaceGameMode.h"
#include "BoatRaceGameState.h"
#include "BoatRaceHUD.h"
#include "BoatTrack.h"
#include "LanDiscovery.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "UnrealClient.h"

namespace
{
	// The world is built in code, so every "level" is the engine's empty map.
	const TCHAR* const RaceMap = TEXT("/Engine/Maps/Entry");
	const int32 MaxAddressLength = 60;

	struct FTypedKey
	{
		FKey Key;
		TCHAR Character;
	};

	/** Keys accepted while typing a host address (IP, host name or host:port). */
	const TArray<FTypedKey>& GetAddressKeys()
	{
		static TArray<FTypedKey> Keys;
		if (Keys.Num() == 0)
		{
			Keys.Add(FTypedKey{ EKeys::Zero, TEXT('0') });
			Keys.Add(FTypedKey{ EKeys::One, TEXT('1') });
			Keys.Add(FTypedKey{ EKeys::Two, TEXT('2') });
			Keys.Add(FTypedKey{ EKeys::Three, TEXT('3') });
			Keys.Add(FTypedKey{ EKeys::Four, TEXT('4') });
			Keys.Add(FTypedKey{ EKeys::Five, TEXT('5') });
			Keys.Add(FTypedKey{ EKeys::Six, TEXT('6') });
			Keys.Add(FTypedKey{ EKeys::Seven, TEXT('7') });
			Keys.Add(FTypedKey{ EKeys::Eight, TEXT('8') });
			Keys.Add(FTypedKey{ EKeys::Nine, TEXT('9') });
			Keys.Add(FTypedKey{ EKeys::NumPadZero, TEXT('0') });
			Keys.Add(FTypedKey{ EKeys::NumPadOne, TEXT('1') });
			Keys.Add(FTypedKey{ EKeys::NumPadTwo, TEXT('2') });
			Keys.Add(FTypedKey{ EKeys::NumPadThree, TEXT('3') });
			Keys.Add(FTypedKey{ EKeys::NumPadFour, TEXT('4') });
			Keys.Add(FTypedKey{ EKeys::NumPadFive, TEXT('5') });
			Keys.Add(FTypedKey{ EKeys::NumPadSix, TEXT('6') });
			Keys.Add(FTypedKey{ EKeys::NumPadSeven, TEXT('7') });
			Keys.Add(FTypedKey{ EKeys::NumPadEight, TEXT('8') });
			Keys.Add(FTypedKey{ EKeys::NumPadNine, TEXT('9') });
			Keys.Add(FTypedKey{ EKeys::Period, TEXT('.') });
			Keys.Add(FTypedKey{ EKeys::Decimal, TEXT('.') });
			Keys.Add(FTypedKey{ EKeys::Hyphen, TEXT('-') });
			Keys.Add(FTypedKey{ EKeys::Subtract, TEXT('-') });
			Keys.Add(FTypedKey{ EKeys::Semicolon, TEXT(':') }); // for host:port

			const FKey Letters[] =
			{
				EKeys::A, EKeys::B, EKeys::C, EKeys::D, EKeys::E, EKeys::F, EKeys::G,
				EKeys::H, EKeys::I, EKeys::J, EKeys::K, EKeys::L, EKeys::M, EKeys::N,
				EKeys::O, EKeys::P, EKeys::Q, EKeys::R, EKeys::S, EKeys::T, EKeys::U,
				EKeys::V, EKeys::W, EKeys::X, EKeys::Y, EKeys::Z
			};
			for (int32 Index = 0; Index < UE_ARRAY_COUNT(Letters); ++Index)
			{
				Keys.Add(FTypedKey{ Letters[Index], static_cast<TCHAR>(TEXT('a') + Index) });
			}
		}
		return Keys;
	}
}

void ABoatRacePlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	if (!IsLocalController())
	{
		return;
	}

	if (bTypingAddress)
	{
		HandleAddressTyping();
		SyncCursor();
		return;
	}

	UpdateTimeTrial(DeltaTime);
	// Once the window exists its size is known: size the render to it.
	if (!bGraphicsApplied && GetWorld()->GetRealTimeSeconds() > 1.0)
	{
		bGraphicsApplied = true;
		if (const UBoatPrefs* GraphicsPrefs = UBoatPrefs::Get(this))
		{
			GraphicsPrefs->ApplyGraphics();
		}
	}
	// Test hook: -Garage=N opens the garage on boat N a few seconds in.
	static int32 TestGarageBoat = -2;
	if (TestGarageBoat == -2)
	{
		TestGarageBoat = -1;
		FParse::Value(FCommandLine::Get(), TEXT("Garage="), TestGarageBoat);
	}
	if (TestGarageBoat >= 0 && GetWorld()->GetRealTimeSeconds() > 6.0 && GetPawn() != nullptr)
	{
		FMenuItem Open;
		Open.Action = EMenuAction::Garage;
		ActivateMenuItem(Open, 1);
		GarageBoat = FMath::Clamp(TestGarageBoat, 0, BoatGarage::NumBoats() - 1);
		GaragePaint = 6;
		TestGarageBoat = -1;
	}
	UpdateGarage(DeltaTime);
	UpdateReplay(DeltaTime);

	// Test hook: -ShotAt=60,90 saves screenshots (with the HUD) at those times.
	static TArray<float> ShotTimes;
	static bool bShotsParsed = false;
	if (!bShotsParsed)
	{
		bShotsParsed = true;
		FString List;
		if (FParse::Value(FCommandLine::Get(), TEXT("ShotAt="), List, false))
		{
			TArray<FString> Parts;
			List.ParseIntoArray(Parts, TEXT(","));
			for (const FString& Part : Parts)
			{
				ShotTimes.Add(FCString::Atof(*Part));
			}
		}
	}
	// Real time, so it still fires while the game is paused (photo mode).
	if (ShotTimes.Num() > 0 && GetWorld() && GetWorld()->GetRealTimeSeconds() >= ShotTimes[0])
	{
		FScreenshotRequest::RequestScreenshot(FString::Printf(TEXT("Shot_%03d"), FMath::RoundToInt(ShotTimes[0])), true, false);
		ShotTimes.RemoveAt(0);
	}

	// Test hooks: -TestPhoto=S enters photo mode S seconds in; -TestShield
	// shields the player's boat once the race is on.
	static float TestPhotoAt = -2.f;
	if (TestPhotoAt == -2.f && !FParse::Value(FCommandLine::Get(), TEXT("TestPhoto="), TestPhotoAt))
	{
		TestPhotoAt = -1.f;
	}
	if (TestPhotoAt > 0.f && GetWorld()->GetTimeSeconds() >= TestPhotoAt)
	{
		TestPhotoAt = -1.f;
		EnterPhotoMode();
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("TestShield")) && HasAuthority())
	{
		const ABoatRaceGameState* ShieldState = GetWorld()->GetGameState<ABoatRaceGameState>();
		if (ABoatPawn* ShieldBoat = Cast<ABoatPawn>(GetPawn()))
		{
			if (ShieldState && ShieldState->RaceState == ERaceState::Racing && !ShieldBoat->IsInvulnerable())
			{
				ShieldBoat->GrantShield(30.f);
			}
		}
	}

	if (bPhotoMode)
	{
		UpdatePhotoMode(DeltaTime);
		SyncCursor();
		return;
	}

	if (FParse::Param(FCommandLine::Get(), TEXT("PhotoTour")))
	{
		RunPhotoTour(DeltaTime);
		return;
	}

	// The title screen is always a menu; leaving it (a race starts) closes it.
	if (IsOnMainMenu())
	{
		if (MenuScreen == EMenuScreen::None || MenuScreen == EMenuScreen::Pause)
		{
			OpenMenu(EMenuScreen::Main);
		}
	}
	else if (MenuScreen == EMenuScreen::Main || (MenuParent == EMenuScreen::Main && MenuScreen != EMenuScreen::None
		&& MenuScreen != EMenuScreen::Pause))
	{
		OpenMenu(EMenuScreen::None);
	}

	// ESC (or Start on a pad) opens the pause menu, or goes back a step.
	const bool bBackPressed = WasInputKeyJustPressed(EKeys::Escape) || WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Right);
	if (MenuScreen == EMenuScreen::None)
	{
		if (WasInputKeyJustPressed(EKeys::Escape) || WasInputKeyJustPressed(EKeys::Gamepad_Special_Left)
			|| WasInputKeyJustPressed(EKeys::P))
		{
			OpenMenu(EMenuScreen::Pause);
		}
	}
	else if (bBackPressed)
	{
		if (MenuScreen == EMenuScreen::Settings || MenuScreen == EMenuScreen::Controls)
		{
			OpenMenu(MenuParent);
		}
		else if (MenuScreen == EMenuScreen::Pause)
		{
			OpenMenu(EMenuScreen::None);
		}
	}

	if (MenuScreen != EMenuScreen::None)
	{
		HandleMenuInput();
		SyncCursor();
		return;
	}
	SyncCursor();

	// In a race: the quick keys still work without the menu.
	if (WasInputKeyJustPressed(EKeys::Tab))
	{
		ServerRequestWeather(IsInputKeyDown(EKeys::LeftShift) || IsInputKeyDown(EKeys::RightShift) ? -1 : 1);
	}
	if (WasInputKeyJustPressed(EKeys::M))
	{
		if (UBoatAudioSubsystem* Audio = UBoatAudioSubsystem::Get(this))
		{
			Audio->ToggleMusic();
		}
	}
	if (WasInputKeyJustPressed(EKeys::Enter) || WasInputKeyJustPressed(EKeys::Gamepad_Special_Right))
	{
		ServerRequestStart();
	}
	if (WasInputKeyJustPressed(EKeys::R) || WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Top))
	{
		ServerRequestReset();
	}
	if (GetNetMode() != NM_Standalone && WasInputKeyJustPressed(EKeys::L))
	{
		LeaveRace();
	}
}

bool ABoatRacePlayerController::IsOnMainMenu() const
{
	const UWorld* World = GetWorld();
	const ABoatRaceGameState* RaceState = World ? World->GetGameState<ABoatRaceGameState>() : nullptr;
	return GetNetMode() == NM_Standalone && RaceState != nullptr && RaceState->RaceState == ERaceState::Lobby
		&& !FParse::Param(FCommandLine::Get(), TEXT("PhotoTour"));
}

void ABoatRacePlayerController::OpenMenu(EMenuScreen Screen)
{
	if (Screen == EMenuScreen::Settings || Screen == EMenuScreen::Controls || Screen == EMenuScreen::Garage || Screen == EMenuScreen::Leaderboard)
	{
		if (MenuScreen == EMenuScreen::Main || MenuScreen == EMenuScreen::Pause)
		{
			MenuParent = MenuScreen;
		}
	}
	else
	{
		MenuParent = Screen == EMenuScreen::None ? EMenuScreen::Main : Screen;
	}
	if (MenuScreen != Screen)
	{
		MenuSelection = 0;
		if (Screen != EMenuScreen::None)
		{
			BoatAudio::Play2D(this, EBoatSound::Beep, 0.25f, 1.6f);
		}
	}
	MenuScreen = Screen;
}

void ABoatRacePlayerController::SyncCursor()
{
	// A mouse pointer whenever a menu is up, none while driving or framing a photo.
	const bool bWant = IsMenuOpen() && !bPhotoMode;
	if (bWant == bCursorShown)
	{
		return;
	}
	bCursorShown = bWant;
	SetShowMouseCursor(bWant);
	if (bWant)
	{
		FInputModeGameAndUI Mode;
		Mode.SetHideCursorDuringCapture(false);
		SetInputMode(Mode);
	}
	else
	{
		SetInputMode(FInputModeGameOnly());
	}
}

void ABoatRacePlayerController::BuildMenuItems(TArray<FMenuItem>& Items) const
{
	Items.Reset();
	const UWorld* World = GetWorld();
	const ABoatRaceGameState* RaceState = World ? World->GetGameState<ABoatRaceGameState>() : nullptr;
	const UBoatAudioSubsystem* Audio = UBoatAudioSubsystem::Get(this);
	const bool bHost = HasAuthority();
	const bool bRacing = RaceState && (RaceState->RaceState == ERaceState::Racing || RaceState->RaceState == ERaceState::Countdown);
	const FString WeatherName = RaceState ? RaceWeather::Get(RaceState->GetWeather()).Name : TEXT("Sunny");
	static const TCHAR* const RivalNames[] = { TEXT("None"), TEXT("Easy"), TEXT("Normal"), TEXT("Hard") };
	const FString RivalName = RivalNames[RaceState ? FMath::Clamp<int32>(RaceState->RivalLevel, 0, 3) : 2];
	const UBoatPrefs* Prefs = UBoatPrefs::Get(this);

	auto Add = [&Items](const FString& Label, EMenuAction Action, const FString& Hint = FString(), const FString& Value = FString(),
		bool bAdjustable = false, bool bEnabled = true, int32 Data = 0)
	{
		FMenuItem& Item = Items.AddDefaulted_GetRef();
		Item.Label = Label;
		Item.Action = Action;
		Item.Hint = Hint;
		Item.Value = Value;
		Item.bAdjustable = bAdjustable;
		Item.bEnabled = bEnabled;
		Item.Data = Data;
	};
	auto Percent = [](float Value) { return FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Value * 100.f)); };

	switch (MenuScreen)
	{
	case EMenuScreen::Main:
	{
		Add(TEXT("Solo race"), EMenuAction::Solo, TEXT("Race three computer rivals"));
		Add(TEXT("Time trial"), EMenuAction::TimeTrial, GetGhostLapTime() > 0.f
			? FString::Printf(TEXT("Beat your ghost: best lap %d:%05.2f"), FMath::FloorToInt(GetGhostLapTime() / 60.f), FMath::Fmod(GetGhostLapTime(), 60.f))
			: FString(TEXT("Set a lap; then race its ghost")));
		Add(TEXT("Garage"), EMenuAction::Garage, TEXT("Boats, paint, nitrous and upgrades"), Prefs ? FString::Printf(TEXT("%d coins"), Prefs->Coins) : FString());
		Add(TEXT("Leaderboard"), EMenuAction::Leaderboard, TEXT("Your best ten races on this machine"));
		Add(TEXT("Host a race"), EMenuAction::Host, TEXT("Friends on your Wi-Fi can join you"));
		const ULanDiscovery* Discovery = GetGameInstance() ? GetGameInstance()->GetSubsystem<ULanDiscovery>() : nullptr;
		const TArray<FFoundRace> NoRaces;
		const TArray<FFoundRace>& Races = Discovery ? Discovery->GetFoundRaces() : NoRaces;
		for (int32 Index = 0; Index < FMath::Min(Races.Num(), BoatTrack::MaxPlayers); ++Index)
		{
			const FFoundRace& Race = Races[Index];
			const bool bFull = Race.Players >= Race.MaxPlayers;
			Add(FString::Printf(TEXT("Join %s"), *Race.Name), EMenuAction::JoinFound, TEXT("A race on your Wi-Fi"),
				bFull ? FString(TEXT("Full")) : (Race.bRacing ? FString(TEXT("Racing")) : FString::Printf(TEXT("%d / %d"), Race.Players, Race.MaxPlayers)),
				false, !bFull, Index);
		}
		if (Races.Num() == 0)
		{
			Add(TEXT("Looking for races on your Wi-Fi..."), EMenuAction::None, FString(), FString(), false, false);
		}
		Add(TEXT("Join by address"), EMenuAction::JoinAddress, TEXT("Type the host's IP address"));
		Add(TEXT("Weather"), EMenuAction::Weather, TEXT("Left / right to change"), WeatherName, true);
		Add(TEXT("Mode"), EMenuAction::Mode, TEXT("Elimination: last place is out every lap"), RaceState && RaceState->bElimination ? TEXT("Elimination") : TEXT("Race"), true);
		Add(TEXT("Track"), EMenuAction::Track, TEXT("The bay clockwise or anticlockwise"), RaceState && RaceState->bReverse ? TEXT("Reverse") : TEXT("Bay circuit"), true);
		Add(TEXT("Laps"), EMenuAction::Laps, TEXT("Left / right to change"), FString::FromInt(RaceState ? RaceState->TotalLaps : 2), true);
		Add(TEXT("Rivals"), EMenuAction::Rivals, TEXT("Computer boats fill the empty seats"), RivalName, true);
		Add(TEXT("Settings"), EMenuAction::Settings, TEXT("Camera, steering, graphics, sound"));
		Add(TEXT("Controls"), EMenuAction::Controls, TEXT("Keys and gamepad"));
		Add(TEXT("Quit game"), EMenuAction::Quit);
		break;
	}
	case EMenuScreen::Pause:
		Add(TEXT("Resume"), EMenuAction::Resume);
		Add(TEXT("Photo mode"), EMenuAction::PhotoMode, TEXT("Free camera, no HUD; F12 saves a picture"));
		if (bHost && !bRacing)
		{
			Add(TEXT("Start race"), EMenuAction::StartRace, TEXT("Empty seats are filled by rivals"));
			Add(TEXT("Weather"), EMenuAction::Weather, TEXT("Left / right to change"), WeatherName, true);
			Add(TEXT("Mode"), EMenuAction::Mode, TEXT("Elimination: last place is out every lap"), RaceState && RaceState->bElimination ? TEXT("Elimination") : TEXT("Race"), true);
			Add(TEXT("Track"), EMenuAction::Track, TEXT("The bay clockwise or anticlockwise"), RaceState && RaceState->bReverse ? TEXT("Reverse") : TEXT("Bay circuit"), true);
		Add(TEXT("Laps"), EMenuAction::Laps, TEXT("Left / right to change"), FString::FromInt(RaceState ? RaceState->TotalLaps : 2), true);
			Add(TEXT("Rivals"), EMenuAction::Rivals, TEXT("Computer boats fill the empty seats"), RivalName, true);
		}
		if (bHost && bRacing)
		{
			Add(TEXT("Restart race"), EMenuAction::Restart, TEXT("Everyone back to the grid"));
		}
		if (bRacing)
		{
			Add(TEXT("Back to last gate"), EMenuAction::ResetBoat, TEXT("Stuck? Start again from the last gate"));
		}
		else
		{
			Add(TEXT("Garage"), EMenuAction::Garage, TEXT("Boats, paint, nitrous and upgrades"), Prefs ? FString::Printf(TEXT("%d coins"), Prefs->Coins) : FString());
		}
		Add(TEXT("Leaderboard"), EMenuAction::Leaderboard, TEXT("Your best ten races on this machine"));
		Add(TEXT("Settings"), EMenuAction::Settings, TEXT("Camera, steering, graphics, sound"));
		Add(TEXT("Controls"), EMenuAction::Controls, TEXT("Keys and gamepad"));
		Add(GetNetMode() == NM_Standalone ? TEXT("Main menu") : (bHost ? TEXT("End race for everyone") : TEXT("Leave race")),
			GetNetMode() == NM_Standalone ? EMenuAction::BackToMenu : EMenuAction::Leave);
		Add(TEXT("Quit game"), EMenuAction::Quit);
		break;
	case EMenuScreen::Settings:
		Add(TEXT("Camera"), EMenuAction::Camera, TEXT("C cycles it while racing"), Prefs ? UBoatPrefs::CameraName(Prefs->GetCamera()) : TEXT("Chase"), true);
		Add(TEXT("Steering sensitivity"), EMenuAction::Steering, TEXT("Lower is gentler"), Percent(Prefs ? Prefs->SteeringSensitivity : 1.f), true);
		Add(TEXT("Graphics quality"), EMenuAction::Graphics, TEXT("Lower runs faster on weak graphics"), UBoatPrefs::GraphicsName(Prefs ? Prefs->GraphicsQuality : -1), true);
		Add(TEXT("Controller rumble"), EMenuAction::Rumble, TEXT("Gamepad vibration"), Prefs && !Prefs->bRumble ? TEXT("Off") : TEXT("On"), true);
		Add(TEXT("Music"), EMenuAction::Music, TEXT("Left / right to change"), Audio && Audio->IsMusicOn() ? TEXT("On") : TEXT("Off"), true);
		Add(TEXT("Music volume"), EMenuAction::MusicVolume, TEXT("Left / right to change"), Percent(Audio ? Audio->MusicVolume : 0.45f), true);
		Add(TEXT("Sound effects volume"), EMenuAction::EffectsVolume, TEXT("Left / right to change"), Percent(Audio ? Audio->EffectsVolume : 1.f), true);
		Add(TEXT("Back"), EMenuAction::Back);
		break;
	case EMenuScreen::Garage:
	{
		if (Prefs == nullptr)
		{
			Add(TEXT("Back"), EMenuAction::Back);
			break;
		}
		const FBoatClass& Boat = BoatGarage::GetBoat(GarageBoat);
		const bool bOwned = Prefs->OwnsBoat(GarageBoat);
		auto PriceTag = [&](bool bHave, bool bFitted, int32 Price)
		{
			return bFitted ? FString(TEXT("fitted")) : (bHave ? FString(TEXT("owned")) : FString::Printf(TEXT("%d c"), Price));
		};
		Add(TEXT("Boat"), EMenuAction::GarageBoat,
			bOwned ? FString::Printf(TEXT("%s  ENTER to race it"), Boat.Blurb) : FString::Printf(TEXT("%s  ENTER buys it"), Boat.Blurb),
			FString::Printf(TEXT("%s  %s"), Boat.Name, *PriceTag(bOwned, bOwned && Prefs->SelectedBoat == GarageBoat, Boat.Price)), true);
		const bool bPaintOwned = Prefs->OwnsPaint(GaragePaint);
		Add(TEXT("Paint"), EMenuAction::GaragePaint, bPaintOwned ? TEXT("Topsides, stripes and helmets") : TEXT("ENTER buys this colour"),
			FString::Printf(TEXT("%s  %s"), BoatGarage::PaintName(GaragePaint), *PriceTag(bPaintOwned, bPaintOwned && Prefs->SelectedPaint == GaragePaint, BoatGarage::PaintPrice)), true);
		const bool bFlameOwned = Prefs->OwnsFlame(GarageFlame);
		Add(TEXT("Nitrous flame"), EMenuAction::GarageFlame, bFlameOwned ? TEXT("The colour of the boost flames") : TEXT("ENTER buys this flame"),
			FString::Printf(TEXT("%s  %s"), BoatGarage::FlameName(GarageFlame), *PriceTag(bFlameOwned, bFlameOwned && Prefs->SelectedFlame == GarageFlame, BoatGarage::FlamePrice)), true);
		for (int32 Index = 0; Index < static_cast<int32>(EUpgrade::Count); ++Index)
		{
			const EUpgrade Upgrade = static_cast<EUpgrade>(Index);
			const int32 Level = Prefs->GetLevel(GarageBoat, Upgrade);
			FString Pips;
			for (int32 Pip = 1; Pip <= BoatGarage::MaxLevel; ++Pip)
			{
				Pips += Pip <= Level ? TEXT("|") : TEXT(".");
			}
			const FString Value = Level >= BoatGarage::MaxLevel ? FString::Printf(TEXT("[%s]  max"), *Pips)
				: FString::Printf(TEXT("[%s]  %d c"), *Pips, BoatGarage::UpgradeCost(Level + 1));
			Add(FString::Printf(TEXT("%s upgrade"), BoatGarage::UpgradeName(Upgrade)), EMenuAction::GarageUpgrade,
				bOwned ? FString::Printf(TEXT("%s.  ENTER buys the next level"), BoatGarage::UpgradeBlurb(Upgrade))
					: FString(TEXT("Buy the boat first")),
				Value, false, true, Index);
		}
		Add(TEXT("Back"), EMenuAction::Back, FString::Printf(TEXT("Wallet: %d coins"), Prefs->Coins));
		break;
	}
	case EMenuScreen::Leaderboard:
	{
		const TArray<UBoatPrefs::FBoardRow> Board = Prefs ? Prefs->GetBoard() : TArray<UBoatPrefs::FBoardRow>();
		static const TCHAR* const Places[] = { TEXT("DNF"), TEXT("1st"), TEXT("2nd"), TEXT("3rd"), TEXT("4th") };
		for (int32 Index = 0; Index < Board.Num(); ++Index)
		{
			const UBoatPrefs::FBoardRow& Row = Board[Index];
			const int32 Seconds = FMath::FloorToInt(Row.Seconds);
			Add(FString::Printf(TEXT("%2d.  %s   %s"), Index + 1, *Row.Name.Left(14), BoatGarage::GetBoat(Row.Boat).Name), EMenuAction::None,
				FString(), FString::Printf(TEXT("%s of %d   %d:%02d   %s   %d pts"), Places[FMath::Clamp(Row.Place, 0, 4)], Row.Racers,
					Seconds / 60, Seconds % 60, Row.bReverse ? TEXT("rev") : TEXT("bay"), Row.Score), false, false);
		}
		if (Board.Num() == 0)
		{
			Add(TEXT("No races yet: finish one to get on the board"), EMenuAction::None, FString(), FString(), false, false);
		}
		if (Prefs != nullptr)
		{
			Add(TEXT("Career"), EMenuAction::None, FString(), FString::Printf(TEXT("%d races   %d wins   %d coins earned"),
				Prefs->RacesRun, Prefs->RacesWon, Prefs->CoinsEarned), false, false);
		}
		Add(TEXT("Back"), EMenuAction::Back);
		break;
	}
	case EMenuScreen::Controls:
	{
		static const TCHAR* const Rows[][2] =
		{
			{ TEXT("Throttle / reverse"), TEXT("W / S    RT / LT") },
			{ TEXT("Steer"), TEXT("A / D    Left stick") },
			{ TEXT("Boost"), TEXT("SHIFT    RB") },
			{ TEXT("Camera view / look back"), TEXT("C / B    R3 / LB") },
			{ TEXT("Torpedo"), TEXT("SPACE / Left mouse    A") },
			{ TEXT("Mine"), TEXT("Q / Right mouse    X") },
			{ TEXT("Homing missile"), TEXT("E    B") },
			{ TEXT("Flares (break a missile lock)"), TEXT("F    D-pad down") },
			{ TEXT("Grenade (co-driver throws)"), TEXT("G    D-pad up") },
			{ TEXT("Back to last gate"), TEXT("R    Y") },
			{ TEXT("Pause menu"), TEXT("ESC    Menu button") },
			{ TEXT("Start race (host)"), TEXT("ENTER    Start") },
			{ TEXT("Change weather (host)"), TEXT("TAB") },
			{ TEXT("Music on / off"), TEXT("M") },
		};
		for (const auto& Row : Rows)
		{
			Add(Row[0], EMenuAction::None, FString(), Row[1], false, false);
		}
		Add(TEXT("Back"), EMenuAction::Back);
		break;
	}
	default:
		break;
	}
}

void ABoatRacePlayerController::HandleMenuInput()
{
	TArray<FMenuItem> Items;
	BuildMenuItems(Items);
	if (Items.Num() == 0)
	{
		return;
	}
	auto Selectable = [&Items](int32 Index) { return Items.IsValidIndex(Index) && Items[Index].bEnabled; };
	MenuSelection = FMath::Clamp(MenuSelection, 0, Items.Num() - 1);
	if (!Selectable(MenuSelection))
	{
		for (int32 Index = 0; Index < Items.Num(); ++Index)
		{
			if (Selectable(Index)) { MenuSelection = Index; break; }
		}
	}

	auto Move = [&](int32 Step)
	{
		for (int32 Tries = 0; Tries < Items.Num(); ++Tries)
		{
			MenuSelection = (MenuSelection + Step + Items.Num()) % Items.Num();
			if (Selectable(MenuSelection))
			{
				BoatAudio::Play2D(this, EBoatSound::Beep, 0.15f, 2.2f);
				return;
			}
		}
	};
	if (WasInputKeyJustPressed(EKeys::Up) || WasInputKeyJustPressed(EKeys::W) || WasInputKeyJustPressed(EKeys::Gamepad_DPad_Up))
	{
		Move(-1);
	}
	if (WasInputKeyJustPressed(EKeys::Down) || WasInputKeyJustPressed(EKeys::S) || WasInputKeyJustPressed(EKeys::Gamepad_DPad_Down))
	{
		Move(1);
	}

	// The mouse: hovering selects, clicking chooses (right-click steps a value back).
	if (const ABoatRaceHUD* RaceHud = GetHUD<ABoatRaceHUD>())
	{
		float MouseX = 0.f, MouseY = 0.f;
		if (GetMousePosition(MouseX, MouseY))
		{
			const int32 Hovered = RaceHud->HitTestMenu(FVector2D(MouseX, MouseY));
			const bool bMoved = !FVector2D(MouseX, MouseY).Equals(LastMouse, 0.5f);
			LastMouse = FVector2D(MouseX, MouseY);
			if (Hovered >= 0 && Selectable(Hovered) && bMoved)
			{
				MenuSelection = Hovered;
			}
			if (Hovered >= 0 && Selectable(Hovered))
			{
				if (WasInputKeyJustPressed(EKeys::LeftMouseButton))
				{
					MenuSelection = Hovered;
					ActivateMenuItem(Items[Hovered], 1);
					return;
				}
				if (WasInputKeyJustPressed(EKeys::RightMouseButton) && Items[Hovered].bAdjustable)
				{
					ActivateMenuItem(Items[Hovered], -1);
					return;
				}
			}
		}
	}

	const FMenuItem& Selected = Items[MenuSelection];
	if (Selected.bAdjustable)
	{
		if (WasInputKeyJustPressed(EKeys::Left) || WasInputKeyJustPressed(EKeys::A) || WasInputKeyJustPressed(EKeys::Gamepad_DPad_Left))
		{
			ActivateMenuItem(Selected, -1);
		}
		if (WasInputKeyJustPressed(EKeys::Right) || WasInputKeyJustPressed(EKeys::D) || WasInputKeyJustPressed(EKeys::Gamepad_DPad_Right))
		{
			ActivateMenuItem(Selected, 1);
		}
	}
	else
	{
		// The title screen's tiles run left to right; in the garage, left and
		// right on any line flick through the boats.
		const int32 Step = (WasInputKeyJustPressed(EKeys::Right) || WasInputKeyJustPressed(EKeys::D) || WasInputKeyJustPressed(EKeys::Gamepad_DPad_Right)) ? 1
			: (WasInputKeyJustPressed(EKeys::Left) || WasInputKeyJustPressed(EKeys::A) || WasInputKeyJustPressed(EKeys::Gamepad_DPad_Left)) ? -1 : 0;
		if (Step != 0 && MenuScreen == EMenuScreen::Main && IsOnMainMenu())
		{
			Move(Step);
		}
		else if (Step != 0 && MenuScreen == EMenuScreen::Garage)
		{
			GarageBoat = (GarageBoat + Step + BoatGarage::NumBoats()) % BoatGarage::NumBoats();
			BoatAudio::Play2D(this, EBoatSound::Beep, 0.2f, 1.8f);
		}
	}
	if (WasInputKeyJustPressed(EKeys::Enter) || WasInputKeyJustPressed(EKeys::SpaceBar) || WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Bottom))
	{
		ActivateMenuItem(Selected, 1);
	}

	// The old one-key shortcuts still work on the title screen.
	if (MenuScreen == EMenuScreen::Main)
	{
		if (WasInputKeyJustPressed(EKeys::H))
		{
			HostRace();
		}
		else if (WasInputKeyJustPressed(EKeys::J))
		{
			bTypingAddress = true;
		}
		else if (WasInputKeyJustPressed(EKeys::Tab))
		{
			ServerRequestWeather(1);
		}
		else
		{
			JoinFoundRaceOnKeyPress();
		}
	}
}

void ABoatRacePlayerController::ActivateMenuItem(const FMenuItem& Item, int32 Step)
{
	if (!Item.bEnabled)
	{
		return;
	}
	UBoatAudioSubsystem* Audio = UBoatAudioSubsystem::Get(this);
	BoatAudio::Play2D(this, EBoatSound::Beep, 0.3f, 1.3f);
	switch (Item.Action)
	{
	case EMenuAction::Solo:
	case EMenuAction::StartRace:
		ServerRequestStart();
		OpenMenu(EMenuScreen::None);
		break;
	case EMenuAction::TimeTrial:
		ServerRequestTimeTrial();
		OpenMenu(EMenuScreen::None);
		break;
	case EMenuAction::Host:
		HostRace();
		break;
	case EMenuAction::JoinFound:
		if (const ULanDiscovery* Discovery = GetGameInstance() ? GetGameInstance()->GetSubsystem<ULanDiscovery>() : nullptr)
		{
			if (Discovery->GetFoundRaces().IsValidIndex(Item.Data))
			{
				JoinRace(Discovery->GetFoundRaces()[Item.Data].Address);
			}
		}
		break;
	case EMenuAction::JoinAddress:
		bTypingAddress = true;
		break;
	case EMenuAction::Weather:
		ServerRequestWeather(Step >= 0 ? 1 : -1);
		break;
	case EMenuAction::Laps:
		ServerRaceOption(0, Step >= 0 ? 1 : -1);
		break;
	case EMenuAction::Rivals:
		ServerRaceOption(1, Step >= 0 ? 1 : -1);
		break;
	case EMenuAction::Mode:
		ServerRaceOption(2, 1);
		break;
	case EMenuAction::Track:
		ServerRaceOption(3, 1);
		break;
	case EMenuAction::Camera:
		if (UBoatPrefs* Prefs = UBoatPrefs::Get(this)) { Prefs->CycleCamera(Step >= 0 ? 1 : -1); }
		break;
	case EMenuAction::Steering:
		if (UBoatPrefs* Prefs = UBoatPrefs::Get(this)) { Prefs->SetSteering(Prefs->SteeringSensitivity + 0.1f * Step); }
		break;
	case EMenuAction::Graphics:
		if (UBoatPrefs* Prefs = UBoatPrefs::Get(this)) { Prefs->SetGraphics(((Prefs->GraphicsQuality + 1 + Step) % 5 + 5) % 5 - 1); }
		break;
	case EMenuAction::Rumble:
		if (UBoatPrefs* Prefs = UBoatPrefs::Get(this)) { Prefs->bRumble = !Prefs->bRumble; Prefs->Persist(); }
		break;
	case EMenuAction::Music:
		if (Audio) { Audio->ToggleMusic(); }
		break;
	case EMenuAction::MusicVolume:
		if (Audio) { Audio->SetMusicVolume(Audio->MusicVolume + 0.1f * Step); }
		break;
	case EMenuAction::EffectsVolume:
		if (Audio) { Audio->SetEffectsVolume(Audio->EffectsVolume + 0.1f * Step); }
		break;
	case EMenuAction::Settings:
		OpenMenu(EMenuScreen::Settings);
		break;
	case EMenuAction::Garage:
		if (const UBoatPrefs* Prefs = UBoatPrefs::Get(this))
		{
			GarageBoat = Prefs->OwnsBoat(Prefs->SelectedBoat) ? Prefs->SelectedBoat : 0;
			GaragePaint = Prefs->SelectedPaint;
			GarageFlame = Prefs->SelectedFlame;
		}
		OpenMenu(EMenuScreen::Garage);
		break;
	case EMenuAction::Leaderboard:
		OpenMenu(EMenuScreen::Leaderboard);
		break;
	case EMenuAction::GarageBoat:
	case EMenuAction::GaragePaint:
	case EMenuAction::GarageFlame:
	{
		UBoatPrefs* Prefs = UBoatPrefs::Get(this);
		if (Prefs == nullptr)
		{
			break;
		}
		int32& Browse = Item.Action == EMenuAction::GarageBoat ? GarageBoat : (Item.Action == EMenuAction::GaragePaint ? GaragePaint : GarageFlame);
		const int32 Count = Item.Action == EMenuAction::GarageBoat ? BoatGarage::NumBoats()
			: (Item.Action == EMenuAction::GaragePaint ? BoatGarage::NumPaints() : BoatGarage::NumFlames());
		const bool bChoose = WasInputKeyJustPressed(EKeys::Enter) || WasInputKeyJustPressed(EKeys::SpaceBar)
			|| WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Bottom) || WasInputKeyJustPressed(EKeys::LeftMouseButton);
		if (!bChoose)
		{
			Browse = (Browse + Step + Count) % Count; // left / right: look at the next one
			break;
		}
		const int32 Coins = Prefs->Coins;
		const bool bDone = Item.Action == EMenuAction::GarageBoat ? Prefs->BuyBoat(Browse)
			: (Item.Action == EMenuAction::GaragePaint ? Prefs->BuyPaint(Browse) : Prefs->BuyFlame(Browse));
		if (!bDone)
		{
			GarageSay(TEXT("Not enough coins yet: win races and collect coins on the course"), false);
		}
		else if (Prefs->Coins < Coins)
		{
			GarageSay(FString::Printf(TEXT("Bought for %d coins"), Coins - Prefs->Coins), true);
		}
		else
		{
			GarageSay(TEXT("Fitted"), true);
		}
		break;
	}
	case EMenuAction::GarageUpgrade:
		if (UBoatPrefs* Prefs = UBoatPrefs::Get(this))
		{
			const EUpgrade Upgrade = static_cast<EUpgrade>(FMath::Clamp(Item.Data, 0, 3));
			const int32 Coins = Prefs->Coins;
			if (!Prefs->OwnsBoat(GarageBoat))
			{
				GarageSay(TEXT("Buy the boat before tuning it"), false);
			}
			else if (Prefs->GetLevel(GarageBoat, Upgrade) >= BoatGarage::MaxLevel)
			{
				GarageSay(TEXT("Already at the top level"), false);
			}
			else if (Prefs->BuyUpgrade(GarageBoat, Upgrade))
			{
				GarageSay(FString::Printf(TEXT("%s level %d for %d coins"), BoatGarage::UpgradeName(Upgrade),
					Prefs->GetLevel(GarageBoat, Upgrade), Coins - Prefs->Coins), true);
			}
			else
			{
				GarageSay(TEXT("Not enough coins yet: win races and collect coins on the course"), false);
			}
		}
		break;
	case EMenuAction::Controls:
		OpenMenu(EMenuScreen::Controls);
		break;
	case EMenuAction::Resume:
		OpenMenu(EMenuScreen::None);
		break;
	case EMenuAction::PhotoMode:
		OpenMenu(EMenuScreen::None);
		EnterPhotoMode();
		break;
	case EMenuAction::Restart:
		ServerRequestRestart();
		OpenMenu(EMenuScreen::None);
		break;
	case EMenuAction::ResetBoat:
		ServerRequestReset();
		OpenMenu(EMenuScreen::None);
		break;
	case EMenuAction::Leave:
		LeaveRace();
		break;
	case EMenuAction::BackToMenu:
		OpenMenu(EMenuScreen::None);
		UGameplayStatics::OpenLevel(this, FName(RaceMap));
		break;
	case EMenuAction::Quit:
		ConsoleCommand(TEXT("quit"));
		break;
	case EMenuAction::Back:
		OpenMenu(MenuParent);
		break;
	default:
		break;
	}
}

void ABoatRacePlayerController::JoinFoundRaceOnKeyPress()
{
	const ULanDiscovery* Discovery = GetGameInstance() ? GetGameInstance()->GetSubsystem<ULanDiscovery>() : nullptr;
	if (Discovery == nullptr)
	{
		return;
	}

	const FKey NumberKeys[][2] =
	{
		{ EKeys::One, EKeys::NumPadOne },
		{ EKeys::Two, EKeys::NumPadTwo },
		{ EKeys::Three, EKeys::NumPadThree },
		{ EKeys::Four, EKeys::NumPadFour },
	};
	const TArray<FFoundRace>& Races = Discovery->GetFoundRaces();
	for (int32 Index = 0; Index < FMath::Min(Races.Num(), BoatTrack::MaxPlayers); ++Index)
	{
		if (WasInputKeyJustPressed(NumberKeys[Index][0]) || WasInputKeyJustPressed(NumberKeys[Index][1]))
		{
			JoinRace(Races[Index].Address);
			return;
		}
	}
}

void ABoatRacePlayerController::HandleAddressTyping()
{
	if (WasInputKeyJustPressed(EKeys::Escape))
	{
		bTypingAddress = false;
		return;
	}
	if (WasInputKeyJustPressed(EKeys::BackSpace))
	{
		if (TypedAddress.Len() > 0)
		{
			TypedAddress.LeftChopInline(1);
		}
		return;
	}
	if (WasInputKeyJustPressed(EKeys::Enter))
	{
		if (TypedAddress.Len() > 0)
		{
			bTypingAddress = false;
			JoinRace(TypedAddress);
		}
		return;
	}

	for (const FTypedKey& Typed : GetAddressKeys())
	{
		if (WasInputKeyJustPressed(Typed.Key) && TypedAddress.Len() < MaxAddressLength)
		{
			TypedAddress.AppendChar(Typed.Character);
		}
	}
}

void ABoatRacePlayerController::HostRace()
{
	// Reload the (empty) map as a listen server. This machine now plays and
	// accepts up to three other players on UDP port 7777.
	UE_LOG(LogBoatRace, Log, TEXT("Hosting a race."));
	UGameplayStatics::OpenLevel(this, FName(RaceMap), true, TEXT("listen"));
}

void ABoatRacePlayerController::JoinRace(const FString& Address)
{
	UE_LOG(LogBoatRace, Log, TEXT("Joining %s"), *Address);
	ClientTravel(Address, TRAVEL_Absolute);
}

void ABoatRacePlayerController::LeaveRace()
{
	// Back to the stand-alone menu. If this machine was the host, the race ends for everyone.
	ClientTravel(RaceMap, TRAVEL_Absolute);
}

void ABoatRacePlayerController::ServerRequestStart_Implementation()
{
	UWorld* World = GetWorld();
	if (ABoatRaceGameMode* RaceMode = World ? World->GetAuthGameMode<ABoatRaceGameMode>() : nullptr)
	{
		RaceMode->RequestStartRace(this);
	}
}

void ABoatRacePlayerController::EnterPhotoMode()
{
	UWorld* World = GetWorld();
	if (World == nullptr || bPhotoMode)
	{
		return;
	}
	FVector Location;
	FRotator Rotation;
	GetPlayerViewPoint(Location, Rotation);
	if (PhotoCamera == nullptr)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		PhotoCamera = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), Location, Rotation, Params);
	}
	if (PhotoCamera == nullptr)
	{
		return;
	}
	PhotoCamera->SetActorLocationAndRotation(Location, Rotation);
	PhotoCamera->GetCameraComponent()->SetFieldOfView(70.f);
	PhotoCamera->GetCameraComponent()->bConstrainAspectRatio = false;
	// Sharp stills: no motion blur (the jump from the boat camera reads as
	// huge motion), and no film grain.
	FPostProcessSettings& Post = PhotoCamera->GetCameraComponent()->PostProcessSettings;
	Post.bOverride_MotionBlurAmount = true;
	Post.MotionBlurAmount = 0.f;
	Post.bOverride_FilmGrainIntensity = true;
	Post.FilmGrainIntensity = 0.f;
	PhotoCamera->GetCameraComponent()->PostProcessBlendWeight = 1.f;
	SetViewTarget(PhotoCamera);
	bPhotoMode = true;
	PhotoClock = 100.f;
	// Freeze the moment when racing alone; others cannot be paused. The
	// controller has to keep ticking while paused, or the camera would freeze too.
	bShouldPerformFullTickWhenPaused = true;
	if (GetNetMode() == NM_Standalone)
	{
		UGameplayStatics::SetGamePaused(this, true);
	}
}

void ABoatRacePlayerController::ExitPhotoMode()
{
	bPhotoMode = false;
	if (GetNetMode() == NM_Standalone)
	{
		UGameplayStatics::SetGamePaused(this, false);
	}
	if (GetPawn() != nullptr)
	{
		SetViewTarget(GetPawn());
	}
}

void ABoatRacePlayerController::UpdatePhotoMode(float DeltaTime)
{
	PhotoClock += DeltaTime;
	if (PhotoCamera == nullptr || WasInputKeyJustPressed(EKeys::Escape) || WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Right))
	{
		ExitPhotoMode();
		return;
	}
	// Mouse or right stick to look; WASD or left stick to fly; Q/E down and up.
	float LookX = 0.f, LookY = 0.f;
	GetInputMouseDelta(LookX, LookY);
	LookX += GetInputAnalogKeyState(EKeys::Gamepad_RightX) * 120.f * DeltaTime;
	LookY += GetInputAnalogKeyState(EKeys::Gamepad_RightY) * 120.f * DeltaTime;
	FRotator Rotation = PhotoCamera->GetActorRotation();
	Rotation.Yaw += LookX * 0.6f;
	Rotation.Pitch = FMath::Clamp(Rotation.Pitch + LookY * 0.6f, -85.f, 85.f);
	Rotation.Roll = 0.f;

	FVector Move = FVector::ZeroVector;
	Move.X += (IsInputKeyDown(EKeys::W) ? 1.f : 0.f) - (IsInputKeyDown(EKeys::S) ? 1.f : 0.f) + GetInputAnalogKeyState(EKeys::Gamepad_LeftY);
	Move.Y += (IsInputKeyDown(EKeys::D) ? 1.f : 0.f) - (IsInputKeyDown(EKeys::A) ? 1.f : 0.f) + GetInputAnalogKeyState(EKeys::Gamepad_LeftX);
	Move.Z += (IsInputKeyDown(EKeys::E) ? 1.f : 0.f) - (IsInputKeyDown(EKeys::Q) ? 1.f : 0.f);
	const float Speed = (IsInputKeyDown(EKeys::LeftShift) ? 6000.f : 1500.f) * DeltaTime;
	FVector Location = PhotoCamera->GetActorLocation()
		+ (Rotation.Vector() * Move.X + FRotationMatrix(Rotation).GetUnitAxis(EAxis::Y) * Move.Y + FVector::UpVector * Move.Z) * Speed;
	Location.Z = FMath::Max(Location.Z, 60.0); // never under the water
	PhotoCamera->SetActorLocationAndRotation(Location, Rotation);

	if (WasInputKeyJustPressed(EKeys::F12) || WasInputKeyJustPressed(EKeys::Enter) || WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Bottom))
	{
		FScreenshotRequest::RequestScreenshot(FString::Printf(TEXT("BoatRace_Photo_%03d"), ++PhotoCount), false, true);
		PhotoClock = 0.f;
		BoatAudio::Play2D(this, EBoatSound::Beep, 0.4f, 2.f);
	}
}

void ABoatRacePlayerController::ServerRequestTimeTrial_Implementation()
{
	UWorld* World = GetWorld();
	if (ABoatRaceGameMode* RaceMode = World ? World->GetAuthGameMode<ABoatRaceGameMode>() : nullptr)
	{
		RaceMode->RequestStartRace(this, false, true);
	}
}

float ABoatRacePlayerController::GetGhostLapTime() const
{
	if (!BestGhost.IsValid())
	{
		TSharedPtr<FGhostLap> Loaded = MakeShared<FGhostLap>();
		if (Loaded->Load())
		{
			const_cast<ABoatRacePlayerController*>(this)->BestGhost = Loaded;
		}
	}
	return BestGhost.IsValid() ? BestGhost->LapTime : 0.f;
}

bool ABoatRacePlayerController::GetGhostDelta(float& OutDelta, float& OutAge) const
{
	OutDelta = GhostDelta;
	OutAge = GetWorld() ? static_cast<float>(GetWorld()->GetTimeSeconds()) - GhostDeltaAt : 100.f;
	return bGhostDeltaValid;
}

void ABoatRacePlayerController::UpdateTimeTrial(float DeltaTime)
{
	UWorld* World = GetWorld();
	const ABoatRaceGameState* RaceState = World ? World->GetGameState<ABoatRaceGameState>() : nullptr;
	const ABoatRacePlayerState* Racer = GetPlayerState<ABoatRacePlayerState>();
	const APawn* Boat = GetPawn();
	const bool bActive = RaceState && RaceState->bTimeTrial && RaceState->RaceState == ERaceState::Racing && Racer && Boat;
	if (!bActive)
	{
		GhostSeenLaps = -1;
		Recording.Reset();
		if (Ghost != nullptr)
		{
			Ghost->Show(nullptr, 0.f);
		}
		return;
	}
	GetGhostLapTime(); // loads the saved ghost the first time
	if (Ghost == nullptr)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Ghost = World->SpawnActor<AGhostBoat>(AGhostBoat::StaticClass(), FTransform::Identity, Params);
	}

	const float Now = RaceState->GetRaceTime();
	// A new lap begins each time the start line is crossed.
	if (Racer->LapsCompleted != GhostSeenLaps)
	{
		if (Recording.IsValid() && GhostSeenLaps >= 0 && Racer->LapsCompleted > GhostSeenLaps)
		{
			Recording->LapTime = Now - GhostLapStart;
			const bool bBetter = !BestGhost.IsValid() || Recording->LapTime < BestGhost->LapTime;
			if (bBetter && Recording->IsValid())
			{
				BestGhost = Recording;
				BestGhost->Save();
				UE_LOG(LogBoatRace, Log, TEXT("New ghost saved: lap of %.2f s, %d samples."), BestGhost->LapTime, BestGhost->Times.Num());
			}
		}
		GhostSeenLaps = Racer->LapsCompleted;
		GhostLapStart = Now;
		GhostSeenGate = Racer->NextGate;
		GhostRecordClock = 0.f;
		Recording = MakeShared<FGhostLap>();
		Recording->Splits.Init(-1.f, BoatTrack::NumGates());
	}
	const float IntoLap = Now - GhostLapStart;

	// Record ten times a second.
	GhostRecordClock -= DeltaTime;
	if (Recording.IsValid() && GhostRecordClock <= 0.f && Racer->LapsCompleted >= 0)
	{
		GhostRecordClock = 0.1f;
		Recording->Times.Add(IntoLap);
		Recording->Locations.Add(Boat->GetActorLocation());
		Recording->Rotations.Add(Boat->GetActorRotation());
	}
	// Splits: ahead or behind the ghost at every gate.
	if (Racer->NextGate != GhostSeenGate)
	{
		const int32 Passed = GhostSeenGate;
		GhostSeenGate = Racer->NextGate;
		if (Recording.IsValid() && Recording->Splits.IsValidIndex(Passed))
		{
			Recording->Splits[Passed] = IntoLap;
		}
		if (BestGhost.IsValid() && BestGhost->Splits.IsValidIndex(Passed) && BestGhost->Splits[Passed] >= 0.f && Passed != 0)
		{
			GhostDelta = IntoLap - BestGhost->Splits[Passed];
			GhostDeltaAt = static_cast<float>(World->GetTimeSeconds());
			bGhostDeltaValid = true;
		}
	}
	if (Ghost != nullptr)
	{
		Ghost->Show(Racer->LapsCompleted >= 0 && BestGhost.IsValid() ? BestGhost.Get() : nullptr, IntoLap);
	}
}

void ABoatRacePlayerController::ServerRequestRestart_Implementation()
{
	UWorld* World = GetWorld();
	if (ABoatRaceGameMode* RaceMode = World ? World->GetAuthGameMode<ABoatRaceGameMode>() : nullptr)
	{
		RaceMode->RequestStartRace(this, true);
	}
}

void ABoatRacePlayerController::ServerRaceOption_Implementation(int32 Option, int32 Step)
{
	UWorld* World = GetWorld();
	if (ABoatRaceGameMode* RaceMode = World ? World->GetAuthGameMode<ABoatRaceGameMode>() : nullptr)
	{
		RaceMode->RequestRaceOption(this, FMath::Clamp(Option, 0, 3), FMath::Clamp(Step, -1, 1));
	}
}

void ABoatRacePlayerController::ServerRequestWeather_Implementation(int32 Step)
{
	UWorld* World = GetWorld();
	if (ABoatRaceGameMode* RaceMode = World ? World->GetAuthGameMode<ABoatRaceGameMode>() : nullptr)
	{
		RaceMode->RequestWeatherChange(this, FMath::Clamp(Step, -1, 1));
	}
}

void ABoatRacePlayerController::RunPhotoTour(float DeltaTime)
{
	// Starts a race with rivals, photographs the grid under the start lights,
	// then visits a different stretch of the course in each kind of weather,
	// and quits. Screenshots land in Saved/Screenshots.
	UWorld* World = GetWorld();
	ABoatRaceGameMode* RaceMode = World ? World->GetAuthGameMode<ABoatRaceGameMode>() : nullptr;
	ABoatRaceGameState* RaceState = World ? World->GetGameState<ABoatRaceGameState>() : nullptr;
	ABoatPawn* Boat = Cast<ABoatPawn>(GetPawn());
	if (RaceMode == nullptr || RaceState == nullptr || Boat == nullptr)
	{
		return;
	}
	const int32 Weathers = static_cast<int32>(ERaceWeather::Count);
	// First the title screen, photographed, then the race.
	static bool bMenuShot = false;
	if (TourClock < 0.f)
	{
		TourClock -= DeltaTime;
		MenuScreen = EMenuScreen::Main;
		if (TourClock < -3.f && !bMenuShot)
		{
			bMenuShot = true;
			FScreenshotRequest::RequestScreenshot(TEXT("Tour_menu"), true, false);
		}
		if (TourClock < -4.f)
		{
			MenuScreen = EMenuScreen::None;
			TourClock = 0.f;
			RaceMode->RequestStartRace(this);
		}
		return;
	}
	TourClock += DeltaTime;
	// One stop shows the pause menu over the race.
	MenuScreen = (TourStep == Weathers + 1) ? EMenuScreen::Pause : EMenuScreen::None;

	const float StepSeconds = 7.f;
	const int32 Step = FMath::FloorToInt((TourClock - 2.f) / StepSeconds);
	if (Step != TourStep && TourClock > 2.f)
	{
		TourStep = Step;
		if (Step == 0)
		{
			FScreenshotRequest::RequestScreenshot(TEXT("Tour_00_grid"), true, false);
			return;
		}
		// The last stop is a beauty shot: sunny, camera swung round to the bow.
		Boat->SetShowcaseCamera(Step == Weathers + 3);
		if (Step > Weathers + 3)
		{
			ConsoleCommand(TEXT("quit"));
			return;
		}
		// Weather can only change between races, so do it as the server would.
		RaceState->SetWeather(Step == Weathers + 3 ? ERaceWeather::Sunny : RaceWeather::FromIndex((Step - 1) % Weathers));
		const int32 Gates = BoatTrack::NumGates();
		const FTransform Spot = BoatTrack::GetGateResetTransform((Step * 5) % Gates);
		Boat->TeleportBoat(Spot.GetLocation(), static_cast<float>(Spot.Rotator().Yaw));
	}
	// The last two stops set off a blast ahead of the boat to show the explosion.
	static int32 BlastStep = -1;
	const float IntoStep = TourClock - 2.f - TourStep * StepSeconds;
	if (TourStep > Weathers && TourStep <= Weathers + 2 && TourStep != BlastStep && IntoStep > (TourStep == Weathers + 1 ? 3.5f : 3.f))
	{
		BlastStep = TourStep;
		BoatBlast::Detonate(World, Boat->GetActorLocation() + Boat->GetActorForwardVector() * 3500.f, 900.f, 0.f, 0.f, Boat);
	}

	// Shoot halfway through each step, once the boat has settled and moved off.
	static int32 ShotStep = -1;
	if (TourStep > 0 && TourStep != ShotStep && TourClock - 2.f - TourStep * StepSeconds > 4.f)
	{
		ShotStep = TourStep;
		const FString Name = TourStep <= Weathers
			? FString::Printf(TEXT("Tour_%02d_%s"), TourStep, RaceWeather::Get(RaceState->GetWeather()).Name)
			: FString::Printf(TEXT("Tour_%02d_extra"), TourStep);
		// With the HUD, so the text, markers and menus are checked too.
		FScreenshotRequest::RequestScreenshot(Name, true, false);
		const FVector Where = Boat->GetActorLocation();
		UE_LOG(LogBoatRace, Log, TEXT("Photo tour: %s, %.0f fps; boat %.0f km/h, throttle %.2f, ground %.0f cm, at %.0f %.0f %.0f"),
			*Name, 1.f / FMath::Max(DeltaTime, 0.001f), Boat->GetSpeedKmh(), Boat->GetThrottleInput(),
			BayTerrain::GetHeight(FVector2D(Where.X, Where.Y)), Where.X, Where.Y, Where.Z);
	}
}

void ABoatRacePlayerController::ServerRequestReset_Implementation()
{
	UWorld* World = GetWorld();
	if (ABoatRaceGameMode* RaceMode = World ? World->GetAuthGameMode<ABoatRaceGameMode>() : nullptr)
	{
		RaceMode->RequestResetBoat(this);
	}
}

bool ABoatRacePlayerController::IsReplaying() const
{
	return Replay != nullptr && Replay->IsPlaying();
}

void ABoatRacePlayerController::UpdateReplay(float DeltaTime)
{
	const ABoatRaceGameState* RaceState = GetWorld() ? GetWorld()->GetGameState<ABoatRaceGameState>() : nullptr;
	if (RaceState == nullptr)
	{
		return;
	}
	if (Replay == nullptr)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Replay = GetWorld()->SpawnActor<AFinishReplay>(AFinishReplay::StaticClass(), FTransform::Identity, Params);
		if (Replay == nullptr)
		{
			return;
		}
	}

	const uint8 Phase = static_cast<uint8>(RaceState->RaceState);
	const bool bEntered = Phase != ReplaySeenState;
	ReplaySeenState = Phase;

	switch (RaceState->RaceState)
	{
	case ERaceState::Countdown:
		if (bEntered)
		{
			Replay->Clear();
			ScoreCard = FScoreCard();
			if (CourseCoins != nullptr) { CourseCoins->ResetRace(); }
		}
		break;
	case ERaceState::Racing:
		Replay->Record(DeltaTime);
		for (APlayerState* Player : RaceState->PlayerArray)
		{
			const ABoatRacePlayerState* Racer = Cast<ABoatRacePlayerState>(Player);
			if (Racer != nullptr && Racer->bFinished && Racer->FinishPosition == 1)
			{
				Replay->MarkWin();
			}
		}
		break;
	case ERaceState::Results:
		// Nothing to replay of a lone time trial; and a photo stays a photo.
		if (bEntered)
		{
			BuildScoreCard();
		}
		if (bEntered && !RaceState->bTimeTrial && !bPhotoMode)
		{
			Replay->Record(0.f);
			Replay->Play(this);
		}
		else if (Replay->IsPlaying())
		{
			if (bPhotoMode || MenuScreen != EMenuScreen::None || WasInputKeyJustPressed(EKeys::Enter)
				|| WasInputKeyJustPressed(EKeys::SpaceBar) || WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Bottom))
			{
				Replay->Stop();
			}
			else
			{
				Replay->Advance(DeltaTime);
			}
		}
		break;
	default:
		Replay->Stop();
		break;
	}
}

// ---------------------------------------------------------------------------
// Garage, coins and the score card
// ---------------------------------------------------------------------------
void ABoatRacePlayerController::ServerSetLoadout_Implementation(FBoatLoadout Loadout)
{
	ABoatRacePlayerState* Racer = GetPlayerState<ABoatRacePlayerState>();
	const ABoatRaceGameState* RaceState = GetWorld() ? GetWorld()->GetGameState<ABoatRaceGameState>() : nullptr;
	if (Racer == nullptr)
	{
		return;
	}
	// Mid-race, a boat stays the boat it started with; the first loadout of
	// someone who joined late is still taken.
	if (Racer->bLoadoutChosen && RaceState != nullptr && RaceState->RaceState == ERaceState::Racing)
	{
		return;
	}
	Racer->Loadout = BoatGarage::Sanitise(Loadout);
	Racer->bLoadoutChosen = true;
}

void ABoatRacePlayerController::SendLoadout(const FBoatLoadout& Loadout)
{
	if (bLoadoutSent && Loadout == SentLoadout)
	{
		return;
	}
	bLoadoutSent = true;
	SentLoadout = Loadout;
	ServerSetLoadout(Loadout);
}

FBoatLoadout ABoatRacePlayerController::GetGarageLoadout() const
{
	const UBoatPrefs* Prefs = UBoatPrefs::Get(this);
	return Prefs ? Prefs->GetLoadoutFor(GarageBoat, GaragePaint, GarageFlame) : FBoatLoadout();
}

const FString& ABoatRacePlayerController::GetGarageMessage(float& OutAge) const
{
	OutAge = GetWorld() ? static_cast<float>(GetWorld()->GetRealTimeSeconds()) - GarageMessageAt : 100.f;
	return GarageMessage;
}

void ABoatRacePlayerController::GarageSay(const FString& Text, bool bGood)
{
	GarageMessage = Text;
	GarageMessageAt = GetWorld() ? static_cast<float>(GetWorld()->GetRealTimeSeconds()) : 0.f;
	if (bGood && !Text.StartsWith(TEXT("Fitted")))
	{
		BoatAudio::Play2D(this, EBoatSound::Purchase, 0.6f);
	}
	else if (!bGood)
	{
		BoatAudio::Play2D(this, EBoatSound::Beep, 0.35f, 0.5f);
	}
}

int32 ABoatRacePlayerController::GetRaceCoins() const
{
	return CourseCoins != nullptr ? CourseCoins->GetCollected() : 0;
}

void ABoatRacePlayerController::UpdateGarage(float DeltaTime)
{
	UWorld* World = GetWorld();
	const UBoatPrefs* Prefs = UBoatPrefs::Get(this);
	if (World == nullptr || Prefs == nullptr)
	{
		return;
	}
	// -NoCoins (a test hook) leaves them out, to measure what they cost.
	if (CourseCoins == nullptr && !FParse::Param(FCommandLine::Get(), TEXT("NoCoins")))
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		CourseCoins = World->SpawnActor<ACourseCoins>(ACourseCoins::StaticClass(), FTransform::Identity, Params);
	}

	const ABoatRaceGameState* RaceState = World->GetGameState<ABoatRaceGameState>();
	const bool bRacing = RaceState && (RaceState->RaceState == ERaceState::Countdown || RaceState->RaceState == ERaceState::Racing);
	if (MenuScreen == EMenuScreen::Garage && bRacing)
	{
		OpenMenu(EMenuScreen::None); // the lights are on: out of the garage
	}

	// The boat raced is always what is owned; the boathouse shows the rest.
	if (GetPlayerState<ABoatRacePlayerState>() != nullptr)
	{
		SendLoadout(Prefs->GetLoadout());
	}

	if (Showroom == nullptr)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Showroom = World->SpawnActor<AGarageShowroom>(AGarageShowroom::StaticClass(), FTransform::Identity, Params);
	}

	// The title screen and the garage look into the boathouse.
	const bool bInGarage = MenuScreen == EMenuScreen::Garage;
	const bool bOnTitle = IsOnMainMenu() && MenuScreen == EMenuScreen::Main && !bTypingAddress;
	const bool bShowroomView = Showroom != nullptr && Showroom->IsReady() && (bInGarage || bOnTitle);
	if (Showroom != nullptr)
	{
		Showroom->SetActive(bShowroomView);
	}
	if (bShowroomView)
	{
		const ABoatRacePlayerState* Me = GetPlayerState<ABoatRacePlayerState>();
		Showroom->ShowLoadout(bInGarage ? GetGarageLoadout() : Prefs->GetLoadout(), Me ? Me->SlotIndex : 0);
		if (GarageCamera == nullptr)
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			GarageCamera = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity, Params);
			if (GarageCamera != nullptr)
			{
				GarageCamera->GetCameraComponent()->bConstrainAspectRatio = false;
			}
		}
		if (GarageCamera != nullptr)
		{
			GarageCamera->GetCameraComponent()->SetFieldOfView(bInGarage ? 55.f : 62.f);
			GarageCamera->SetActorTransform(Showroom->GetCameraTransform(bInGarage, static_cast<float>(World->GetRealTimeSeconds())));
			if (!bGarageView)
			{
				bGarageView = true;
				SetViewTargetWithBlend(GarageCamera, 0.f);
			}
		}
	}
	else if (bGarageView)
	{
		bGarageView = false;
		if (APawn* Boat = GetPawn())
		{
			SetViewTargetWithBlend(Boat, 0.f);
		}
	}
}

bool ABoatRacePlayerController::IsShowroomView() const
{
	return bGarageView;
}

void ABoatRacePlayerController::BuildScoreCard()
{
	ScoreCard = FScoreCard();
	const UWorld* World = GetWorld();
	const ABoatRaceGameState* RaceState = World ? World->GetGameState<ABoatRaceGameState>() : nullptr;
	const ABoatRacePlayerState* Me = GetPlayerState<ABoatRacePlayerState>();
	UBoatPrefs* Prefs = UBoatPrefs::Get(this);
	if (RaceState == nullptr || Me == nullptr || Prefs == nullptr || Me->SlotIndex < 0)
	{
		return;
	}
	int32 Racers = 0;
	for (const APlayerState* Player : RaceState->PlayerArray)
	{
		const ABoatRacePlayerState* Racer = Cast<ABoatRacePlayerState>(Player);
		Racers += Racer && Racer->SlotIndex >= 0 ? 1 : 0;
	}

	static const TCHAR* const Ordinals[] = { TEXT("1st"), TEXT("2nd"), TEXT("3rd"), TEXT("4th") };
	const int32 Place = Me->FinishPosition;
	const bool bPlaced = Place >= 1 && (Me->bFinished || Me->bEliminated);
	auto Line = [this](const FString& Text, int32 Coins) { ScoreCard.Lines.Emplace(Text, Coins); ScoreCard.Coins += Coins; };

	if (bPlaced)
	{
		Line(Racers > 1 ? FString::Printf(TEXT("Finished %s of %d"), Ordinals[FMath::Clamp(Place - 1, 0, 3)], Racers)
			: FString(TEXT("Time trial lap")), BoatGarage::CoinsForPlace(Place, Racers));
	}
	else
	{
		Line(TEXT("Did not finish"), 0);
	}
	const int32 Picked = GetRaceCoins();
	Line(FString::Printf(TEXT("Coins collected  x%d"), Picked), Picked * BoatGarage::CoinValue);
	Line(FString::Printf(TEXT("Hits on rivals  x%d"), Me->Hits), Me->Hits * 10);
	const bool bClean = Me->Wrecks == 0 && bPlaced;
	if (bClean)
	{
		Line(TEXT("Clean race: never wrecked"), 30);
	}
	else if (Me->Wrecks > 0)
	{
		Line(FString::Printf(TEXT("Wrecked  x%d"), Me->Wrecks), 0);
	}

	static const int32 PlacePoints[] = { 1000, 700, 450, 250 };
	ScoreCard.Score = (bPlaced && Racers > 1 ? PlacePoints[FMath::Clamp(Place - 1, 0, 3)] : 0) + Picked * 10 + Me->Hits * 60
		+ (bClean ? 250 : 0) - Me->Wrecks * 80;
	ScoreCard.Score = FMath::Max(ScoreCard.Score, 0);

	// Test runs (the autopilot, the photo tour) drive themselves: they show a
	// card but must not pay into, or rank on, the player's own save.
	const bool bTestRun = FParse::Param(FCommandLine::Get(), TEXT("Autopilot")) || FParse::Param(FCommandLine::Get(), TEXT("PhotoTour"));
	if (bTestRun)
	{
		ScoreCard.Rank = -1;
	}
	else if (RaceState->bTimeTrial || Racers <= 1)
	{
		// Alone there is nobody to beat; the ghost keeps the records.
		Prefs->Coins += ScoreCard.Coins;
		Prefs->CoinsEarned += ScoreCard.Coins;
		Prefs->Persist();
	}
	else
	{
		ScoreCard.Rank = Prefs->RecordRace(ScoreCard.Score, Me->GetPlayerName(), Me->Loadout.Boat, bPlaced ? Place : 0, Racers,
			Me->bFinished ? Me->FinishTime : 0.f, ScoreCard.Coins);
	}
	ScoreCard.Wallet = Prefs->Coins;
	ScoreCard.bValid = true;
	UE_LOG(LogBoatRace, Log, TEXT("Score card: %d points, %d coins (%d picked up), board rank %d, wallet %d"),
		ScoreCard.Score, ScoreCard.Coins, Picked, ScoreCard.Rank, ScoreCard.Wallet);
}
