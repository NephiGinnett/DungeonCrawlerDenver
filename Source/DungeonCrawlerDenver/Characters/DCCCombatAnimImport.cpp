// DCC.ImportCombatAnims (2026-11-19): editor only. Imports the Mixamo clips that
// Tools/rebake_mixamo_batch.py baked onto CJ's rig, and binds every sequence to
// SK_DCCCrawler_Skeleton. A new skeleton from the FBX would refuse to play.
//
//   UnrealEditor.exe <project> -ExecCmds="DCC.ImportCombatAnims,quit" -unattended -nosplash -log

#if WITH_EDITOR

#include "CoreMinimal.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "AssetImportTask.h"
#include "AssetToolsModule.h"
#include "Factories/FbxAnimSequenceImportData.h"
#include "Factories/FbxFactory.h"
#include "Factories/FbxImportUI.h"
#include "FileHelpers.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogDCCCombatAnim, Log, All);

namespace DCCCombatAnimImport
{
	const TCHAR* SkeletonPath = TEXT("/Game/DCC/Characters/Shared/SK_DCCCrawler_Skeleton.SK_DCCCrawler_Skeleton");
	const TCHAR* DestPath = TEXT("/Game/DCC/Characters/Shared/AnimsFixed");

	const TCHAR* Files[] = {
		TEXT("A_DCC_Stab.fbx"),
		TEXT("A_DCC_Slash.fbx"),
		TEXT("A_DCC_Overhead.fbx"),
		TEXT("A_DCC_Blunt.fbx"),
		TEXT("A_DCC_Hit.fbx"),
		TEXT("A_DCC_Cast.fbx"),
		TEXT("A_DCC_Wave.fbx"),
		TEXT("A_DCC_GS_Idle.fbx"),
		TEXT("A_DCC_GS_Walk.fbx"),
		TEXT("A_DCC_GS_Run.fbx"),
		TEXT("A_DCC_SNS_Idle.fbx"),
		TEXT("A_DCC_SNS_Walk.fbx"),
		TEXT("A_DCC_SNS_Run.fbx"),
	};

	void DisableInterchangeFbx()
	{
		FModuleManager::Get().LoadModule(TEXT("InterchangeImport"));
		if (IConsoleVariable* Cvar = IConsoleManager::Get().FindConsoleVariable(TEXT("Interchange.FeatureFlags.Import.FBX")))
		{
			Cvar->Set(0, ECVF_SetByCode);
			UE_LOG(LogDCCCombatAnim, Display, TEXT("Interchange FBX import is off for this run."));
		}
		else
		{
			UE_LOG(LogDCCCombatAnim, Warning, TEXT("Interchange FBX cvar was not registered."));
		}
	}

	void ImportOne(USkeleton* Skeleton, const FString& FullPath)
	{
		const FString AssetName = FPaths::GetBaseFilename(FullPath);
		UFbxFactory* Factory = NewObject<UFbxFactory>(GetTransientPackage());
		Factory->SetDetectImportTypeOnImport(false);
		UFbxImportUI* UI = Factory->ImportUI;
		UI->bAutomatedImportShouldDetectType = false;
		UI->MeshTypeToImport = FBXIT_Animation;
		UI->OriginalImportType = FBXIT_Animation;
		UI->bImportMesh = false;
		UI->bImportAsSkeletal = true;
		UI->bImportAnimations = true;
		UI->bImportMaterials = false;
		UI->bImportTextures = false;
		UI->bCreatePhysicsAsset = false;
		UI->Skeleton = Skeleton;
		if (UI->AnimSequenceImportData)
		{
			UI->AnimSequenceImportData->bImportMeshesInBoneHierarchy = false;
			UI->AnimSequenceImportData->AnimationLength = FBXALIT_ExportedTime;
		}

		UAssetImportTask* Task = NewObject<UAssetImportTask>(GetTransientPackage());
		Task->Filename = FullPath;
		Task->DestinationPath = DestPath;
		Task->DestinationName = AssetName;
		Task->bAutomated = true;
		Task->bSave = true;
		Task->bReplaceExisting = true;
		Task->bReplaceExistingSettings = true;
		Task->Factory = Factory;
		Task->Options = UI;
		Factory->SetAssetImportTask(Task);

		TArray<UAssetImportTask*> Tasks;
		Tasks.Add(Task);
		FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get().ImportAssetTasks(Tasks);

		int32 Bound = 0;
		for (UObject* Object : Task->GetObjects())
		{
			UAnimSequence* Sequence = Cast<UAnimSequence>(Object);
			if (!Sequence)
			{
				UE_LOG(LogDCCCombatAnim, Display, TEXT("Imported extra %s"), *GetNameSafe(Object));
				continue;
			}
			if (Sequence->GetSkeleton() != Skeleton)
			{
				UE_LOG(LogDCCCombatAnim, Warning, TEXT("%s arrived on %s. Rebinding to the shared crawler."),
					*Sequence->GetName(), *GetNameSafe(Sequence->GetSkeleton()));
				Sequence->SetSkeleton(Skeleton);
				Sequence->MarkPackageDirty();
			}
			UE_LOG(LogDCCCombatAnim, Display, TEXT("Sequence %s skeleton %s"),
				*Sequence->GetPathName(), *GetNameSafe(Sequence->GetSkeleton()));
			if (Sequence->GetSkeleton() == Skeleton)
			{
				++Bound;
			}
		}
		if (Bound == 0)
		{
			UE_LOG(LogDCCCombatAnim, Warning, TEXT("No animation sequence from %s. Paths: %s"),
				*AssetName, *FString::Join(Task->ImportedObjectPaths, TEXT(", ")));
		}

		TArray<UPackage*> Packages;
		for (UObject* Object : Task->GetObjects())
		{
			if (Object)
			{
				Packages.AddUnique(Object->GetPackage());
			}
		}
		if (Packages.Num() > 0)
		{
			UEditorLoadingAndSavingUtils::SavePackages(Packages, false);
		}
	}

	void Run(const TArray<FString>&)
	{
		DisableInterchangeFbx();
		USkeleton* Skeleton = LoadObject<USkeleton>(nullptr, SkeletonPath);
		if (!Skeleton)
		{
			UE_LOG(LogDCCCombatAnim, Error, TEXT("Missing skeleton %s"), SkeletonPath);
			return;
		}

		const FString Dir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("SourceArt/CombatAnims/fbx"));
		int32 Imported = 0;
		for (const TCHAR* File : Files)
		{
			const FString FullPath = Dir / File;
			if (!FPaths::FileExists(FullPath))
			{
				UE_LOG(LogDCCCombatAnim, Warning, TEXT("Missing bake %s"), *FullPath);
				continue;
			}
			ImportOne(Skeleton, FullPath);
			++Imported;
		}
		UE_LOG(LogDCCCombatAnim, Display, TEXT("DCC.ImportCombatAnims done. Files touched: %d"), Imported);
	}
}

static FAutoConsoleCommand GCombatAnimImportCommand(
	TEXT("DCC.ImportCombatAnims"),
	TEXT("Import the baked combat FBX clips onto SK_DCCCrawler_Skeleton."),
	FConsoleCommandWithArgsDelegate::CreateStatic(&DCCCombatAnimImport::Run));

#endif
