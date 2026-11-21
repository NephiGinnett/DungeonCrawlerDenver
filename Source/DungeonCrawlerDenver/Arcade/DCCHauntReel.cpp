// Dead Air's end of round (2026-11-15): a data extraction scene, then the Scroller, a fake social feed with one post
// per uploaded tape (Zoe: "use the scroller app program to just flick through scenes on a fake social media ... each
// getting a ranking based on their popularity"). The feed's look and feel (one post fills the phone, it loops, you
// flick to the next, it auto-scrolls) come from The Familiar's scroller; nothing of its code or content is used.
//
// A post's video is not video. Every machine already sees every body, monster and trap through replication, so every
// machine records a light timeline of the round (ten frames a second: where each body was, and where each crew member's
// camera pointed). The server, which scored every shot, picks the clips and replicates the list. Each machine then
// replays those clips with local stand-ins for the characters, through scene captures placed where the cameras were,
// and the HUD frames them as found footage. Shared moments become MULTICAM tiles.

#include "Arcade/DCCArcadeHaunt.h"
#include "Arcade/DCCWalletComponent.h"
#include "Session/DCCProfile.h"
#include "Favor/DCCFavorComponent.h"
#include "Stats/DCCStatsComponent.h"
#include "InputCoreTypes.h"

#include "Characters/DCCCharacterBase.h"
#include "Characters/DCCEnemyCharacter.h"
#include "Animation/AnimationAsset.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	FString CrewName(const ADCCCharacterBase* C)
	{
		const APlayerState* State = C ? C->GetPlayerState() : nullptr;
		// Puppets with no player (the trailer's crew) go by their display name.
		return State ? State->GetPlayerName() : (C ? C->DisplayName.ToString() : FString());
	}

	constexpr double ClipSeconds = 5.0;
}

namespace
{
	int32 BucketOf(uint8 Label)
	{
		// 1 lamp, 2 ghost, 12 story ghost. 4/5/6 monsters. 7-11 traps.
		if (Label == 1 || Label == 2 || Label == 12) { return 0; }
		if (Label >= 4 && Label <= 6 || Label == 13) { return 1; }
		if (Label >= 7 && Label <= 11) { return 2; }
		return -1;
	}
}

FString ADCCArcadeHaunt::LabelTitle(uint8 Label)
{
	switch (Label)
	{
	case 1: return TEXT("A LAMP WITH NO MINER");
	case 2: return TEXT("GHOST MINER");
	case 3: return TEXT("THE DEEPEST STOPE");
	case 4: return TEXT("SOMETHING IN THE DARK");
	case 5: return TEXT("IT WENT FOR THEM");
	case 6: return TEXT("CREW DOWN");
	case 7: return TEXT("POWDER");
	case 8: return TEXT("FIREDAMP");
	case 9: return TEXT("RUNAWAY CART");
	case 10: return TEXT("THE PLANKS GAVE WAY");
	case 11: return TEXT("LIGHTS OUT");
	case 12: return TEXT("THE STORY'S GHOST");
	case 13: return TEXT("IT HIT THE CAMERA");
	default: return TEXT("FOUND FOOTAGE");
	}
}

// ---- server: pick the clips ------------------------------------------------------------------------------------------

