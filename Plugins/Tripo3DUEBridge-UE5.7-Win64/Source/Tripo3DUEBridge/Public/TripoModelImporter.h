// Copyright Tripo3D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FTripoImportContext
{
	FString FileId;
	FString FileName;
	FString FileType;
	FString ModelFilePath;
	FString TempDir;
	FString ErrorMessage;

	bool IsValid() const { return !ModelFilePath.IsEmpty(); }
};

/**
 * Model importer for Tripo3D assets
 * Handles ZIP extraction, texture import, and model import (FBX, GLB, GLTF, OBJ)
 */
class TRIPO3DUEBRIDGE_API FTripoModelImporter
{
public:
	static bool ImportModel(
		const FString& FileId,
		const FString& FileName,
		const FString& FileType,
		const TArray<uint8>& FileData
	);

	// Phase 1: Extract/save files to disk. Thread-safe, no UObject access.
	static bool PrepareModelFile(
		const FString& FileId,
		const FString& FileName,
		const FString& FileType,
		const TArray<uint8>& FileData,
		FTripoImportContext& OutContext
	);

	// Phase 2: Import into UE engine. MUST be called on GameThread.
	static bool ImportModelToEngine(const FTripoImportContext& Context);

	static void DeleteDirectoryWithRetry(const FString& DirectoryPath, int32 MaxRetries = 5, float RetryDelay = 0.1f);

private:
	static bool ExtractZip(const TArray<uint8>& ZipData, const FString& ExtractPath);

	static bool FindModelFile(const FString& DirectoryPath, FString& OutModelPath);

	static bool IsNormalMapTexture(const FString& Filename);

	static int32 ImportTextures(const FString& SourcePath, const FString& DestPackagePath);

	static bool ImportFBX(const FString& SourcePath, const FString& DestPackagePath, UObject*& OutImportedAsset);

	static void OrganizeImportedAssets(const TArray<FString>& ImportedObjectPaths, const FString& BasePackagePath, const FString& FbxBaseName);

	static bool SpawnInLevel(UObject* Asset, const FString& ModelName);
};
