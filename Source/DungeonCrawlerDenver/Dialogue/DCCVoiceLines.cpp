#include "Dialogue/DCCVoiceLines.h"

#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "HAL/PlatformFileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundWaveProcedural.h"

namespace
{
	/** A plain 16-bit PCM WAV: the format the studio exports. Anything else is refused. */
	bool ReadPcm16(const FString& Path, TArray<uint8>& OutPcm, int32& OutRate, int32& OutChannels)
	{
		TArray<uint8> Bytes;
		if (!FFileHelper::LoadFileToArray(Bytes, *Path) || Bytes.Num() < 44
			|| FMemory::Memcmp(Bytes.GetData(), "RIFF", 4) != 0 || FMemory::Memcmp(Bytes.GetData() + 8, "WAVE", 4) != 0)
		{
			return false;
		}
		auto U16 = [&Bytes](int32 At) { return int32(Bytes[At]) | (int32(Bytes[At + 1]) << 8); };
		auto U32 = [&Bytes](int32 At) { return int32(Bytes[At]) | (int32(Bytes[At + 1]) << 8) | (int32(Bytes[At + 2]) << 16) | (int32(Bytes[At + 3]) << 24); };

		int32 Format = 0, Bits = 0;
		OutRate = 0;
		OutChannels = 0;
		int32 At = 12;
		while (At + 8 <= Bytes.Num())
		{
			const int32 Size = U32(At + 4);
			if (FMemory::Memcmp(Bytes.GetData() + At, "fmt ", 4) == 0 && At + 24 <= Bytes.Num())
			{
				Format = U16(At + 8);
				OutChannels = U16(At + 10);
				OutRate = U32(At + 12);
				Bits = U16(At + 22);
			}
			else if (FMemory::Memcmp(Bytes.GetData() + At, "data", 4) == 0)
			{
				const int32 Start = At + 8;
				const int32 Length = FMath::Min(Size, Bytes.Num() - Start);
				if (Format != 1 || Bits != 16 || OutRate <= 0 || OutChannels <= 0 || Length <= 0)
				{
					return false;
				}
				OutPcm.Append(Bytes.GetData() + Start, Length);
				return true;
			}
			At += 8 + Size + (Size & 1);
		}
		return false;
	}

	USoundAttenuation* VoiceAttenuation()
	{
		static TWeakObjectPtr<USoundAttenuation> Cached;
		if (!Cached.IsValid())
		{
			USoundAttenuation* Attenuation = NewObject<USoundAttenuation>();
			Attenuation->Attenuation.bAttenuate = true;
			Attenuation->Attenuation.bSpatialize = true;
			Attenuation->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
			Attenuation->Attenuation.AttenuationShapeExtents = FVector(900.0f, 0.0f, 0.0f); // full volume within 9 m (2026-11-23: louder)
			Attenuation->Attenuation.FalloffDistance = 2800.0f;                              // gone by about 37 m
			Attenuation->AddToRoot();
			Cached = Attenuation;
		}
		return Cached.Get();
	}
}

namespace DCCVoice
{
	FString FindLine(FName Speaker, FName Row)
	{
		const FString File = FString::Printf(TEXT("VO_%s.wav"), *Row.ToString());
		const FString Candidates[] = {
			FPaths::Combine(FPaths::ProjectDir(), TEXT("SourceArt/Voice/NPC"), Speaker.ToString(), File),
			FPaths::Combine(FPaths::ProjectDir(), TEXT("Voice/NPC"), Speaker.ToString(), File),
		};
		for (const FString& Path : Candidates)
		{
			if (FPaths::FileExists(Path))
			{
				return Path;
			}
		}
		return FString();
	}

	UAudioComponent* PlayAt(const UObject* WorldContext, const FString& WavPath, const FVector& Location, float Volume)
	{
		UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
		if (!World || WavPath.IsEmpty() || World->GetNetMode() == NM_DedicatedServer)
		{
			return nullptr;
		}

		TArray<uint8> Pcm;
		int32 Rate = 0, Channels = 0;
		if (!ReadPcm16(WavPath, Pcm, Rate, Channels))
		{
			UE_LOG(LogTemp, Warning, TEXT("[DCC Voice] %s is not a 16-bit PCM WAV; not played."), *WavPath);
			return nullptr;
		}

		USoundWaveProcedural* Wave = NewObject<USoundWaveProcedural>(World);
		Wave->SetSampleRate(Rate);
		Wave->NumChannels = Channels;
		Wave->Duration = float(Pcm.Num()) / float(Rate * Channels * 2);
		Wave->SoundGroup = SOUNDGROUP_Voice;
		Wave->bLooping = false;
		Wave->QueueAudio(Pcm.GetData(), Pcm.Num());

		// 1.8x (2026-11-23, Zoe: "NPC voices should be a bit louder"). The Kokoro lines come out quiet next to the music.
		UAudioComponent* Voice = UGameplayStatics::SpawnSoundAtLocation(World, Wave, Location, FRotator::ZeroRotator, Volume * 1.8f, 1.0f, 0.0f, VoiceAttenuation(), nullptr, true);
		if (Voice)
		{
			Voice->StopDelayed(Wave->Duration + 0.1f); // a procedural wave never ends by itself
		}
		return Voice;
	}
}
