// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Types/InteractionType.h"
#include "InteractableInterface.generated.h"

// このインターフェースを実装するクラスは、インタラクションが可能であることを示す。
UINTERFACE(MinimalAPI)
class UInteractableInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class EVENTSYSTEM_API IInteractableInterface
{
	GENERATED_BODY()

public:
	// ギミックの種類を返す（UI側で使用）
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	EInteractionType GetInteractionType() const;

	// インタラクション実行（VR/非VR共通）
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	void OnInteract(AActor* InstigatorActor);

	// アイコンを表示すべき座標（Socketなど）を返す
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	FVector GetInteractionAnchorLocation() const;
};
