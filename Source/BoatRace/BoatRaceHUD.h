#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "BoatRaceHUD.generated.h"

class ABoatPawn;
class ABoatRaceGameState;
class ABoatRacePlayerState;
class UFont;
class UTexture2D;

/**
 * Everything on screen, drawn straight onto the canvas so the project needs no
 * UI assets at all.
 *
 * Laid out like a racing game: standing and lap clock top left, the order with
 * time gaps under it, speed bottom left, hull and weapons bottom right, and a
 * map of the circuit top right.
 */
UCLASS(Config = Game)
class ABoatRaceHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void DrawHUD() override;

	/** Set false in DefaultGame.ini to hide the circuit map. */
	UPROPERTY(Config)
	bool bShowMiniMap = true;

private:
	/** A player's place in the order, with how far behind the leader they are. */
	struct FStanding
	{
		const ABoatRacePlayerState* Player = nullptr;
		const ABoatPawn* Boat = nullptr;
		/** Distance covered along the racing line, cm. */
		float Progress = 0.f;
		/** Seconds behind the leader, estimated. 0 for the leader. */
		float GapSeconds = 0.f;
	};

	void BuildStandings(TArray<FStanding>& OutStandings) const;

	void DrawMenu(float Width, float Height, float Scale);
	void DrawLobby(const ABoatRaceGameState* RaceState, const TArray<FStanding>& Standings, float Width, float Height, float Scale);
	void DrawCountdown(const ABoatRaceGameState* RaceState, float Width, float Height, float Scale);
	void DrawRaceHud(const ABoatRaceGameState* RaceState, const TArray<FStanding>& Standings, float Width, float Height, float Scale);
	void DrawResults(const ABoatRaceGameState* RaceState, const TArray<FStanding>& Standings, float Width, float Height, float Scale);

	void DrawStandingsBlock(const ABoatRaceGameState* RaceState, const TArray<FStanding>& Standings,
		const ABoatRacePlayerState* Me, float X, float Y, float Scale);
	void DrawSpeed(const ABoatPawn* Boat, float Width, float Height, float Scale);
	void DrawStatus(const ABoatPawn* Boat, float Width, float Height, float Scale);
	void DrawMiniMap(const TArray<FStanding>& Standings, const ABoatRacePlayerState* Me, float Width, float Height, float Scale);
	void DrawKillFeed(const ABoatRaceGameState* RaceState, float Width, float Height, float Scale);
	void DrawGateCompass(const ABoatPawn* Boat, const ABoatRacePlayerState* RacePlayer, float Width, float Scale);
	void DrawNameTags(float Scale);
	void DrawDamageEdge(const ABoatPawn* Boat, float Width, float Height);
	void DrawStartLights(const ABoatRaceGameState* RaceState, float Width, float Height, float Scale);
	void DrawBanner(float Width, float Height, float Scale);
	void DrawWrongWay(const ABoatPawn* Boat, const ABoatRacePlayerState* Me, float Width, float Height, float Scale);

	/** Sounds and banners for the moments of a race: lights, laps, places, the flag. */
	void UpdateRaceCues(const ABoatRaceGameState* RaceState, const TArray<FStanding>& Standings);
	void ShowBanner(const FString& Text, const FLinearColor& Color, float Seconds = 2.5f);

	/** Text with a drop shadow. Align: 0 left, 0.5 centred, 1 right of X. */
	void DrawLabel(const FString& Text, float X, float Y, float Size, const FLinearColor& Color,
		float Align = 0.f, bool bLarge = false);
	void DrawBar(float X, float Y, float Width, float Height, float Fraction, const FLinearColor& Color);

	static FString FormatTime(float Seconds);
	static FString FormatGap(float Seconds);
	static FString Ordinal(int32 Number);

	/** Optional Higgsfield art at /Game/Higgsfield/T_Title. */
	UPROPERTY()
	TObjectPtr<UTexture2D> TitleTexture;

	/** Circuit bounds, worked out once for the map. */
	FString BannerText;
	FLinearColor BannerColor = FLinearColor::White;
	float BannerUntil = 0.f;
	int32 SeenLights = -1;
	int32 SeenLaps = -1;
	int32 SeenPosition = 0;
	int32 SeenState = -1;
	bool bSeenFinish = false;
	float WrongWayTime = 0.f;

	FVector2D MapMin = FVector2D::ZeroVector;
	FVector2D MapMax = FVector2D::ZeroVector;
	bool bMapBoundsReady = false;
};
