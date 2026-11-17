#include "UI/DCCGameLogos.h"

#include "Engine/Texture2D.h"
#include "UObject/StrongObjectPtr.h"

namespace DCCGameLogos
{
	namespace
	{
		// 12x12 glyphs, doubled to 24x24 in the middle of the tile. 'x' is lit.
		const TMap<FName, TArray<const TCHAR*>>& Glyphs()
		{
			static const TMap<FName, TArray<const TCHAR*>> Map = {
				{ TEXT("Crawler"), { // a sword
					TEXT("..........xx"), TEXT(".........xxx"), TEXT("........xxx."), TEXT(".......xxx.."), TEXT("......xxx..."), TEXT(".x...xxx...."),
					TEXT(".xx.xxx....."), TEXT("..xxxx......"), TEXT("...xx......."), TEXT("..x.xx......"), TEXT(".x...xx....."), TEXT("x...........") } },
				{ TEXT("Haunt"), { // a ghost
					TEXT("...xxxxxx..."), TEXT("..xxxxxxxx.."), TEXT(".xxxxxxxxxx."), TEXT(".xx..xx..xx."), TEXT(".xx..xx..xx."), TEXT(".xxxxxxxxxx."),
					TEXT(".xxxxxxxxxx."), TEXT(".xxxx..xxxx."), TEXT(".xxxxxxxxxx."), TEXT(".xxxxxxxxxx."), TEXT(".xx.xx.xx.x."), TEXT(".x..x..x..x.") } },
				{ TEXT("Climb"), { // a peak
					TEXT("............"), TEXT(".....xx....."), TEXT("....xxxx...."), TEXT("...xx..xx..."), TEXT("...xxxxxxx.."), TEXT("..xxxxxxxxx."),
					TEXT("..xxxxxxxxx."), TEXT(".xxxxxxxxxxx"), TEXT(".xxxxxxxxxxx"), TEXT("xxxxxxxxxxxx"), TEXT("............"), TEXT("............") } },
				{ TEXT("Kart"), { // the tow magnet
					TEXT("............"), TEXT("..xx....xx.."), TEXT("..xx....xx.."), TEXT("..........."), TEXT("..xx....xx.."), TEXT("..xx....xx.."),
					TEXT("..xx....xx.."), TEXT("..xxx..xxx.."), TEXT("...xxxxxx..."), TEXT("....xxxx...."), TEXT("............"), TEXT("............") } },
				{ TEXT("Neon"), { // a bolt
					TEXT("......xxx..."), TEXT(".....xxx...."), TEXT("....xxx....."), TEXT("...xxx......"), TEXT("..xxxxxxx..."), TEXT(".....xxx...."),
					TEXT("....xxx....."), TEXT("...xxx......"), TEXT("..xxx......."), TEXT(".xx........."), TEXT("............"), TEXT("............") } },
				{ TEXT("Audit"), { // rising bars
					TEXT("............"), TEXT("..........xx"), TEXT("..........xx"), TEXT(".......xx.xx"), TEXT(".......xx.xx"), TEXT("....xx.xx.xx"),
					TEXT("....xx.xx.xx"), TEXT(".xx.xx.xx.xx"), TEXT(".xx.xx.xx.xx"), TEXT("xxxxxxxxxxxx"), TEXT("............"), TEXT("............") } },
				{ TEXT("Gate"), { // a portal arch
					TEXT("...xxxxxx..."), TEXT("..xx....xx.."), TEXT(".xx.xxxx.xx."), TEXT(".x.xx..xx.x."), TEXT(".x.x....x.x."), TEXT(".x.x....x.x."),
					TEXT(".x.x....x.x."), TEXT(".x.x....x.x."), TEXT(".x.x....x.x."), TEXT("xxxx....xxxx"), TEXT("............"), TEXT("............") } },
			};
			return Map;
		}
	}

	UTexture2D* Logo(FName GameId, const FLinearColor& Colour)
	{
		static TMap<FName, TStrongObjectPtr<UTexture2D>> Cache;
		if (TStrongObjectPtr<UTexture2D>* Found = Cache.Find(GameId))
		{
			return Found->Get();
		}
		const TArray<const TCHAR*>* Glyph = Glyphs().Find(GameId);
		if (!Glyph)
		{
			return nullptr;
		}
		constexpr int32 S = 32;
		UTexture2D* Texture = UTexture2D::CreateTransient(S, S, PF_B8G8R8A8);
		if (!Texture || !Texture->GetPlatformData() || Texture->GetPlatformData()->Mips.IsEmpty())
		{
			return nullptr;
		}
		Texture->Filter = TF_Nearest;
		Texture->SRGB = true;
		const FLinearColor C = Colour.GetClamped();
		const FColor Ink = FMath::Lerp(C, FLinearColor::White, 0.35f).ToFColor(true);
		const FColor Fill = (C * 0.28f + FLinearColor(0.02f, 0.02f, 0.04f)).ToFColor(true);
		const FColor Rim = (C * 0.75f).ToFColor(true);
		TArray<FColor> Pixels;
		Pixels.Init(FColor(0, 0, 0, 0), S * S);
		// A tile with its corners clipped, a rim in the game's colour.
		for (int32 Y = 0; Y < S; ++Y)
		{
			for (int32 X = 0; X < S; ++X)
			{
				const int32 DX = FMath::Min(X, S - 1 - X), DY = FMath::Min(Y, S - 1 - Y);
				if (DX + DY < 3)
				{
					continue; // clipped corner
				}
				Pixels[Y * S + X] = (DX < 2 || DY < 2 || DX + DY < 5) ? Rim : Fill;
			}
		}
		for (int32 GY = 0; GY < Glyph->Num() && GY < 12; ++GY)
		{
			const TCHAR* Row = (*Glyph)[GY];
			for (int32 GX = 0; Row && GX < 12 && Row[GX]; ++GX)
			{
				if (Row[GX] != TEXT('x'))
				{
					continue;
				}
				for (int32 d = 0; d < 4; ++d)
				{
					Pixels[(4 + GY * 2 + d / 2) * S + (4 + GX * 2 + d % 2)] = Ink;
				}
			}
		}
		FTexture2DMipMap& Mip = Texture->GetPlatformData()->Mips[0];
		FMemory::Memcpy(Mip.BulkData.Lock(LOCK_READ_WRITE), Pixels.GetData(), S * S * sizeof(FColor));
		Mip.BulkData.Unlock();
		Texture->UpdateResource();
		Cache.Add(GameId, TStrongObjectPtr<UTexture2D>(Texture));
		return Texture;
	}
}