void ADCCArcadeHaunt::BuildReel()
{
	// Every crawler posts. One player: their best 4 moments. Two: 2 each. Three or more: at least 1 each.
	// A moment with no views still posts. Clips are about 5 seconds and do not overlap.
	struct FPick { FString Who; double Peak = 0.0; float Score = 0.0f; uint8 Label = 0; };
	const int32 Crew = FMath::Max(1, Uploaders.Num());
	const int32 Each = Crew <= 1 ? 4 : Crew == 2 ? 2 : 1;

	TArray<FDCCReelClip> Posts;
	TArray<float> Popularity;
	for (const FString& Who : Uploaders)
	{
		TArray<FPick> Candidates;
		if (const TArray<FShotMark>* Marks = ShotLog.Find(Who))
		{
			FPick Cur;
			float CurPeak = -1.0f;
			double Last = -100.0;
			auto Flush = [&]()
			{
				if (CurPeak > 0.0f) { Cur.Who = Who; Candidates.Add(Cur); }
				Cur = FPick();
				CurPeak = -1.0f;
			};
			for (const FShotMark& M : *Marks)
			{
				if (M.T - Last > 1.0) { Flush(); }
				Last = M.T;
				Cur.Score += M.Shot;
				if (M.Shot > CurPeak) { CurPeak = M.Shot; Cur.Peak = M.T; Cur.Label = M.Label; }
			}
			Flush();
			Candidates.Sort([](const FPick& A, const FPick& B) { return A.Score > B.Score; });
		}

		TArray<FPick> Chosen;
		for (const FPick& Candidate : Candidates)
		{
			if (Chosen.Num() >= Each) { break; }
			bool bOverlap = false;
			for (const FPick& Have : Chosen)
			{
				if (FMath::Abs(Have.Peak - Candidate.Peak) < ClipSeconds) { bOverlap = true; break; }
			}
			if (!bOverlap) { Chosen.Add(Candidate); }
		}
		const double Tail = ServerTime() - 5.0;
		for (int32 Pad = 0; Chosen.Num() < Each && Pad < Each; ++Pad)
		{
			FPick Filler;
			Filler.Who = Who;
			Filler.Peak = Tail - Pad * (ClipSeconds + 0.4);
			Filler.Score = 0.1f;
			Filler.Label = 0;
			bool bOverlap = false;
			for (const FPick& Have : Chosen)
			{
				if (FMath::Abs(Have.Peak - Filler.Peak) < ClipSeconds) { bOverlap = true; break; }
			}
			if (!bOverlap) { Chosen.Add(Filler); }
		}

		const FDCCFootageEntry* Entry = FindEntry(Who);
		const ADCCCharacterBase* Crawler = nullptr;
		for (TActorIterator<ADCCCharacterBase> It(GetWorld()); It; ++It)
		{
			if (It->GetPlayerState() && It->GetPlayerState()->GetPlayerName() == Who) { Crawler = *It; break; }
		}
		const int32 Charisma = Crawler && Crawler->Stats ? Crawler->Stats->GetEnhanced().GetScore(EDCCAbility::Charisma) : 3;
		const int32 Favor = Crawler && Crawler->Favor ? Crawler->Favor->Favor : 0;
		const TSet<int32>* Read = ReadBy.Find(Who);
		const int32 Notes = Read ? Read->Num() : 0;
		const int32 Stories = StoriesDone.Num();
		const float Views = Entry ? float(Entry->Views) : 0.0f;
		const TArray<FShotMark>* Marks = ShotLog.Find(Who);

		for (const FPick& Best : Chosen)
		{
			const float Pop = Views * (1.0f + 0.08f * Charisma) + FMath::Max(0, Favor) * 25.0f + Notes * 400.0f
				+ Stories * 1500.0f + Best.Score * 20.0f + FMath::FRandRange(0.0f, 300.0f);

			int32 Ghost = 0, Monster = 0, Trap = 0;
			if (Marks)
			{
				const double From = Best.Peak - ClipSeconds * 0.55;
				const double To = Best.Peak + ClipSeconds * 0.45;
				for (const FShotMark& M : *Marks)
				{
					if (M.T < From || M.T > To) { continue; }
					switch (BucketOf(M.Label))
					{
					case 0: ++Ghost; break;
					case 1: ++Monster; break;
					case 2: ++Trap; break;
					default: break;
					}
				}
			}

			FDCCReelClip Post;
			Post.Cams.Add(Who);
			Post.From = Best.Peak - ClipSeconds * 0.55;
			Post.To = Best.Peak + ClipSeconds * 0.45;
			Post.Title = LabelTitle(Best.Label);
			Post.Views = FMath::RoundToInt(Views);
			Post.Likes = FMath::RoundToInt(Pop * 0.11f);
			Post.Comments = FMath::RoundToInt(Pop * 0.006f) + 3;
			Post.Shares = FMath::RoundToInt(Pop * 0.0025f);
			Post.Card = FString::Printf(TEXT("ghost +%d\nmonster +%d\ntrap +%d\nnotes %d    stories %d\n%s views"),
				Ghost, Monster, Trap, Notes, Stories, *FText::AsNumber(Post.Views).ToString());
			Posts.Add(Post);
			Popularity.Add(Pop);
		}
	}

	// Rank, and pay by rank: trending pays best, everyone who posted gets something.
	TArray<int32> Order;
	for (int32 i = 0; i < Posts.Num(); ++i) { Order.Add(i); }
	Order.Sort([&Popularity](int32 A, int32 B) { return Popularity[A] > Popularity[B]; });
	static const int32 RankTickets[] = { 30, 20, 12 };
	static const int32 RankCredits[] = { 15, 10, 6 };
	TArray<APlayerController*> Players;
	GetPlayers(Players);
	for (int32 r = 0; r < Order.Num(); ++r)
	{
		FDCCReelClip& Post = Posts[Order[r]];
		Post.Rank = r + 1;
		Post.Tickets = r < 3 ? RankTickets[r] : 6;
		Post.Credits = r < 3 ? RankCredits[r] : 3;
		Post.Card += FString::Printf(TEXT("\n%d likes    +%d tickets"), Post.Likes, Post.Tickets);
		for (APlayerController* PC : Players)
		{
			if (PC && PC->PlayerState && PC->PlayerState->GetPlayerName() == Post.Cams[0])
			{
				UDCCWalletComponent::AwardTickets(PC->PlayerState, Post.Tickets, FString::Printf(TEXT("#%d on the Scroller"), Post.Rank));
				UDCCWalletComponent::AwardCredits(PC->PlayerState, Post.Credits, FString::Printf(TEXT("#%d on the Scroller"), Post.Rank));
				FDCCUpload Saved;
				Saved.Id = FString::Printf(TEXT("Haunt|%s|%.1f|%s"), *Post.Cams[0], Post.From, *Post.Title);
				Saved.Title = Post.Title;
				Saved.Moment = Post.Title;
				Saved.Views = Post.Views;
				Saved.Likes = Post.Likes;
				Saved.Comments = Post.Comments;
				Saved.Shares = Post.Shares;
				Saved.When = FDateTime::Now().ToString(TEXT("%Y-%m-%d"));
				Saved.Game = TEXT("Dead Air");
				Saved.CommentLines.Add(TEXT("the way the lamp went BLUE"));
				Saved.CommentLines.Add(TEXT("Mount Fourteener is NOT normal"));
				Saved.CommentLines.Add(Post.Card);
				DCCProfile::AddUpload(PC->PlayerState, Saved);
			}
		}
	}
	// The feed shows them best first, the way a feed would.
	Reel.Reset();
	for (int32 i : Order) { Reel.Add(Posts[i]); }
	UE_LOG(LogTemp, Log, TEXT("[DCC Scroller] %d posts."), Reel.Num());
	for (const FDCCReelClip& Post : Reel)
	{
		UE_LOG(LogTemp, Log, TEXT("[DCC Scroller]   #%d %s  %s  %d likes"), Post.Rank, *Post.Cams[0], *Post.Title, Post.Likes);
	}
}

