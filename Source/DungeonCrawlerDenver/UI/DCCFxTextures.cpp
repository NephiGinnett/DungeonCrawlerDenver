#include "UI/DCCFxTextures.h"

#include "Engine/Texture2D.h"
#include "TextureResource.h"

namespace
{
	UTexture2D* Make(int32 N, TFunctionRef<float(float X, float Y)> Shape)
	{
		UTexture2D* T = UTexture2D::CreateTransient(N, N, PF_B8G8R8A8);
		if (!T)
		{
			return nullptr;
		}
		T->SRGB = false;
		T->Filter = TF_Bilinear;
		T->AddressX = TA_Clamp;
		T->AddressY = TA_Clamp;
		FTexture2DMipMap& Mip = T->GetPlatformData()->Mips[0];
		FColor* Pixels = static_cast<FColor*>(Mip.BulkData.Lock(LOCK_READ_WRITE));
		for (int32 y = 0; y < N; ++y)
		{
			for (int32 x = 0; x < N; ++x)
			{
				// -1..1 across the square.
				const float U = (x + 0.5f) / N * 2.0f - 1.0f, V = (y + 0.5f) / N * 2.0f - 1.0f;
				const uint8 A = uint8(FMath::Clamp(Shape(U, V), 0.0f, 1.0f) * 255.0f);
				Pixels[y * N + x] = FColor(A, A, A, A);
			}
		}
		Mip.BulkData.Unlock();
		T->UpdateResource();
		T->AddToRoot();
		return T;
	}
}

namespace DCCFx
{
	UTexture2D* Glow()
	{
		static UTexture2D* T = Make(128, [](float U, float V)
		{
			const float R = FMath::Sqrt(U * U + V * V);
			return FMath::Square(FMath::Max(0.0f, 1.0f - R));
		});
		return T;
	}

	UTexture2D* Rays()
	{
		static UTexture2D* T = Make(256, [](float U, float V)
		{
			const float R = FMath::Sqrt(U * U + V * V);
			const float A = FMath::Atan2(V, U);
			// Sixteen soft spokes of two widths, fading out toward the edge.
			const float Spokes = FMath::Pow(FMath::Abs(FMath::Cos(A * 8.0f)), 14.0f) + 0.5f * FMath::Pow(FMath::Abs(FMath::Cos(A * 8.0f + PI * 0.5f)), 30.0f);
			return Spokes * FMath::Max(0.0f, 1.0f - R) * FMath::Min(1.0f, R * 4.0f);
		});
		return T;
	}

	UTexture2D* Sparkle()
	{
		static UTexture2D* T = Make(64, [](float U, float V)
		{
			const float R = FMath::Sqrt(U * U + V * V);
			// A bright core and two thin crossing beams.
			const float Core = FMath::Square(FMath::Max(0.0f, 1.0f - R * 3.0f));
			const float BeamX = FMath::Max(0.0f, 1.0f - FMath::Abs(V) * 14.0f) * FMath::Max(0.0f, 1.0f - FMath::Abs(U));
			const float BeamY = FMath::Max(0.0f, 1.0f - FMath::Abs(U) * 14.0f) * FMath::Max(0.0f, 1.0f - FMath::Abs(V));
			return Core + BeamX + BeamY;
		});
		return T;
	}

	UTexture2D* Diamond()
	{
		static UTexture2D* T = Make(64, [](float U, float V)
		{
			const float D = FMath::Abs(U) + FMath::Abs(V);
			return FMath::Clamp((1.0f - D) * 12.0f, 0.0f, 1.0f);
		});
		return T;
	}
}
