/*==================================================================================
    Copyright (c) 2008, binaryzebra
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
    * Neither the name of the binaryzebra nor the
    names of its contributors may be used to endorse or promote products
    derived from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE binaryzebra AND CONTRIBUTORS "AS IS" AND
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL binaryzebra BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
=====================================================================================*/



#include "DAVAEngine.h"
#include "Entity/Component.h"
#include "Main/mainwindow.h"

#include <QPushButton>
#include <QFile>
#include <QTextStream>

#include "DockProperties/PropertyEditor.h"
#include "MaterialEditor/MaterialEditor.h"
#include "Tools/QtPropertyEditor/QtPropertyData/QtPropertyDataIntrospection.h"
#include "Tools/QtPropertyEditor/QtPropertyData/QtPropertyDataDavaVariant.h"
#include "Tools/QtPropertyEditor/QtPropertyData/QtPropertyDataDavaKeyedArchive.h"
#include "Tools/QtPropertyEditor/QtPropertyData/QtPropertyDataKeyedArchiveMember.h"
#include "Tools/QtPropertyEditor/QtPropertyData/QtPropertyDataInspColl.h"
#include "Tools/QtPropertyEditor/QtPropertyData/QtPropertyDataInspMember.h"
#include "Tools/QtPropertyEditor/QtPropertyData/QtPropertyDataInspDynamic.h"
#include "Tools/QtPropertyEditor/QtPropertyData/QtPropertyDataMetaObject.h"
#include "Commands2/MetaObjModifyCommand.h"
#include "Commands2/InspMemberModifyCommand.h"

#include "PropertyEditorStateHelper.h"
#include "Qt/Project/ProjectManager.h"

#include "ActionComponentEditor.h"
#include "SoundBrowser/FMODSoundBrowser.h"

PropertyEditor::PropertyEditor(QWidget *parent /* = 0 */, bool connectToSceneSignals /*= true*/)
	: QtPropertyEditor(parent)
	, viewMode(VIEW_NORMAL)
	, curNode(NULL)
	, treeStateHelper(this, curModel)
	, favoriteGroup(NULL)
{
	if(connectToSceneSignals)
	{
		QObject::connect(SceneSignals::Instance(), SIGNAL(Activated(SceneEditor2 *)), this, SLOT(sceneActivated(SceneEditor2 *)));
		QObject::connect(SceneSignals::Instance(), SIGNAL(Deactivated(SceneEditor2 *)), this, SLOT(sceneDeactivated(SceneEditor2 *)));
		QObject::connect(SceneSignals::Instance(), SIGNAL(CommandExecuted(SceneEditor2 *, const Command2*, bool)), this, SLOT(CommandExecuted(SceneEditor2 *, const Command2*, bool )));
		QObject::connect(SceneSignals::Instance(), SIGNAL(SelectionChanged(SceneEditor2 *, const EntityGroup *, const EntityGroup *)), this, SLOT(sceneSelectionChanged(SceneEditor2 *, const EntityGroup *, const EntityGroup *)));
	}
	posSaver.Attach(this, "DocPropetyEditor");

	DAVA::VariantType v = posSaver.LoadValue("splitPos");
	if(v.GetType() == DAVA::VariantType::TYPE_INT32) header()->resizeSection(0, v.AsInt32());

	SetUpdateTimeout(5000);
	SetEditTracking(true);
	setMouseTracking(true);

	LoadScheme("~doc:/PropEditorDefault.scheme");
}

PropertyEditor::~PropertyEditor()
{
	DAVA::VariantType v(header()->sectionSize(0));
	posSaver.SaveValue("splitPos", v);

	SafeRelease(curNode);
}

void PropertyEditor::SetEntities(const EntityGroup *selected)
{
/*
	DAVA::KeyedArchive *ka = new DAVA::KeyedArchive();
	ResetProperties();
	AppendProperty("test", new QtPropertyDataDavaKeyedArcive(ka));

	return;
*/

    //TODO: support multiselected editing

	SafeRelease(curNode);
    if(NULL != selected && selected->Size() == 1)
 	{
         curNode = SafeRetain(selected->GetEntity(0));

		 // ensure that custom properties exist
		 // this call will create them if they are not created yet
		 curNode->GetCustomProperties();
 	}

    ResetProperties();
	SaveScheme("~doc:/PropEditorDefault.scheme");
}

