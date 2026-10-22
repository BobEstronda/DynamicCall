// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
//#include "UObject/Object.h"
#include "DynamicCallTypes.generated.h"

UENUM(BlueprintType)
enum class EDynamicValueType : uint8
{
    None,
    Bool,
    Int,
    Float,
    String,
    Name,
    Object // DataAssets, Actors, Components, Any UObject
};

UENUM(BlueprintType)
enum class EDynamicStepKind : uint8
{
    Member UMETA(DisplayName = "Property / Function"),
    Component UMETA(DisplayName = "Component")
};

UENUM(BlueprintType)
enum class EDynamicPathSource : uint8
{
    Context        UMETA(DisplayName = "Context"),
    SpecificActor  UMETA(DisplayName = "Specific Actor"),
};

// What the editor lists in the member dropdown of a step.
/*
enum class EDynamicMemberFilter : uint8
{
    Objects, // intermediate steps: members that return an object
    Values,  // last step of a "Value" path: properties or functions that return a value
    Actions  // last step of an "Action" path: any function (with or without return)
};
*/

DYNAMICCALL_API FString DynamicTypeToString(EDynamicValueType Type, const UClass* ObjectClass = nullptr);

// Runtime value (non-reflective). Bool/Int/Float use Num.
struct DYNAMICCALL_API FDynamicValue
{

    EDynamicValueType Type = EDynamicValueType::Bool;
    double Num = 0.0;
    FString Str;
    FName Name;
    TObjectPtr<UObject> Obj = nullptr;

    /*
    static FDynamicValue MakeBool(bool Value)
    {
        FDynamicValue Out;
        Out.Type = EDynamicValueType::Bool;
        Out.Num = Value ? 1.0 : 0.0;
        return Out;
    }

    static FDynamicValue MakeInt(int64 Value)
    {
        FDynamicValue Out;
        Out.Type = EDynamicValueType::Int;
        Out.Num = static_cast<double>(Value);
        return Out;
    }

    static FDynamicValue MakeFloat(double Value)
    {
        FDynamicValue Out;
        Out.Type = EDynamicValueType::Float;
        Out.Num = Value;
        return Out;
    }

    static FDynamicValue MakeString(const FString& Value)
    {
        FDynamicValue Out;
        Out.Type = EDynamicValueType::String;
        Out.Str = Value;
        return Out;
    }

    static FDynamicValue MakeName(FName Value)
    {
        FDynamicValue Out;
        Out.Type = EDynamicValueType::Name;
        Out.Name = Value;
        return Out;
    }

    static FDynamicValue MakeObject(UObject* Value)
    {
        FDynamicValue Out;
        Out.Type = EDynamicValueType::Object;
        Out.Obj = Value;
        return Out;
    }
    */
};

// Named objects available to a path as its source ("Instigator", "Owner", anything you want).
/*
struct DYNAMICCALL_API FDynamicContext
{
    TMap<FName, TWeakObjectPtr<UObject>> Objects;

    FDynamicContext& Set(FName Key, UObject* Object) { Objects.Add(Key, Object); return *this; }

    UObject* Get(FName Key) const
    {
        const TWeakObjectPtr<UObject>* Found = Objects.Find(Key);
        return Found ? Found->Get() : nullptr;
    }

    // Convenience: fills the keys "Instigator" and "Owner". 
    static FDynamicContext Make(AActor* Instigator, AActor* Owner);
};
*/

USTRUCT(BlueprintType)
struct DYNAMICCALL_API FDynamicLiteral
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, Category = "Literal")
    EDynamicValueType Type = EDynamicValueType::Float;

    UPROPERTY(EditAnywhere, Category = "Literal")
    bool bValue = false;

    UPROPERTY(EditAnywhere, Category = "Literal")
    int32 IntValue = 0;

    UPROPERTY(EditAnywhere, Category = "Literal")
    double FloatValue = 0.0;

    UPROPERTY(EditAnywhere, Category = "Literal")
    FString StringValue;

    UPROPERTY(EditAnywhere, Category = "Literal")
    FName NameValue;

    UPROPERTY(EditAnywhere, Category = "Literal")
    TObjectPtr<UObject> ObjectValue = nullptr;

    UPROPERTY(VisibleAnywhere, Category = "Literal")
    TSubclassOf<UObject> ObjectClass;

    void SetType(EDynamicValueType NewType, UClass* NewObjectClass = nullptr);
    FDynamicValue ToValue() const;
    FString ToDisplayString() const;
};

