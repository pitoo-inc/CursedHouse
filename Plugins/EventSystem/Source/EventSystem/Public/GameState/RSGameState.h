// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "RSGameState.generated.h"

/**
 * 
 */
UCLASS()
class EVENTSYSTEM_API ARSGameState : public AGameStateBase
{
	GENERATED_BODY()
	
public:
	// 共有アイテムボックスのリスト
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "RS|Inventory")
	TArray<FName> GlobalBoxItems;

	// 共有アイテムの追加
	UFUNCTION(BlueprintCallable, Category = "RS|Inventory")
	void AddGlobalItem(FName ItemID);

	// 共有アイテムの所持チェック
	UFUNCTION(BlueprintPure, Category = "RS|Inventory")
	bool HasGlobalItem(FName ItemID) const;

	// 共有アイテムの削除
	UFUNCTION(BlueprintCallable, Category = "RS|Inventory")
	void RemoveGlobalItem(FName ItemID);
};
