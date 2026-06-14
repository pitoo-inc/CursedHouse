// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Component/ConditionSourceComponent.h"
#include "GameEventData.generated.h"

//条件の組み合わせロジックを定義するEnum
UENUM(BlueprintType)
enum class EConditionLogic : uint8
{
	AND UMETA(DisplayName = "AND (All Conditions)"),
	OR UMETA(DisplayName = "OR (Any Conditions)"),
	COMMON UMETA(DisplayName = "COMMON (Common Conditions)"),
	Sequence    UMETA(DisplayName = "Sequence (In Order)")
};


// ターゲットアクターの挙動を定義するEnum
UENUM(BlueprintType)
enum class ETargetType : uint8
{
	EveryTime UMETA(DisplayName = "Every Time"),
	OneShot UMETA(DisplayName = "One-Shot (Once)"),
	Stateful UMETA(DisplayName = "Stateful (ON/OFF Control)"),
};

// 条件ソースの挙動を定義するEnum
USTRUCT(BlueprintType)
struct FConditionSource
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Game Event")
	TSoftObjectPtr<AActor> SourceActor;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Game Event")
	ESourceBehavior SourceBehavior;
};

// ターゲットアクターと、そのタイプを関連付ける構造体
USTRUCT(BlueprintType)
struct FEventTarget
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Game Event")
	TSoftObjectPtr<AActor> TargetActor;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Game Event")
	ETargetType TargetType;
};


/**
 * 
 */
UCLASS()
class EVENTSYSTEM_API UGameEventData : public UDataAsset
{
	GENERATED_BODY()

public :
	// イベントの対象となるアクターへのソフト参照。
// ドア、宝箱、ライト、壁など、種類を問わず設定可能。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Game Event")
	TArray<FEventTarget> EventTargets;

	// 条件ソースへのソフト参照と、その挙動のセット。条件ソースは、スイッチ、プレイヤーの位置、特定のアクションの完了など、様々なゲーム内要素を指すことができます。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Game Event")
	TArray<FConditionSource> ConditionSources;

	// 条件ソースの状態をどのように組み合わせてイベントをトリガーするかを定義するロジック。ANDはすべての条件が満たされたとき、ORはどれか一つの条件が満たされたときにイベントがトリガーされます。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Game Event")
	EConditionLogic ConditionLogic;

};
