#ifndef CUBEMAPTEXTUREBROWSER_H
#define CUBEMAPTEXTUREBROWSER_H

#include "Classes/Qt/CubemapEditor/CubeListItemDelegate.h"

#include <REPlatform/Scene/SceneEditor2.h>

#include <Base/BaseTypes.h>

#include <QDialog>
#include <QListWidget>

namespace Ui
{
class CubeMapTextureBrowser;
}

class CubeMapTextureBrowser : public QDialog
{
    Q_OBJECT

public:
    explicit CubeMapTextureBrowser(DAVA::SceneEditor2* currentScene, QWidget* parent = 0);
    ~CubeMapTextureBrowser();

protected:
    CubeListItemDelegate cubeListItemDelegate;
    DAVA::SceneEditor2* scene;

protected:
    void ReloadTexturesFromUI(QString& path);
    void ReloadTextures(const DAVA::String& rootPath);
    void ConnectSignals();
    void RestoreListSelection(int currentRow);
    int GetCheckedItemsCount();
    void UpdateCheckedState();
    bool ValidateTextureAndFillThumbnails(DAVA::FilePath& fp,
                                          DAVA::Vector<QImage*>& icons,
                                          DAVA::Vector<QSize>& actualSize);

protected slots:

    void OnChooseDirectoryClicked();
    void OnReloadClicked();
    void OnCreateCubemapClicked();
    void OnEditCubemap(const QModelIndex& index);
    void OnItemCheckStateChanged(const QModelIndex& index);
    void OnDeleteSelectedItemsClicked();

private:
    Ui::CubeMapTextureBrowser* ui;
};

#endif // CUBEMAPTEXTUREBROWSER_H
