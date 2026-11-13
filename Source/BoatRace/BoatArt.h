#pragma once

#include "CoreMinimal.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UMeshComponent;
class UStaticMesh;

/**
 * The Blender models (Scripts/make_models.py, imported to /Game/Models) and the
 * rules for colouring them.
 *
 * Every material in those models is named Part_RRGGBB. Rather than trust what
 * an importer made of the materials, the game repaints each slot from that
 * hex colour on the engine's basic shape material. That keeps the look the
 * same whatever imported it, and lets the weather dust things with snow, light
 * the lamps at night and paint each boat and buoy in its own colour.
 *
 * Everything here degrades: a missing model returns null and the caller falls
 * back to its simple shapes.
 */
namespace BoatArt
{
	/** /Game/Models/SM_<Name>, or null when the models have not been imported. */
	UStaticMesh* Model(const TCHAR* Name);

	/** /Game/Models/<Name> (M_Glow, M_Soft, M_VertexColor), or null. */
	UMaterialInterface* Material(const TCHAR* Name);

	/** The engine's basic shape material, which has a "Color" parameter. */
	UMaterialInterface* BasicMaterial();

	/**
	 * Uniform scale that makes a model come out at its Blender size, whatever
	 * unit the importer chose. ModelHeightMetres is the height make_models.py
	 * printed for it. Multiply by any further size you want.
	 */
	float UnitScale(const UStaticMesh* Mesh, float ModelHeightMetres);

	/** How a repaint should treat the slots. */
	struct FLook
	{
		/** Colour for Paint_* slots; alpha 0 keeps the model's own. */
		FLinearColor Paint = FLinearColor(0.f, 0.f, 0.f, 0.f);
		/** 0 none .. 1 a full winter coat on foliage, roofs, grass and rock. */
		float Snow = 0.f;
		/** Windows glow; Paint_* slots glow softly too (pylons at night). */
		bool bNight = false;
		/** Multiplies every colour; the weather uses it to grey things down. */
		float Brightness = 1.f;
	};

	/**
	 * Gives every slot of a mesh component its colour. Returns the materials of
	 * the Paint_* slots, so a caller can re-colour them later (the next gate
	 * pulses). Safe on components with no model.
	 */
	TArray<UMaterialInstanceDynamic*> Paint(UMeshComponent* Component, const FLook& Look = FLook());

	/** A translucent puff material instance (smoke, spray, rain), or the basic
	 *  material when M_Soft is missing. Sets Color, and Opacity if it has one. */
	UMaterialInstanceDynamic* SoftMaterial(UObject* Outer, const FLinearColor& Color, float Opacity);

	/** An unlit glowing material instance, or a bright basic one. */
	UMaterialInstanceDynamic* GlowMaterial(UObject* Outer, const FLinearColor& Color);

	/** Sets Color (and Opacity when it is a soft material) on any of the above. */
	void SetPuff(UMaterialInstanceDynamic* Material, const FLinearColor& Color, float Opacity);

	/** How much light the weather gives unlit puffs: 1 by day, low at night. */
	void SetAmbientLight(float Light);
}
