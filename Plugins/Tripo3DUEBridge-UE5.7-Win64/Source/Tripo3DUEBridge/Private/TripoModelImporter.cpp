#include "TripoModelImporter.h"
#include "TripoVersionCompat.h"
#include "TripoProtocol.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "AssetImportTask.h"
#include "Factories/FbxFactory.h"
#include "Factories/FbxImportUI.h"
#include "Factories/FbxStaticMeshImportData.h"
#include "Factories/FbxSkeletalMeshImportData.h"
#include "Factories/FbxAnimSequenceImportData.h"
#include "Factories/FbxTextureImportData.h"
#include "Factories/TextureFactory.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInterface.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Editor.h"
#include "LevelEditorViewport.h"
#include "PackageTools.h"
#include "ObjectTools.h"
#include "FileHelpers.h"
#include "Misc/MessageDialog.h"
#include "Containers/Ticker.h"
#include "Async/Async.h"
#include "InterchangeGenericAnimationPipeline.h"
#include "InterchangeGenericAssetsPipeline.h"
#include "InterchangeManager.h"

// For ZIP extraction - UE has built-in zip support
#include "Serialization/Archive.h"

// Include minizip for ZIP extraction
THIRD_PARTY_INCLUDES_START
#include "zlib.h"
#include "minizip/unzip.h"
THIRD_PARTY_INCLUDES_END

// ————— Minizip Memory Stream —————

struct FMinizipMemoryStream
{
	const uint8* Data;
	int64 Size;
	int64 Position;
};

static voidpf ZCALLBACK MemOpen(voidpf Opaque, const char* /*Filename*/, int /*Mode*/)
{
	return Opaque;
}

static uLong ZCALLBACK MemRead(voidpf Opaque, voidpf /*Stream*/, void* Buf, uLong Size)
{
	FMinizipMemoryStream* Mem = static_cast<FMinizipMemoryStream*>(Opaque);
	int64 Remaining = Mem->Size - Mem->Position;
	uLong ToRead = static_cast<uLong>(FMath::Min(static_cast<int64>(Size), Remaining));
	if (ToRead > 0)
	{
		FMemory::Memcpy(Buf, Mem->Data + Mem->Position, ToRead);
		Mem->Position += ToRead;
	}
	return ToRead;
}

static uLong ZCALLBACK MemWrite(voidpf /*Opaque*/, voidpf /*Stream*/, const void* /*Buf*/, uLong /*Size*/)
{
	return 0;
}

static long ZCALLBACK MemTell(voidpf Opaque, voidpf /*Stream*/)
{
	FMinizipMemoryStream* Mem = static_cast<FMinizipMemoryStream*>(Opaque);
	return static_cast<long>(Mem->Position);
}

static long ZCALLBACK MemSeek(voidpf Opaque, voidpf /*Stream*/, uLong Offset, int Origin)
{
	FMinizipMemoryStream* Mem = static_cast<FMinizipMemoryStream*>(Opaque);
	int64 NewPos = 0;
	switch (Origin)
	{
	case ZLIB_FILEFUNC_SEEK_SET:
		NewPos = static_cast<int64>(Offset);
		break;
	case ZLIB_FILEFUNC_SEEK_CUR:
		NewPos = Mem->Position + static_cast<int64>(Offset);
		break;
	case ZLIB_FILEFUNC_SEEK_END:
		NewPos = Mem->Size + static_cast<int64>(Offset);
		break;
	default:
		return -1;
	}
	if (NewPos < 0 || NewPos > Mem->Size)
	{
		return -1;
	}
	Mem->Position = NewPos;
	return 0;
}

static int ZCALLBACK MemClose(voidpf /*Opaque*/, voidpf /*Stream*/)
{
	return 0;
}

static int ZCALLBACK MemError(voidpf /*Opaque*/, voidpf /*Stream*/)
{
	return 0;
}

// ————— Static Helpers —————

static FString SanitizeAssetName(const FString& RawName, const FString& FileId)
{
	FString BaseName = FPaths::GetBaseFilename(RawName);

	BaseName.ReplaceInline(TEXT(" "), TEXT("_"));

	const TCHAR IllegalChars[] = TEXT("/\\:*?\"<>|");
	for (int32 i = 0; IllegalChars[i] != '\0'; ++i)
	{
		BaseName.ReplaceCharInline(IllegalChars[i], TEXT('_'));
	}

	BaseName.TrimStartAndEndInline();

	if (BaseName.IsEmpty())
	{
		BaseName = FString::Printf(TEXT("Model_%s"), *FileId.Left(8));
	}

	return BaseName;
}

static FString MakeUniqueAssetPath(const FString& BasePath)
{
	FAssetRegistryModule& Reg = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

	FString FinalPath = BasePath;
	int32 Suffix = 1;
	while (Reg.Get().HasAssets(FName(*FinalPath), true))
	{
		FinalPath = FString::Printf(TEXT("%s_%d"), *BasePath, Suffix++);
	}
	return FinalPath;
}

