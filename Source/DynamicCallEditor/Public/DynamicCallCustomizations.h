// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
//#include "UObject/Object.h"
//#include "DynamicCallCustomizations.generated.h"
#include "DynamicCallTypes.h"
#include "IPropertyTypeCustomization.h"
#include "PropertyHandle.h"

class IPropertyUtilities;
class SSearchableComboBox;
//struct FDynamicPath;
//struct FDynamicStep;

/** FDynamicPath: summary header ("Instigator -> [Inventory] -> HasItem(X)  [bool]") + Source / Actor / Type / Steps. */
class FDynamicPathCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance() { return MakeShared<FDynamicPathCustomization>(); }

	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> Handle, FDetailWidgetRow& Row, IPropertyTypeCustomizationUtils& Utils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> Handle, IDetailChildrenBuilder& Children, IPropertyTypeCustomizationUtils& Utils) override;

private:
	//FText GetSourceLabel() const;
	void OnSourceSelected(TSharedPtr<FString> Item, ESelectInfo::Type SelectInfo);

	TSharedPtr<IPropertyHandle> PathHandle;
	//TSharedPtr<IPropertyHandle> KeyHandle;
	TWeakPtr<IPropertyUtilities> Utils;

	//TArray<TSharedPtr<FString>> SourceOptions;
	//TArray<FName> SourceKeys; // NAME_None = "Specific Actor"
	//TSharedPtr<SSearchableComboBox> SourceCombo;
};

/** FDynamicStep: [kind] [searchable member dropdown OR component class picker]; children = parameters. */
class FDynamicStepCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance() { return MakeShared<FDynamicStepCustomization>(); }

	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> Handle, FDetailWidgetRow& Row, IPropertyTypeCustomizationUtils& Utils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> Handle, IDetailChildrenBuilder& Children, IPropertyTypeCustomizationUtils& Utils) override;

private:
	FDynamicPath* GetPath() const;
	FDynamicStep* GetStep() const;
	//FString GetPathMode() const;
	void RebuildOptions();
	FText GetMemberLabel() const;
	void OnMemberSelected(TSharedPtr<FString> Item, ESelectInfo::Type SelectInfo);

	TSharedPtr<IPropertyHandle> StepHandle;
	TSharedPtr<IPropertyHandle> KindHandle;
	TSharedPtr<IPropertyHandle> MemberHandle;
	TSharedPtr<IPropertyHandle> ComponentHandle;
	TWeakPtr<IPropertyUtilities> Utils;

	TArray<TSharedPtr<FString>> Options;
	TArray<FName> OptionNames;
	TSharedPtr<SSearchableComboBox> Combo;
};

/** FDynamicParam: "ParamName | widget of the right type" on a single row. */
class FDynamicParamCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance() { return MakeShared<FDynamicParamCustomization>(); }

	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> Handle, FDetailWidgetRow& Row, IPropertyTypeCustomizationUtils& Utils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> Handle, IDetailChildrenBuilder& Children, IPropertyTypeCustomizationUtils& Utils) override {}
};

/** FDynamicLiteral: a single row with only the widget that matches its Type (checkbox, number, text, asset picker). */
class FDynamicLiteralCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance() { return MakeShared<FDynamicLiteralCustomization>(); }

	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> Handle, FDetailWidgetRow& Row, IPropertyTypeCustomizationUtils& Utils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> Handle, IDetailChildrenBuilder& Children, IPropertyTypeCustomizationUtils& Utils) override {}
};
