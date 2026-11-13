#include "BoatArt.h"

#include "BoatRace.h"

#include "Components/MeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Engine/Texture.h"

namespace
{
	template <typename T>
	T* LoadQuiet(const FString& Path)
	{
		return LoadObject<T>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

	/** The colour and the part name ("Paint", "Leaf") from a slot like Leaf_5CB646. */
	bool ParseSlot(const FString& SlotName, FString& OutPart, FLinearColor& OutColor)
	{
		// Importers sometimes add their own prefix or suffix ("MI_", "_001"), so
		// look for the last run of six hex digits after an underscore.
		int32 Underscore = INDEX_NONE;
		for (int32 Index = SlotName.Len() - 7; Index >= 0; --Index)
		{
			if (SlotName[Index] != TEXT('_'))
			{
				continue;
			}
			bool bHex = true;
			for (int32 Digit = 1; Digit <= 6; ++Digit)
			{
				bHex &= FChar::IsHexDigit(SlotName[Index + Digit]);
			}
			const bool bEnds = (Index + 7 == SlotName.Len()) || !FChar::IsHexDigit(SlotName[Index + 7]);
			if (bHex && bEnds)
			{
				Underscore = Index;
				break;
			}
		}
		if (Underscore == INDEX_NONE)
		{
			return false;
		}

		OutColor = FLinearColor::FromSRGBColor(FColor::FromHex(SlotName.Mid(Underscore + 1, 6)));
		OutPart = SlotName.Left(Underscore);
		int32 Slash = INDEX_NONE;
		if (OutPart.FindLastChar(TEXT('/'), Slash))
		{
			OutPart.RightChopInline(Slash + 1);
		}
		OutPart.RemoveFromStart(TEXT("MI_"));
		OutPart.RemoveFromStart(TEXT("M_"));
		return true;
	}

	/** Which master material a part uses, and how it is set up. */
	enum class EFinish : uint8 { Paint, Livery, Surface, Glass, Foam };

	struct FFinish
	{
		EFinish Kind = EFinish::Surface;
		float Roughness = 0.6f;
		float Metallic = 0.f;
		float Grain = 0.1f;
		float Specular = 0.5f;
	};

	/** The look of a real thing for each part name a model uses. */
	FFinish FinishFor(const FString& Part)
	{
		struct FRow { const TCHAR* Prefix; FFinish Finish; };
		static const FRow Rows[] =
		{
			// Hull graphics, gelcoat and painted parts: glossy, under a clear coat.
			{ TEXT("Livery"),    { EFinish::Livery } },
			{ TEXT("Paint"),     { EFinish::Paint, 0.28f, 0.2f } },
			{ TEXT("Hull"),      { EFinish::Paint, 0.22f, 0.f } },
			{ TEXT("Bottom"),    { EFinish::Paint, 0.3f, 0.f } },
			{ TEXT("Cowl"),      { EFinish::Paint, 0.25f, 0.25f } },
			{ TEXT("Console"),   { EFinish::Paint, 0.25f, 0.f } },
			{ TEXT("Coaming"),   { EFinish::Paint, 0.25f, 0.f } },
			{ TEXT("Cabin"),     { EFinish::Paint, 0.25f, 0.f } },
			{ TEXT("Top"),       { EFinish::Paint, 0.3f, 0.f } },
			{ TEXT("Body"),      { EFinish::Paint, 0.35f, 0.3f } },
			{ TEXT("Nose"),      { EFinish::Paint, 0.3f, 0.2f } },
			{ TEXT("Fin"),       { EFinish::Paint, 0.35f, 0.2f } },
			{ TEXT("Duck"),      { EFinish::Paint, 0.35f, 0.f } },
			{ TEXT("Beak"),      { EFinish::Paint, 0.35f, 0.f } },
			{ TEXT("Pontoon"),   { EFinish::Paint, 0.45f, 0.f } },
			{ TEXT("Band"),      { EFinish::Paint, 0.35f, 0.f } },
			{ TEXT("Sponsor"),   { EFinish::Paint, 0.4f, 0.f } },
			{ TEXT("Decal"),     { EFinish::Paint, 0.3f, 0.f } },
			{ TEXT("Stripe"),    { EFinish::Paint, 0.35f, 0.f } },
			{ TEXT("Hatch"),     { EFinish::Paint, 0.35f, 0.f } },
			{ TEXT("Radar"),     { EFinish::Paint, 0.3f, 0.f } },
			{ TEXT("Box"),       { EFinish::Surface, 0.55f, 0.35f, 0.25f } },
			{ TEXT("Steel"),     { EFinish::Surface, 0.45f, 0.4f, 0.15f } },
			{ TEXT("Tower"),     { EFinish::Surface, 0.4f, 0.6f, 0.05f } },
			{ TEXT("Funnel"),    { EFinish::Surface, 0.5f, 0.3f, 0.1f } },
			// Bright metal: stainless rails, props, cleats, trusses.
			{ TEXT("Chrome"),    { EFinish::Surface, 0.12f, 1.f, 0.f } },
			{ TEXT("Prop"),      { EFinish::Surface, 0.18f, 1.f, 0.f } },
			{ TEXT("Rail"),      { EFinish::Surface, 0.2f, 1.f, 0.f } },
			{ TEXT("Horn"),      { EFinish::Surface, 0.3f, 1.f, 0.f } },
			{ TEXT("Hanger"),    { EFinish::Surface, 0.3f, 1.f, 0.f } },
			{ TEXT("Rib"),       { EFinish::Surface, 0.3f, 1.f, 0.05f } },
			{ TEXT("Mast"),      { EFinish::Surface, 0.25f, 1.f, 0.f } },
			{ TEXT("Truss"),     { EFinish::Surface, 0.35f, 1.f, 0.05f } },
			{ TEXT("Frame"),     { EFinish::Surface, 0.35f, 0.3f, 0.05f } },
			// Glass and water.
			{ TEXT("Glass"),     { EFinish::Glass } },
			{ TEXT("Window"),    { EFinish::Glass } },
			{ TEXT("Spray"),     { EFinish::Foam } },
			{ TEXT("Foam"),      { EFinish::Foam } },
			// Upholstery, rubber and moulded plastic.
			{ TEXT("Seat"),      { EFinish::Surface, 0.65f, 0.f, 0.12f, 0.35f } },
			{ TEXT("Cushion"),   { EFinish::Surface, 0.65f, 0.f, 0.12f, 0.35f } },
			{ TEXT("Collar"),    { EFinish::Surface, 0.8f, 0.f, 0.05f, 0.3f } },
			{ TEXT("Engine"),    { EFinish::Surface, 0.4f, 0.f, 0.02f } },
			{ TEXT("Leg"),       { EFinish::Surface, 0.4f, 0.1f, 0.02f } },
			{ TEXT("Bracket"),   { EFinish::Surface, 0.45f, 0.1f, 0.05f } },
			{ TEXT("Dash"),      { EFinish::Surface, 0.5f, 0.f, 0.03f } },
			{ TEXT("Wheel"),     { EFinish::Surface, 0.55f, 0.f, 0.03f } },
			{ TEXT("Trim"),      { EFinish::Surface, 0.5f, 0.f, 0.03f } },
			{ TEXT("Shell"),     { EFinish::Surface, 0.45f, 0.4f, 0.1f } },
			{ TEXT("Panel"),     { EFinish::Surface, 0.5f, 0.f, 0.03f } },
			{ TEXT("Sole"),      { EFinish::Surface, 0.75f, 0.f, 0.2f } },
			{ TEXT("Deck"),      { EFinish::Surface, 0.7f, 0.f, 0.25f } },
			// Natural and built things: rough, with grain.
			{ TEXT("Rock"),      { EFinish::Surface, 0.9f, 0.f, 0.6f } },
			{ TEXT("Stone"),     { EFinish::Surface, 0.9f, 0.f, 0.55f } },
			{ TEXT("Concrete"),  { EFinish::Surface, 0.9f, 0.f, 0.35f } },
			{ TEXT("Brick"),     { EFinish::Surface, 0.9f, 0.f, 0.45f } },
			{ TEXT("Wall"),      { EFinish::Surface, 0.85f, 0.f, 0.2f } },
			{ TEXT("Gable"),     { EFinish::Surface, 0.85f, 0.f, 0.2f } },
			{ TEXT("Roof"),      { EFinish::Surface, 0.6f, 0.f, 0.25f } },
			{ TEXT("Road"),      { EFinish::Surface, 0.9f, 0.f, 0.4f } },
			{ TEXT("Plank"),     { EFinish::Surface, 0.8f, 0.f, 0.5f } },
			{ TEXT("Post"),      { EFinish::Surface, 0.8f, 0.f, 0.4f } },
			{ TEXT("Wood"),      { EFinish::Surface, 0.8f, 0.f, 0.5f } },
			{ TEXT("Bark"),      { EFinish::Surface, 0.95f, 0.f, 0.6f } },
			{ TEXT("Leaf"),      { EFinish::Surface, 0.8f, 0.f, 0.45f, 0.35f } },
			{ TEXT("Needle"),    { EFinish::Surface, 0.85f, 0.f, 0.45f, 0.35f } },
			{ TEXT("Frond"),     { EFinish::Surface, 0.75f, 0.f, 0.35f, 0.35f } },
			{ TEXT("Grass"),     { EFinish::Surface, 0.9f, 0.f, 0.5f } },
			{ TEXT("Moss"),      { EFinish::Surface, 0.9f, 0.f, 0.5f } },
			{ TEXT("Canvas"),    { EFinish::Surface, 0.8f, 0.f, 0.15f } },
			{ TEXT("Ice"),       { EFinish::Surface, 0.15f, 0.f, 0.15f } },
			{ TEXT("Snow"),      { EFinish::Surface, 0.7f, 0.f, 0.2f } },
		};
		for (const FRow& Row : Rows)
		{
			if (Part.StartsWith(Row.Prefix))
			{
				return Row.Finish;
			}
		}
		return FFinish();
	}

	/** How much a part shows snow: foliage and roofs a lot, rock a little. */
	float SnowAffinity(const FString& Part)
	{
		static const TPair<const TCHAR*, float> Table[] =
		{
			{ TEXT("Leaf"), 0.55f }, { TEXT("Needle"), 0.5f }, { TEXT("Frond"), 0.4f },
			{ TEXT("Grass"), 0.9f }, { TEXT("Moss"), 0.85f }, { TEXT("Roof"), 0.85f },
			{ TEXT("Canopy"), 0.6f }, { TEXT("Rock"), 0.3f }, { TEXT("Stone"), 0.35f },
			{ TEXT("Deck"), 0.25f }, { TEXT("Plank"), 0.5f }, { TEXT("Snow"), 0.f },
		};
		for (const TPair<const TCHAR*, float>& Entry : Table)
		{
			if (Part.StartsWith(Entry.Key))
			{
				return Entry.Value;
			}
		}
		return 0.f;
	}
}

UStaticMesh* BoatArt::Model(const TCHAR* Name)
{
	static TMap<FString, TWeakObjectPtr<UStaticMesh>> Cache;
	const FString Key(Name);
	if (const TWeakObjectPtr<UStaticMesh>* Found = Cache.Find(Key))
	{
		if (Found->IsValid())
		{
			return Found->Get();
		}
	}
	UStaticMesh* Mesh = LoadQuiet<UStaticMesh>(FString::Printf(TEXT("/Game/Models/SM_%s.SM_%s"), Name, Name));
	Cache.Add(Key, Mesh);
	return Mesh;
}

UMaterialInterface* BoatArt::Material(const TCHAR* Name)
{
	return LoadQuiet<UMaterialInterface>(FString::Printf(TEXT("/Game/Models/%s.%s"), Name, Name));
}

UMaterialInterface* BoatArt::BasicMaterial()
{
	static TWeakObjectPtr<UMaterialInterface> Basic;
	if (!Basic.IsValid())
	{
		Basic = LoadQuiet<UMaterialInterface>(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	}
	return Basic.Get();
}

float BoatArt::UnitScale(const UStaticMesh* Mesh, float ModelHeightMetres)
{
	if (Mesh == nullptr || ModelHeightMetres <= 0.f)
	{
		return 1.f;
	}
	const float Height = static_cast<float>(Mesh->GetBounds().BoxExtent.Z) * 2.f;
	return Height > 1.f ? (ModelHeightMetres * 100.f) / Height : 1.f;
}

TArray<UMaterialInstanceDynamic*> BoatArt::Paint(UMeshComponent* Component, const FLook& Look)
{
	TArray<UMaterialInstanceDynamic*> PaintSlots;
	if (Component == nullptr)
	{
		return PaintSlots;
	}

	UMaterialInterface* Basic = BasicMaterial();
	UMaterialInterface* Glow = Material(TEXT("M_Glow"));
	const TArray<FName> SlotNames = Component->GetMaterialSlotNames();
	const int32 Count = Component->GetNumMaterials();

	for (int32 Slot = 0; Slot < Count; ++Slot)
	{
		FString Part;
		FLinearColor Color;
		bool bParsed = SlotNames.IsValidIndex(Slot) && ParseSlot(SlotNames[Slot].ToString(), Part, Color);
		if (!bParsed)
		{
			const UMaterialInterface* Current = Component->GetMaterial(Slot);
			bParsed = Current != nullptr && ParseSlot(Current->GetName(), Part, Color);
		}
		if (!bParsed || Basic == nullptr)
		{
			continue; // Not one of ours: leave whatever the importer made.
		}

		const bool bPaint = Part.StartsWith(TEXT("Paint"));
		if (bPaint && Look.Paint.A > 0.f)
		{
			Color = FLinearColor(Look.Paint.R, Look.Paint.G, Look.Paint.B, 1.f);
		}

		const bool bLamp = Part.StartsWith(TEXT("Lamp")) || Part.StartsWith(TEXT("Glow"));
		const bool bWindowLit = Look.bNight && Part.StartsWith(TEXT("Window"));
		const bool bPaintLit = Look.bNight && bPaint;

		UMaterialInstanceDynamic* Instance = nullptr;
		const FFinish Finish = FinishFor(Part);
		const bool bLivery = Finish.Kind == EFinish::Livery;
		if (bLivery && Look.Paint.A > 0.f)
		{
			Color = FLinearColor(Look.Paint.R, Look.Paint.G, Look.Paint.B, 1.f);
		}
		if ((bLamp || bWindowLit || (bPaintLit && !bLivery)) && Glow != nullptr)
		{
			Instance = UMaterialInstanceDynamic::Create(Glow, Component);
			const float Strength = bLamp ? 5.f : (bWindowLit ? 3.f : 0.9f);
			Instance->SetVectorParameterValue(TEXT("Color"), Color * Strength);
		}
		else
		{
			const float Snow = FMath::Clamp(Look.Snow * SnowAffinity(Part), 0.f, 1.f);
			Color = FMath::Lerp(Color, FLinearColor(0.86f, 0.9f, 0.96f), Snow) * Look.Brightness;

			// The realistic master materials when import_assets.sh has built
			// them; the engine's basic shape material otherwise.
			static const TCHAR* const Names[] = { TEXT("M_Paint"), TEXT("M_Livery"), TEXT("M_Surface"), TEXT("M_Glass"), TEXT("M_Foam") };
			UMaterialInterface* Master = Material(Names[static_cast<int32>(Finish.Kind)]);
			if (Finish.Kind == EFinish::Livery && Master == nullptr)
			{
				Master = Material(TEXT("M_Paint"));
			}
			Instance = UMaterialInstanceDynamic::Create(Master ? Master : Basic, Component);
			switch (Finish.Kind)
			{
			case EFinish::Glass:
				Instance->SetVectorParameterValue(TEXT("Color"), Color * 0.6f);
				Instance->SetScalarParameterValue(TEXT("Opacity"), 0.45f);
				break;
			case EFinish::Foam:
				Instance->SetVectorParameterValue(TEXT("Color"), Color);
				Instance->SetScalarParameterValue(TEXT("Opacity"), 0.f);
				break;
			case EFinish::Livery:
				Instance->SetVectorParameterValue(TEXT("Color"), Color);
				if (Part.StartsWith(TEXT("LiveryCruise")))
				{
					if (UTexture* Cruise = LoadQuiet<UTexture>(TEXT("/Game/Models/T_LiveryCruise.T_LiveryCruise")))
					{
						Instance->SetTextureParameterValue(TEXT("Mask"), Cruise);
					}
					Instance->SetVectorParameterValue(TEXT("Accent"), FLinearColor(0.01f, 0.02f, 0.06f));
				}
				break;
			default:
				Instance->SetVectorParameterValue(TEXT("Color"), Color);
				Instance->SetScalarParameterValue(TEXT("Roughness"), Finish.Roughness);
				Instance->SetScalarParameterValue(TEXT("Metallic"), Finish.Metallic);
				Instance->SetScalarParameterValue(TEXT("Grain"), Finish.Grain);
				Instance->SetScalarParameterValue(TEXT("Specular"), Finish.Specular);
				break;
			}
		}
		Component->SetMaterial(Slot, Instance);
		if (bPaint || Part.StartsWith(TEXT("Livery")))
		{
			PaintSlots.Add(Instance);
		}
	}
	return PaintSlots;
}

UMaterialInstanceDynamic* BoatArt::SoftMaterial(UObject* Outer, const FLinearColor& Color, float Opacity)
{
	UMaterialInterface* Soft = Material(TEXT("M_Soft"));
	UMaterialInterface* Parent = Soft != nullptr ? Soft : BasicMaterial();
	if (Parent == nullptr)
	{
		return nullptr;
	}
	UMaterialInstanceDynamic* Instance = UMaterialInstanceDynamic::Create(Parent, Outer);
	SetPuff(Instance, Color, Opacity);
	return Instance;
}

UMaterialInstanceDynamic* BoatArt::GlowMaterial(UObject* Outer, const FLinearColor& Color)
{
	UMaterialInterface* Glow = Material(TEXT("M_Glow"));
	UMaterialInterface* Parent = Glow != nullptr ? Glow : BasicMaterial();
	if (Parent == nullptr)
	{
		return nullptr;
	}
	UMaterialInstanceDynamic* Instance = UMaterialInstanceDynamic::Create(Parent, Outer);
	Instance->SetVectorParameterValue(TEXT("Color"), Color);
	return Instance;
}

namespace { float GPuffLight = 1.f; }

void BoatArt::SetPuff(UMaterialInstanceDynamic* Material, const FLinearColor& Color, float Opacity)
{
	if (Material == nullptr)
	{
		return;
	}
	// Puffs are unlit, so spray and smoke would glow white in the dark; the
	// weather dims them to the light there is. Flames (colours brighter than
	// white) are their own light and are left alone.
	const bool bFlame = Color.GetMax() > 1.5f;
	const float Light = bFlame ? 1.f : GPuffLight;
	Material->SetVectorParameterValue(TEXT("Color"), FLinearColor(Color.R * Light, Color.G * Light, Color.B * Light, Color.A));
	Material->SetScalarParameterValue(TEXT("Opacity"), FMath::Clamp(Opacity, 0.f, 1.f));
}

void BoatArt::SetAmbientLight(float Light)
{
	GPuffLight = FMath::Clamp(Light, 0.02f, 1.f);
}