static FString BuildAnimationAssetName(const FString& BasePackagePath, const FString& FbxBaseName, const FString& SourceName)
{
	const FString UniqueSafeName = FPaths::GetCleanFilename(BasePackagePath);

	FString AnimName = SourceName;
	if (AnimName.StartsWith(FbxBaseName))
	{
		AnimName = AnimName.Mid(FbxBaseName.Len());
	}

	AnimName.TrimStartInline();
	while (AnimName.StartsWith(TEXT("_")) || AnimName.StartsWith(TEXT("-")) || AnimName.StartsWith(TEXT(".")))
	{
		AnimName.RightChopInline(1, EAllowShrinking::No);
	}

	AnimName.ReplaceInline(TEXT(" "), TEXT("_"));
	const TCHAR IllegalChars[] = TEXT("/\\:*?\"<>|");
	for (int32 i = 0; IllegalChars[i] != '\0'; ++i)
	{
		AnimName.ReplaceCharInline(IllegalChars[i], TEXT('_'));
	}

	return FString::Printf(TEXT("%s_%s"), *UniqueSafeName, *AnimName);
}

static void AppendRootPackageAnimations(const FString& BasePackagePath, TArray<UObject*>& AnimationObjects)
{
	TSet<FString> SeenPaths;
	for (UObject* AnimationObject : AnimationObjects)
	{
		if (AnimationObject)
		{
			SeenPaths.Add(AnimationObject->GetPathName());
		}
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	TArray<FAssetData> RootAssets;
	AssetRegistryModule.Get().GetAssetsByPath(FName(*BasePackagePath), RootAssets, false);

	int32 AddedCount = 0;
	for (const FAssetData& AssetData : RootAssets)
	{
		if (AssetData.AssetClassPath != UAnimSequence::StaticClass()->GetClassPathName())
		{
			continue;
		}

		UObject* AnimationAsset = AssetData.GetAsset();
		if (!AnimationAsset)
		{
			continue;
		}

		const FString ObjectPath = AnimationAsset->GetPathName();
		if (SeenPaths.Contains(ObjectPath))
		{
			continue;
		}

		AnimationObjects.Add(AnimationAsset);
		SeenPaths.Add(ObjectPath);
		AddedCount++;
	}

	if (AddedCount > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[Tripo] Recovered %d animation asset(s) from root package scan"), AddedCount);
	}
}

// ————— Public API —————

bool FTripoModelImporter::PrepareModelFile(
	const FString& FileId,
	const FString& FileName,
	const FString& FileType,
	const TArray<uint8>& FileData,
	FTripoImportContext& OutContext)
{
	OutContext.FileId = FileId;
	OutContext.FileName = FileName;
	OutContext.FileType = FileType;

	UE_LOG(LogTemp, Log, TEXT("Preparing model file: %s (Type: %s, Size: %.2f MB)"),
		*FileName, *FileType, FileData.Num() / 1024.0f / 1024.0f);

	// Create temp directory
	OutContext.TempDir = FPaths::Combine(
		FPaths::ProjectIntermediateDir(),
		TripoProtocol::TEMP_DIR_NAME,
		FileId
	);

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	if (!PlatformFile.CreateDirectoryTree(*OutContext.TempDir))
	{
		OutContext.ErrorMessage = FString::Printf(TEXT("Failed to create temp directory: %s"), *OutContext.TempDir);
		UE_LOG(LogTemp, Error, TEXT("%s"), *OutContext.ErrorMessage);
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("Created temp directory: %s"), *OutContext.TempDir);

	// Handle different file types
	if (FileType.Equals(TEXT("zip"), ESearchCase::IgnoreCase))
	{
		UE_LOG(LogTemp, Log, TEXT("Extracting ZIP file: %s"), *FileName);

		if (!ExtractZip(FileData, OutContext.TempDir))
		{
			OutContext.ErrorMessage = TEXT("Failed to extract ZIP file");
			UE_LOG(LogTemp, Error, TEXT("%s"), *OutContext.ErrorMessage);
			return false;
		}

		if (!FindModelFile(OutContext.TempDir, OutContext.ModelFilePath))
		{
			OutContext.ErrorMessage = TEXT("No FBX, GLB, GLTF, or OBJ file found in ZIP archive");
			UE_LOG(LogTemp, Error, TEXT("%s"), *OutContext.ErrorMessage);
			return false;
		}
		UE_LOG(LogTemp, Log, TEXT("Found model file: %s"), *FPaths::GetCleanFilename(OutContext.ModelFilePath));
	}
	else if (FileType.Equals(TEXT("fbx"), ESearchCase::IgnoreCase) ||
	         FileType.Equals(TEXT("obj"), ESearchCase::IgnoreCase) ||
	         FileType.Equals(TEXT("glb"), ESearchCase::IgnoreCase) ||
	         FileType.Equals(TEXT("gltf"), ESearchCase::IgnoreCase))
	{
		OutContext.ModelFilePath = FPaths::Combine(OutContext.TempDir, FileName);
		if (!FFileHelper::SaveArrayToFile(FileData, *OutContext.ModelFilePath))
		{
			OutContext.ErrorMessage = FString::Printf(TEXT("Failed to save model file to: %s"), *OutContext.ModelFilePath);
			UE_LOG(LogTemp, Error, TEXT("%s"), *OutContext.ErrorMessage);
			return false;
		}
	}
	else
	{
		OutContext.ErrorMessage = FString::Printf(TEXT("Unsupported file type: %s"), *FileType);
		UE_LOG(LogTemp, Error, TEXT("%s"), *OutContext.ErrorMessage);
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("Model file prepared successfully: %s"), *OutContext.ModelFilePath);
	return true;
}

bool FTripoModelImporter::ImportModelToEngine(const FTripoImportContext& Context)
{
	check(IsInGameThread());

	if (!Context.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("Invalid import context"));
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("Importing model to engine: %s"), *Context.FileName);

	FString SafeName = SanitizeAssetName(Context.FileName, Context.FileId);
	FString AssetPackagePath = MakeUniqueAssetPath(
		FPaths::Combine(TripoProtocol::ASSET_IMPORT_ROOT, SafeName));

	FString UniqueSafeName = FPaths::GetCleanFilename(AssetPackagePath);

	UObject* ImportedAsset = nullptr;
	if (!ImportFBX(Context.ModelFilePath, AssetPackagePath, ImportedAsset))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to import model: %s"), *Context.ModelFilePath);
		DeleteDirectoryWithRetry(Context.TempDir);
		return false;
	}

	// Rename imported mesh, material, and texture assets to use the display name
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		TArray<FAssetData> AllAssets;
		AssetRegistryModule.Get().GetAssetsByPath(FName(*AssetPackagePath), AllAssets, true);

		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
		TArray<FAssetRenameData> RenameList;

		TArray<FString> TextureSuffixes = {
			TEXT("_basecolor"), TEXT("_base_color"), TEXT("_albedo"),
			TEXT("_normal"), TEXT("_normalmap"), TEXT("_norm"),
			TEXT("_roughness"), TEXT("_rough"),
			TEXT("_metallic"), TEXT("_metalness"),
			TEXT("_ao"), TEXT("_ambientocclusion"),
			TEXT("_emissive"), TEXT("_emission"),
			TEXT("_opacity"), TEXT("_alpha"),
			TEXT("_height"), TEXT("_displacement")
		};

		int32 MatIndex = 0;
		int32 TexIndex = 0;
		for (const FAssetData& AssetData : AllAssets)
		{
			UObject* Asset = AssetData.GetAsset();
			if (!Asset) continue;

			FString NewName;
			FString OldName = Asset->GetName();

			if (Asset->IsA<UStaticMesh>() || Asset->IsA<USkeletalMesh>())
			{
				NewName = UniqueSafeName;
			}
			else if (Asset->IsA<UMaterialInterface>())
			{
				NewName = (MatIndex == 0)
					? FString::Printf(TEXT("%s_Mat"), *UniqueSafeName)
					: FString::Printf(TEXT("%s_Mat_%d"), *UniqueSafeName, MatIndex);
				MatIndex++;
			}
			else if (Asset->IsA<UTexture2D>())
			{
				FString LowerOldName = OldName.ToLower();
				FString PreservedSuffix;

				for (const FString& Suffix : TextureSuffixes)
				{
					if (LowerOldName.Contains(Suffix))
					{
						PreservedSuffix = Suffix;
						break;
					}
				}

				if (!PreservedSuffix.IsEmpty())
				{
					NewName = FString::Printf(TEXT("%s%s"), *UniqueSafeName, *PreservedSuffix);
				}
				else
				{
					NewName = (TexIndex == 0)
						? FString::Printf(TEXT("%s_Tex"), *UniqueSafeName)
						: FString::Printf(TEXT("%s_Tex_%d"), *UniqueSafeName, TexIndex);
					TexIndex++;
				}
			}
			else if (Asset->IsA<USkeleton>())
			{
				NewName = FString::Printf(TEXT("%s_Skeleton"), *UniqueSafeName);
			}
			else if (Asset->IsA<UPhysicsAsset>())
			{
				NewName = FString::Printf(TEXT("%s_PhysicsAsset"), *UniqueSafeName);
			}

			if (!NewName.IsEmpty() && OldName != NewName)
			{
				FAssetRenameData RenameData;
				RenameData.Asset = Asset;
				RenameData.NewPackagePath = AssetData.PackagePath.ToString();
				RenameData.NewName = NewName;
				RenameList.Add(RenameData);
			}
		}

		if (RenameList.Num() > 0)
		{
			AssetTools.RenameAssets(RenameList);
			UE_LOG(LogTemp, Log, TEXT("[Tripo] Renamed %d assets to use display name: %s"), RenameList.Num(), *UniqueSafeName);

			if (ImportedAsset)
			{
				FString MeshPath = FString::Printf(TEXT("%s/%s.%s"), *AssetPackagePath, *UniqueSafeName, *UniqueSafeName);
				UObject* Renamed = LoadObject<UObject>(nullptr, *MeshPath);
				if (Renamed)
				{
					ImportedAsset = Renamed;
				}
			}
		}
	}

	FString ActorName = Context.FileName;
	if (ActorName.EndsWith(TEXT(".zip"), ESearchCase::IgnoreCase))
	{
		ActorName = FPaths::GetBaseFilename(ActorName);
	}
	if (ImportedAsset && !SpawnInLevel(ImportedAsset, ActorName))
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to spawn model in level (import succeeded)"));
	}

	DeleteDirectoryWithRetry(Context.TempDir);

	UE_LOG(LogTemp, Log, TEXT("Successfully imported %s"), *Context.FileName);
	return true;
}