void PropertyEditor::SetViewMode(eViewMode mode)
{
	if(viewMode != mode)
	{
		viewMode = mode;
        ResetProperties();
	}
}

PropertyEditor::eViewMode PropertyEditor::GetViewMode() const
{
	return viewMode;
}

void PropertyEditor::SetFavoritesEditMode(bool set)
{
	if(favoritesEditMode != set)
	{
		favoritesEditMode = set;
		ResetProperties();
	}
}

bool PropertyEditor::GetFavoritesEditMode() const
{
	return favoritesEditMode;
}

void PropertyEditor::ResetProperties()
{
    // Store the current Property Editor Tree state before switching to the new node.
	// Do not clear the current states map - we are using one storage to share opened
	// Property Editor nodes between the different Scene Nodes.
	treeStateHelper.SaveTreeViewState(false);

	RemovePropertyAll();
	favoriteGroup = NULL;

	if(NULL != curNode)
	{
		// create data tree, but don't add it to the property editor
		QtPropertyData *root = new QtPropertyData();

		// add info about current entity
		QtPropertyData *curEntityData = CreateInsp(curNode, curNode->GetTypeInfo());
		root->ChildAdd(curNode->GetTypeInfo()->Name(), curEntityData);

		// add info about components
		for(int32 i = 0; i < Component::COMPONENT_COUNT; ++i)
		{
			Component *component = curNode->GetComponent(i);
			if(component)
			{
				QtPropertyData *componentData = CreateInsp(component, component->GetTypeInfo());
				root->ChildAdd(component->GetTypeInfo()->Name(), componentData);
			}
		}

		ApplyFavorite(root);
		ApplyModeFilter(root);
		ApplyCustomButtons(root);

		// add not empty rows from root
		while(0 != root->ChildCount())
		{
			QtPropertyData *row = root->ChildGet(0);
			root->ChildExtract(row);

			if(row->ChildCount() > 0)
			{
				AppendProperty(row->GetName(), row);
				ApplyStyle(row, QtPropertyEditor::HEADER_STYLE);
			}
		}

		delete root;
	}
    
	// Restore back the tree view state from the shared storage.
	if (!treeStateHelper.IsTreeStateStorageEmpty())
	{
		treeStateHelper.RestoreTreeViewState();
	}
	else
	{
		// Expand the root elements as default value.
		expandToDepth(0);
	}
}

void PropertyEditor::ApplyModeFilter(QtPropertyData *parent)
{
	if(NULL != parent)
	{
		for(int i = 0; i < parent->ChildCount(); ++i)
		{
			bool toBeRemove = false;
			QtPropertyData *data = parent->ChildGet(i);

			// show only editable items and favorites
			if(viewMode == VIEW_NORMAL)
			{
				if(!data->IsEditable())
				{
					toBeRemove = true;
				}
			}
			// show all editable/viewable items
			else if(viewMode == VIEW_ADVANCED)
			{

			}
			// show only favorite items
			else if(viewMode == VIEW_FAVORITES_ONLY)
			{
				PropEditorUserData *userData = GetUserData(data);
				if(userData->type == PropEditorUserData::ORIGINAL)
				{
					toBeRemove = true;

					// remove from favorite data back link to the original data
					// because original data will be removed from properties
					QtPropertyData *favorite = userData->associatedData;
					if(NULL != favorite)
					{
						GetUserData(favorite)->associatedData = NULL;
					}
				}
			}

			if(toBeRemove)
			{
				parent->ChildRemove(data);
				i--;
			}
			else
			{
				// apply mode to data childs
				ApplyModeFilter(data);
			}
		}
	}
}

void PropertyEditor::ApplyFavorite(QtPropertyData *data)
{
	if(NULL != data)
	{
		if(scheme.contains(data->GetPath()))
		{
			SetFavorite(data, true);
		}

		// go through childs
		for(int i = 0; i < data->ChildCount(); ++i)
		{
			ApplyFavorite(data->ChildGet(i));
		}
	}
}

