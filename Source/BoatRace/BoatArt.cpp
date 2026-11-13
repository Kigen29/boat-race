#include "BoatArt.h"

#include "BoatRace.h"

#include "Components/MeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

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
		if ((bLamp || bWindowLit || bPaintLit) && Glow != nullptr)
		{
			Instance = UMaterialInstanceDynamic::Create(Glow, Component);
			const float Strength = bLamp ? 5.f : (bWindowLit ? 3.f : 0.9f);
			Instance->SetVectorParameterValue(TEXT("Color"), Color * Strength);
		}
		else
		{
			const float Snow = FMath::Clamp(Look.Snow * SnowAffinity(Part), 0.f, 1.f);
			Color = FMath::Lerp(Color, FLinearColor(0.86f, 0.9f, 0.96f), Snow) * Look.Brightness;
			Instance = UMaterialInstanceDynamic::Create(Basic, Component);
			Instance->SetVectorParameterValue(TEXT("Color"), Color);
		}
		Component->SetMaterial(Slot, Instance);
		if (bPaint)
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

void BoatArt::SetPuff(UMaterialInstanceDynamic* Material, const FLinearColor& Color, float Opacity)
{
	if (Material == nullptr)
	{
		return;
	}
	Material->SetVectorParameterValue(TEXT("Color"), Color);
	Material->SetScalarParameterValue(TEXT("Opacity"), FMath::Clamp(Opacity, 0.f, 1.f));
}