USTRUCT(BlueprintType)
struct DYNAMICCALL_API FDynamicParam
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, Category = "Param")
    FName ParamName;

    UPROPERTY(EditAnywhere, Category = "Param")
    FDynamicLiteral Value;
};

struct DYNAMICCALL_API FDynamicMemberInfo
{
    FName Name;
    FString Label;

    EDynamicValueType Type = EDynamicValueType::Bool;
    UClass* ObjectClass = nullptr;

    const UFunction* Func = nullptr;
    const FProperty* Prop = nullptr;
    bool bIsFunction = false;

    //bool IsFunction() const { return Function != nullptr; }
    //bool HasReturnValue() const { return Function && Function->GetReturnProperty() != nullptr; }
};

/*
struct DYNAMICCALL_API FDynamicResult
{
    bool bSuccess = false;
    bool bHasValue = false;
    FDynamicValue Value;

    static FDynamicResult Success()
    {
        FDynamicResult R;
        R.bSuccess = true;
        return R;
    }
};
*/


// Step: one hop of the chain. actor -> component -> property/function -> ...
USTRUCT(BlueprintType)
struct DYNAMICCALL_API FDynamicStep
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Step")
    EDynamicStepKind Kind = EDynamicStepKind::Member;

    // Property or function name (chosen from a dropdown in the editor).
    UPROPERTY(EditAnywhere, Category = "Step")
    FName Member;

    UPROPERTY(EditAnywhere, Category = "Step")
    TSubclassOf<UActorComponent> ComponentClass;

    // Generated automatically from the chosen function.
    UPROPERTY(EditAnywhere, EditFixedSize, Category = "Step")
    TArray<FDynamicParam> Params;

    // Runs this step on Obj. Out.Type == None when the function returns nothing.
    bool Resolve(UObject* Obj, FDynamicValue& Out, FString* Err = nullptr) const;

    // Rebuilds Params from the function signature (keeps values with same name/type).
    void SyncParams(const FDynamicMemberInfo* Info);

    static bool FindMember(const UClass* Class, FName Name, FDynamicMemberInfo& Out);
    static void CollectMembers(const UClass* Class, /*EDynamicMemberFilter Filter,*/ TArray<FDynamicMemberInfo>&Out);
};


// Path: source + chain of steps.   THE struct you put in your own classes
USTRUCT(BlueprintType)
struct DYNAMICCALL_API FDynamicPath
{
    GENERATED_BODY()

    // Key in the FDynamicContext. None = use SpecificActor.
    //UPROPERTY(EditAnywhere, Category = "Path")
    //FName ContextKey;
    UPROPERTY(EditAnywhere, Category = "Path")
    EDynamicPathSource Source = EDynamicPathSource::Context;

    UPROPERTY(EditAnywhere, Category = "Path")
    TObjectPtr<AActor> SpecificActor;

    // Editor only: used to list the members available for the first step (e.g. ABaseCharacter). Not checked at runtime.
    UPROPERTY(EditAnywhere, Category = "Path", meta = (DisplayName = "Actor Type", EditCondition = "Source != EDynamicPathSource::SpecificActor", EditConditionHides))
    TSubclassOf<AActor> SourceClass = AActor::StaticClass();

    UPROPERTY(EditAnywhere, Category = "Path")
    TArray<FDynamicStep> Steps;

    /**
     * Runs the whole chain. OutValue receives the value of the last step (None for void functions).
     * With zero steps the source object itself is returned as an Object value.
     * On failure returns false and fills OutError with a readable reason.
     */
    bool Invoke(/*const FDynamicContext& Ctx,*/ AActor* Ctx, FDynamicValue* OutValue = nullptr, FString* OutError = nullptr) const;

    //UObject* GetSourceObject(/*const FDynamicContext& Ctx*/ ) const;

    // --- static analysis (used by the editor, safe at runtime) ---
    UClass* GetStartClass() const;
    // Class of the object the step at Index operates on (nullptr if the chain is broken before it).
    UClass* GetClassAtStep(int32 Index) const;
    // Type of the final result. false = invalid chain.
    bool ResolveType(EDynamicValueType& OutType, UClass*& OutClass) const;

    // Regenerates the Params of every step. Returns true if anything changed.
    bool EditorSync();

    FString ToDisplayString() const;
};