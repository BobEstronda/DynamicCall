// Fill out your copyright notice in the Description page of Project Settings.


#include "DynamicCallTypes.h"

namespace
{
	const TCHAR* TypeName(EDynamicValueType T)
	{
		switch (T)
		{
		case EDynamicValueType::None:   return TEXT("void");
		case EDynamicValueType::Bool:   return TEXT("bool");
		case EDynamicValueType::Int:    return TEXT("int");
		case EDynamicValueType::Float:  return TEXT("float");
		case EDynamicValueType::String: return TEXT("string");
		case EDynamicValueType::Name:   return TEXT("name");
		case EDynamicValueType::Object: return TEXT("object");
		}
		return TEXT("?");
	}

	void SetError(FString* Err, const FString& Msg)
	{
		if (Err) *Err = Msg;
	}

	// Maps an FProperty to a supported value type.
	bool GetPropertyType(const FProperty* P, EDynamicValueType& OutType, UClass*& OutClass)
	{
		OutClass = nullptr;
		if (P->IsA<FBoolProperty>()) { OutType = EDynamicValueType::Bool; return true; }
		if (const FNumericProperty* N = CastField<const FNumericProperty>(P))
		{
			OutType = N->IsFloatingPoint() ? EDynamicValueType::Float : EDynamicValueType::Int;
			return true;
		}
		if (P->IsA<FStrProperty>()) { OutType = EDynamicValueType::String; return true; }
		if (P->IsA<FNameProperty>()) { OutType = EDynamicValueType::Name;   return true; }
		if (const FObjectPropertyBase* O = CastField<const FObjectPropertyBase>(P))
		{
			OutType = EDynamicValueType::Object;
			OutClass = O->PropertyClass;
			return true;
		}
		return false;
	}

	bool ReadProperty(const FProperty* P, const void* Addr, FDynamicValue& Out)
	{
		EDynamicValueType T;
		UClass* C;
		if (!GetPropertyType(P, T, C)) return false;

		Out = FDynamicValue();
		Out.Type = T;
		switch (T)
		{
		case EDynamicValueType::Bool:
			Out.Num = CastField<const FBoolProperty>(P)->GetPropertyValue(Addr) ? 1.0 : 0.0; 
			break;
		case EDynamicValueType::Int:
			Out.Num = (double)CastField<const FNumericProperty>(P)->GetSignedIntPropertyValue(Addr); 
			break;
		case EDynamicValueType::Float:
			Out.Num = CastField<const FNumericProperty>(P)->GetFloatingPointPropertyValue(Addr); 
			break;
		case EDynamicValueType::String:
			Out.Str = CastField<const FStrProperty>(P)->GetPropertyValue(Addr);
			break;
		case EDynamicValueType::Name:
			Out.Name = CastField<const FNameProperty>(P)->GetPropertyValue(Addr);
			break;
		case EDynamicValueType::Object:
			Out.Obj = CastField<const FObjectPropertyBase>(P)->GetObjectPropertyValue(Addr);
			break;
		default: 
			break;
		}
		return true;
	}

	void WriteProperty(const FProperty* P, void* Addr, const FDynamicValue& V)
	{
		if (const FBoolProperty* B = CastField<const FBoolProperty>(P))
		{
			B->SetPropertyValue(Addr, V.Num != 0.0);
		}
		else if (const FNumericProperty* N = CastField<const FNumericProperty>(P))
		{
			if (N->IsFloatingPoint()) N->SetFloatingPointPropertyValue(Addr, V.Num);
			else                      N->SetIntPropertyValue(Addr, (int64)V.Num);
		}
		else if (const FStrProperty* S = CastField<const FStrProperty>(P))    S->SetPropertyValue(Addr, V.Str);
		else if (const FNameProperty* Nm = CastField<const FNameProperty>(P)) Nm->SetPropertyValue(Addr, V.Name);
		else if (const FObjectPropertyBase* O = CastField<const FObjectPropertyBase>(P))
		{
			if (!V.Obj || V.Obj->IsA(O->PropertyClass)) // never write a wrong type
			{
				O->SetObjectPropertyValue(Addr, V.Obj);
			}
		}
	}

	FString FunctionLabelName(const UFunction* F)
	{
#if WITH_EDITOR
		const FString& DisplayName = F->GetMetaData(TEXT("DisplayName"));
		if (!DisplayName.IsEmpty()) return DisplayName;
#endif
		return F->GetName();
	}

