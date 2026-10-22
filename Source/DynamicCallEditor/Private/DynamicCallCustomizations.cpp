// Fill out your copyright notice in the Description page of Project Settings.


#include "DynamicCallCustomizations.h"
#include "DynamicCallTypes.h"

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "IDetailPropertyRow.h"
#include "IPropertyUtilities.h"
#include "PropertyCustomizationHelpers.h"
#include "Widgets/Input/SSearchableComboBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	/** Raw pointer to the edited struct (first selected object). Reads only: writes go through handles. */
	template <typename T>
	T* GetStructPtr(const TSharedPtr<IPropertyHandle>& H)
	{
		if (!H.IsValid()) return nullptr;
		TArray<void*> Raw;
		H->AccessRawData(Raw);
		return Raw.Num() > 0 ? static_cast<T*>(Raw[0]) : nullptr;
	}

	/**
	 * Rebuilds the property nodes (not only the layout rows) on the next tick. Needed because the generated
	 * Params array is changed from C++, so the old property nodes still think it has the previous size.
	 */
	FSimpleDelegate MakeRefreshDelegate(IPropertyTypeCustomizationUtils& CU)
	{
		TWeakPtr<IPropertyUtilities> Weak = CU.GetPropertyUtilities();
		return FSimpleDelegate::CreateLambda([Weak]()
			{
				if (TSharedPtr<IPropertyUtilities> U = Weak.Pin())
				{
					U->RequestForceRefresh();
				}
			});
	}

	/** The widget that fits the literal's current type. */
	TSharedRef<SWidget> MakeLiteralWidget(const TSharedPtr<IPropertyHandle>& LiteralH, IPropertyTypeCustomizationUtils& CU)
	{
		const FDynamicLiteral* L = GetStructPtr<FDynamicLiteral>(LiteralH);
		const EDynamicValueType T = L ? L->Type : EDynamicValueType::Float;

		switch (T)
		{
		case EDynamicValueType::Bool:
			return LiteralH->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDynamicLiteral, bValue))->CreatePropertyValueWidget();
		case EDynamicValueType::Int:
			return LiteralH->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDynamicLiteral, IntValue))->CreatePropertyValueWidget();
		case EDynamicValueType::Float:
			return LiteralH->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDynamicLiteral, FloatValue))->CreatePropertyValueWidget();
		case EDynamicValueType::String:
			return LiteralH->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDynamicLiteral, StringValue))->CreatePropertyValueWidget();
		case EDynamicValueType::Name:
			return LiteralH->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDynamicLiteral, NameValue))->CreatePropertyValueWidget();
		case EDynamicValueType::Object:
		{
			const UClass* Allowed = (L && L->ObjectClass) ? L->ObjectClass.Get() : UObject::StaticClass();
			return SNew(SObjectPropertyEntryBox)
				.PropertyHandle(LiteralH->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDynamicLiteral, ObjectValue)))
				.AllowedClass(Allowed)
				.ThumbnailPool(CU.GetThumbnailPool());
		}
		default:
			break;
		}
		return SNullWidget::NullWidget;
	}
}

// ===========================================================================
// FDynamicPath
// ===========================================================================
//FText FDynamicPathCustomization::GetSourceLabel() const
//{
//	const FDynamicPath* P = GetStructPtr<FDynamicPath>(PathHandle);
//	if (!P) return FText::GetEmpty();
//	if (P->ContextKey.IsNone()) return FText::FromString(TEXT("Specific Actor"));
//
//	const FString Name = P->ContextKey.ToString();
//	return FText::FromString(SourceKeys.Contains(P->ContextKey) ? Name : Name + TEXT("   (not available here)"));
//}

