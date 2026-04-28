#include "SpartaGameState.h"
#include "SpartaGameInstance.h"
#include "SpartaPlayerController.h"
#include "SpawnVolume.h"
#include "CoinItem.h"
#include "BaseItem.h"

#include "Kismet/GameplayStatics.h"
#include "Components/TextBlock.h"
#include "Blueprint/UserWidget.h"
#include "Engine/Engine.h"

ASpartaGameState::ASpartaGameState()
{
	Score = 0;
	SpawnedCoinCount = 0;
	CollectedCoinCount = 0;

	CurrentLevelIndex = 0;
	CurrentWaveIndex = 0;
	MaxLevels = 3;

	bIsWaveInProgress = false;
}

void ASpartaGameState::BeginPlay()
{
	Super::BeginPlay();

	StartLevel();

	GetWorldTimerManager().SetTimer(
		HUDUpdateTimerHandle,
		this,
		&ASpartaGameState::UpdateHUD,
		0.1f,
		true
	);
}

int32 ASpartaGameState::GetScore() const
{
	return Score;
}

void ASpartaGameState::AddScore(int32 Amount)
{
	Score += Amount;

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (USpartaGameInstance* SpartaGameInstance = Cast<USpartaGameInstance>(GameInstance))
		{
			SpartaGameInstance->AddToScore(Amount);
		}
	}
}

void ASpartaGameState::SetupWaveConfigsForCurrentLevel()
{
	WaveConfigs.Empty();

	auto AddWave = [this](float InDuration, int32 InSpawnCount)
		{
			FWaveConfig NewWave;
			NewWave.WaveDuration = InDuration;
			NewWave.ItemSpawnCount = InSpawnCount;
			WaveConfigs.Add(NewWave);
		};

	if (CurrentLevelIndex == 0)
	{
		AddWave(30.0f, 15);
		AddWave(25.0f, 20);
		AddWave(20.0f, 25);
	}
	else if (CurrentLevelIndex == 1)
	{
		AddWave(25.0f, 20);
		AddWave(20.0f, 28);
		AddWave(15.0f, 35);
	}
	else
	{
		AddWave(20.0f, 25);
		AddWave(15.0f, 35);
		AddWave(12.0f, 45);
	}
}

void ASpartaGameState::StartLevel()
{
	if (APlayerController* PlayerController = GetWorld()->GetFirstPlayerController())
	{
		if (ASpartaPlayerController* SpartaPlayerController = Cast<ASpartaPlayerController>(PlayerController))
		{
			SpartaPlayerController->ShowGameHUD();
		}
	}

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (USpartaGameInstance* SpartaGameInstance = Cast<USpartaGameInstance>(GameInstance))
		{
			CurrentLevelIndex = SpartaGameInstance->CurrentLevelIndex;
		}
	}

	SetupWaveConfigsForCurrentLevel();
	CurrentWaveIndex = 0;

	StartWave();
}

void ASpartaGameState::StartWave()
{
	if (!WaveConfigs.IsValidIndex(CurrentWaveIndex))
	{
		EndLevel();
		return;
	}

	GetWorldTimerManager().ClearTimer(LevelTimerHandle);
	ClearAllSpawnedItems();

	SpawnedCoinCount = 0;
	CollectedCoinCount = 0;
	bIsWaveInProgress = true;

	TArray<AActor*> FoundVolumes;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASpawnVolume::StaticClass(), FoundVolumes);

	if (FoundVolumes.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("SpawnVolume is not in the map."));
		return;
	}

	const FWaveConfig& CurrentWave = WaveConfigs[CurrentWaveIndex];

	for (int32 i = 0; i < CurrentWave.ItemSpawnCount; i++)
	{
		int32 RandomIndex = FMath::RandRange(0, FoundVolumes.Num() - 1);
		ASpawnVolume* SpawnVolume = Cast<ASpawnVolume>(FoundVolumes[RandomIndex]);

		if (SpawnVolume)
		{
			AActor* SpawnedActor = SpawnVolume->SpawnRandomItem();

			if (SpawnedActor && SpawnedActor->IsA(ACoinItem::StaticClass()))
			{
				SpawnedCoinCount++;
			}
		}
	}

	GetWorldTimerManager().SetTimer(
		LevelTimerHandle,
		this,
		&ASpartaGameState::OnWaveTimeUp,
		CurrentWave.WaveDuration,
		false
	);

	UE_LOG(LogTemp, Warning, TEXT("Level %d - Wave %d Start! (Time: %.1f, Spawn: %d)"),
		CurrentLevelIndex + 1,
		CurrentWaveIndex + 1,
		CurrentWave.WaveDuration,
		CurrentWave.ItemSpawnCount
	);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			2.0f,
			FColor::Yellow,
			FString::Printf(TEXT("Wave %d Start!"), CurrentWaveIndex + 1)
		);
	}

	UpdateHUD();
}