	/**
	 * A function is callable when: BlueprintCallable, not static, and every input parameter has a supported
	 * type with no output params. The return value is optional: RetType is None for void or unsupported returns.
	 */
	bool CheckFunction(const UFunction* F, EDynamicValueType& RetType, UClass*& RetClass, FString* OutArgs)
	{
		if (!F || !F->HasAnyFunctionFlags(FUNC_BlueprintCallable) || F->HasAnyFunctionFlags(FUNC_Static)) return false;
		if (F->GetOwnerClass() == UObject::StaticClass()) return false;

		RetType = EDynamicValueType::None;
		RetClass = nullptr;
		if (const FProperty* Ret = F->GetReturnProperty())
		{
			EDynamicValueType T;
			UClass* C;
			if (GetPropertyType(Ret, T, C)) { RetType = T; RetClass = C; }
		}

		for (TFieldIterator<FProperty> It(F); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			if (It->HasAnyPropertyFlags(CPF_ReturnParm)) continue;
			// const T& parameters are flagged OutParm|ConstParm|ReferenceParm; only reject real out params
			if (It->HasAnyPropertyFlags(CPF_OutParm) && !It->HasAnyPropertyFlags(CPF_ConstParm)) return false;

			EDynamicValueType PT;
			UClass* PC;
			if (!GetPropertyType(*It, PT, PC)) return false;

			if (OutArgs)
			{
				if (!OutArgs->IsEmpty()) *OutArgs += TEXT(", ");
				*OutArgs += It->GetName();
			}
		}
		return true;
	}

	bool BuildFunctionInfo(const UFunction* F, FDynamicMemberInfo& Out)
	{
		EDynamicValueType T;
		UClass* C;
		FString Args;
		if (!CheckFunction(F, T, C, &Args)) return false;

#if WITH_EDITOR
		// functions that only make sense inside a Blueprint graph
		if (F->HasMetaData(TEXT("Latent")) || F->HasMetaData(TEXT("WorldContext")) ||
			F->HasMetaData(TEXT("BlueprintInternalUseOnly")) || F->HasMetaData(TEXT("DeprecatedFunction")))
		{
			return false;
		}
#endif

		Out.Name = F->GetFName();
		Out.Type = T;
		Out.ObjectClass = C;
		Out.Func = F;
		Out.Prop = nullptr;
		Out.bIsFunction = true;
		Out.Label = FString::Printf(TEXT("%s(%s)  ->  %s"), *FunctionLabelName(F), *Args, *DynamicTypeToString(T, C));
		return true;
	}

	bool BuildPropertyInfo(const FProperty* P, FDynamicMemberInfo& Out)
	{
		if (P->HasAnyPropertyFlags(CPF_Deprecated)) return false;
		if (!P->HasAnyPropertyFlags(CPF_BlueprintVisible | CPF_Edit)) return false;

		EDynamicValueType T;
		UClass* C;
		if (!GetPropertyType(P, T, C)) return false;

		Out.Name = P->GetFName();
		Out.Type = T;
		Out.ObjectClass = C;
		Out.Func = nullptr;
		Out.Prop = P;
		Out.bIsFunction = false;
		Out.Label = FString::Printf(TEXT("%s  :  %s"), *Out.Name.ToString(), *DynamicTypeToString(T, C));
		return true;
	}

	/** Calls a UFunction through ProcessEvent using a hand-built parameter buffer. */
	bool CallFunction(UObject* Obj, UFunction* Func, const TArray<FDynamicParam>& Params, FDynamicValue& Out, FString* Err)
	{
		EDynamicValueType RT;
		UClass* RC;
		if (!CheckFunction(Func, RT, RC, nullptr))
		{
			SetError(Err, FString::Printf(TEXT("function '%s' cannot be called (needs BlueprintCallable, supported parameter types, no output params)"), *Func->GetName()));
			return false;
		}

		const FProperty* Ret = Func->GetReturnProperty();
		uint8* Buf = (uint8*)FMemory_Alloca(Func->ParmsSize);
		FMemory::Memzero(Buf, Func->ParmsSize);

		// construct every parameter (FString etc. must be constructed before use)
		for (TFieldIterator<FProperty> It(Func); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			It->InitializeValue_InContainer(Buf);
		}

		// fill inputs by name
		for (TFieldIterator<FProperty> It(Func); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			if (It->HasAnyPropertyFlags(CPF_ReturnParm)) continue;
			const FName PName = It->GetFName();
			if (const FDynamicParam* P = Params.FindByPredicate([&](const FDynamicParam& X) { return X.ParamName == PName; }))
			{
				WriteProperty(*It, It->ContainerPtrToValuePtr<void>(Buf), P->Value.ToValue());
			}
		}

		Obj->ProcessEvent(Func, Buf);

		Out = FDynamicValue(); // None = void
		bool bOk = true;
		if (Ret && RT != EDynamicValueType::None)
		{
			bOk = ReadProperty(Ret, Ret->ContainerPtrToValuePtr<void>(Buf), Out);
		}

		for (TFieldIterator<FProperty> It(Func); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			It->DestroyValue_InContainer(Buf);
		}
		return bOk;
	}

