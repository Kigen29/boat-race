#include "../BoatRacePlayerController.h"
#include "../BoatTrack.h"
#include "../BayTerrain.h"
#include "Components/SceneComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoatMenuNavigationTest, "BoatRace.Frontend.BackNavigation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBoatMenuNavigationTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ABoatRacePlayerController* PC = World->SpawnActor<ABoatRacePlayerController>();
	for (EMenuScreen Parent : { EMenuScreen::Main, EMenuScreen::Pause })
	{
		for (EMenuScreen Child : { EMenuScreen::Garage, EMenuScreen::Leaderboard, EMenuScreen::Settings, EMenuScreen::Controls })
		{
			PC->OpenMenu(Parent);
			PC->OpenMenu(Child);
			PC->GoBack();
			TestTrue(FString::Printf(TEXT("Back from screen %d returns to %d"), static_cast<int32>(Child), static_cast<int32>(Parent)), PC->GetMenuScreen() == Parent);
		}
	}
	PC->OpenMenu(EMenuScreen::Pause);
	PC->GoBack();
	TestTrue(TEXT("Back from pause resumes"), PC->GetMenuScreen() == EMenuScreen::None);
	PC->OpenMenu(EMenuScreen::Main);
	PC->bTypingAddress = true;
	PC->TypedAddress = TEXT("127.");
	PC->GoBack();
	TestFalse(TEXT("Back cancels address entry"), PC->IsTypingAddress());
	PC->GoBack();
	TestTrue(TEXT("Main menu remains reachable"), PC->GetMenuScreen() == EMenuScreen::Main);
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoatCircuitTest, "BoatRace.Frontend.Circuits",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBoatCircuitTest::RunTest(const FString& Parameters)
{
	const int32 Previous = BoatTrack::GetCircuit();
	const bool Reverse = BoatTrack::IsReversed();
	TSet<FString> Keys;
	TSet<int32> Lengths;
	for (int32 Circuit = 0; Circuit < BoatTrack::NumCircuits(); ++Circuit)
	{
		BoatTrack::SetCircuit(Circuit);
		for (bool bReverse : { false, true })
		{
			BoatTrack::SetReversed(bReverse);
			Keys.Add(BoatTrack::RecordPrefix());
			Lengths.Add(FMath::RoundToInt(BoatTrack::GetLapLength()));
			TestEqual(TEXT("Every course has sixteen ordered gates"), BoatTrack::NumGates(), 16);
			for (const FBoatGate& Gate : BoatTrack::GetGates())
			{
				const FVector2D Before = Gate.Center - Gate.Forward * 100.f;
				const FVector2D After = Gate.Center + Gate.Forward * 100.f;
				TestTrue(TEXT("Forward crossing scores"), BoatTrack::CrossesGate(Gate, Before, After));
				TestFalse(TEXT("Backward crossing does not score"), BoatTrack::CrossesGate(Gate, After, Before));
				TestTrue(TEXT("Both gate buoys are in water"), BayTerrain::GetHeight(Gate.Inner) < 0.f && BayTerrain::GetHeight(Gate.Outer) < 0.f);
			}
			for (int32 Slot = 0; Slot < BoatTrack::MaxPlayers; ++Slot)
			{
				const FVector Position = BoatTrack::GetGridTransform(Slot).GetLocation();
				TestTrue(TEXT("Both direction grids float in water"), BayTerrain::GetHeight(FVector2D(Position)) < 0.f);
			}
			TestTrue(TEXT("Cumulative distance cache matches selected course"), BoatTrack::GetDistanceToGate(15) < BoatTrack::GetLapLength());
			TestEqual(TEXT("Obstacle cache rebuilt"), BoatTrack::GetRocks().Num(), 9);
		}
	}
	TestEqual(TEXT("Six independent save keys"), Keys.Num(), 6);
	TestEqual(TEXT("Three different circuit lengths"), Lengths.Num(), 3);
	BoatTrack::SetCircuit(0);
	BoatTrack::SetReversed(false);
	TestEqual(TEXT("Original save key is preserved"), BoatTrack::RecordPrefix(), FString());
	BoatTrack::SetCircuit(Previous);
	BoatTrack::SetReversed(Reverse);
	return true;
}
#endif