bool FTripoModelImporter::ImportModel(
	const FString& FileId,
	const FString& FileName,
	const FString& FileType,
	const TArray<uint8>& FileData)
{
	FTripoImportContext Context;

	if (!PrepareModelFile(FileId, FileName, FileType, FileData, Context))
	{
		if (!Context.TempDir.IsEmpty())
		{
			DeleteDirectoryWithRetry(Context.TempDir);
		}
		return false;
	}

	return ImportModelToEngine(Context);
}

// ————— Internal —————

bool FTripoModelImporter::ExtractZip(const TArray<uint8>& ZipData, const FString& ExtractPath)
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	FString NormalizedExtractRoot = FPaths::ConvertRelativePathToFull(ExtractPath);
	constexpr int64 MaxZipEntrySizeBytes = 500ll * 1024ll * 1024ll;
	FPaths::NormalizeFilename(NormalizedExtractRoot);
	FPaths::CollapseRelativeDirectories(NormalizedExtractRoot);
	if (!NormalizedExtractRoot.EndsWith(TEXT("/")))
	{
		NormalizedExtractRoot += TEXT("/");
	}
	
	// Create extract directory
	if (!PlatformFile.CreateDirectoryTree(*ExtractPath))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to create extraction directory: %s"), *ExtractPath);
		return false;
	}

	// Setup memory stream for minizip (no temporary file needed)
	FMinizipMemoryStream MemStream;
	MemStream.Data = ZipData.GetData();
	MemStream.Size = ZipData.Num();
	MemStream.Position = 0;

	zlib_filefunc_def FileFuncs;
	FileFuncs.zopen_file = MemOpen;
	FileFuncs.zread_file = MemRead;
	FileFuncs.zwrite_file = MemWrite;
	FileFuncs.ztell_file = MemTell;
	FileFuncs.zseek_file = MemSeek;
	FileFuncs.zclose_file = MemClose;
	FileFuncs.zerror_file = MemError;
	FileFuncs.opaque = &MemStream;

	// Open ZIP from memory
	unzFile ZipFile = unzOpen2(nullptr, &FileFuncs);
	if (!ZipFile)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to open ZIP from memory"));
		return false;
	}

	// Get global info
	unz_global_info GlobalInfo;
	if (unzGetGlobalInfo(ZipFile, &GlobalInfo) != UNZ_OK)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to get ZIP global info"));
		unzClose(ZipFile);
		return false;
	}

	// Extract all files
	for (uLong i = 0; i < GlobalInfo.number_entry; ++i)
	{
		char Filename[512];
		unz_file_info FileInfo;

		if (unzGetCurrentFileInfo(ZipFile, &FileInfo, Filename, sizeof(Filename), nullptr, 0, nullptr, 0) != UNZ_OK)
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to get file info for entry %d"), i);
			continue;
		}

		FString EntryName = UTF8_TO_TCHAR(Filename);
		const bool bIsDirectory = EntryName.EndsWith(TEXT("/")) || EntryName.EndsWith(TEXT("\\"));
		FPaths::NormalizeFilename(EntryName);

		if (EntryName.IsEmpty() || EntryName.StartsWith(TEXT("/")) || EntryName.Contains(TEXT(":")))
		{
			UE_LOG(LogTemp, Error, TEXT("Skipping suspicious ZIP entry: %s"), *EntryName);
			if ((i + 1) < GlobalInfo.number_entry)
			{
				unzGoToNextFile(ZipFile);
			}
			continue;
		}

		FString FullPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(NormalizedExtractRoot, EntryName));
		FPaths::NormalizeFilename(FullPath);
		FPaths::CollapseRelativeDirectories(FullPath);

		if (!FullPath.StartsWith(NormalizedExtractRoot, ESearchCase::IgnoreCase))
		{
			UE_LOG(LogTemp, Error, TEXT("Skipping suspicious ZIP entry: %s"), *EntryName);
			if ((i + 1) < GlobalInfo.number_entry)
			{
				unzGoToNextFile(ZipFile);
			}
			continue;
		}

		// Check if it's a directory
		if (bIsDirectory)
		{
			PlatformFile.CreateDirectoryTree(*FullPath);
		}
		else
		{
			// Create directory for file
			FString DirectoryPath = FPaths::GetPath(FullPath);
			PlatformFile.CreateDirectoryTree(*DirectoryPath);

			// Open file in ZIP
			if (unzOpenCurrentFile(ZipFile) != UNZ_OK)
			{
				UE_LOG(LogTemp, Error, TEXT("Failed to open file in ZIP: %s"), *EntryName);
				unzGoToNextFile(ZipFile);
				continue;
			}

			// Read and extract file
			if (static_cast<int64>(FileInfo.uncompressed_size) > MaxZipEntrySizeBytes)
			{
				UE_LOG(LogTemp, Error, TEXT("Skipping oversized ZIP entry (%lld bytes): %s"),
					static_cast<long long>(FileInfo.uncompressed_size), *EntryName);
				unzCloseCurrentFile(ZipFile);
				unzGoToNextFile(ZipFile);
				continue;
			}

			TArray<uint8> FileData;
			FileData.SetNumUninitialized(static_cast<int32>(FileInfo.uncompressed_size));

			int BytesRead = unzReadCurrentFile(ZipFile, FileData.GetData(), FileData.Num());
			unzCloseCurrentFile(ZipFile);

			if (BytesRead < 0)
			{
				UE_LOG(LogTemp, Error, TEXT("Failed to read file from ZIP: %s"), *EntryName);
				unzGoToNextFile(ZipFile);
				continue;
			}

			// Write to disk
			if (!FFileHelper::SaveArrayToFile(FileData, *FullPath))
			{
				UE_LOG(LogTemp, Error, TEXT("Failed to write extracted file: %s"), *FullPath);
				unzGoToNextFile(ZipFile);
				continue;
			}
		}

		// Move to next file
		if ((i + 1) < GlobalInfo.number_entry)
		{
			if (unzGoToNextFile(ZipFile) != UNZ_OK)
			{
				UE_LOG(LogTemp, Error, TEXT("Failed to move to next file in ZIP"));
				break;
			}
		}
	}

	unzClose(ZipFile);

	UE_LOG(LogTemp, Log, TEXT("Extracted %d files from ZIP"), GlobalInfo.number_entry);
	return true;
}