	// One hop of static analysis: class C + step S -> type/class of the result.
	bool Advance(const UClass* C, const FDynamicStep& S, EDynamicValueType& OutType, UClass*& OutClass)
	{
		if (S.Kind == EDynamicStepKind::Component)
		{
			if (!S.ComponentClass) return false;
			OutType = EDynamicValueType::Object;
			OutClass = S.ComponentClass;
			return true;
		}
		FDynamicMemberInfo Info;
		if (!C || !FDynamicStep::FindMember(C, S.Member, Info)) return false;
		OutType = Info.Type;
		OutClass = Info.ObjectClass;
		return true;
	}

	// Hash of everything the editor needs to know changed in the generated params.
	uint32 ParamsHash(const TArray<FDynamicStep>& Steps)
	{
		uint32 H = 0;
		for (const FDynamicStep& S : Steps)
		{
			H = HashCombine(H, GetTypeHash(S.Params.Num()));
			for (const FDynamicParam& P : S.Params)
			{
				H = HashCombine(H, GetTypeHash(P.ParamName));
				H = HashCombine(H, GetTypeHash((uint8)P.Value.Type));
				H = HashCombine(H, GetTypeHash(P.Value.ObjectClass.Get()));
			}
		}
		return H;
	}
}

FString DynamicTypeToString(EDynamicValueType Type, const UClass* ObjectClass)
{
	if (Type == EDynamicValueType::Object && ObjectClass)
	{
		return FString::Printf(TEXT("object (%s)"), *ObjectClass->GetName());
	}
	return TypeName(Type);
}

/*
FDynamicContext FDynamicContext::Make(AActor* Instigator, AActor* Owner)
{
	FDynamicContext Ctx;
	Ctx.Set(TEXT("Instigator"), Instigator);
	Ctx.Set(TEXT("Owner"), Owner);
	return Ctx;
}
*/

void FDynamicLiteral::SetType(EDynamicValueType NewType, UClass* NewObjectClass)
{
	if (Type != NewType)
	{
		Type = NewType;
		bValue = false;
		IntValue = 0;
		FloatValue = 0.0;
		StringValue.Reset();
		NameValue = NAME_None;
		ObjectValue = nullptr;
	}
	ObjectClass = NewObjectClass;
	if (ObjectValue && ObjectClass && !ObjectValue->IsA(ObjectClass))
	{
		ObjectValue = nullptr;
	}
}

FDynamicValue FDynamicLiteral::ToValue() const
{
	FDynamicValue V;
	V.Type = Type;
	switch (Type)
	{
	case EDynamicValueType::Bool:   V.Num = bValue ? 1.0 : 0.0; break;
	case EDynamicValueType::Int:    V.Num = IntValue; break;
	case EDynamicValueType::Float:  V.Num = FloatValue; break;
	case EDynamicValueType::String: V.Str = StringValue; break;
	case EDynamicValueType::Name:   V.Name = NameValue; break;
	case EDynamicValueType::Object: V.Obj = ObjectValue; break;
	default: break;
	}
	return V;
}

FString FDynamicLiteral::ToDisplayString() const
{
	switch (Type)
	{
	case EDynamicValueType::Bool:   return bValue ? TEXT("true") : TEXT("false");
	case EDynamicValueType::Int:    return FString::FromInt(IntValue);
	case EDynamicValueType::Float:  return FString::SanitizeFloat(FloatValue);
	case EDynamicValueType::String: return FString::Printf(TEXT("\"%s\""), *StringValue);
	case EDynamicValueType::Name:   return NameValue.ToString();
	case EDynamicValueType::Object: return ObjectValue ? ObjectValue->GetName() : FString(TEXT("None"));
	default: break;
	}
	return FString();
}

