// Created by chankeep on 2/21/2024.
//

#ifndef FISHY_RENDERMANAGER_H
#define FISHY_RENDERMANAGER_H

#include "../core/Fishy.h"
#include <QTreeWidget>

namespace Fishy
{

    class SceneManager : public QObject
    {
        Q_OBJECT
    public:
        SceneManager(QWidget* previewWidget, QTreeWidget *sceneTree);
        ~SceneManager() override;


        Qt3DExtras::Qt3DWindow *getQt3DView() const
        {
            return qt3DView;
        }

        Film* getFilm() const
        {
            return film;
        }

        Scene* getScene() const
        {
            return scene;
        }

        QTreeWidgetItem* getCurItem()
        {
            return currentTreeItem;
        }

    public slots:
        void changeCurrentEntity(QTreeWidgetItem*, int);

    signals:
        void currentEntityChanged(Qt3DCore::QEntity* );

    private:
        QTreeWidget *sceneTree;
        QWidget* previewWidget;
        QWidget* renderWidget;
        QTreeWidgetItem* currentTreeItem;

        Scene* scene;
        Film* film;
        Qt3DExtras::Qt3DWindow *qt3DView;

        uint width, height;
    };

} // Fishy

#endif //FISHY_RENDERMANAGER_H