void PropertyEditor::ApplyCustomButtons(QtPropertyData *data)
{
	if(NULL != data)
	{
		const DAVA::MetaInfo *meta = data->MetaInfo();

		if(NULL != meta)
		{
			if(DAVA::MetaInfo::Instance<DAVA::ActionComponent>() == meta)
			{
				// Add optional button to edit action component
				QtPropertyToolButton *editActions = data->AddButton();
				editActions->setIcon(QIcon(":/QtIcons/settings.png"));
				editActions->setAutoRaise(true);

				QObject::connect(editActions, SIGNAL(pressed()), this, SLOT(ActionEditComponent()));
			}
			else if(DAVA::MetaInfo::Instance<DAVA::RenderObject>() == meta)
			{
				// Add optional button to bake transform render object
				QtPropertyToolButton *bakeButton = data->AddButton();
				bakeButton->setToolTip("Bake Transform");
				bakeButton->setIcon(QIcon(":/QtIcons/transform_bake.png"));
				bakeButton->setIconSize(QSize(12, 12));
				bakeButton->setAutoRaise(true);

				QObject::connect(bakeButton, SIGNAL(pressed()), this, SLOT(ActionBakeTransform()));
			}
			else if(DAVA::MetaInfo::Instance<DAVA::NMaterial>() == meta)
			{
				// Add optional button to bake transform render object
				QtPropertyToolButton *goToMaterialButton = data->AddButton();
				goToMaterialButton->setToolTip("Edit material");
				goToMaterialButton->setIcon(QIcon(":/QtIcons/3d.png"));
				goToMaterialButton->setIconSize(QSize(12, 12));
				goToMaterialButton->setAutoRaise(true);

				QObject::connect(goToMaterialButton, SIGNAL(pressed()), this, SLOT(ActionEditMaterial()));
			}
		}

		// go through childs
		for(int i = 0; i < data->ChildCount(); ++i)
		{
			ApplyCustomButtons(data->ChildGet(i));
		}
	}
}

QtPropertyData* PropertyEditor::CreateInsp(void *object, const DAVA::InspInfo *info)
{
	QtPropertyData *ret = NULL;

	if(NULL != info)
	{
		bool hasMembers = false;
		const InspInfo *baseInfo = info;

		// check if there are any members in introspection
		while(NULL != baseInfo)
		{
			if(baseInfo->MembersCount() > 0)
			{
				hasMembers = true;
				break;
			}
			baseInfo = baseInfo->BaseInfo();
		}

		ret = new QtPropertyDataIntrospection(object, info, false);

		// add members is we can
		if(hasMembers)
		{
			while(NULL != baseInfo)
			{
				for(int i = 0; i < baseInfo->MembersCount(); ++i)
				{
					const DAVA::InspMember *member = baseInfo->Member(i);

					if(!ExcludeMember(baseInfo, member))
					{
						QtPropertyData *memberData = CreateInspMember(object, member);
						ret->ChildAdd(member->Name(), memberData);
					}
				}

				baseInfo = baseInfo->BaseInfo();
			}
		}
    }

	return ret;
}

QtPropertyData* PropertyEditor::CreateInspMember(void *object, const DAVA::InspMember *member)
{
	QtPropertyData* ret = NULL;

	if(NULL != member && (member->Flags() & DAVA::I_VIEW))
	{
		void *momberObject = member->Data(object);
		const DAVA::InspInfo *memberIntrospection = member->Type()->GetIntrospection(momberObject);
		bool isKeyedArchive = (member->Type() == DAVA::MetaInfo::Instance<DAVA::KeyedArchive*>());

		if(NULL != memberIntrospection && !isKeyedArchive)
		{
			ret = CreateInsp(momberObject, memberIntrospection);
		}
		else
		{
			if(member->Collection() && !isKeyedArchive)
			{
				ret = CreateInspCollection(momberObject, member->Collection());
			}
			else
			{
				ret = QtPropertyDataIntrospection::CreateMemberData(object, member);
			}
		}
	}

	return ret;
}