//void FDynamicPathCustomization::OnSourceSelected(TSharedPtr<FString> Item, ESelectInfo::Type SelectInfo)
//{
//	if (!Item.IsValid() || SelectInfo == ESelectInfo::Direct) return;
//
//	const int32 Idx = SourceOptions.IndexOfByKey(Item);
//	if (!SourceKeys.IsValidIndex(Idx)) return;
//
//	KeyHandle->SetValue(SourceKeys[Idx]);
//	if (TSharedPtr<IPropertyUtilities> U = Utils.Pin())
//	{
//		U->RequestForceRefresh();
//	}
//}

void FDynamicPathCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> Handle, FDetailWidgetRow& Row, IPropertyTypeCustomizationUtils& CU)
{
	PathHandle = Handle;
	TSharedPtr<IPropertyHandle> H = Handle;

	Row.NameContent()
		[
			Handle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MinDesiredWidth(320.f)
		[
			SNew(STextBlock)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.Text_Lambda([H]()
					{
						const FDynamicPath* P = GetStructPtr<FDynamicPath>(H);
						if (!P) return FText::GetEmpty();

						EDynamicValueType T = EDynamicValueType::None;
						UClass* C = nullptr;
						const FString Suffix = P->ResolveType(T, C)
							? FString::Printf(TEXT("    [%s]"), *DynamicTypeToString(T, C))
							: FString(TEXT("    [invalid chain]"));
						return FText::FromString(P->ToDisplayString() + Suffix);
					})
				.ColorAndOpacity_Lambda([H]()
					{
						const FDynamicPath* P = GetStructPtr<FDynamicPath>(H);
						EDynamicValueType T = EDynamicValueType::None;
						UClass* C = nullptr;
						return (P && P->ResolveType(T, C)) ? FSlateColor::UseForeground() : FSlateColor(FLinearColor(1.f, 0.35f, 0.35f));
					})
		];
}

void FDynamicPathCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> Handle, IDetailChildrenBuilder& Children, IPropertyTypeCustomizationUtils& CU)
{
	PathHandle = Handle;
	Utils = CU.GetPropertyUtilities();
	TSharedPtr<IPropertyHandle> H = Handle;

	TSharedPtr<IPropertyHandle> SourceH = Handle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDynamicPath, Source));
	TSharedPtr<IPropertyHandle> ActorH = Handle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDynamicPath, SpecificActor));
	TSharedPtr<IPropertyHandle> ClassH = Handle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDynamicPath, SourceClass));
	TSharedPtr<IPropertyHandle> StepsH = Handle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDynamicPath, Steps));

	// mudanças estruturais reconstroem o painel (os params gerados dependem delas)
	const FSimpleDelegate Refresh = MakeRefreshDelegate(CU);
	SourceH->SetOnPropertyValueChanged(Refresh);
	ActorH->SetOnPropertyValueChanged(Refresh);
	ClassH->SetOnPropertyValueChanged(Refresh);
	if (TSharedPtr<IPropertyHandleArray> Arr = StepsH->AsArray())
	{
		Arr->SetOnNumElementsChanged(Refresh);
	}

	Children.AddProperty(SourceH.ToSharedRef()); // combo padrão do enum

	Children.AddProperty(ActorH.ToSharedRef())
		.Visibility(TAttribute<EVisibility>::CreateLambda([H]()
			{
				const FDynamicPath* P = GetStructPtr<FDynamicPath>(H);
				return (P && P->Source == EDynamicPathSource::SpecificActor) ? EVisibility::Visible : EVisibility::Collapsed;
			}));

	// o hint de tipo só é necessário quando o ator não diz a classe sozinho
	Children.AddProperty(ClassH.ToSharedRef())
		.Visibility(TAttribute<EVisibility>::CreateLambda([H]()
			{
				const FDynamicPath* P = GetStructPtr<FDynamicPath>(H);
				return (P && (P->Source == EDynamicPathSource::Context || !P->SpecificActor)) ? EVisibility::Visible : EVisibility::Collapsed;
			}));

	Children.AddProperty(StepsH.ToSharedRef()); // cada elemento é desenhado por FDynamicStepCustomization
}

// ===========================================================================
// FDynamicStep
// ===========================================================================
FDynamicStep* FDynamicStepCustomization::GetStep() const
{
	return GetStructPtr<FDynamicStep>(StepHandle);
}

