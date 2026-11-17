#pragma once

#include "CoreMinimal.h"

class ADCCArcadeArena;
class ADCCSprayTag;
class AActor;
class APawn;
class APlayerController;
class APlayerState;
class UMaterialInterface;
class UTexture2D;
class UWorld;
struct FSlateBrush;

/**
 * Spray tags (2026-11-16). Buy the can at the Prize Counter and pick a picture; it is cut square and pixelated to
 * Res x Res. Then Z sprays it on whatever you are looking at, on foot or from the truck. Everyone sees it, and the
 * host keeps every arena's tags in Saved/Sprays until the host clears them (CLEAR ALL SPRAYS at the counter).
 *
 * The picture lives on the painter's own machine (Saved/Sprays/Mine_<name>.rgb) and travels with each spray, so
 * nothing about it needs to be in the profile; the profile only remembers that the can was bought.
 */
namespace DCCSpray
{
	constexpr int32 Res = 48;
	constexpr int32 Bytes = Res * Res * 3;
	constexpr int32 Price = 60;
	constexpr float Reach = 900.0f;        // how far away you can paint
	constexpr float HalfSize = 70.0f;      // a tag is 1.4 m square
	constexpr int32 MaxPerArena = 60;      // past this, the oldest is painted over
	inline const FName CanId(TEXT("SprayCan"));

	/** Client: the Windows picture picker, then crop, pixelate and keep it as this crawler's tag. False if cancelled
	 *  or unreadable (Error says why). Blocks while the dialog is open. */
	bool PickPicture(const FString& ProfileKey, FString& Error);

	/** Client: read a picture file and make it this crawler's tag (what PickPicture does after the dialog). */
	bool UsePicture(const FString& ProfileKey, const FString& Path, FString& Error);

	/** Read a picture file into a tag's pixels (crop, pixelate, band), without keeping it anywhere. */
	bool DecodePicture(const FString& Path, TArray<uint8>& OutRGB, FString& Error);

	/** Server: paint a tag at At, facing out of the surface along Normal, upright for someone looking along ViewDir.
	 *  Sticks to the surface if that moves. No ownership or distance checks (ServerSpray does those). */
	ADCCSprayTag* PaintTag(UWorld* World, const FVector& At, const FVector& Normal, const FVector& ViewDir, const TArray<uint8>& RGB,
		const FString& Painter, const AActor* Ignore = nullptr);

	/** Client: this crawler's tag, if one has been picked on this machine. */
	bool LoadMine(const FString& ProfileKey, TArray<uint8>& OutRGB);

	/** Client: a brush of this crawler's tag for the shop, or null. */
	const FSlateBrush* PreviewBrush(const FString& ProfileKey);

	/** Client, on the Z key: trace from the camera and ask the server to paint there. */
	void TryLocalSpray(APlayerController* PC);

	/** Server: check and paint one tag. Painter is the pawn that asked (crawler or truck). */
	void ServerSpray(APawn* Painter, APlayerState* PS, const FVector& At, const FVector& Normal, const FVector& ViewDir, const TArray<uint8>& RGB);

	/** Server: put back the tags saved for this arena. */
	void Restore(ADCCArcadeArena* Arena);

	/** Server (host): remove every tag in this world and forget every saved one. */
	void ClearAll(UWorld* World);

	/** A small texture of a picture, square, nearest-filtered so it stays pixelated. */
	UTexture2D* MakeTexture(const TArray<uint8>& RGB);

	/** The decal material, with its picture parameter set to Texture. Null if the pack material is missing. */
	UMaterialInterface* MakeMaterial(UObject* Outer, UTexture2D* Texture);
}
