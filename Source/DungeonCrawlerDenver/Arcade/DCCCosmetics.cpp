#include "Arcade/DCCCosmetics.h"

#include "UI/DCCDamageSkins.h"

namespace DCCCosmetics
{
	const FName Head(TEXT("Head"));
	const FName Back(TEXT("Back"));
	const FName Shoulders(TEXT("Shoulders"));
	const FName Aura(TEXT("Aura"));
	const FName Numbers(TEXT("Numbers"));

	namespace
	{
		const FLinearColor Brown(0.32f, 0.17f, 0.07f);
		const FLinearColor DarkBrown(0.12f, 0.06f, 0.03f);
		const FLinearColor Gold(1.0f, 0.75f, 0.2f);
		const FLinearColor White(0.95f, 0.95f, 0.95f);

		TArray<FItem> Build()
		{
			TArray<FItem> Items;
			auto Add = [&Items](const TCHAR* Id, const TCHAR* Name, const TCHAR* Blurb, FName Slot, int32 Price, EAnchor Anchor, TArray<FPart> Parts)
			{
				FItem Item;
				Item.Id = FName(Id);
				Item.Name = Name;
				Item.Blurb = Blurb;
				Item.Slot = Slot;
				Item.Price = Price;
				Item.Anchor = Anchor;
				Item.Parts = MoveTemp(Parts);
				Items.Add(MoveTemp(Item));
			};

			// ---- Head --------------------------------------------------------
			Add(TEXT("PartyHat"), TEXT("Party Hat"), TEXT("For when you summit. Or don't. It's still a party."),
				Head, 40, EAnchor::HeadTop, {
					{ TEXT("Cone"), FVector(22, 22, 30), FVector(0, 0, 15), FRotator::ZeroRotator, FLinearColor(1.0f, 0.3f, 0.75f) },
					{ TEXT("Sphere"), FVector(7, 7, 7), FVector(0, 0, 31), FRotator::ZeroRotator, White },
				});
			Add(TEXT("TrafficCone"), TEXT("Repossessed Traffic Cone"), TEXT("Mile High Recovery Solutions would like it back."),
				Head, 80, EAnchor::HeadTop, {
					{ TEXT("Cone"), FVector(26, 26, 40), FVector(0, 0, 18), FRotator::ZeroRotator, FLinearColor(1.0f, 0.38f, 0.05f) },
					{ TEXT("Cylinder"), FVector(18, 18, 5), FVector(0, 0, 22), FRotator::ZeroRotator, White },
				});
			Add(TEXT("TenGallon"), TEXT("Fort Tiner Ten-Gallon"), TEXT("Holds about two gallons. Nobody at the fort will tell you where the other eight went."),
				Head, 120, EAnchor::HeadTop, {
					{ TEXT("Cylinder"), FVector(62, 62, 3), FVector(0, 0, 1), FRotator::ZeroRotator, Brown },
					{ TEXT("Cylinder"), FVector(29, 29, 19), FVector(0, 0, 11), FRotator::ZeroRotator, Brown },
					{ TEXT("Cylinder"), FVector(30, 30, 4), FVector(0, 0, 4), FRotator::ZeroRotator, DarkBrown },
				});
			Add(TEXT("MinersHelmet"), TEXT("Miner's Helmet"), TEXT("The lamp is on. Dark rooms stop being a guess."),
				Head, 150, EAnchor::HeadTop, {
					{ TEXT("Sphere"), FVector(34, 34, 26), FVector(0, 0, 3), FRotator::ZeroRotator, FLinearColor(1.0f, 0.8f, 0.1f) },
					{ TEXT("Cylinder"), FVector(36, 36, 3), FVector(0, 0, -2), FRotator::ZeroRotator, FLinearColor(0.85f, 0.65f, 0.08f) },
					{ TEXT("Cylinder"), FVector(9, 9, 6), FVector(16, 0, 8), FRotator(90, 0, 0), FLinearColor(0.2f, 0.2f, 0.22f) },
					{ TEXT("Sphere"), FVector(7, 7, 7), FVector(19, 0, 8), FRotator::ZeroRotator, FLinearColor(3.0f, 3.0f, 2.4f) },
				});
			Add(TEXT("FloorBossCrown"), TEXT("Floor Boss Crown"), TEXT("Gold-coloured. The System AI asks that you not test this claim."),
				Head, 400, EAnchor::HeadTop, {
					{ TEXT("Cylinder"), FVector(27, 27, 10), FVector(0, 0, 5), FRotator::ZeroRotator, Gold },
					{ TEXT("Cone"), FVector(7, 7, 10), FVector(12, 0, 14), FRotator::ZeroRotator, Gold },
					{ TEXT("Cone"), FVector(7, 7, 10), FVector(-12, 0, 14), FRotator::ZeroRotator, Gold },
					{ TEXT("Cone"), FVector(7, 7, 10), FVector(0, 12, 14), FRotator::ZeroRotator, Gold },
					{ TEXT("Cone"), FVector(7, 7, 10), FVector(0, -12, 14), FRotator::ZeroRotator, Gold },
					{ TEXT("Sphere"), FVector(5, 5, 5), FVector(13, 0, 6), FRotator::ZeroRotator, FLinearColor(2.0f, 0.1f, 0.2f) },
				});

			// ---- Back --------------------------------------------------------
			Add(TEXT("TrailPack"), TEXT("Trail Backpack"), TEXT("Bedroll on top. Climbing costs less air. The tag was right about that much."),
				Back, 100, EAnchor::UpperBack, {
					{ TEXT("Cube"), FVector(18, 32, 38), FVector(-17, 0, -6), FRotator::ZeroRotator, FLinearColor(0.15f, 0.35f, 0.18f) },
					{ TEXT("Cylinder"), FVector(11, 11, 36), FVector(-16, 0, 17), FRotator(0, 0, 90), FLinearColor(0.15f, 0.25f, 0.6f) },
				});
			Add(TEXT("Cape"), TEXT("Dramatic Cape"), TEXT("Falls hurt less. The drama is unchanged."),
				Back, 200, EAnchor::UpperBack, {
					{ TEXT("Cube"), FVector(3, 44, 78), FVector(-15, 0, -36), FRotator(-8, 0, 0), FLinearColor(0.7f, 0.05f, 0.1f) },
					{ TEXT("Cube"), FVector(6, 46, 6), FVector(-12, 0, 2), FRotator::ZeroRotator, Gold },
				});
			Add(TEXT("HobbyJetpack"), TEXT("Hobby Jetpack"), TEXT("Hold space. It hops, then it hovers until the tanks cough. They are small tanks."),
				Back, 300, EAnchor::UpperBack, {
					{ TEXT("Cylinder"), FVector(13, 13, 36), FVector(-17, 9, -4), FRotator::ZeroRotator, FLinearColor(0.7f, 0.72f, 0.78f) },
					{ TEXT("Cylinder"), FVector(13, 13, 36), FVector(-17, -9, -4), FRotator::ZeroRotator, FLinearColor(0.7f, 0.72f, 0.78f) },
					{ TEXT("Cone"), FVector(11, 11, 9), FVector(-17, 9, -26), FRotator(180, 0, 0), FLinearColor(0.15f, 0.15f, 0.18f) },
					{ TEXT("Cone"), FVector(11, 11, 9), FVector(-17, -9, -26), FRotator(180, 0, 0), FLinearColor(0.15f, 0.15f, 0.18f) },
					{ TEXT("Sphere"), FVector(7, 7, 9), FVector(-17, 9, -32), FRotator::ZeroRotator, FLinearColor(3.0f, 1.0f, 0.2f) },
					{ TEXT("Sphere"), FVector(7, 7, 9), FVector(-17, -9, -32), FRotator::ZeroRotator, FLinearColor(3.0f, 1.0f, 0.2f) },
				});

			// ---- Shoulders (the armour) --------------------------------------
			Add(TEXT("CardboardPlate"), TEXT("Cardboard Plate Armor"), TEXT("Turns aside a little of a hit. Paper cuts remain a risk."),
				Shoulders, 60, EAnchor::UpperBack, {
					{ TEXT("Cube"), FVector(30, 44, 40), FVector(0, 0, -12), FRotator::ZeroRotator, FLinearColor(0.55f, 0.4f, 0.22f) },
					{ TEXT("Cube"), FVector(20, 16, 8), FVector(0, 24, 6), FRotator(0, 0, -20), FLinearColor(0.5f, 0.36f, 0.2f) },
					{ TEXT("Cube"), FVector(20, 16, 8), FVector(0, -24, 6), FRotator(0, 0, 20), FLinearColor(0.5f, 0.36f, 0.2f) },
				});
			Add(TEXT("SpikedPauldrons"), TEXT("Spiked Pauldrons"), TEXT("Turns aside more of a hit. Hugs end quickly."),
				Shoulders, 250, EAnchor::UpperBack, {
					{ TEXT("Sphere"), FVector(20, 20, 16), FVector(0, 22, 6), FRotator::ZeroRotator, FLinearColor(0.35f, 0.35f, 0.38f) },
					{ TEXT("Sphere"), FVector(20, 20, 16), FVector(0, -22, 6), FRotator::ZeroRotator, FLinearColor(0.35f, 0.35f, 0.38f) },
					{ TEXT("Cone"), FVector(6, 6, 13), FVector(0, 24, 18), FRotator(0, 0, 20), FLinearColor(0.85f, 0.85f, 0.9f) },
					{ TEXT("Cone"), FVector(6, 6, 13), FVector(0, -24, 18), FRotator(0, 0, -20), FLinearColor(0.85f, 0.85f, 0.9f) },
				});
			Add(TEXT("PitBossPlate"), TEXT("Pit Boss Plate"), TEXT("Turns aside a lot of a hit. Still warm."),
				Shoulders, 450, EAnchor::UpperBack, {
					{ TEXT("Cube"), FVector(32, 46, 42), FVector(0, 0, -12), FRotator::ZeroRotator, FLinearColor(0.12f, 0.1f, 0.12f) },
					{ TEXT("Cube"), FVector(34, 10, 30), FVector(1, 0, -10), FRotator::ZeroRotator, FLinearColor(1.8f, 0.35f, 0.05f) },
					{ TEXT("Sphere"), FVector(22, 22, 18), FVector(0, 25, 7), FRotator::ZeroRotator, FLinearColor(0.15f, 0.12f, 0.14f) },
					{ TEXT("Sphere"), FVector(22, 22, 18), FVector(0, -25, 7), FRotator::ZeroRotator, FLinearColor(0.15f, 0.12f, 0.14f) },
				});

			// ---- Aura --------------------------------------------------------
			{
				TArray<FPart> Halo;
				for (int32 i = 0; i < 12; ++i)
				{
					const float A = 2.0f * PI * i / 12.0f;
					Halo.Add({ TEXT("Sphere"), FVector(5, 5, 5), FVector(FMath::Cos(A) * 15.0f, FMath::Sin(A) * 15.0f, 16.0f), FRotator::ZeroRotator, FLinearColor(2.5f, 2.0f, 0.6f) });
				}
				Add(TEXT("UsedHalo"), TEXT("Halo (Used)"), TEXT("Stamina comes back a little faster. Previous owner was fine, probably."),
					Aura, 350, EAnchor::HeadTop, MoveTemp(Halo));
			}
			{
				TArray<FPart> Orbit;
				for (int32 i = 0; i < 3; ++i)
				{
					const float A = 2.0f * PI * i / 3.0f;
					Orbit.Add({ TEXT("Sphere"), FVector(8, 8, 8), FVector(FMath::Cos(A) * 28.0f, FMath::Sin(A) * 28.0f, 4.0f), FRotator::ZeroRotator,
						i == 0 ? FLinearColor(0.3f, 2.0f, 2.5f) : i == 1 ? FLinearColor(2.5f, 0.4f, 2.0f) : FLinearColor(2.5f, 2.0f, 0.3f) });
				}
				Add(TEXT("SponsorOrbs"), TEXT("Sponsor Orbs"), TEXT("You move a little quicker. They are watching the pace."),
					Aura, 500, EAnchor::HeadTop, MoveTemp(Orbit));
			}
			// ---- Numbers (2026-11-23): damage skins, nothing worn on the body ----
			for (const DCCDamageSkins::FSkin& Skin : DCCDamageSkins::All())
			{
				Add(*Skin.Id.ToString(), Skin.Name, Skin.Blurb, Numbers, Skin.Price, EAnchor::HeadTop, {});
			}
			return Items;
		}
	}

	const TArray<FItem>& Catalog()
	{
		static const TArray<FItem> Items = Build();
		return Items;
	}

	const FItem* Find(FName Id)
	{
		return Catalog().FindByPredicate([Id](const FItem& Item) { return Item.Id == Id; });
	}

	FString SlotLabel(FName Slot)
	{
		return Slot == Shoulders ? FString(TEXT("Armor")) : Slot.ToString();
	}

	const TArray<FBoothWeapon>& BoothWeapons()
	{
		static const TArray<FBoothWeapon> Rows = {
			{ FName(TEXT("ConfettiCrossbow")), TEXT("Confetti Crossbow"), TEXT("Left click shoots. Right mouse aims. It used to fire confetti. It does not, now."), 80 },
			{ FName(TEXT("NailGunPro")), TEXT("Cordless Nail Gun"), TEXT("Left click shoots. Right mouse aims. Left behind mid-renovation. The renovation was never finished."), 120 },
			{ FName(TEXT("FlareGunEmergency")), TEXT("Emergency Flare Gun"), TEXT("Left click shoots. Right mouse aims. Break glass. The glass has been broken for a while."), 100 },
		};
		return Rows;
	}
}