bool FTripoModelImporter::FindModelFile(const FString& DirectoryPath, FString& OutModelPath)
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	
	TArray<FString> FoundFiles;
	PlatformFile.FindFilesRecursively(FoundFiles, *DirectoryPath, nullptr);

	int32 BestPriority = TNumericLimits<int32>::Max();
	FString BestMatch;

	for (const FString& FilePath : FoundFiles)
	{
		FString Extension = FPaths::GetExtension(FilePath).ToLower();
		int32 CurrentPriority = TNumericLimits<int32>::Max();

		if (Extension == TEXT("fbx"))
		{
			CurrentPriority = 0;
		}
		else if (Extension == TEXT("glb"))
		{
			CurrentPriority = 1;
		}
		else if (Extension == TEXT("gltf"))
		{
			CurrentPriority = 2;
		}
		else if (Extension == TEXT("obj"))
		{
			CurrentPriority = 3;
		}

		if (CurrentPriority < BestPriority)
		{
			BestPriority = CurrentPriority;
			BestMatch = FilePath;

			if (BestPriority == 0)
			{
				break;
			}
		}
	}

	if (BestPriority != TNumericLimits<int32>::Max())
	{
		OutModelPath = BestMatch;
		return true;
	}

	return false;
}

bool FTripoModelImporter::IsNormalMapTexture(const FString& Filename)
{
	FString LowerFilename = Filename.ToLower();
	
	// Check for common normal map naming patterns
	return LowerFilename.Contains(TEXT("normal")) ||
	       LowerFilename.Contains(TEXT("_n.")) ||
	       LowerFilename.Contains(TEXT("_nrm.")) ||
	       LowerFilename.Contains(TEXT("_norm.")) ||
	       LowerFilename.EndsWith(TEXT("_n.png")) ||
	       LowerFilename.EndsWith(TEXT("_n.jpg")) ||
	       LowerFilename.EndsWith(TEXT("_n.tga")) ||
	       LowerFilename.EndsWith(TEXT("_n.tif"));
}