QtPropertyData* PropertyEditor::CreateInspCollection(void *object, const DAVA::InspColl *collection)
{
	QtPropertyData *ret = new QtPropertyDataInspColl(object, collection, false);

	if(NULL != collection && collection->Size(object) > 0)
	{
		int index = 0;
		DAVA::MetaInfo *valueType = collection->ItemType();
		DAVA::InspColl::Iterator i = collection->Begin(object);
		while(NULL != i)
		{
			if(NULL != valueType->GetIntrospection())
			{
				void * itemObject = collection->ItemData(i);
				const DAVA::InspInfo *itemInfo = valueType->GetIntrospection(itemObject);

				QtPropertyData *inspData = CreateInsp(itemObject, itemInfo);
				ret->ChildAdd(QString::number(index), inspData);
			}
			else
			{
				if(!valueType->IsPointer())
				{
					QtPropertyDataMetaObject *childData = new QtPropertyDataMetaObject(collection->ItemPointer(i), valueType);
					ret->ChildAdd(QString::number(index), childData);
				}
				else
				{
					QString s;
					QtPropertyData* childData = new QtPropertyData(s.sprintf("[%p] Pointer", collection->ItemData(i)));
					childData->SetEnabled(false);

					if(collection->ItemKeyType() == DAVA::MetaInfo::Instance<DAVA::FastName>())
					{
						const DAVA::FastName *fname = (const DAVA::FastName *) collection->ItemKeyData(i);
						ret->ChildAdd(fname->operator*(), childData);
					}
					else
					{
						ret->ChildAdd(QString::number(index), childData);
					}
				}
			}

			index++;
			i = collection->Next(i);
		}
	}

	return ret;
}

QtPropertyData* PropertyEditor::CreateClone(QtPropertyData *original)
{
	QtPropertyDataIntrospection *inspData = dynamic_cast<QtPropertyDataIntrospection *>(original);
	if(NULL != inspData)
	{
		return CreateInsp(inspData->object, inspData->info);
	}

	QtPropertyDataInspMember *memberData = dynamic_cast<QtPropertyDataInspMember *>(original);
	if(NULL != memberData)
	{
		return CreateInspMember(memberData->object, memberData->member);
	}

	QtPropertyDataInspDynamic *memberDymanic = dynamic_cast<QtPropertyDataInspDynamic *>(original);
	if(NULL != memberData)
	{
		return CreateInspMember(memberDymanic->object, memberDymanic->dynamicInfo->GetMember());
	}

	QtPropertyDataMetaObject *metaData  = dynamic_cast<QtPropertyDataMetaObject *>(original);
	if(NULL != metaData)
	{
		return new QtPropertyDataMetaObject(metaData->object, metaData->meta);
	}

	QtPropertyDataInspColl *memberCollection = dynamic_cast<QtPropertyDataInspColl *>(original);
	if(NULL != memberCollection)
	{
		return CreateInspCollection(memberCollection->object, memberCollection->collection);
	}

	QtPropertyDataDavaKeyedArcive *memberArch = dynamic_cast<QtPropertyDataDavaKeyedArcive *>(original);
	if(NULL != memberArch)
	{
		return new QtPropertyDataDavaKeyedArcive(memberArch->archive);
	}

	QtPropertyKeyedArchiveMember *memberArchMem = dynamic_cast<QtPropertyKeyedArchiveMember *>(original);
	if(NULL != memberArchMem)
	{
		return new QtPropertyKeyedArchiveMember(memberArchMem->archive, memberArchMem->key);
	}

	return new QtPropertyData(original->GetName(), original->GetFlags());
}

void PropertyEditor::sceneActivated(SceneEditor2 *scene)
{
	if(NULL != scene)
	{
        const EntityGroup selection = scene->selectionSystem->GetSelection();
		SetEntities(&selection);
	}
}

void PropertyEditor::sceneDeactivated(SceneEditor2 *scene)
{
	SetEntities(NULL);
}

void PropertyEditor::sceneSelectionChanged(SceneEditor2 *scene, const EntityGroup *selected, const EntityGroup *deselected)
{
    SetEntities(selected);
}

void PropertyEditor::CommandExecuted(SceneEditor2 *scene, const Command2* command, bool redo)
{
	int cmdId = command->GetId();

	switch (cmdId)
	{
	case CMDID_COMPONENT_ADD:
	case CMDID_COMPONENT_REMOVE:
	case CMDID_CONVERT_TO_SHADOW:
	case CMDID_PARTICLE_EMITTER_LOAD_FROM_YAML:
		if(command->GetEntity() == curNode)
		{
			ResetProperties();
		}
		break;
	default:
		OnUpdateTimeout();
		break;
	}
}