bool FDynamicStep::Resolve(UObject* Obj, FDynamicValue& Out, FString* Err) const
{
	if (!Obj)
	{
		SetError(Err, TEXT("target object is null"));
		return false;
	}

	if (Kind == EDynamicStepKind::Component)
	{
		const AActor* Actor = Cast<AActor>(Obj);
		if (!Actor)
		{
			SetError(Err, FString::Printf(TEXT("'%s' is not an actor, cannot look for components"), *Obj->GetName()));
			return false;
		}
		if (!ComponentClass)
		{
			SetError(Err, TEXT("component class is not set"));
			return false;
		}
		Out = FDynamicValue();
		Out.Type = EDynamicValueType::Object;
		Out.Obj = Actor->FindComponentByClass(ComponentClass); // may be null: next step reports it
		return true;
	}

	if (Member.IsNone())
	{
		SetError(Err, TEXT("no property/function selected"));
		return false;
	}

	if (UFunction* F = Obj->FindFunction(Member))
	{
		return CallFunction(Obj, F, Params, Out, Err);
	}
	if (const FProperty* P = Obj->GetClass()->FindPropertyByName(Member))
	{
		if (ReadProperty(P, P->ContainerPtrToValuePtr<void>(Obj), Out)) return true;
		SetError(Err, FString::Printf(TEXT("property '%s' has an unsupported type"), *Member.ToString()));
		return false;
	}

	SetError(Err, FString::Printf(TEXT("'%s' not found on %s"), *Member.ToString(), *Obj->GetClass()->GetName()));
	return false;
}