void ADCCArcadeHaunt::TickReelServer()
{
	const double Now = ServerTime();
	if (ReelPhase == 1 && Now >= ExtractStart + ExtractSeconds)
	{
		if (Reel.Num() > 0)
		{
			ReelPhase = 2;
			ReelStart = Now;
		}
		else
		{
			ReelPhase = 3;
			ReturnToLobbyIn(20.0f);
		}
		ForceNetUpdate();
	}
	else if (ReelPhase == 2 && Now >= ReelStart + PostSeconds * Reel.Num() + 2.0)
	{
		// The feed's done: the trending board, then the vote.
		StatusLines.Reset();
		FString Board;
		for (const FDCCReelClip& Post : Reel)
		{
			Board += FString::Printf(TEXT("%s#%d %s %s likes"), Board.IsEmpty() ? TEXT("SCROLLER TRENDING:  ") : TEXT("     "), Post.Rank, *Post.Cams[0],
				*ADCCArcadeHaunt::CompactCount(Post.Likes));
		}
		StatusLines.Add(Board);
		StatusLines.Add(TEXT("Tickets and credits for your rank are in your wallet."));
		ReelPhase = 3;
		ReturnToLobbyIn(20.0f);
		ForceNetUpdate();
	}
}

bool ADCCArcadeHaunt::CurrentClip(int32& OutIndex, double& OutPlayTime, double& OutIntoClip) const
{
	if (ReelPhase != 2 || Reel.IsEmpty())
	{
		return false;
	}
	OutIndex = FMath::Clamp(FeedIndex, 0, Reel.Num() - 1);
	const FDCCReelClip& Post = Reel[OutIndex];
	const double Len = FMath::Max(0.5, Post.To - Post.From);
	OutIntoClip = FMath::Max(0.0, ServerTime() - FeedSince);
	OutPlayTime = Post.From + FMath::Fmod(OutIntoClip, Len); // a post loops, like on the feed
	return true;
}

