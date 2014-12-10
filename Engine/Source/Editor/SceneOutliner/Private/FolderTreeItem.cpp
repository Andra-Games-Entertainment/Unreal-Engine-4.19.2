// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SceneOutlinerPrivatePCH.h"
#include "SSceneOutliner.h"

#include "SceneOutlinerFilters.h"

#include "EditorActorFolders.h"
#include "FolderTreeItem.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "SceneOutliner_FolderTreeItem"

namespace SceneOutliner
{

FDragValidationInfo FFolderDropTarget::ValidateDrop(FDragDropPayload& DraggedObjects, UWorld& World) const
{
	if (DraggedObjects.Folders)
	{
		// Iterate over all the folders that have been dragged
		for (FName DraggedFolder : DraggedObjects.Folders.GetValue())
		{
			FName Leaf = GetFolderLeafName(DraggedFolder);
			FName Parent = GetParentPath(DraggedFolder);

			if (Parent == DestinationPath)
			{
				FFormatNamedArguments Args;
				Args.Add(TEXT("SourceName"), FText::FromName(Leaf));

				FText Text;
				if (DestinationPath.IsNone())
				{
					Text = FText::Format(LOCTEXT("FolderAlreadyAssignedRoot", "{SourceName} is already assigned to root"), Args);
				}
				else
				{
					Args.Add(TEXT("DestPath"), FText::FromName(DestinationPath));
					Text = FText::Format(LOCTEXT("FolderAlreadyAssigned", "{SourceName} is already assigned to {DestPath}"), Args);
				}
				
				return FDragValidationInfo(FActorDragDropGraphEdOp::ToolTip_IncompatibleGeneric, Text);
			}

			const FString DragFolderPath = DraggedFolder.ToString();
			const FString LeafName = Leaf.ToString();
			const FString DstFolderPath = DestinationPath.IsNone() ? FString() : DestinationPath.ToString();
			const FString NewPath = DstFolderPath / LeafName;

			if (FActorFolders::Get().GetFolderProperties(World, FName(*NewPath)))
			{
				// The folder already exists
				FFormatNamedArguments Args;
				Args.Add(TEXT("DragName"), FText::FromString(LeafName));
				return FDragValidationInfo(FActorDragDropGraphEdOp::ToolTip_IncompatibleGeneric,
					FText::Format(LOCTEXT("FolderAlreadyExistsRoot", "A folder called \"{DragName}\" already exists at this level"), Args));
			}
			else if (DragFolderPath == DstFolderPath || DstFolderPath.StartsWith(DragFolderPath + "/"))
			{
				// Cannot drag as a child of itself
				FFormatNamedArguments Args;
				Args.Add(TEXT("FolderPath"), FText::FromName(DraggedFolder));
				return FDragValidationInfo(
					FActorDragDropGraphEdOp::ToolTip_IncompatibleGeneric,
					FText::Format(LOCTEXT("ChildOfItself", "Cannot move \"{FolderPath}\" to be a child of itself"), Args));
			}
		}
	}

	if (DraggedObjects.Actors)
	{
		// Iterate over all the folders that have been dragged
		for (auto WeakActor : DraggedObjects.Actors.GetValue())
		{
			AActor* Actor = WeakActor.Get();

			if (Actor->GetFolderPath() == DestinationPath)
			{
				FFormatNamedArguments Args;
				Args.Add(TEXT("SourceName"), FText::FromString(Actor->GetActorLabel()));

				FText Text;
				if (DestinationPath.IsNone())
				{
					Text = FText::Format(LOCTEXT("FolderAlreadyAssignedRoot", "{SourceName} is already assigned to root"), Args);
				}
				else
				{
					Args.Add(TEXT("DestPath"), FText::FromName(DestinationPath));
					Text = FText::Format(LOCTEXT("FolderAlreadyAssigned", "{SourceName} is already assigned to {DestPath}"), Args);
				}
				
				return FDragValidationInfo(FActorDragDropGraphEdOp::ToolTip_IncompatibleGeneric, Text);
			}
		}
	}

	// Everything else is a valid operation
	if (DestinationPath.IsNone())
	{
		return FDragValidationInfo(FActorDragDropGraphEdOp::ToolTip_CompatibleGeneric, LOCTEXT("MoveToRoot", "Move to root"));
	}
	else
	{
		return FDragValidationInfo(FActorDragDropGraphEdOp::ToolTip_CompatibleGeneric, FText::Format(LOCTEXT("MoveInto", "Move into \"{DropPath}\""), FText::FromName(DestinationPath)));
	}
}

void FFolderDropTarget::OnDrop(FDragDropPayload& DraggedObjects, UWorld& World, const FDragValidationInfo& ValidationInfo, TSharedRef<SWidget> DroppedOnWidget)
{
	const FScopedTransaction Transaction( LOCTEXT("MoveOutlinerItems", "Move Scene Outliner Items") );

	if (DraggedObjects.Folders)
	{
		for (FName Folder : DraggedObjects.Folders.GetValue())
		{
			MoveFolderTo(Folder, DestinationPath, World);
		}
	}

	if (DraggedObjects.Actors)
	{
		for (auto WeakActor : DraggedObjects.Actors.GetValue())
		{
			AActor* Actor = WeakActor.Get();
			if (Actor)
			{
				Actor->SetFolderPath(DestinationPath);
			}
		}
	}
}

FFolderTreeItem::FFolderTreeItem(FName InPath)
	: Path(InPath)
	, LeafName(GetFolderLeafName(InPath))
{
}

FTreeItemPtr FFolderTreeItem::FindParent(const FTreeItemMap& ExistingItems) const
{
	const FName ParentPath = GetParentPath(Path);
	if (!ParentPath.IsNone())
	{
		return ExistingItems.FindRef(ParentPath);
	}

	if (SharedData->RepresentingWorld)
	{
		return ExistingItems.FindRef(SharedData->RepresentingWorld);
	}

	return nullptr;
}

FTreeItemPtr FFolderTreeItem::CreateParent() const
{
	const FName ParentPath = GetParentPath(Path);
	if (!ParentPath.IsNone())
	{
		return MakeShareable(new FFolderTreeItem(ParentPath));
	}

	if (SharedData->RepresentingWorld)
	{
		return MakeShareable(new FWorldTreeItem(SharedData->RepresentingWorld));
	}

	return nullptr;
}

void FFolderTreeItem::Visit(const ITreeItemVisitor& Visitor) const
{
	Visitor.Visit(*this);
}

void FFolderTreeItem::Visit(const IMutableTreeItemVisitor& Visitor)
{
	Visitor.Visit(*this);
}

FTreeItemID FFolderTreeItem::GetID() const
{
	return FTreeItemID(Path);
}

FString FFolderTreeItem::GetDisplayString() const
{
	return LeafName.ToString();
}

int32 FFolderTreeItem::GetTypeSortPriority() const
{
	return ETreeItemSortOrder::Folder;
}


bool FFolderTreeItem::CanInteract() const
{
	return Flags.bInteractive;
}

void FFolderTreeItem::Delete()
{
	if (!SharedData->RepresentingWorld)
	{
		return;
	}

	struct FResetActorFolders : IMutableTreeItemVisitor
	{
		virtual void Visit(FActorTreeItem& ActorItem) const override
		{
			if (AActor* Actor = ActorItem.Actor.Get())
			{
				Actor->SetFolderPath(FName());
			}	
		}
		virtual void Visit(FFolderTreeItem& FolderItem) const override
		{
			FResetActorFolders ResetFolders;
			for (auto& Child : FolderItem.GetChildren())
			{
				Child.Pin()->Visit(ResetFolders);
			}
			FolderItem.Delete();
		}
	};

	const FScopedTransaction Transaction( LOCTEXT("DeleteFolder", "Delete Folder") );

	FResetActorFolders ResetFolders;
	for (auto& Child : GetChildren())
	{
		Child.Pin()->Visit(ResetFolders);
	}

	FActorFolders::Get().DeleteFolder(*SharedData->RepresentingWorld, Path);
}

void FFolderTreeItem::CreateSubFolder(TWeakPtr<SSceneOutliner> WeakOutliner)
{
	auto Outliner = WeakOutliner.Pin();

	if (Outliner.IsValid() && SharedData->RepresentingWorld)
	{
		const FScopedTransaction Transaction(LOCTEXT("UndoAction_CreateFolder", "Create Folder"));

		const FName NewFolderName = FActorFolders::Get().GetDefaultFolderName(*SharedData->RepresentingWorld, Path);
		FActorFolders::Get().CreateFolder(*SharedData->RepresentingWorld, NewFolderName);

		// At this point the new folder will be in our newly added list, so select it and open a rename when it gets refreshed
		Outliner->OnItemAdded(NewFolderName, ENewItemAction::Select | ENewItemAction::Rename);
	}
}

void FFolderTreeItem::OnExpansionChanged()
{
	if (!SharedData->RepresentingWorld)
	{
		return;
	}

	// Update the central store of folder properties with this folder's new expansion state
	if (FActorFolderProps* Props = FActorFolders::Get().GetFolderProperties(*SharedData->RepresentingWorld, Path))
	{
		Props->bIsExpanded = Flags.bIsExpanded;
	}
}

void FFolderTreeItem::GenerateContextMenu(FMenuBuilder& MenuBuilder, SSceneOutliner& Outliner)
{
	auto SharedOutliner = StaticCastSharedRef<SSceneOutliner>(Outliner.AsShared());

	const FSlateIcon NewFolderIcon(FEditorStyle::GetStyleSetName(), "SceneOutliner.NewFolderIcon");
	
	MenuBuilder.AddMenuEntry(LOCTEXT("CreateSubFolder", "Create Sub Folder"), FText(), NewFolderIcon, FUIAction(FExecuteAction::CreateSP(this, &FFolderTreeItem::CreateSubFolder, TWeakPtr<SSceneOutliner>(SharedOutliner))));
	MenuBuilder.AddMenuEntry(LOCTEXT("RenameFolder", "Rename"), FText(), FSlateIcon(), FUIAction(FExecuteAction::CreateSP(&Outliner, &SSceneOutliner::InitiateRename, AsShared())));
	MenuBuilder.AddMenuEntry(LOCTEXT("DeleteFolder", "Delete"), FText(), FSlateIcon(), FUIAction(FExecuteAction::CreateSP(this, &FFolderTreeItem::Delete)));
}

void FFolderTreeItem::PopulateDragDropPayload(FDragDropPayload& Payload) const
{
	if (!Payload.Folders)
	{
		Payload.Folders = FFolderPaths();
	}
	Payload.Folders->Add(Path);
}

FDragValidationInfo FFolderTreeItem::ValidateDrop(FDragDropPayload& DraggedObjects, UWorld& World) const
{
	FFolderDropTarget Target(Path);
	return Target.ValidateDrop(DraggedObjects, World);
}

void FFolderTreeItem::OnDrop(FDragDropPayload& DraggedObjects, UWorld& World, const FDragValidationInfo& ValidationInfo, TSharedRef<SWidget> DroppedOnWidget)
{
	FFolderDropTarget Target(Path);
	return Target.OnDrop(DraggedObjects, World, ValidationInfo, DroppedOnWidget);
}

}	// namespace SceneOutliner

#undef LOCTEXT_NAMESPACE