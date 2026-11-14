#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BoatGarage.h"
#include "BoatShowroom.generated.h"

class UBoxComponent;
class UPointLightComponent;
class USpotLightComponent;
class UStaticMeshComponent;

/**
 * The garage you can see: the team's boathouse (the Blender "Hangar"),
 * standing over the water in a quiet cove away from the course, with the
 * boat being looked at floating in its slip under work lights.
 *
 * Local to each machine. The menus look through it: the title screen from
 * the slip mouth, the garage from the quay as the camera walks round the boat.
 */
UCLASS()
class AGarageShowroom : public AActor
{
	GENERATED_BODY()

public:
	AGarageShowroom();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Lights on and the boat bobbing only while a menu looks in. */
	void SetActive(bool bActive);

	/** Floats this boat, painted, in the slip (cheap if already shown). */
	void ShowLoadout(const FBoatLoadout& Loadout, int32 Slot);

	/** Where to look from: the title screen's view from the slip mouth, or
	 *  the garage's from the quay, drifting round the boat over time. */
	FTransform GetCameraTransform(bool bGarageView, float Seconds) const;

	bool IsReady() const { return bPlaced; }

	static AGarageShowroom* Find(const UWorld* World);

protected:
	UPROPERTY(VisibleAnywhere, Category = "Showroom")
	TObjectPtr<UStaticMeshComponent> Hangar;

	UPROPERTY(VisibleAnywhere, Category = "Showroom")
	TObjectPtr<UStaticMeshComponent> Boat;

	UPROPERTY()
	TArray<TObjectPtr<UPointLightComponent>> WorkLights;

	UPROPERTY()
	TObjectPtr<USpotLightComponent> KeyLight;

	UPROPERTY()
	TArray<TObjectPtr<UBoxComponent>> Walls;

private:
	/** A cove with the bank behind, open water in front and the course well clear. */
	bool FindSpot(FVector2D& OutMouth, FVector2D& OutSeaward) const;
	FVector ToWorld(const FVector& LocalMetres) const;

	FBoatLoadout Shown;
	int32 ShownSlot = -1;
	bool bHasShown = false;
	bool bPlaced = false;
	bool bActive = false;
	/** Where the boat floats, metres in hangar space (+X out to sea). */
	FVector BoatSpot = FVector(-6.5, 0.0, 0.0);
	TWeakObjectPtr<AActor> ClearedTerrain;
	void ClearTerrainProps();
	FVector BoatCentring = FVector::ZeroVector;
	float BoatYaw = 0.f;
};
