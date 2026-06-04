// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "TriggerObserver.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UTriggerObserver : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class EVENTSYSTEM_API ITriggerObserver
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	// Blueprintで実装可能な関数。プレイヤーからのインタラクション時に呼び出される。
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Interaction")
	void OnActorEntered(AActor* InstigatorActor);
};