void ASpartaGameState::OnWaveTimeUp()
{
	EndWave();
}

void ASpartaGameState::OnCoinCollected()
{
	if (!bIsWaveInProgress)
	{
		return;
	}

	CollectedCoinCount++;

	UE_LOG(LogTemp, Warning, TEXT("Coin Collected: %d / %d"),
		CollectedCoinCount,
		SpawnedCoinCount
	);

	if (SpawnedCoinCount > 0 && CollectedCoinCount >= SpawnedCoinCount)
	{
		EndWave();
	}
}

void ASpartaGameState::EndWave()
{
	if (!bIsWaveInProgress)
	{
		return;
	}

	bIsWaveInProgress = false;
	GetWorldTimerManager().ClearTimer(LevelTimerHandle);

	ClearAllSpawnedItems();

	CurrentWaveIndex++;

	if (CurrentWaveIndex >= WaveConfigs.Num())
	{
		EndLevel();
	}
	else
	{
		StartWave();
	}
}

void ASpartaGameState::EndLevel()
{
	GetWorldTimerManager().ClearTimer(LevelTimerHandle);
	ClearAllSpawnedItems();

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (USpartaGameInstance* SpartaGameInstance = Cast<USpartaGameInstance>(GameInstance))
		{
			CurrentLevelIndex++;
			SpartaGameInstance->CurrentLevelIndex = CurrentLevelIndex;
		}
	}

	if (CurrentLevelIndex >= MaxLevels)
	{
		OnGameOver();
		return;
	}

	if (LevelMapNames.IsValidIndex(CurrentLevelIndex))
	{
		UGameplayStatics::OpenLevel(GetWorld(), LevelMapNames[CurrentLevelIndex]);
	}
	else
	{
		OnGameOver();
	}
}

void ASpartaGameState::OnGameOver()
{
	if (APlayerController* PlayerController = GetWorld()->GetFirstPlayerController())
	{
		if (ASpartaPlayerController* SpartaPlayerController = Cast<ASpartaPlayerController>(PlayerController))
		{
			SpartaPlayerController->SetPause(true);
			SpartaPlayerController->ShowMainMenu(true);
		}
	}
}

void ASpartaGameState::ClearAllSpawnedItems()
{
	TArray<AActor*> FoundItems;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ABaseItem::StaticClass(), FoundItems);

	for (AActor* Item : FoundItems)
	{
		if (Item)
		{
			Item->Destroy();
		}
	}
}

void ASpartaGameState::UpdateHUD()
{
	if (APlayerController* PlayerController = GetWorld()->GetFirstPlayerController())
	{
		if (ASpartaPlayerController* SpartaPlayerController = Cast<ASpartaPlayerController>(PlayerController))
		{
			if (UUserWidget* HUDWidget = SpartaPlayerController->GetHUDWidget())
			{
				if (UTextBlock* TimeText = Cast<UTextBlock>(HUDWidget->GetWidgetFromName(TEXT("Time"))))
				{
					float RemainingTime = GetWorldTimerManager().GetTimerRemaining(LevelTimerHandle);
					RemainingTime = FMath::Max(0.0f, RemainingTime);

					TimeText->SetText(FText::FromString(
						FString::Printf(TEXT("Time: %.1f"), RemainingTime)
					));
				}

				if (UTextBlock* ScoreText = Cast<UTextBlock>(HUDWidget->GetWidgetFromName(TEXT("Score"))))
				{
					if (UGameInstance* GameInstance = GetGameInstance())
					{
						if (USpartaGameInstance* SpartaGameInstance = Cast<USpartaGameInstance>(GameInstance))
						{
							ScoreText->SetText(FText::FromString(
								FString::Printf(TEXT("Score: %d"), SpartaGameInstance->TotalScore)
							));
						}
					}
				}

				if (UTextBlock* LevelIndexText = Cast<UTextBlock>(HUDWidget->GetWidgetFromName(TEXT("Level"))))
				{
					int32 VisibleWave = WaveConfigs.Num() > 0
						? FMath::Min(CurrentWaveIndex + 1, WaveConfigs.Num())
						: 0;

					LevelIndexText->SetText(FText::FromString(
						FString::Printf(TEXT("Level: %d | Wave: %d/%d"),
							CurrentLevelIndex + 1,
							VisibleWave,
							WaveConfigs.Num())
					));
				}
			}
		}
	}
}