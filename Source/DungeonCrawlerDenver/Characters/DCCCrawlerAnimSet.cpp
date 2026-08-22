#include "Characters/DCCCrawlerAnimSet.h"

#include "Characters/DCCEnemyCharacter.h"
#include "Animation/AnimSequence.h"
#include "UObject/ConstructorHelpers.h"

namespace DCCCrawlerAnimSet
{
	void Apply(ADCCEnemyCharacter& Character, float EmoteChanceOnKill)
	{
		// Closest thing to a standing idle pose in the imported set. Left nullable
		// rather than required: ADCCCharacterBase::UpdateLocomotionAnimation already
		// falls back Run -> Walk -> Idle, so this is a nicety, not a dependency.
		static ConstructorHelpers::FObjectFinder<UAnimSequence> Idle(
			TEXT("/Game/DCC/Characters/Shared/Anims/A_DCC_OA_Alert.A_DCC_OA_Alert"));
		static ConstructorHelpers::FObjectFinder<UAnimSequence> Walk(
			TEXT("/Game/DCC/Characters/Shared/Anims/A_DCC_HW_Walking.A_DCC_HW_Walking"));
		static ConstructorHelpers::FObjectFinder<UAnimSequence> Run(
			TEXT("/Game/DCC/Characters/Shared/Anims/A_DCC_HW_Running.A_DCC_HW_Running"));

		// The one real attack clip in the set. This is what makes melee visible at
		// all - Multicast_PlayAttack no-ops silently on a null AttackAnimation,
		// which is why combat looked like nothing was happening.
		static ConstructorHelpers::FObjectFinder<UAnimSequence> Attack(
			TEXT("/Game/DCC/Characters/Shared/Anims/A_DCC_DW_ChargedSlash.A_DCC_DW_ChargedSlash"));

		// The consolidation's three headline wins - all of these existed on one
		// character's rig and were unreachable by everyone else until the skeletons
		// were merged. All three slots were empty on every character before this.
		static ConstructorHelpers::FObjectFinder<UAnimSequence> Cast(
			TEXT("/Game/DCC/Characters/Shared/Anims/A_DCC_DW_CallGesture.A_DCC_DW_CallGesture"));
		static ConstructorHelpers::FObjectFinder<UAnimSequence> Climb(
			TEXT("/Game/DCC/Characters/Shared/Anims/A_DCC_DW_ClimbStairs.A_DCC_DW_ClimbStairs"));
		static ConstructorHelpers::FObjectFinder<UAnimSequence> FistBump(
			TEXT("/Game/DCC/Characters/Shared/Anims/A_DCC_HW_AgreeGesture.A_DCC_HW_AgreeGesture"));

		// A STAND-IN, not a hit reaction. There is no flinch clip in the project -
		// the 24 shared clips are locomotion, dances, gestures, one attack and one
		// climb - so the alert pose is borrowed and compressed to a quarter second,
		// which reads as a jolt. Replace this the moment a reaction pack lands;
		// nothing else has to change, because it is just a slot.
		static ConstructorHelpers::FObjectFinder<UAnimSequence> HitReact(
			TEXT("/Game/DCC/Characters/Shared/Anims/A_DCC_OA_Alert.A_DCC_OA_Alert"));

		static ConstructorHelpers::FObjectFinder<UAnimSequence> EmoteBoomDance(
			TEXT("/Game/DCC/Characters/Shared/Anims/A_DCC_HW_BoomDance.A_DCC_HW_BoomDance"));
		static ConstructorHelpers::FObjectFinder<UAnimSequence> EmoteAllNightDance(
			TEXT("/Game/DCC/Characters/Shared/Anims/A_DCC_AO_AllNightDance.A_DCC_AO_AllNightDance"));
		static ConstructorHelpers::FObjectFinder<UAnimSequence> EmoteCardioDance(
			TEXT("/Game/DCC/Characters/Shared/Anims/A_DCC_DW_CardioDance.A_DCC_DW_CardioDance"));
		static ConstructorHelpers::FObjectFinder<UAnimSequence> EmoteAgreeHW(
			TEXT("/Game/DCC/Characters/Shared/Anims/A_DCC_HW_AgreeGesture.A_DCC_HW_AgreeGesture"));
		static ConstructorHelpers::FObjectFinder<UAnimSequence> EmoteAgreeAO(
			TEXT("/Game/DCC/Characters/Shared/Anims/A_DCC_AO_AgreeGesture.A_DCC_AO_AgreeGesture"));
		static ConstructorHelpers::FObjectFinder<UAnimSequence> EmoteSpinJump(
			TEXT("/Game/DCC/Characters/Shared/Anims/A_DCC_AO_SpinJump.A_DCC_AO_SpinJump"));

		// Three more that were sitting unused in Shared/Anims. Squats and a boxing
		// warmup read as "waiting around" rather than "celebrating", which is what
		// the pool was short of - six dance clips made every kill look identical.
		static ConstructorHelpers::FObjectFinder<UAnimSequence> EmoteAirSquatAO(
			TEXT("/Game/DCC/Characters/Shared/Anims/A_DCC_AO_AirSquat.A_DCC_AO_AirSquat"));
		static ConstructorHelpers::FObjectFinder<UAnimSequence> EmoteAirSquatDW(
			TEXT("/Game/DCC/Characters/Shared/Anims/A_DCC_DW_AirSquat.A_DCC_DW_AirSquat"));
		static ConstructorHelpers::FObjectFinder<UAnimSequence> EmoteBoxingWarmup(
			TEXT("/Game/DCC/Characters/Shared/Anims/A_DCC_OA_BoxingWarmup.A_DCC_OA_BoxingWarmup"));

		if (Idle.Succeeded())     { Character.IdleAnimation = Idle.Object; }
		if (Walk.Succeeded())     { Character.WalkAnimation = Walk.Object; }
		if (Run.Succeeded())      { Character.RunAnimation = Run.Object; }
		if (Attack.Succeeded())   { Character.AttackAnimation = Attack.Object; }
		if (Cast.Succeeded())     { Character.CastAnimation = Cast.Object; }
		if (Climb.Succeeded())    { Character.ClimbAnimation = Climb.Object; }
		if (FistBump.Succeeded()) { Character.FistBumpAnimation = FistBump.Object; }
		if (HitReact.Succeeded()) { Character.HitReactionAnimation = HitReact.Object; }

		Character.EmoteAnimations.Reset();
		for (const auto* Finder : { &EmoteBoomDance, &EmoteAllNightDance, &EmoteCardioDance,
			&EmoteAgreeHW, &EmoteAgreeAO, &EmoteSpinJump,
			&EmoteAirSquatAO, &EmoteAirSquatDW, &EmoteBoxingWarmup })
		{
			if (Finder->Succeeded())
			{
				Character.EmoteAnimations.Add(Finder->Object);
			}
		}

		// Was 0 on every ally Blueprint, so nobody ever celebrated a kill.
		Character.EmoteChanceOnKill = EmoteChanceOnKill;
	}
}
