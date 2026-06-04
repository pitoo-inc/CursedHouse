// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ActivatableInterface.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UActivatableInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class EVENTSYSTEM_API IActivatableInterface
{
	GENERATED_BODY()

public:
// Blueprintで実装可能な関数。アクティベーションイベントを処理する。
UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Activation")
void OnActivate(bool bConditionMet, AActor* Activator);
};