// ---- every machine: record the round ---------------------------------------------------------------------------------

void ADCCArcadeHaunt::RecordFrame()
{
	const double Now = ServerTime();
	if (Now < NextRecordAt || !GetWorld())
	{
		return;
	}
	NextRecordAt = Now + 0.1;
	FRecFrame& Frame = Recording.AddDefaulted_GetRef();
	Frame.T = Now;
	const APlayerController* LocalPC = GetWorld()->GetFirstPlayerController();
	for (TActorIterator<ADCCCharacterBase> It(GetWorld()); It; ++It)
	{
		ADCCCharacterBase* C = *It;
		FRecBody Body;
		Body.Who = C;
		Body.Loc = C->GetActorLocation();
		Body.Yaw = C->GetActorRotation().Yaw;
		Body.Speed = C->GetVelocity().Size2D();
		Body.bAlive = C->IsAlive();
		Frame.Bodies.Add(Body);
		if (!C->IsA<ADCCEnemyCharacter>())
		{
			const FString Name = CrewName(C);
			if (!Name.IsEmpty())
			{
				FRecCam Cam;
				Cam.Name = Name;
				const bool bLocal = LocalPC && LocalPC->GetPawn() == C;
				Cam.Rot = bLocal ? LocalPC->GetControlRotation() : C->GetBaseAimRotation();
				// The body cam sits at the chest, a little forward: the shot the scoring judged.
				Cam.Loc = C->GetPawnViewLocation() + Cam.Rot.Vector() * 20.0f;
				Frame.Cams.Add(Cam);
			}
		}
	}
	// Ten minutes at most; a round is five.
	if (Recording.Num() > 6000)
	{
		Recording.RemoveAt(0, 600);
	}
}

// ---- every machine: the red light over whoever is filming ------------------------------------------------------------

void ADCCArcadeHaunt::TickRecLights()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	const bool bBlinkOn = FMath::Fmod(ServerTime(), 1.0) < 0.55;
	for (TActorIterator<ADCCCharacterBase> It(World); It; ++It)
	{
		ADCCCharacterBase* C = *It;
		if (C->IsA<ADCCEnemyCharacter>())
		{
			continue;
		}
		if (!RecLights.Contains(C))
		{
			UStaticMeshComponent* Bulb = NewObject<UStaticMeshComponent>(C);
			Bulb->SetStaticMesh(Sphere);
			Bulb->SetupAttachment(C->GetRootComponent());
			Bulb->SetRelativeLocation(FVector(0, 0, 125));
			Bulb->SetRelativeScale3D(FVector(0.1f));
			Bulb->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Bulb->SetCastShadow(false);
			if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
			{
				UMaterialInstanceDynamic* Red = UMaterialInstanceDynamic::Create(Base, Bulb);
				Red->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.0f, 0.02f, 0.02f));
				Bulb->SetMaterial(0, Red);
			}
			Bulb->RegisterComponent();
			UPointLightComponent* Glow = NewObject<UPointLightComponent>(C);
			Glow->SetupAttachment(Bulb);
			Glow->SetIntensity(900.0f);
			Glow->SetAttenuationRadius(260.0f);
			Glow->SetLightColor(FLinearColor(1.0f, 0.05f, 0.05f));
			Glow->SetCastShadows(false);
			Glow->RegisterComponent();
			RecLights.Add(C, Glow);
			RecBulbs.Add(C, Bulb);
		}
		const bool bShow = C->bRecordingFootage && bBlinkOn && ReelPhase < 2;
		if (UPointLightComponent* Glow = RecLights.FindRef(C).Get()) { Glow->SetVisibility(bShow); }
		if (UStaticMeshComponent* Bulb = RecBulbs.FindRef(C).Get()) { Bulb->SetVisibility(bShow); }
	}
}