FDynamicPath* FDynamicStepCustomization::GetPath() const
{
	// element -> Steps array -> Path struct
	if (!StepHandle.IsValid()) return nullptr;
	TSharedPtr<IPropertyHandle> ArrayH = StepHandle->GetParentHandle();
	if (!ArrayH.IsValid()) return nullptr;
	return GetStructPtr<FDynamicPath>(ArrayH->GetParentHandle());
}

//FString FDynamicStepCustomization::GetPathMode() const
//{
//	if (!StepHandle.IsValid()) return FString();
//	TSharedPtr<IPropertyHandle> ArrayH = StepHandle->GetParentHandle();
//	TSharedPtr<IPropertyHandle> PathH = ArrayH.IsValid() ? ArrayH->GetParentHandle() : nullptr;
//	return PathH.IsValid() ? PathH->GetMetaData(TEXT("PathMode")) : FString();
//}

void FDynamicStepCustomization::RebuildOptions()
{
	Options.Reset();
	OptionNames.Reset();

	const FDynamicPath* Path = GetPath();
	const int32 Index = StepHandle.IsValid() ? StepHandle->GetIndexInArray() : INDEX_NONE;
	if (!Path || Index == INDEX_NONE) return;

	const UClass* C = Path->GetClassAtStep(Index);
	if (!C) return;

	// intermediate steps must give an object; the last step depends on the path mode
	const bool bLast = (Index == Path->Steps.Num() - 1);
	//const bool bAction = GetPathMode() == TEXT("Action");
	//const EDynamicMemberFilter Filter = !bLast ? EDynamicMemberFilter::Objects
	//	: (bAction ? EDynamicMemberFilter::Actions : EDynamicMemberFilter::Values);

	TArray<FDynamicMemberInfo> Infos;
	FDynamicStep::CollectMembers(C, /*Filter,*/ Infos);

	for (const FDynamicMemberInfo& I : Infos)
	{
		Options.Add(MakeShared<FString>(I.Label));
		OptionNames.Add(I.Name);
	}
}

FText FDynamicStepCustomization::GetMemberLabel() const
{
	const FDynamicStep* S = GetStep();
	if (!S || S->Member.IsNone()) return FText::FromString(TEXT("<select property or function>"));

	const FDynamicPath* Path = GetPath();
	const UClass* C = Path ? Path->GetClassAtStep(StepHandle->GetIndexInArray()) : nullptr;

	FDynamicMemberInfo Info;
	if (C && FDynamicStep::FindMember(C, S->Member, Info))
	{
		return FText::FromString(Info.Label);
	}
	return FText::FromString(S->Member.ToString() + TEXT("   (invalid)"));
}

void FDynamicStepCustomization::OnMemberSelected(TSharedPtr<FString> Item, ESelectInfo::Type SelectInfo)
{
	if (!Item.IsValid() || SelectInfo == ESelectInfo::Direct) return;

	const int32 Idx = Options.IndexOfByKey(Item);
	if (!OptionNames.IsValidIndex(Idx)) return;

	// The edit triggers PostEditChange -> the editor module hook regenerates Params for this and later steps.
	MemberHandle->SetValue(OptionNames[Idx]);

	if (TSharedPtr<IPropertyUtilities> U = Utils.Pin())
	{
		U->RequestForceRefresh();
	}
}

void FDynamicStepCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> Handle, FDetailWidgetRow& Row, IPropertyTypeCustomizationUtils& CU)
{
	StepHandle = Handle;
	KindHandle = Handle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDynamicStep, Kind));
	MemberHandle = Handle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDynamicStep, Member));
	ComponentHandle = Handle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDynamicStep, ComponentClass));
	Utils = CU.GetPropertyUtilities();

	const FSimpleDelegate Refresh = MakeRefreshDelegate(CU);
	KindHandle->SetOnPropertyValueChanged(Refresh);
	ComponentHandle->SetOnPropertyValueChanged(Refresh);

	RebuildOptions();

	const TSharedRef<FDynamicStepCustomization> Self = StaticCastSharedRef<FDynamicStepCustomization>(AsShared());

	Row.NameContent()
		[
			SNew(STextBlock)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.Text_Lambda([Handle]()
					{
						return FText::FromString(FString::Printf(TEXT("->  step %d"), Handle->GetIndexInArray() + 1));
					})
		]
	.ValueContent()
		.MinDesiredWidth(420.f)
		[
			SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.f, 0.f, 4.f, 0.f)
				[
					KindHandle->CreatePropertyValueWidget()
				]

				// Property / Function: searchable dropdown
				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				[
					SNew(SBox)
						.Visibility_Lambda([Self]()
							{
								const FDynamicStep* S = Self->GetStep();
								return (S && S->Kind == EDynamicStepKind::Member) ? EVisibility::Visible : EVisibility::Collapsed;
							})
						[
							SAssignNew(Combo, SSearchableComboBox)
								.OptionsSource(&Options)
								.OnGenerateWidget_Lambda([](TSharedPtr<FString> Item) -> TSharedRef<SWidget>
									{
										return SNew(STextBlock).Text(FText::FromString(Item.IsValid() ? *Item : FString()));
									})
								.OnSelectionChanged(Self, &FDynamicStepCustomization::OnMemberSelected)
								.OnComboBoxOpening_Lambda([Self]()
									{
										Self->RebuildOptions();
										if (Self->Combo.IsValid())
										{
											Self->Combo->RefreshOptions();
										}
									})
								.Content()
								[
									SNew(STextBlock)
										.Font(IDetailLayoutBuilder::GetDetailFont())
										.Text_Lambda([Self]() { return Self->GetMemberLabel(); })
								]
						]
				]

			// Component: class picker
			+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				[
					SNew(SBox)
						.Visibility_Lambda([Self]()
							{
								const FDynamicStep* S = Self->GetStep();
								return (S && S->Kind == EDynamicStepKind::Component) ? EVisibility::Visible : EVisibility::Collapsed;
							})
						[
							ComponentHandle->CreatePropertyValueWidget()
						]
				]
		];
}

void FDynamicStepCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> Handle, IDetailChildrenBuilder& Children, IPropertyTypeCustomizationUtils& CU)
{
	// Function parameters, one per row, without the array header
	TSharedPtr<IPropertyHandle> ParamsH = Handle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDynamicStep, Params));
	TSharedPtr<IPropertyHandleArray> Arr = ParamsH.IsValid() ? ParamsH->AsArray() : nullptr;
	if (!Arr.IsValid()) return;

	uint32 Num = 0;
	Arr->GetNumElements(Num);
	for (uint32 i = 0; i < Num; ++i)
	{
		Children.AddProperty(Arr->GetElement(i));
	}
}

// ===========================================================================
// FDynamicParam / FDynamicLiteral
// ===========================================================================
void FDynamicParamCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> Handle, FDetailWidgetRow& Row, IPropertyTypeCustomizationUtils& CU)
{
	TSharedPtr<IPropertyHandle> NameH = Handle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDynamicParam, ParamName));
	TSharedPtr<IPropertyHandle> ValueH = Handle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDynamicParam, Value));

	Row.NameContent()
		[
			SNew(STextBlock)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.Text_Lambda([NameH]()
					{
						FName N;
						NameH->GetValue(N);
						return FText::FromString(TEXT("      ") + N.ToString());
					})
		]
	.ValueContent()
		.MinDesiredWidth(200.f)
		[
			MakeLiteralWidget(ValueH, CU)
		];
}

void FDynamicLiteralCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> Handle, FDetailWidgetRow& Row, IPropertyTypeCustomizationUtils& CU)
{
	Row.NameContent()
		[
			Handle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MinDesiredWidth(200.f)
		[
			MakeLiteralWidget(Handle, CU)
		];
}

