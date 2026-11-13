#include "BoatRacePlayerController.h"

#include "BayTerrain.h"
#include "BoatAudio.h"
#include "BoatGhost.h"
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
	if (Screen == EMenuScreen::Settings || Screen == EMenuScreen::Controls)
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
	// A mouse pointer whenever a menu is up, none while driving.
	const bool bWant = IsMenuOpen();
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
		Add(TEXT("Laps"), EMenuAction::Laps, TEXT("Left / right to change"), FString::FromInt(RaceState ? RaceState->TotalLaps : 2), true);
		Add(TEXT("Rivals"), EMenuAction::Rivals, TEXT("Computer boats fill the empty seats"), RivalName, true);
		Add(TEXT("Settings"), EMenuAction::Settings, TEXT("Camera, steering, graphics, sound"));
		Add(TEXT("Controls"), EMenuAction::Controls, TEXT("Keys and gamepad"));
		Add(TEXT("Quit game"), EMenuAction::Quit);
		break;
	}
	case EMenuScreen::Pause:
		Add(TEXT("Resume"), EMenuAction::Resume);
		if (bHost && !bRacing)
		{
			Add(TEXT("Start race"), EMenuAction::StartRace, TEXT("Empty seats are filled by rivals"));
			Add(TEXT("Weather"), EMenuAction::Weather, TEXT("Left / right to change"), WeatherName, true);
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
		if (UBoatPrefs* Prefs = UBoatPrefs::Get(this)) { Prefs->bRumble = !Prefs->bRumble; Prefs->SaveConfig(); }
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
	case EMenuAction::Controls:
		OpenMenu(EMenuScreen::Controls);
		break;
	case EMenuAction::Resume:
		OpenMenu(EMenuScreen::None);
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
		RaceMode->RequestRaceOption(this, FMath::Clamp(Option, 0, 1), FMath::Clamp(Step, -1, 1));
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