// ---- every machine: play the reel ------------------------------------------------------------------------------------

void ADCCArcadeHaunt::EndReelLocal()
{
	for (TPair<TWeakObjectPtr<ADCCCharacterBase>, TWeakObjectPtr<AActor>>& Pair : Puppets)
	{
		if (AActor* Puppet = Pair.Value.Get()) { Puppet->Destroy(); }
	}
	Puppets.Reset();
	if (AActor* Rig = ReelRig.Get()) { Rig->Destroy(); }
	ReelCaptures.Reset();
	ReelLamps.Reset();
	ReelTargets.Reset();
	if (bReelHidRealCast)
	{
		bReelHidRealCast = false;
		for (TActorIterator<ADCCCharacterBase> It(GetWorld()); It; ++It)
		{
			It->SetActorHiddenInGame(false);
		}
	}
}

void ADCCArcadeHaunt::TickReelLocal(float DeltaSeconds)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const double Now = ServerTime();

	// Extraction: a pulse of light over the cage while the tapes upload.
	if (ReelPhase == 1)
	{
		if (!ExtractGlow.IsValid() && VanBox.IsValid)
		{
			ExtractGlow = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform(VanBox.GetCenter() + FVector(0, 0, 260)));
			if (AActor* Glow = ExtractGlow.Get())
			{
				UPointLightComponent* L = NewObject<UPointLightComponent>(Glow);
				Glow->SetRootComponent(L);
				L->SetWorldLocation(VanBox.GetCenter() + FVector(0, 0, 260));
				L->SetLightColor(FLinearColor(0.3f, 1.0f, 0.5f));
				L->SetAttenuationRadius(1600.0f);
				L->SetCastShadows(false);
				L->RegisterComponent();
				LocalActors.Add(Glow);
			}
		}
		if (AActor* Glow = ExtractGlow.Get())
		{
			if (UPointLightComponent* L = Cast<UPointLightComponent>(Glow->GetRootComponent()))
			{
				L->SetIntensity(3000.0f + 2500.0f * FMath::Sin(Now * 6.0));
			}
		}
		return;
	}
	if (ReelPhase != 2)
	{
		if (ReelPhase == 3 && (ReelRig.IsValid() || bReelHidRealCast))
		{
			EndReelLocal();
		}
		return;
	}

	// Flicking through the feed: wheel, arrows or J/K; it moves on by itself after PostSeconds.
	if (FeedSince <= 0.0)
	{
		FeedSince = Now;
		FeedIndex = 0;
	}
	if (APlayerController* Viewer = World->GetFirstPlayerController())
	{
		const bool bNext = Viewer->WasInputKeyJustPressed(EKeys::MouseScrollDown) || Viewer->WasInputKeyJustPressed(EKeys::Down) || Viewer->WasInputKeyJustPressed(EKeys::J);
		const bool bPrev = Viewer->WasInputKeyJustPressed(EKeys::MouseScrollUp) || Viewer->WasInputKeyJustPressed(EKeys::Up) || Viewer->WasInputKeyJustPressed(EKeys::K);
		if (bNext || bPrev || Now - FeedSince >= FeedAutoSeconds)
		{
			FeedIndex = (FeedIndex + (bPrev ? -1 : 1) + Reel.Num()) % FMath::Max(1, Reel.Num());
			FeedSince = Now;
		}
	}

	int32 ClipIndex = 0;
	double PlayTime = 0.0, Into = 0.0;
	if (!CurrentClip(ClipIndex, PlayTime, Into) || Recording.Num() < 2)
	{
		return;
	}
	const FDCCReelClip& Clip = Reel[ClipIndex];

	// The real cast steps out of the way: only the stand-ins are in the replay.
	if (!bReelHidRealCast)
	{
		bReelHidRealCast = true;
		for (TActorIterator<ADCCCharacterBase> It(World); It; ++It)
		{
			It->SetActorHiddenInGame(true);
		}
	}

	// The camera rig: up to three captures, each with a lamp like the one that lit the shot.
	if (!ReelRig.IsValid())
	{
		ReelRig = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform(GetActorLocation()));
		AActor* Rig = ReelRig.Get();
		USceneComponent* Root = NewObject<USceneComponent>(Rig, TEXT("Root"));
		Rig->SetRootComponent(Root);
		Root->RegisterComponent();
		for (int32 c = 0; c < 1; ++c)
		{
			// Portrait, like a phone video.
			UTextureRenderTarget2D* Target = NewObject<UTextureRenderTarget2D>(this);
			Target->InitAutoFormat(540, 960);
			Target->UpdateResourceImmediate(true);
			ReelTargets.Add(Target);
			USceneCaptureComponent2D* Capture = NewObject<USceneCaptureComponent2D>(Rig);
			Capture->SetupAttachment(Root);
			Capture->TextureTarget = Target;
			Capture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
			Capture->FOVAngle = 62.0f;
			Capture->bCaptureEveryFrame = true;
			Capture->bCaptureOnMovement = false;
			Capture->RegisterComponent();
			ReelCaptures.Add(Capture);
			USpotLightComponent* Lamp = NewObject<USpotLightComponent>(Rig);
			Lamp->SetupAttachment(Capture);
			Lamp->SetIntensity(9000.0f);
			Lamp->SetAttenuationRadius(3200.0f);
			Lamp->SetOuterConeAngle(32.0f);
			Lamp->SetInnerConeAngle(14.0f);
			Lamp->SetLightColor(FLinearColor(1.0f, 0.9f, 0.72f));
			Lamp->SetCastShadows(true);
			Lamp->RegisterComponent();
			ReelLamps.Add(Lamp);
		}
	}

	// The two recorded frames either side of the play time.
	int32 Lo = 0, Hi = Recording.Num() - 1;
	while (Hi - Lo > 1)
	{
		const int32 Mid = (Lo + Hi) / 2;
		(Recording[Mid].T <= PlayTime ? Lo : Hi) = Mid;
	}
	const FRecFrame& A = Recording[Lo];
	const FRecFrame& B = Recording[Hi];
	const float Alpha = B.T > A.T ? FMath::Clamp(float((PlayTime - A.T) / (B.T - A.T)), 0.0f, 1.0f) : 0.0f;

	// The stand-ins: a copy of each body's mesh, moved along its recorded path, walking or standing.
	TSet<TWeakObjectPtr<ADCCCharacterBase>> Present;
	for (const FRecBody& BodyA : A.Bodies)
	{
		ADCCCharacterBase* Who = BodyA.Who.Get();
		if (!Who)
		{
			continue;
		}
		const FRecBody* BodyB = B.Bodies.FindByPredicate([&BodyA](const FRecBody& X) { return X.Who == BodyA.Who; });
		const FVector Loc = BodyB ? FMath::Lerp(BodyA.Loc, BodyB->Loc, Alpha) : BodyA.Loc;
		const float Yaw = BodyB ? FMath::Lerp(BodyA.Yaw, BodyA.Yaw + FMath::FindDeltaAngleDegrees(BodyA.Yaw, BodyB->Yaw), Alpha) : BodyA.Yaw;
		const float Speed = BodyB ? FMath::Lerp(BodyA.Speed, BodyB->Speed, Alpha) : BodyA.Speed;
		Present.Add(BodyA.Who);

		TWeakObjectPtr<AActor>& Puppet = Puppets.FindOrAdd(BodyA.Who);
		USkeletalMeshComponent* Source = Who->GetMesh();
		if (!Puppet.IsValid() && Source && Source->GetSkeletalMeshAsset())
		{
			AActor* Stand = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform(FRotator(0, Yaw, 0), Loc));
			USceneComponent* Root = NewObject<USceneComponent>(Stand, TEXT("Root"));
			Stand->SetRootComponent(Root);
			Root->RegisterComponent();
			USkeletalMeshComponent* Skin = NewObject<USkeletalMeshComponent>(Stand, TEXT("Skin"));
			Skin->SetupAttachment(Root);
			Skin->SetSkeletalMeshAsset(Source->GetSkeletalMeshAsset());
			for (int32 m = 0; m < Source->GetNumMaterials(); ++m)
			{
				Skin->SetMaterial(m, Source->GetMaterial(m));
			}
			Skin->SetRelativeTransform(Source->GetRelativeTransform());
			Skin->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			if (Source->GetAnimationMode() == EAnimationMode::AnimationBlueprint && Source->GetAnimClass())
			{
				Skin->SetAnimInstanceClass(Source->GetAnimClass());
			}
			else
			{
				Skin->SetAnimationMode(EAnimationMode::AnimationSingleNode);
			}
			Skin->RegisterComponent();
			Puppet = Stand;
		}
		if (AActor* Stand = Puppet.Get())
		{
			Stand->SetActorLocationAndRotation(Loc, FRotator(0.0f, Yaw, 0.0f));
			Stand->SetActorHiddenInGame(!BodyA.bAlive);
			if (USkeletalMeshComponent* Skin = Stand->FindComponentByClass<USkeletalMeshComponent>())
			{
				if (Skin->GetAnimationMode() == EAnimationMode::AnimationSingleNode)
				{
					UAnimationAsset* Want = Speed > 300.0f && Who->RunAnimation ? Who->RunAnimation.Get()
						: Speed > 40.0f && Who->WalkAnimation ? Who->WalkAnimation.Get() : Who->IdleAnimation.Get();
					if (Want && Skin->AnimationData.AnimToPlay != Want)
					{
						Skin->PlayAnimation(Want, true);
						Skin->AnimationData.AnimToPlay = Want;
					}
				}
			}
		}
	}
	for (TPair<TWeakObjectPtr<ADCCCharacterBase>, TWeakObjectPtr<AActor>>& Pair : Puppets)
	{
		if (!Present.Contains(Pair.Key))
		{
			if (AActor* Stand = Pair.Value.Get()) { Stand->SetActorHiddenInGame(true); }
		}
	}

	// The cameras, where each one was.
	for (int32 c = 0; c < ReelCaptures.Num(); ++c)
	{
		USceneCaptureComponent2D* Capture = ReelCaptures[c].Get();
		if (!Capture)
		{
			continue;
		}
		const bool bUsed = Clip.Cams.IsValidIndex(c);
		Capture->bCaptureEveryFrame = bUsed;
		if (USpotLightComponent* Lamp = ReelLamps.IsValidIndex(c) ? ReelLamps[c].Get() : nullptr)
		{
			Lamp->SetVisibility(bUsed);
		}
		if (!bUsed)
		{
			continue;
		}
		const FRecCam* CamA = A.Cams.FindByPredicate([&](const FRecCam& X) { return X.Name == Clip.Cams[c]; });
		const FRecCam* CamB = B.Cams.FindByPredicate([&](const FRecCam& X) { return X.Name == Clip.Cams[c]; });
		if (CamA)
		{
			const FVector Loc = CamB ? FMath::Lerp(CamA->Loc, CamB->Loc, Alpha) : CamA->Loc;
			const FRotator Rot = CamB ? FMath::Lerp(CamA->Rot, CamB->Rot, Alpha) : CamA->Rot;
			// A handheld wobble: found footage is never steady.
			const FRotator Shake(FMath::PerlinNoise1D(Now * 1.7 + c) * 1.2f, FMath::PerlinNoise1D(Now * 1.3 + 7 + c) * 1.2f, 0.0f);
			Capture->SetWorldLocationAndRotation(Loc, Rot + Shake);
		}
	}

	// The trap moments in this stretch happen again, on cue.
	if (ReelPlayedClip != ClipIndex || PlayTime < ReelLastPlayTime)
	{
		ReelPlayedClip = ClipIndex;
		ReelEventsPlayed.Reset();
	}
	ReelLastPlayTime = PlayTime;
	for (const FDCCMineEvent& E : Events)
	{
		if (E.Time >= Clip.From && E.Time <= PlayTime && !ReelEventsPlayed.Contains(E.Id))
		{
			ReelEventsPlayed.Add(E.Id);
			if (E.Kind == 3) { Burst(TEXT("/Game/Legendary_Forge/Fx/Fire/NS_fire"), E.At, 2.0, 2.0f); Burst(TEXT("/Game/Legendary_Forge/Fx/Sparks/NS_sparks"), E.At, 1.5, 1.5f); }
			if (E.Kind == 4) { Burst(TEXT("/Game/Legendary_Forge/Fx/Fire/NS_fire"), E.At + FVector(0, 0, 150), 1.2, 3.0f); }
		}
	}
}
