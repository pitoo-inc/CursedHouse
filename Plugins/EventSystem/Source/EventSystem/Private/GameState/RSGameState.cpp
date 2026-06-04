// Fill out your copyright notice in the Description page of Project Settings.


#include "GameState/RSGameState.h"

void ARSGameState::AddGlobalItem(FName ItemID)
{
	GlobalBoxItems.Add(ItemID);
	UE_LOG(LogTemp, Log, TEXT("RS_Log: [GameState] Global Item Stored: %s"), *ItemID.ToString());
}

bool ARSGameState::HasGlobalItem(FName ItemID) const
{
	return GlobalBoxItems.Contains(ItemID);
}

void ARSGameState::RemoveGlobalItem(FName ItemID)
{
	GlobalBoxItems.RemoveSingle(ItemID);
	UE_LOG(LogTemp, Log, TEXT("RS_Log: [GameState] Global Item Removed: %s"), *ItemID.ToString());
}