// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Actor/InteractableBase.h"
#include "GimmickTriggerBase.generated.h"

class UConditionSourceComponent; // 独自コンポーネント

/**
 * 
 */
UCLASS()
class EVENTSYSTEM_API AGimmickTriggerBase : public AInteractableBase
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AGimmickTriggerBase();

protected:
	// ==========================================
	// コンポーネント
	// ==========================================
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UConditionSourceComponent* ConditionSource; // 条件ソースコンポーネント

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Switch")
	bool bIsToggled = false; // スイッチのON/OFF状態
};
