// Fill out your copyright notice in the Description page of Project Settings.


#include "BBMWorldFactory.h"
#include "BBMWorldPreset.h"


UBBMWorldFactory::UBBMWorldFactory(const FObjectInitializer& ObjectInitializer)
{
	bCreateNew = true;
	SupportedClass = UWorld::StaticClass();
	ColorTolerance = 10.f;
}

void UBBMWorldFactory::SetWorldPreset(UBBMWorldPreset* InWorldPreset)
{
	if (InWorldPreset)
	{
		WorldPreset = InWorldPreset;

	}
}

UObject* UBBMWorldFactory::FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled)
{
	const bool bAddToRoot = false;
	UWorld* NewWorld = UWorld::CreateWorld(EWorldType::Inactive, false, InName, Cast<UPackage>(InParent), bAddToRoot, ERHIFeatureLevel::Num);
	NewWorld->Modify();
	GEditor->InitBuilderBrush(NewWorld);
	NewWorld->SetFlags(Flags);
	NewWorld->ThumbnailInfo = NewObject<UWorldThumbnailInfo>(NewWorld, NAME_None, RF_Transactional);

	int32 WorldUnit = 100;
	

	if (!WorldPreset)
	{
		return nullptr;
	}

	FTransform DefaultTransform;

	//Spawn Light Stuffs
	ADirectionalLight* Light = NewWorld->SpawnActor<ADirectionalLight>(ADirectionalLight::StaticClass(), WorldPreset->MainLightTransform);
	Light->SetBrightness(3);
	ASphereReflectionCapture* Reflection = NewWorld->SpawnActor<ASphereReflectionCapture>(ASphereReflectionCapture::StaticClass(), DefaultTransform);


	//Spawn Sky Sphere
	NewWorld->SpawnActor<AActor>(WorldPreset->SkySphere, DefaultTransform);

	//Get Texture Data
	int32 WorldWidth;
	int32 WorldHeight;

	TArray<FColor> Pixels = GetTexturePixels(Filename, WorldWidth, WorldHeight);

	//Spawn Floor
	FTransform Transform;
	Transform.SetLocation(FVector(0, 0, 0));
	Transform.SetScale3D(FVector(WorldWidth, WorldHeight , 1));
	NewWorld->SpawnActor<AStaticMeshActor>(WorldPreset->PresetActors["Floor"], Transform);

	//Create World
	CreateWorldFromTextureData(NewWorld, Pixels, WorldWidth, WorldHeight, WorldUnit);


	return NewWorld;
}


TArray<FColor> UBBMWorldFactory::GetTexturePixels(const FString& Filename, int32& InWidth, int32& InHeight)
{

	TArray<FColor> ResultPixels;
	bool ValidPath = true;
	UTexture2D* Texture = FImageUtils::ImportFileAsTexture2D(Filename);

	if (!Texture)
	{
		ValidPath = false;
		Texture = WorldPreset->Texture;
		if (!Texture)
		{
			return ResultPixels;

		}
	}


	InWidth = Texture->GetSizeX();
	InHeight = Texture->GetSizeY();

	int32 Size = InWidth * InHeight;
	ResultPixels.SetNumZeroed(Size);


	if (ValidPath)
	{
		FByteBulkData Data = Texture->PlatformData->Mips[0].BulkData;
		FColor* Pixels = static_cast<FColor*>(Data.Lock(LOCK_READ_ONLY));

		for (int32 Index = 0; Index < ResultPixels.Num(); Index++)
		{
			FColor* Color = Pixels + Index;
			ResultPixels[Index] = *Color;
		}

		Data.Unlock();
	}
	else
	{
		uint8* Data = Texture->Source.LockMip(0);

		int32 ByteOffset = 0;

		for (int32 Index = 0; Index < ResultPixels.Num(); Index++)
		{
			uint8* B = Data + ByteOffset + 0;
			uint8* G = Data + ByteOffset + 1;
			uint8* R = Data + ByteOffset + 2;
			uint8* A = Data + ByteOffset + 3;

			FColor Color = FColor(*R, *G, *B, *A);
			ResultPixels[Index] = Color;
			ByteOffset += 4;

		}

		Texture->Source.UnlockMip(0);
	}

		


	return ResultPixels;

}

void UBBMWorldFactory::CreateWorldFromTextureData(UWorld* InWorld, const TArray<FColor> Pixels, const int32 InWidth, const int32 InHeight, const int32 WorldUnit)
{

	int32 XOffset = -InWidth * 0.5f * WorldUnit;
	int32 YOffset = -InHeight * 0.5f * WorldUnit;


	for (int32 Index = 0; Index < Pixels.Num(); Index++)
	{
		FColor Color = Pixels[Index];
		if (Color.A != 0)
		{
			FTransform Transform;
			int32 HalfWidth = Transform.GetScale3D().X * 0.5f * WorldUnit;
			int32 HalfHeight = Transform.GetScale3D().Y * 0.5f * WorldUnit;
			Transform.SetLocation(FVector(XOffset + HalfWidth, YOffset + HalfHeight, WorldUnit));

			for (TPair<FString, FColor> Preset : WorldPreset->PresetColorPattern)
			{
				if (SpawnPreset(InWorld, Preset.Key, Color, Transform))
				{
					break;
				}
			}


		}

		XOffset += WorldUnit;

		if (XOffset >= InWidth * 0.5f * WorldUnit)
		{
			XOffset = -InWidth * 0.5f * WorldUnit;
			YOffset += WorldUnit;
		}


	}

}

bool UBBMWorldFactory::IsColorInToleranceRange(const FColor& InColorA, const FColor& InColorB, const float Tolerance)
{
	bool result = false;

	

	uint8 RA = InColorA.R;
	uint8 GA = InColorA.G;
	uint8 BA = InColorA.B;
	uint8 AA = InColorA.A;



	uint8 RB = InColorB.R;
	uint8 GB = InColorB.G;
	uint8 BB = InColorB.B;
	uint8 AB = InColorB.A;

	if (RA< RB - Tolerance || RA> RB + Tolerance)
	{
		return result;
	}

	if (GA< GB - Tolerance || GA> GB + Tolerance)
	{
		return result;
	}

	if (BA< BB - Tolerance || BA> BB + Tolerance)
	{
		return result;
	}

	if (AA< AB - Tolerance || AA> AB + Tolerance)
	{
		return result;
	}

	result = true;
	return result;

}

bool UBBMWorldFactory::SpawnPreset(UWorld* InWorld, const FString& Key, const FColor& InColor, const FTransform& InTransform)
{
	bool result = false;

	if (IsColorInToleranceRange(InColor, WorldPreset->PresetColorPattern[Key], ColorTolerance))
	{

		InWorld->SpawnActor<AActor>(WorldPreset->PresetActors[Key], InTransform);
		result = true;
	}

	return result;
}

