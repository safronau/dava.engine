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


#include "LibraryModel.h"

#include <QIcon>
#include <QMimeData>

#include "Model/PackageHierarchy/PackageNode.h"
#include "Model/PackageHierarchy/ImportedPackagesNode.h"
#include "Model/PackageHierarchy/PackageControlsNode.h"
#include "Model/PackageHierarchy/ControlNode.h"
#include "Model/PackageHierarchy/PackageNode.h"
#include "Model/ControlProperties/RootProperty.h"
#include "Model/ControlProperties/ClassProperty.h"
#include "Model/ControlProperties/CustomClassProperty.h"

#include "Utils/QtDavaConvertion.h"
#include "UI/IconHelper.h"

using namespace DAVA;

LibraryModel::LibraryModel(PackageNode *_root, QObject *parent) 
    : QStandardItemModel(parent)
    , root(SafeRetain(_root))
    , defaultControlsRootItem(nullptr)
    , controlsRootItem(nullptr)
    , importedPackageRootItem(nullptr)
{
    root->AddListener(this);
    defaultControls
        << "UIControl"
        << "UIButton"
        << "UIStaticText"
        << "UITextField"
        << "UISlider"
        << "UIList"
        << "UIListCell"
        << "UIScrollBar"
        << "UIScrollView"
        << "UISpinner"
        << "UISwitch"
        << "UIParticles";
    BuildModel();
}

LibraryModel::~LibraryModel()
{
    root->RemoveListener(this);
    SafeRelease(root);
}

Qt::ItemFlags LibraryModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
    {
        return Qt::NoItemFlags;
    }

    return QAbstractItemModel::flags(index) | Qt::ItemIsUserCheckable | Qt::ItemIsDragEnabled;
}

QStringList LibraryModel::mimeTypes() const
{
    return QStringList() << "text/plain";
}

QMimeData *LibraryModel::mimeData(const QModelIndexList &indexes) const
{
    for(const auto &index : indexes)
    {
        if (index.isValid())
        {
            QMimeData *data = new QMimeData();
            data->setText(defaultControls.at(index.row()));
            return data;
        }
    }
    return nullptr;
}

QModelIndex LibraryModel::indexByNode(const void *node, const QStandardItem *item) const
{
    DVASSERT(nullptr != node);
    DVASSERT(nullptr != item);
    void* ptr = item->data().value<void*>();
    if (ptr == node)
    {
        return item->index();
    }
    for (auto row = item->rowCount() - 1; row >= 0; --row)
    {
        QModelIndex index = indexByNode(node, item->child(row));
        if (index.isValid())
        {
            return index;
        }
    }
    return QModelIndex();
}

void LibraryModel::BuildModel()
{
    defaultControlsRootItem = new QStandardItem(tr("Default controls"));
    defaultControlsRootItem->setBackground(QBrush(Qt::lightGray));
    invisibleRootItem()->appendRow(defaultControlsRootItem);
    for (const auto &defaultControl : defaultControls)
    {
        auto item = new QStandardItem(
            QIcon(IconHelper::GetIconPathForClassName(defaultControl)),
            defaultControl
            ); 
        defaultControlsRootItem->appendRow(item);
    }
    const auto packageControls = root->GetPackageControlsNode();
    if (packageControls->GetCount())
    {
        CreateControlsRootItem();
        for (int i = 0; i < packageControls->GetCount(); i++)
        {
            AddControl(packageControls->Get(i));
        }
    }
    const auto importedPackagesNode = root->GetImportedPackagesNode();
    if (importedPackagesNode->GetCount())
    {
        CreateImportPackagesRootItem();
        for (int i = 0; i < importedPackagesNode->GetCount(); ++i)
        {
            AddImportedControl(importedPackagesNode->GetImportedPackage(i));
        }
    }
}

void LibraryModel::AddControl(ControlNode* node)
{
    DVASSERT(nullptr != node);
    DVASSERT(nullptr != controlsRootItem);
    auto item = new QStandardItem(
        QIcon(IconHelper::GetCustomIconPath()),
        QString::fromStdString(node->GetName())
        );
    item->setData(QVariant::fromValue(static_cast<void*>(node)));
    controlsRootItem->appendRow(item);
}

void LibraryModel::AddImportedControl(PackageNode* node)
{
    DVASSERT(nullptr != node);
    DVASSERT(nullptr != importedPackageRootItem);
    auto importedPackageItem = new QStandardItem(QString::fromStdString(node->GetName()));
    importedPackageItem->setData(QVariant::fromValue(static_cast<void*>(node)));
    importedPackageRootItem->appendRow(importedPackageItem);
    const auto controls = node->GetPackageControlsNode();
    for (int j = 0; j < controls->GetCount(); ++j)
    {
        const auto subNode = controls->Get(j);
        auto item = new QStandardItem(
            QIcon(IconHelper::GetCustomIconPath()),
            QString::fromStdString(subNode->GetName())
            );
        item->setData(QVariant::fromValue(static_cast<void*>(subNode)));
        importedPackageItem->appendRow(item);
    }
}

