// Fill out your copyright notice in the Description page of Project Settings.


#include "Component/RSInventoryComponent.h"
#include "PlayerState/RSPlayerState.h"
#include "GameState/RSGameState.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

// Sets default values for this component's properties
URSInventoryComponent::URSInventoryComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;

	InventoryType = ERSInventoryType::Personal; // デフォルトは個人用
}

void URSInventoryComponent::AddItem(FName ItemID)
{
	if (InventoryType == ERSInventoryType::Personal)
	{
		// 自分がくっついている親（Pawn）を取得
		if (APawn* OwnerPawn = Cast<APawn>(GetOwner()))
		{
			// Pawnの魂（PlayerState）を取得してアクセス
			if (ARSPlayerState* PS = OwnerPawn->GetPlayerState<ARSPlayerState>())
			{
				PS->AddPersonalItem(ItemID);
			}
		}
	}
	else
	{
		// 世界の金庫（GameState）を取得してアクセス
		if (ARSGameState* GS = Cast<ARSGameState>(UGameplayStatics::GetGameState(this)))
		{
			GS->AddGlobalItem(ItemID);
		}
	}
}

void URSInventoryComponent::RemoveItem(FName ItemID)
{
	if (InventoryType == ERSInventoryType::Personal)
	{
		if (APawn* OwnerPawn = Cast<APawn>(GetOwner()))
		{
			if (ARSPlayerState* PS = OwnerPawn->GetPlayerState<ARSPlayerState>())
			{
				PS->RemovePersonalItem(ItemID);
			}
		}
	}
	else
	{
		if (ARSGameState* GS = Cast<ARSGameState>(UGameplayStatics::GetGameState(this)))
		{
			GS->RemoveGlobalItem(ItemID);
		}
	}
}

bool URSInventoryComponent::HasItem(FName ItemID) const
{
	if (InventoryType == ERSInventoryType::Personal)
	{
		if (APawn* OwnerPawn = Cast<APawn>(GetOwner()))
		{
			if (ARSPlayerState* PS = OwnerPawn->GetPlayerState<ARSPlayerState>())
			{
				return PS->HasPersonalItem(ItemID);
			}
		}
	}
	else
	{
		if (ARSGameState* GS = Cast<ARSGameState>(UGameplayStatics::GetGameState(this)))
		{
			return GS->HasGlobalItem(ItemID);
		}
	}
	return false;
}