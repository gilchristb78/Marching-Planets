// Copyright Epic Games, Inc. All Rights Reserved.

#include "VoxelGameMode.h"
#include "VoxelCharacter.h"
#include "UObject/ConstructorHelpers.h"

AVoxelGameMode::AVoxelGameMode()
	: Super()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnClassFinder(TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter"));
	DefaultPawnClass = PlayerPawnClassFinder.Class;

}
