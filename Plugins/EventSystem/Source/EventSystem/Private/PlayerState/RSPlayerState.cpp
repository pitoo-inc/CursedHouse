// Fill out your copyright notice in the Description page of Project Settings.


#include "PlayerState/RSPlayerState.h"

void ARSPlayerState::AddPersonalItem(FName ItemID)
{
	PersonalItems.Add(ItemID);
	UE_LOG(LogTemp, Log, TEXT("RS_Log: [PlayerState] Item Added: %s"), *ItemID.ToString());
}

bool ARSPlayerState::HasPersonalItem(FName ItemID) const
{
	return PersonalItems.Contains(ItemID);
}

void ARSPlayerState::RemovePersonalItem(FName ItemID)
{
	PersonalItems.RemoveSingle(ItemID); // 1つだけ消費する
	UE_LOG(LogTemp, Log, TEXT("RS_Log: [PlayerState] Item Removed: %s"), *ItemID.ToString());
}