int32 FTripoModelImporter::ImportTextures(const FString& SourcePath, const FString& DestPackagePath)
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	TArray<FString> TextureExtensions = { TEXT("png"), TEXT("jpg"), TEXT("jpeg"), TEXT("tga"), TEXT("bmp"), TEXT("tif"), TEXT("tiff") };
	TArray<FString> FoundFiles;
	PlatformFile.FindFilesRecursively(FoundFiles, *SourcePath, nullptr);

	int32 ImportCount = 0;
	UTextureFactory* TextureFactory = NewObject<UTextureFactory>();
	TextureFactory->AddToRoot(); // Prevent GC during import

	for (const FString& FilePath : FoundFiles)
	{
		FString Extension = FPaths::GetExtension(FilePath).ToLower();
		if (!TextureExtensions.Contains(Extension))
		{
			continue;
		}

		FString Filename = FPaths::GetBaseFilename(FilePath);
		FString PackageName = FPaths::Combine(DestPackagePath, Filename);
		
		// Configure texture factory for normal maps
		bool bIsNormalMap = IsNormalMapTexture(Filename);
		if (bIsNormalMap)
		{
			UE_LOG(LogTemp, Log, TEXT("Detected normal map: %s"), *Filename);
			TextureFactory->CompressionSettings = TC_Normalmap;
			TextureFactory->LODGroup = TEXTUREGROUP_WorldNormalMap;
			TextureFactory->bFlipNormalMapGreenChannel = false;
		}
		else
		{
			TextureFactory->CompressionSettings = TC_Default;
			TextureFactory->LODGroup = TEXTUREGROUP_World;
		}

		// Import texture
		UPackage* Package = CreatePackage(*PackageName);
		Package->FullyLoad();

		bool bOutCanceled = false;
		UObject* ImportedTexture = TextureFactory->ImportObject(
			UTexture2D::StaticClass(),
			Package,
			*Filename,
			RF_Public | RF_Standalone,
			FilePath,
			nullptr,
			bOutCanceled
		);

		if (ImportedTexture)
		{
			// Additional normal map settings
			if (bIsNormalMap)
			{
				UTexture2D* Texture = Cast<UTexture2D>(ImportedTexture);
				if (Texture)
				{
					Texture->SRGB = false;
					Texture->CompressionSettings = TC_Normalmap;
					Texture->LODGroup = TEXTUREGROUP_WorldNormalMap;
					Texture->UpdateResource();
				}
			}

			FAssetRegistryModule::AssetCreated(ImportedTexture);
			ImportedTexture->MarkPackageDirty();
			ImportCount++;

			UE_LOG(LogTemp, Log, TEXT("Imported texture: %s%s"), 
				*Filename, bIsNormalMap ? TEXT(" (Normal Map)") : TEXT(""));
		}
	}

	TextureFactory->RemoveFromRoot();
	return ImportCount;
}