void PropertyEditor::OnItemEdited(const QModelIndex &index)
{
	QtPropertyEditor::OnItemEdited(index);

	QtPropertyData *propData = GetProperty(index);

	if(NULL != propData)
	{
		Command2 *command = (Command2 *) propData->CreateLastCommand();
		if(NULL != command)
		{
			SceneEditor2 *curScene = QtMainWindow::Instance()->GetCurrentScene();
			if(NULL != curScene)
			{
				curScene->Exec(command);
			}
		}

		// go top, unit we find property item, that has other associated
		// and we should update it to ensure that 
		// new/old items was also added/removed for that associated data
		QtPropertyData *parent = propData;
		while(NULL != parent)
		{
			PropEditorUserData *userData = GetUserData(parent);
			if(NULL != userData->associatedData)
			{
				userData->associatedData->UpdateValue(true);
				break;
			}

			parent = parent->Parent();
		}
	}
}

void PropertyEditor::mouseReleaseEvent(QMouseEvent *event)
{
	bool skipEvent = false;
	QModelIndex index = indexAt(event->pos());

	// handle favorite state toggle for item under mouse
	if(favoritesEditMode && index.parent().isValid() && index.column() == 0)
	{
		QRect rect = visualRect(index);
		rect.setX(0);
		rect.setWidth(16);

		if(rect.contains(event->pos()))
		{
			QtPropertyData *data = GetProperty(index);
			if(NULL != data && !IsParentFavorite(data))
			{
				SetFavorite(data, !IsFavorite(data));
				skipEvent = true;
			}
		}
	}

	if(!skipEvent)
	{
		QtPropertyEditor::mouseReleaseEvent(event);
	}
}

void PropertyEditor::drawRow(QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index) const
{
	static QIcon favIcon = QIcon(":/QtIcons/star.png");
	static QIcon nfavIcon = QIcon(":/QtIcons/star_empty.png");

	// custom draw for favorites edit mode
	QStyleOptionViewItemV4 opt = option;
	if(index.parent().isValid() && favoritesEditMode)
	{
		QtPropertyData *data = GetProperty(index);
		if(NULL != data)
		{
			if(!IsParentFavorite(data))
			{
				if(IsFavorite(data))
				{
					favIcon.paint(painter, opt.rect.x(), opt.rect.y(), 16, opt.rect.height());
				}
				else
				{
					nfavIcon.paint(painter, opt.rect.x(), opt.rect.y(), 16, opt.rect.height());
				}
			}
		}
	}

	QtPropertyEditor::drawRow(painter, opt, index);
}

void PropertyEditor::ActionEditComponent()
{
	if(NULL != curNode)
	{
		ActionComponentEditor editor;

		editor.SetComponent((DAVA::ActionComponent*)curNode->GetComponent(DAVA::Component::ACTION_COMPONENT));
		editor.exec();

		ResetProperties();
	}	
}

void PropertyEditor::ActionBakeTransform()
{
	if(NULL != curNode)
	{
		DAVA::RenderObject * ro = GetRenderObject(curNode);
		if(NULL != ro)
		{
			ro->BakeTransform(curNode->GetLocalTransform());
			curNode->SetLocalTransform(DAVA::Matrix4::IDENTITY);
		}
	}
}

void PropertyEditor::ActionEditMaterial()
{
	QtPropertyToolButton *btn = dynamic_cast<QtPropertyToolButton *>(QObject::sender());

	if(NULL != btn)
	{
		QtPropertyDataIntrospection *data = dynamic_cast<QtPropertyDataIntrospection *>(btn->GetPropertyData());
		if(NULL != data)
		{
			QtMainWindow::Instance()->OnMaterialEditor();
			MaterialEditor::Instance()->SelectMaterial((DAVA::NMaterial *) data->object);
		}
	}
}

bool PropertyEditor::IsParentFavorite(QtPropertyData *data) const
{
	bool ret = false;

	QtPropertyData *parent = data->Parent();
	while(NULL != parent)
	{
		if(IsFavorite(parent))
		{
			ret = true;
			break;
		}
		else
		{
			parent = parent->Parent();
		}
	}

	return ret;
}

