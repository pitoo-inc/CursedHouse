// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Types/InteractionType.h"
#include "InteractionHUDWidget.generated.h"

/**
 * 
 */
UCLASS()
class EVENTSYSTEM_API UInteractionHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 近距離・遠距離のUI状態を切り替える関数（Blueprintで実装）
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "UI")
	void SetUIState(EInteractionUIState NewState);

	// UI更新用のイベント（BP側で実装する）
	UFUNCTION(BlueprintImplementableEvent, Category = "RS|UI")
	void UpdateTextPrompt(EInteractionType Type);
};