bool FTripoModelImporter::ImportFBX(const FString& SourcePath, const FString& DestPackagePath, UObject*& OutImportedAsset)
{
	// Verify file exists
	if (!FPaths::FileExists(SourcePath))
	{
		UE_LOG(LogTemp, Error, TEXT("FBX file not found: %s"), *SourcePath);
		return false;
	}

	// Create and configure AssetImportTask (UE5 recommended API).
	// UE 5.4 uses UFbxImportUI to force the legacy FBX importer.
	// UE 5.5+ leaves Options unset and falls back to the engine's default importer path.
	UAssetImportTask* ImportTask = NewObject<UAssetImportTask>();
	if (!ImportTask)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to create AssetImportTask"));
		return false;
	}

	ImportTask->Filename = SourcePath;
	ImportTask->DestinationPath = DestPackagePath;
	ImportTask->bAutomated = true;  // No dialogs
	ImportTask->bReplaceExisting = true;
	ImportTask->bSave = false;  // Don't auto-save, we'll handle that

	bool bUsingDefaultImporter = true;

#if TRIPO_HAS_FBX_IMPORT_UI
	UFbxImportUI* ImportUI = NewObject<UFbxImportUI>();
	if (!ImportUI)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to create UFbxImportUI for legacy FBX validation branch"));
		return false;
	}

	ImportUI->bImportMesh = true;
	ImportUI->bImportAnimations = true;
	ImportUI->bImportAsSkeletal = true;
	ImportUI->bAutomatedImportShouldDetectType = false;
	ImportUI->MeshTypeToImport = FBXIT_SkeletalMesh;
	ImportUI->OriginalImportType = FBXIT_SkeletalMesh;
	ImportUI->bImportMaterials = true;
	ImportUI->bImportTextures = true;

#if TRIPO_HAS_SKELETAL_GEOMETRY_FLAG
	ImportUI->bImportAsSkeletalGeometry = true;
	UE_LOG(LogTemp, Log, TEXT("[Tripo] Legacy FBX validation branch: bImportAsSkeletalGeometry = true"));
#endif

	UFbxStaticMeshImportData* StaticMeshData = ImportUI->StaticMeshImportData;
	if (StaticMeshData)
	{
		StaticMeshData->ImportUniformScale = TRIPO_FBX_SCALE_FACTOR;
		StaticMeshData->bCombineMeshes = false;
		StaticMeshData->bGenerateLightmapUVs = true;
		StaticMeshData->bAutoGenerateCollision = true;

		UE_LOG(LogTemp, Log, TEXT("[Tripo] Legacy FBX validation branch scale factor: %.2f"),
			StaticMeshData->ImportUniformScale);
	}

#if TRIPO_NEEDS_SKELETAL_MESH_SCALE_FIX
	// UE5.4: skeletal mesh ImportUniformScale is not inherited from static mesh settings,
	// must be set explicitly or the mesh will be 100x too small.
	UFbxSkeletalMeshImportData* SkeletalMeshData = ImportUI->SkeletalMeshImportData;
	if (SkeletalMeshData)
	{
		SkeletalMeshData->ImportUniformScale = TRIPO_FBX_SCALE_FACTOR;
		UE_LOG(LogTemp, Log, TEXT("[Tripo][UE5.4] Skeletal mesh scale factor: %.2f"), SkeletalMeshData->ImportUniformScale);
	}
#endif // TRIPO_NEEDS_SKELETAL_MESH_SCALE_FIX

	UFbxAnimSequenceImportData* AnimData = ImportUI->AnimSequenceImportData;
	if (AnimData)
	{
		AnimData->bSnapToClosestFrameBoundary = true;
	}

	UFbxTextureImportData* TextureData = ImportUI->TextureImportData;
	if (TextureData)
	{
		TextureData->MaterialSearchLocation = TRIPO_MATERIAL_SEARCH_LOCATION;
	}

	ImportTask->Options = ImportUI;
	bUsingDefaultImporter = false;

	UE_LOG(LogTemp, Log, TEXT("[Tripo] Legacy FBX validation branch: using UFbxImportUI"));
	UE_LOG(LogTemp, Log, TEXT("[Tripo] Legacy FBX import config: detectType=%s, importAsSkeletal=%s, importAnimations=%s"),
		ImportUI->bAutomatedImportShouldDetectType ? TEXT("true") : TEXT("false"),
		ImportUI->bImportAsSkeletal ? TEXT("true") : TEXT("false"),
		ImportUI->bImportAnimations ? TEXT("true") : TEXT("false"));
	UE_LOG(LogTemp, Log, TEXT("[Tripo] bImportMaterials: %s, bImportTextures: %s"),
		ImportUI->bImportMaterials ? TEXT("true") : TEXT("false"),
		ImportUI->bImportTextures ? TEXT("true") : TEXT("false"));