void LibraryModel::CreateControlsRootItem()
{
    DVASSERT(nullptr == controlsRootItem);
    controlsRootItem = new QStandardItem(tr("Package controls"));
    controlsRootItem->setData(QVariant::fromValue(static_cast<void*>(root->GetPackageControlsNode())));
    controlsRootItem->setBackground(QBrush(Qt::lightGray));
    invisibleRootItem()->insertRow(1 , controlsRootItem);
}

void LibraryModel::CreateImportPackagesRootItem()
{
    DVASSERT(nullptr == importedPackageRootItem);
    importedPackageRootItem = new QStandardItem(tr("Importred controls"));
    importedPackageRootItem->setData(QVariant::fromValue(static_cast<void*>(root->GetImportedPackagesNode())));
    importedPackageRootItem->setBackground(QBrush(Qt::lightGray));
    invisibleRootItem()->appendRow(importedPackageRootItem);
}

void LibraryModel::ControlPropertyWasChanged(ControlNode *node, AbstractProperty *property)
{
    String name = property->GetName();
    std::transform(name.begin(), name.end(), name.begin(), ::tolower);
    if (name == "name")
    {
        QModelIndex index = indexByNode(node, invisibleRootItem());
        if (index.isValid())
        {
            auto item = itemFromIndex(index);
            if (nullptr != item)
            {
                auto text = property->GetValue().AsString();
                item->setText(QString::fromStdString(text));
            }
        }
    }
}

void LibraryModel::ControlWillBeAdded(ControlNode *node, ControlsContainerNode *destination, int row)
{
    Q_UNUSED(node);
    Q_UNUSED(destination);
    Q_UNUSED(row);
}

void LibraryModel::ControlWasAdded(ControlNode *node, ControlsContainerNode *destination, int row)
{
    Q_UNUSED(destination);
    Q_UNUSED(row);
    DVASSERT(nullptr != node);
    if (nullptr == controlsRootItem)
    {
        CreateControlsRootItem();
    }
    if (controlsRootItem->data().value<void*>() == node->GetParent())
    {
        const QModelIndex destIndex = indexByNode(node, controlsRootItem); //check that we already do not have this item 
        if (!destIndex.isValid())
        {
            AddControl(node);
        }
    }
}

void LibraryModel::ControlWillBeRemoved(ControlNode *node, ControlsContainerNode *from)
{
    Q_UNUSED(from);
    DVASSERT(nullptr != node);
    QModelIndex index = indexByNode(node, controlsRootItem);
    if (index.isValid())
    {
        removeRow(index.row(), index.parent());
    }
    if (controlsRootItem->rowCount() == 0)
    {
        removeRow(controlsRootItem->row());
        controlsRootItem = nullptr;
    }
}

void LibraryModel::ControlWasRemoved(ControlNode *node, ControlsContainerNode *from)
{
    Q_UNUSED(node);
    Q_UNUSED(from);
}

void LibraryModel::ImportedPackageWillBeAdded(PackageNode *node, ImportedPackagesNode *to, int index)
{
    Q_UNUSED(node);
    Q_UNUSED(to);
    Q_UNUSED(index);
}

void LibraryModel::ImportedPackageWasAdded(PackageNode *node, ImportedPackagesNode *to, int index)
{
    Q_UNUSED(to);
    Q_UNUSED(index);
    DVASSERT(nullptr != node);
    if (nullptr == importedPackageRootItem)
    {
        CreateImportPackagesRootItem();
    }
    if (importedPackageRootItem->data().value<void*>() == node->GetParent())
    {
        const QModelIndex destIndex = indexByNode(node, importedPackageRootItem); //check that we already do not have this item 
        if (!destIndex.isValid())
        {
            AddImportedControl(node);
        }
    }
}

void LibraryModel::ImportedPackageWillBeRemoved(PackageNode *node, ImportedPackagesNode *from)
{
    Q_UNUSED(from);
    DVASSERT(nullptr != node);
    QModelIndex parentIndex = indexByNode(node, importedPackageRootItem);
    if (parentIndex.isValid())
    {
        removeRow(parentIndex.row(), parentIndex.parent());
    }
    if (importedPackageRootItem->rowCount() == 0)
    {
        removeRow(importedPackageRootItem->row());
        importedPackageRootItem = nullptr;
    }
}

void LibraryModel::ImportedPackageWasRemoved(PackageNode *node, ImportedPackagesNode *from)
{
    Q_UNUSED(node);
    Q_UNUSED(from);
}
