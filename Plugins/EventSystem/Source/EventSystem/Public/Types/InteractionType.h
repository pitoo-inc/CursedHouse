// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "InteractionType.generated.h"

UENUM(BlueprintType)
enum class EInteractionType : uint8
{
	None        UMETA(DisplayName = "なし"),
	Examine     UMETA(DisplayName = "調べる"),
	PickUp      UMETA(DisplayName = "拾う"),
	Activate    UMETA(DisplayName = "起動/操作"),
	Talk        UMETA(DisplayName = "話す"),
	Destroy     UMETA(DisplayName = "攻撃/破壊"),
	Grab        UMETA(DisplayName = "掴む"),
};


// ヴィレッジ風：UIの表示状態
UENUM(BlueprintType)
enum class EInteractionUIState : uint8
{
	None,   // 何もなし
	Focus,  // 遠距離：ドット（〇）表示
	Active  // 近距離：[E]アイコン＋アクション名
};