#else
	UInterchangePipelineStackOverride* PipelineOverride = NewObject<UInterchangePipelineStackOverride>();
	if (!PipelineOverride)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to create UInterchangePipelineStackOverride"));
		return false;
	}

	UInterchangeGenericAssetsPipeline* GenericAssetsPipeline = NewObject<UInterchangeGenericAssetsPipeline>(PipelineOverride);
	if (!GenericAssetsPipeline || !GenericAssetsPipeline->AnimationPipeline)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to create Interchange generic assets pipeline override"));
		return false;
	}

	GenericAssetsPipeline->AnimationPipeline->bSnapToClosestFrameBoundary = true;
	PipelineOverride->AddPipeline(GenericAssetsPipeline);
	ImportTask->Options = PipelineOverride;

	UE_LOG(LogTemp, Log, TEXT("[Tripo] Interchange override: bSnapToClosestFrameBoundary = true"));
	UE_LOG(LogTemp, Log, TEXT("[Tripo] Validation branch: using bare UAssetImportTask with default importer auto-detection (UE 5.5+)"));
#endif

	UE_LOG(LogTemp, Log, TEXT("Starting FBX import: %s -> %s"), *SourcePath, *DestPackagePath);
	UE_LOG(LogTemp, Log, TEXT("[Tripo] ImportTask->Options assigned: %s"),
		bUsingDefaultImporter ? TEXT("false") : TEXT("true"));

	// Execute import
	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
	AssetToolsModule.Get().ImportAssetTasks({ImportTask});

	// Check import results
	if (ImportTask->ImportedObjectPaths.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("FBX import failed - no objects imported from: %s"), *SourcePath);
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("FBX import succeeded, imported %d objects"), ImportTask->ImportedObjectPaths.Num());

	// Find the main mesh asset from imported objects BEFORE organizing
	// (organizing will move assets and invalidate paths)
	int32 ImportedAnimationCount = 0;
	int32 ImportedSkeletonCount = 0;
	for (const FString& ImportedPath : ImportTask->ImportedObjectPaths)
	{
		UObject* ImportedObject = LoadObject<UObject>(nullptr, *ImportedPath);
		if (!ImportedObject)
		{
			continue;
		}

		UE_LOG(LogTemp, Log, TEXT("Imported object: %s (%s)"), *ImportedObject->GetName(), *ImportedObject->GetClass()->GetName());

		if (ImportedObject->IsA<UAnimSequence>())
		{
			ImportedAnimationCount++;
		}
		else if (ImportedObject->GetClass()->GetName() == TEXT("Skeleton"))
		{
			ImportedSkeletonCount++;
		}

		// Find StaticMesh or SkeletalMesh
		if (ImportedObject->IsA<UStaticMesh>() || ImportedObject->IsA<USkeletalMesh>())
		{
			OutImportedAsset = ImportedObject;
			UE_LOG(LogTemp, Log, TEXT("Found main mesh asset: %s"), *OutImportedAsset->GetName());
			break;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[Tripo] Imported animation assets: %d, skeleton assets: %d"),
		ImportedAnimationCount, ImportedSkeletonCount);

	// Organize imported assets into folders (do this after finding the mesh)
	FString FbxBaseName = FPaths::GetBaseFilename(SourcePath);
	OrganizeImportedAssets(ImportTask->ImportedObjectPaths, DestPackagePath, FbxBaseName);

	// Return success if we found a mesh
	if (OutImportedAsset)
	{
		UE_LOG(LogTemp, Log, TEXT("Successfully imported FBX: %s (%s)"), 
			*OutImportedAsset->GetName(), *OutImportedAsset->GetClass()->GetName());
		return true;
	}

	// If no mesh found but objects were imported, try to find any valid object after organizing
	if (ImportTask->ImportedObjectPaths.Num() > 0)
	{
		// Scan the destination folder for any mesh assets
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		TArray<FAssetData> AssetDataList;
		AssetRegistryModule.Get().GetAssetsByPath(FName(*DestPackagePath), AssetDataList, true);
		
		for (const FAssetData& AssetData : AssetDataList)
		{
			if (AssetData.AssetClassPath == UStaticMesh::StaticClass()->GetClassPathName() ||
			    AssetData.AssetClassPath == USkeletalMesh::StaticClass()->GetClassPathName())
			{
				OutImportedAsset = AssetData.GetAsset();
				if (OutImportedAsset)
				{
					UE_LOG(LogTemp, Warning, TEXT("Found mesh after organizing: %s (%s)"), 
						*OutImportedAsset->GetName(), *OutImportedAsset->GetClass()->GetName());
					return true;
				}
			}
		}
	}

	UE_LOG(LogTemp, Error, TEXT("No suitable asset found in import results"));
	return false;
}

