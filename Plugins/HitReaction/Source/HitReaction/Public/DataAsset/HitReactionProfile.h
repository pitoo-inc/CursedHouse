// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Animation/AnimMontage.h"
#include "HitReactionProfile.generated.h"


// --- 部位ごとのよろけ設定構造体 ---
USTRUCT(BlueprintType)
struct HITREACTION_API FStaggerData
{
	GENERATED_BODY()

	// よろけを誘発するために必要な累積ダメージ量
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stagger")
	float Threshold = 100.0f;

	// 再生するアニメーションモンタージュ
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stagger")
	TObjectPtr<UAnimMontage> StaggerMontage;

	// この部位でよろけた時に格闘（メレー）が可能になるか
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stagger")
	bool bCanTriggerMelee = false;
};

/**
 * 
 */
UCLASS()
class HITREACTION_API UHitReactionProfile : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// 基本体力（部位とは別の全体HP用）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Stats")
	float MaxHealth = 100.0f;

	// 部位（物理マテリアル）と設定のマップ
	// Key: Physical Material, Value: よろけ設定
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Stats")
	TMap<TObjectPtr<UPhysicalMaterial>, FStaggerData> StaggerSettings;

	// UStaggerProfile クラス内に追加
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stagger | Recovery")
	float RecoveryRatePerSecond = 5.0f; // 1秒間に回復するダメージ蓄積値
};
