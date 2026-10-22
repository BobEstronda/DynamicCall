// Copyright Epic Games, Inc. All Rights Reserved.

#include "DynamicCallEditor.h"

#include "DynamicCallTypes.h"
#include "DynamicCallCustomizations.h"

//#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectGlobals.h"

//#include "DynamicCallTypes.h"
//#include "DynamicCallCustomizations.h"

#define LOCTEXT_NAMESPACE "FDynamicCallEditorModule"

namespace
{
	TMap<const UStruct*, bool> GContainsPathCache;

	/** Does this class/struct (directly or through nested structs / struct arrays) hold an FDynamicPath? */
	bool ContainsPath(const UStruct* S)
	{
		if (const bool* Cached = GContainsPathCache.Find(S)) return *Cached;
		GContainsPathCache.Add(S, false); // guards against recursive structs

		bool bHas = false;
		for (TFieldIterator<FProperty> It(S); It && !bHas; ++It)
		{
			const FStructProperty* SP = CastField<FStructProperty>(*It);
			if (!SP)
			{
				if (const FArrayProperty* AP = CastField<FArrayProperty>(*It)) SP = CastField<FStructProperty>(AP->Inner);
			}
			if (SP && (SP->Struct == FDynamicPath::StaticStruct() || ContainsPath(SP->Struct)))
			{
				bHas = true;
			}
		}
		GContainsPathCache.Add(S, bHas);
		return bHas;
	}

	void SyncContainer(const UStruct* S, void* Data);

	void SyncStruct(const UScriptStruct* S, void* Ptr)
	{
		if (S == FDynamicPath::StaticStruct())
		{
			static_cast<FDynamicPath*>(Ptr)->EditorSync();
		}
		else if (ContainsPath(S))
		{
			SyncContainer(S, Ptr);
		}
	}

	void SyncContainer(const UStruct* S, void* Data)
	{
		for (TFieldIterator<FProperty> It(S); It; ++It)
		{
			if (const FStructProperty* SP = CastField<FStructProperty>(*It))
			{
				for (int32 i = 0; i < SP->ArrayDim; ++i)
				{
					SyncStruct(SP->Struct, SP->ContainerPtrToValuePtr<void>(Data, i));
				}
			}
			else if (const FArrayProperty* AP = CastField<FArrayProperty>(*It))
			{
				if (const FStructProperty* Inner = CastField<FStructProperty>(AP->Inner))
				{
					FScriptArrayHelper Helper(AP, AP->ContainerPtrToValuePtr<void>(Data));
					for (int32 i = 0; i < Helper.Num(); ++i)
					{
						SyncStruct(Inner->Struct, Helper.GetRawPtr(i));
					}
				}
			}
		}
	}

	/**
	 * Called for every property edit of every object in the editor. If the object holds FDynamicPath structs,
	 * regenerate their parameters. This is what lets ANY class use FDynamicPath without writing editor code.
	 */
	void OnObjectPropertyChanged(UObject* Obj, FPropertyChangedEvent& Event)
	{
		if (!Obj || Event.ChangeType == EPropertyChangeType::Interactive) return;

		const UClass* Class = Obj->GetClass();
		if (!ContainsPath(Class)) return;

		SyncContainer(Class, Obj);
	}
}

void FDynamicCallEditorModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module

	FPropertyEditorModule& PE = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

	PE.RegisterCustomPropertyTypeLayout(FDynamicPath::StaticStruct()->GetFName(),
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FDynamicPathCustomization::MakeInstance));
	PE.RegisterCustomPropertyTypeLayout(FDynamicStep::StaticStruct()->GetFName(),
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FDynamicStepCustomization::MakeInstance));
	PE.RegisterCustomPropertyTypeLayout(FDynamicParam::StaticStruct()->GetFName(),
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FDynamicParamCustomization::MakeInstance));
	PE.RegisterCustomPropertyTypeLayout(FDynamicLiteral::StaticStruct()->GetFName(),
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FDynamicLiteralCustomization::MakeInstance));
	PE.NotifyCustomizationModuleChanged();

	ObjectChangedHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.AddStatic(&OnObjectPropertyChanged);
}

void FDynamicCallEditorModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(ObjectChangedHandle);
	GContainsPathCache.Empty();

	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PE = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PE.UnregisterCustomPropertyTypeLayout(FDynamicPath::StaticStruct()->GetFName());
		PE.UnregisterCustomPropertyTypeLayout(FDynamicStep::StaticStruct()->GetFName());
		PE.UnregisterCustomPropertyTypeLayout(FDynamicParam::StaticStruct()->GetFName());
		PE.UnregisterCustomPropertyTypeLayout(FDynamicLiteral::StaticStruct()->GetFName());
	}
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FDynamicCallEditorModule, DynamicCallEditor)