void FDynamicStep::SyncParams(const FDynamicMemberInfo* Info)
{
	if (!Info || !Info->Func)
	{
		Params.Reset();
		return;
	}

	TArray<FDynamicParam> Old = MoveTemp(Params);
	Params.Reset();

	for (TFieldIterator<FProperty> It(Info->Func); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
	{
		if (It->HasAnyPropertyFlags(CPF_ReturnParm)) continue;

		EDynamicValueType T;
		UClass* C;
		if (!GetPropertyType(*It, T, C)) continue;

		const FName PName = It->GetFName();
		FDynamicParam P;
		if (const FDynamicParam* Prev = Old.FindByPredicate([&](const FDynamicParam& O) { return O.ParamName == PName && O.Value.Type == T; }))
		{
			P = *Prev;
		}
		P.ParamName = PName;
		P.Value.SetType(T, C);
		Params.Add(MoveTemp(P));
	}
}

bool FDynamicStep::FindMember(const UClass* Class, FName Name, FDynamicMemberInfo& Out)
{
	if (!Class || Name.IsNone()) return false;
	if (const UFunction* F = Class->FindFunctionByName(Name)) return BuildFunctionInfo(F, Out);
	if (const FProperty* P = Class->FindPropertyByName(Name)) return BuildPropertyInfo(P, Out);
	return false;
}

void FDynamicStep::CollectMembers(const UClass* Class, /*EDynamicMemberFilter Filter,*/ TArray<FDynamicMemberInfo>& Out)
{
	if (!Class) return;

	/*
	auto Accept = [Filter](const FDynamicMemberInfo& I)
		{
			switch (Filter)
			{
			case EDynamicMemberFilter::Objects: return I.Type == EDynamicValueType::Object;
			case EDynamicMemberFilter::Values:  return I.Type != EDynamicValueType::None;
			case EDynamicMemberFilter::Actions: return I.bIsFunction;
			}
			return false;
		};*/

	TSet<FName> Seen;
	for (TFieldIterator<UFunction> It(Class); It; ++It)
	{
		FDynamicMemberInfo Info;
		if (BuildFunctionInfo(*It, Info) /*&& Accept(Info)*/ && !Seen.Contains(Info.Name))
		{
			Seen.Add(Info.Name);
			Out.Add(Info);
		}
	}
	for (TFieldIterator<FProperty> It(Class); It; ++It)
	{
		FDynamicMemberInfo Info;
		if (BuildPropertyInfo(*It, Info) /* && Accept(Info)*/ && !Seen.Contains(Info.Name))
		{
			Seen.Add(Info.Name);
			Out.Add(Info);
		}
	}
	Out.Sort([](const FDynamicMemberInfo& A, const FDynamicMemberInfo& B) { return A.Label < B.Label; });
}

bool FDynamicPath::Invoke(/*const FDynamicContext& Ctx,*/ AActor* Ctx, FDynamicValue* OutValue, FString* OutError) const
{
	const bool bUsingCtx = Source == EDynamicPathSource::Context;
	UObject* Cur = /*GetSourceObject(Ctx)*/ bUsingCtx ? Ctx : static_cast<UObject*>(SpecificActor);
	if (!Cur)
	{
		SetError(OutError, bUsingCtx
			? FString(TEXT("context is invalid"))
			: FString::Printf(TEXT("SpecificActor is invalid: '%s'"), *SpecificActor.GetFName().ToString()));
		return false;
	}

	FDynamicValue Last;
	Last.Type = EDynamicValueType::Object;
	Last.Obj = Cur;

	for (int32 i = 0; i < Steps.Num(); ++i)
	{
		FDynamicValue V;
		FString StepError;
		if (!Steps[i].Resolve(Cur, V, &StepError))
		{
			SetError(OutError, FString::Printf(TEXT("step %d: %s"), i + 1, *StepError));
			return false;
		}
		Last = V;

		if (i < Steps.Num() - 1)
		{
			if (V.Type != EDynamicValueType::Object || !V.Obj)
			{
				SetError(OutError, FString::Printf(TEXT("step %d did not return a valid object (component missing / null reference?)"), i + 1));
				return false;
			}
			Cur = V.Obj;
		}
	}

	if (OutValue) *OutValue = Last;
	return true;
}

//UObject* FDynamicPath::GetSourceObject(/*const FDynamicContext& Ctx*/) const
//{
//	return ContextKey.IsNone() ? static_cast<UObject*>(SpecificActor) : Ctx.Get(ContextKey);
//}

UClass* FDynamicPath::GetStartClass() const 
{
	if (SpecificActor) return SpecificActor->GetClass();
	return SourceClass ? SourceClass.Get() : AActor::StaticClass();
}

UClass* FDynamicPath::GetClassAtStep(int32 Index) const
{
	UClass* C = GetStartClass();
	for (int32 i = 0; i < Index && Steps.IsValidIndex(i); ++i)
	{
		EDynamicValueType T;
		UClass* OC = nullptr;
		if (!Advance(C, Steps[i], T, OC) || T != EDynamicValueType::Object) return nullptr;
		C = OC;
	}
	return C;
}

bool FDynamicPath::ResolveType(EDynamicValueType& OutType, UClass*& OutClass) const
{
	UClass* C = GetStartClass();
	EDynamicValueType T = EDynamicValueType::Object;
	UClass* OC = C;

	for (const FDynamicStep& S : Steps)
	{
		if (T != EDynamicValueType::Object) return false; // previous step did not give an object
		if (!Advance(C, S, T, OC)) return false;
		C = OC;
	}
	OutType = T;
	OutClass = OC;
	return true;
}

bool FDynamicPath::EditorSync()
{
	const uint32 Before = ParamsHash(Steps);
	UClass* C = GetStartClass();

	for (FDynamicStep& S : Steps)
	{
		if (S.Kind == EDynamicStepKind::Component)
		{
			S.Params.Reset();
		}
		else
		{
			FDynamicMemberInfo Info;
			if (C && FDynamicStep::FindMember(C, S.Member, Info)) S.SyncParams(&Info);
			else if (C)                                            S.Params.Reset();
			// C null: chain already broken above, leave params alone
		}

		EDynamicValueType T = EDynamicValueType::Object;
		UClass* OC = nullptr;
		C = (Advance(C, S, T, OC) && T == EDynamicValueType::Object) ? OC : nullptr;
	}
	return Before != ParamsHash(Steps);
}

FString FDynamicPath::ToDisplayString() const 
{
	FString Out;
	if (Source == EDynamicPathSource::SpecificActor) 
	{
		Out = SpecificActor ? SpecificActor->GetName() : FString(TEXT("(actor?)"));
	}
	else 
	{
		Out = SourceClass ? SourceClass->GetFName().ToString() : FString(TEXT("(SourceClass?)"));
	}

	for (const FDynamicStep& S : Steps)
	{
		Out += TEXT(" -> ");
		if (S.Kind == EDynamicStepKind::Component)
		{
			Out += FString::Printf(TEXT("[%s]"), S.ComponentClass ? *S.ComponentClass->GetName() : TEXT("?"));
			continue;
		}

		Out += S.Member.IsNone() ? FString(TEXT("?")) : S.Member.ToString();
		if (S.Params.Num() > 0)
		{
			TArray<FString> Args;
			for (const FDynamicParam& P : S.Params) Args.Add(P.Value.ToDisplayString());
			Out += TEXT("(") + FString::Join(Args, TEXT(", ")) + TEXT(")");
		}
	}
	return Out;
}