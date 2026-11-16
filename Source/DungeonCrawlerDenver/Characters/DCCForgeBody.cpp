#include "Characters/DCCForgeBody.h"

#include "Characters/DCCCharacterBase.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/SkeletalMesh.h"
#include "UObject/ConstructorHelpers.h"

namespace DCCForgeBody
{
	USkeletalMesh* Body(int32 Index)
	{
		static ConstructorHelpers::FObjectFinder<USkeletalMesh> B0(TEXT("/Game/DCC/Characters/NPCs/NPC_casual_300/SK_NPC_casual_300.SK_NPC_casual_300"));
		static ConstructorHelpers::FObjectFinder<USkeletalMesh> B1(TEXT("/Game/DCC/Characters/NPCs/NPC_worker_301/SK_NPC_worker_301.SK_NPC_worker_301"));
		static ConstructorHelpers::FObjectFinder<USkeletalMesh> B2(TEXT("/Game/DCC/Characters/NPCs/NPC_office_302/SK_NPC_office_302.SK_NPC_office_302"));
		static ConstructorHelpers::FObjectFinder<USkeletalMesh> B3(TEXT("/Game/DCC/Characters/NPCs/NPC_casual_303/SK_NPC_casual_303.SK_NPC_casual_303"));
		static ConstructorHelpers::FObjectFinder<USkeletalMesh> B4(TEXT("/Game/DCC/Characters/NPCs/NPC_worker_304/SK_NPC_worker_304.SK_NPC_worker_304"));
		static ConstructorHelpers::FObjectFinder<USkeletalMesh> B5(TEXT("/Game/DCC/Characters/NPCs/NPC_office_305/SK_NPC_office_305.SK_NPC_office_305"));
		static ConstructorHelpers::FObjectFinder<USkeletalMesh> B6(TEXT("/Game/DCC/Characters/NPCs/NPC_casual_306/SK_NPC_casual_306.SK_NPC_casual_306"));
		static ConstructorHelpers::FObjectFinder<USkeletalMesh> B7(TEXT("/Game/DCC/Characters/NPCs/NPC_worker_307/SK_NPC_worker_307.SK_NPC_worker_307"));
		static ConstructorHelpers::FObjectFinder<USkeletalMesh> B8(TEXT("/Game/DCC/Characters/NPCs/NPC_office_308/SK_NPC_office_308.SK_NPC_office_308"));
		static ConstructorHelpers::FObjectFinder<USkeletalMesh> B9(TEXT("/Game/DCC/Characters/NPCs/NPC_casual_309/SK_NPC_casual_309.SK_NPC_casual_309"));
		static ConstructorHelpers::FObjectFinder<USkeletalMesh> B10(TEXT("/Game/DCC/Characters/NPCs/NPC_worker_310/SK_NPC_worker_310.SK_NPC_worker_310"));
		static ConstructorHelpers::FObjectFinder<USkeletalMesh> B11(TEXT("/Game/DCC/Characters/NPCs/NPC_office_311/SK_NPC_office_311.SK_NPC_office_311"));
		USkeletalMesh* All[] = { B0.Object.Get(), B1.Object.Get(), B2.Object.Get(), B3.Object.Get(), B4.Object.Get(), B5.Object.Get(), B6.Object.Get(), B7.Object.Get(), B8.Object.Get(), B9.Object.Get(), B10.Object.Get(), B11.Object.Get() };
		const int32 N = UE_ARRAY_COUNT(All);
		USkeletalMesh* Pick = All[((Index % N) + N) % N];
		return Pick ? Pick : B0.Object.Get();
	}

	void Apply(ADCCCharacterBase& Character, USkeletalMesh* InBody, float Scale)
	{
		static ConstructorHelpers::FObjectFinder<UAnimSequence> Idle(TEXT("/Game/DCC/Characters/NPCs/Anims/NPC_MM_Idle.NPC_MM_Idle"));
		static ConstructorHelpers::FObjectFinder<UAnimSequence> Walk(TEXT("/Game/DCC/Characters/NPCs/Anims/NPC_MF_Unarmed_Walk_Fwd.NPC_MF_Unarmed_Walk_Fwd"));
		static ConstructorHelpers::FObjectFinder<UAnimSequence> Jog(TEXT("/Game/DCC/Characters/NPCs/Anims/NPC_MF_Unarmed_Jog_Fwd.NPC_MF_Unarmed_Jog_Fwd"));
		static ConstructorHelpers::FObjectFinder<UAnimSequence> Attack(TEXT("/Game/DCC/Characters/NPCs/Anims/NPC_MM_Attack_01.NPC_MM_Attack_01"));
		static ConstructorHelpers::FObjectFinder<UAnimSequence> Flinch(TEXT("/Game/DCC/Characters/NPCs/Anims/NPC_MM_HitReact_Front_Lgt_01.NPC_MM_HitReact_Front_Lgt_01"));

		if (USkeletalMeshComponent* Mesh = Character.GetMesh())
		{
			if (InBody)
			{
				Mesh->SetSkeletalMesh(InBody);
			}
			Mesh->SetRelativeScale3D(FVector(Scale));
			// Forge bodies stand on their feet at the origin and face +Y, like the mannequin.
			Mesh->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
			Mesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
			if (const UCapsuleComponent* Capsule = Character.GetCapsuleComponent())
			{
				Mesh->SetRelativeLocation(FVector(0.0f, 0.0f, -Capsule->GetUnscaledCapsuleHalfHeight()));
			}
		}
		Character.IdleAnimation = Idle.Object;
		Character.WalkAnimation = Walk.Object;
		Character.RunAnimation = Jog.Object;
		Character.AttackAnimation = Attack.Object;
		Character.CastAnimation = Attack.Object;
		Character.HitReactionAnimation = Flinch.Object;
		// Crawler-only clips (climb, fist bump, the emote pool) are left empty: a Forge person does not do those.
		Character.ClimbAnimation = nullptr;
		Character.FistBumpAnimation = nullptr;
	}
}