void FTripoModelImporter::OrganizeImportedAssets(const TArray<FString>& ImportedObjectPaths, const FString& BasePackagePath, const FString& FbxBaseName)
{
	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
	IAssetTools& AssetTools = AssetToolsModule.Get();

	// Single pass: load each object once and classify
	TArray<UObject*> TextureObjects;
	TArray<UObject*> AnimationObjects;

	for (const FString& ImportedPath : ImportedObjectPaths)
	{
		UObject* ImportedObject = LoadObject<UObject>(nullptr, *ImportedPath);
		if (!ImportedObject)
		{
			continue;
		}

		if (ImportedObject->IsA<UTexture>())
		{
			TextureObjects.Add(ImportedObject);
		}
		else if (ImportedObject->GetClass()->GetName() == TEXT("AnimSequence"))
		{
			AnimationObjects.Add(ImportedObject);
		}
	}

	if (AnimationObjects.Num() == 0)
	{
		AppendRootPackageAnimations(BasePackagePath, AnimationObjects);
	}

	// Move textures to Textures subfolder
	if (TextureObjects.Num() > 0)
	{
		FString TexturesFolder = FPaths::Combine(BasePackagePath, TEXT("Textures"));

		TArray<FAssetRenameData> RenameData;
		for (UObject* TextureObject : TextureObjects)
		{
			FAssetRenameData RenameEntry;
			RenameEntry.Asset = TextureObject;
			RenameEntry.NewPackagePath = TexturesFolder;
			RenameEntry.NewName = TextureObject->GetName();
			RenameData.Add(RenameEntry);
		}

		AssetTools.RenameAssets(RenameData);
		UE_LOG(LogTemp, Log, TEXT("Moved %d texture(s) to Textures folder"), RenameData.Num());
	}

	// Move animations to Animations subfolder
	if (AnimationObjects.Num() > 0)
	{
		FString AnimationsFolder = FPaths::Combine(BasePackagePath, TEXT("Animations"));

		TArray<FAssetRenameData> RenameData;
		for (UObject* AnimObject : AnimationObjects)
		{
			FAssetRenameData RenameEntry;
			RenameEntry.Asset = AnimObject;
			RenameEntry.NewPackagePath = AnimationsFolder;
			RenameEntry.NewName = BuildAnimationAssetName(BasePackagePath, FbxBaseName, AnimObject->GetName());

			RenameData.Add(RenameEntry);
		}

		AssetTools.RenameAssets(RenameData);
		UE_LOG(LogTemp, Log, TEXT("Moved %d animation(s) to Animations folder"), RenameData.Num());
	}
}

bool FTripoModelImporter::SpawnInLevel(UObject* Asset, const FString& ModelName)
{
	if (!Asset)
	{
		return false;
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("No valid world found for spawning actor"));
		return false;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	// Don't set a fixed name - let UE generate unique names automatically
	// SpawnParams.Name = FName(*ModelName);

	AActor* SpawnedActor = nullptr;

	// Handle Static Mesh
	if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(Asset))
	{
		SpawnedActor = World->SpawnActor<AActor>(AActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
		if (SpawnedActor)
		{
			UStaticMeshComponent* MeshComponent = NewObject<UStaticMeshComponent>(SpawnedActor);
			MeshComponent->SetStaticMesh(StaticMesh);
			MeshComponent->RegisterComponent();
			SpawnedActor->SetRootComponent(MeshComponent);

			// Validate materials were imported and applied
			if (MeshComponent && MeshComponent->GetStaticMesh())
			{
				int32 NumMaterials = MeshComponent->GetNumMaterials();
				UE_LOG(LogTemp, Log, TEXT("[Tripo] Spawned static mesh has %d material slot(s)"), NumMaterials);

				// Check each material slot
				for (int32 i = 0; i < NumMaterials; i++)
				{
					UMaterialInterface* Mat = MeshComponent->GetMaterial(i);
					if (!Mat)
					{
						UE_LOG(LogTemp, Warning, TEXT("[Tripo] Material slot %d is null"), i);
					}
					else
					{
						UE_LOG(LogTemp, Log, TEXT("[Tripo] Material slot %d: %s"), i, *Mat->GetName());
					}
				}
			}
		}
	}
	// Handle Skeletal Mesh
	else if (USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(Asset))
	{
		SpawnedActor = World->SpawnActor<AActor>(AActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
		if (SpawnedActor)
		{
			USkeletalMeshComponent* MeshComponent = NewObject<USkeletalMeshComponent>(SpawnedActor);
			MeshComponent->SetSkeletalMesh(SkeletalMesh);
			MeshComponent->RegisterComponent();
			SpawnedActor->SetRootComponent(MeshComponent);
		}
	}

	if (SpawnedActor)
	{
		// Set actor label (display name) without causing name conflicts
		SpawnedActor->SetActorLabel(ModelName);
		
		GEditor->SelectNone(false, true);
		GEditor->SelectActor(SpawnedActor, true, true);
		
		UE_LOG(LogTemp, Log, TEXT("Spawned actor: %s"), *SpawnedActor->GetActorLabel());
		return true;
	}

	return false;
}

void FTripoModelImporter::DeleteDirectoryWithRetry(const FString& DirectoryPath, int32 MaxRetries, float RetryDelay)
{
	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [DirectoryPath, MaxRetries, RetryDelay]()
	{
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

		for (int32 i = 0; i < MaxRetries; ++i)
		{
			if (PlatformFile.DeleteDirectoryRecursively(*DirectoryPath))
			{
				UE_LOG(LogTemp, Log, TEXT("Deleted temp directory: %s"), *DirectoryPath);
				return;
			}

			if (i < MaxRetries - 1)
			{
				UE_LOG(LogTemp, Warning, TEXT("Failed to delete directory (attempt %d/%d), retrying..."),
					i + 1, MaxRetries);
				FPlatformProcess::Sleep(RetryDelay);
			}
		}

		UE_LOG(LogTemp, Error, TEXT("Failed to delete directory after %d attempts: %s"),
			MaxRetries, *DirectoryPath);
	});
}
