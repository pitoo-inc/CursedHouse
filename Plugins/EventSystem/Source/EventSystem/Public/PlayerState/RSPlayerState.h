// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "RSPlayerState.generated.h"

/**
 * 
 */
UCLASS()
class EVENTSYSTEM_API ARSPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	// 個人の持ち物リスト（FNameの配列で軽量に管理）
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "RS|Inventory")
	TArray<FName> PersonalItems;

	// アイテムの追加
	UFUNCTION(BlueprintCallable, Category = "RS|Inventory")
	void AddPersonalItem(FName ItemID);

	// アイテムの所持チェック
	UFUNCTION(BlueprintPure, Category = "RS|Inventory")
	bool HasPersonalItem(FName ItemID) const;

	// アイテムの削除
	UFUNCTION(BlueprintCallable, Category = "RS|Inventory")
	void RemovePersonalItem(FName ItemID);
};