bool PropertyEditor::IsFavorite(QtPropertyData *data) const
{
	bool ret = false;

	if(NULL != data)
	{
		PropEditorUserData *userData = GetUserData(data);
		ret = userData->isFavorite;
	}

	return ret;
}

void PropertyEditor::SetFavorite(QtPropertyData *data, bool favorite)
{
	if(NULL == favoriteGroup)
	{
		favoriteGroup = GetProperty(InsertHeader("Favorites", 0));
	}

	if(NULL != data)
	{
		PropEditorUserData *userData = GetUserData(data);

		switch(userData->type)
		{
			case PropEditorUserData::ORIGINAL:
				if(userData->isFavorite != favorite)
				{
					// it is in favorite no, so we are going to remove it from favorites
					if(userData->isFavorite)
					{
						DVASSERT(NULL != userData->associatedData);

						QtPropertyData *favorite = userData->associatedData;
						favoriteGroup->ChildRemove(favorite);
						userData->associatedData = NULL;
						userData->isFavorite = false;

						scheme.remove(data->GetPath());
					}
					// new item should be added to favorites list
					else
					{
						DVASSERT(NULL == userData->associatedData);

						QtPropertyData *favorite = CreateClone(data);
						ApplyCustomButtons(favorite);

						favoriteGroup->ChildAdd(data->GetName(), favorite);
						userData->associatedData = favorite;
						userData->isFavorite = true;

						// create user data for added favorite, that will have COPY type,
						// and associatedData will point to the original property
						favorite->SetUserData(new PropEditorUserData(PropEditorUserData::COPY, data, true));

						scheme.insert(data->GetPath());
					}

					data->EmitDataChanged(QtPropertyData::VALUE_SET);
				}
				break;

			case PropEditorUserData::COPY:
				if(userData->isFavorite != favorite)
				{
					// copy of the original data can only be removed
					DVASSERT(!favorite);
					
					QtPropertyData *original = userData->associatedData;

					// copy of the original data should always have a pointer to the original property data
					if(NULL != original)
					{
						PropEditorUserData *originalUserData = GetUserData(original);
						originalUserData->associatedData = NULL;
						originalUserData->isFavorite = false;
					}

					scheme.remove(data->GetPath());
					favoriteGroup->ChildRemove(data);
				}
				break;

			default:
				DVASSERT(false && "Unknown userData type");
				break;
		}
	}

	// if there is no favorite items - remove favorite group
	if(favoriteGroup->ChildCount() == 0)
	{
		RemoveProperty(favoriteGroup);
		favoriteGroup = NULL;
	}
}

PropEditorUserData* PropertyEditor::GetUserData(QtPropertyData *data) const
{
	PropEditorUserData *userData = (PropEditorUserData*) data->GetUserData();
	if(NULL == userData)
	{
		userData = new PropEditorUserData(PropEditorUserData::ORIGINAL);
		data->SetUserData(userData);
	}

	return userData;
}

void PropertyEditor::LoadScheme(const DAVA::FilePath &path)
{
	// first, we open the file
	QFile file(path.GetAbsolutePathname().c_str());
	if(file.open(QIODevice::ReadOnly))
	{
		scheme.clear();

		QTextStream qin(&file);
		while(!qin.atEnd())
		{
			scheme.insert(qin.readLine());
		}

 		file.close();
	}
}

void PropertyEditor::SaveScheme(const DAVA::FilePath &path)
{
	// first, we open the file
	QFile file(path.GetAbsolutePathname().c_str());
	if(file.open(QIODevice::WriteOnly))
	{
		QTextStream qout(&file);
		foreach(const QString &value, scheme)
		{
			qout << value << endl;
		}
		file.close();
	}
}

bool PropertyEditor::ExcludeMember(const DAVA::InspInfo *info, const DAVA::InspMember *member)
{
	bool exclude = false;

	if(info->Type() == DAVA::MetaInfo::Instance<DAVA::NMaterial>())
	{
		// don't show material properties. they should be edited in materialEditor
		exclude = true;
	}

	return exclude;
}
