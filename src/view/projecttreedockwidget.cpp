#include "projecttreedockwidget.h"
#include "ui_projecttreedockwidget.h"

/*
This file is part of the project Sabrina
Copyright (C) 2018  Paragon <french.paragon@gmail.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include <QMenu>
#include <QInputDialog>

#include "editableitemtypespecializedaction.h"

#include "../model/editableitemfactory.h"
#include "editorfactorymanager.h"

namespace Aline {

ProjectTreeDockWidget::ProjectTreeDockWidget(MainWindow *parent) :
	QDockWidget(parent),
	_mw_parent(parent),
	_newItemMenu(nullptr),
	ui(new Ui::ProjectTreeDockWidget)
{
	_internalModel = new QSortFilterProxyModel(this);

	ui->setupUi(this);

	ui->treeView->setModel(_internalModel);

	projectChanged(_mw_parent->currentProject());

	connect(_mw_parent, &MainWindow::currentProjectChanged,
			this, &ProjectTreeDockWidget::projectChanged);

	connect(ui->treeView, &QTreeView::doubleClicked,
			this, &ProjectTreeDockWidget::receiveDoubleClick);

	connect(ui->treeView->selectionModel(), &QItemSelectionModel::selectionChanged,
			this, &ProjectTreeDockWidget::selectionChanged);

	connect(ui->pushButtonSuppr, &QPushButton::clicked,
			this, &ProjectTreeDockWidget::supprButtonClicked);

	ui->treeView->setSelectionMode(QAbstractItemView::SingleSelection);
	ui->treeView->setDragDropMode(QAbstractItemView::DragDrop);
	ui->treeView->setDragEnabled(true);
	ui->treeView->setAcceptDrops(true);
	ui->treeView->setDropIndicatorShown(true);

	ui->treeView->setContextMenuPolicy(Qt::CustomContextMenu);

	connect(ui->treeView, &QTreeView::customContextMenuRequested,
			this, &ProjectTreeDockWidget::buildTreeContextMenu);
}

ProjectTreeDockWidget::~ProjectTreeDockWidget()
{
	delete ui;
}

void ProjectTreeDockWidget::selectionChanged() {

	QModelIndexList selection = ui->treeView->selectionModel()->selectedIndexes();

	if (selection.size() > 0 && !_mw_parent->isEditingAnItem()) {

		QString ref = ui->treeView->model()->data(selection.first(), EditableItemManager::ItemRefRole).toString();

		_mw_parent->currentProject()->setActiveItem(ref);
	}
}

void ProjectTreeDockWidget::onItemCreationTriggered(QString itemTypeRef) {

	Q_EMIT itemCreationTriggered(itemTypeRef, itemTypeRef);

}

void ProjectTreeDockWidget::projectChanged(Aline::EditableItemManager* project) {

	if(_newItemFactoryWatcher) {
		disconnect(_newItemFactoryWatcher);
	}

	if (_itemCreationTrigger) {
		disconnect(_itemCreationTrigger);
	}

	if (_itemSuppresionTrigger) {
		disconnect(_itemSuppresionTrigger);
	}

	if (project == nullptr) {
		setEnabled(false);
		return;
	} else {
		setEnabled(true);
	}

	reselectProject(project);
	rebuildMenu(project);

	_newItemFactoryWatcher = connect(project->factoryManager(), &Aline::EditableItemFactoryManager::rowsInserted,
									 this, &ProjectTreeDockWidget::rebuildMenuWithoutProject);

	_itemCreationTrigger = connect(this, &ProjectTreeDockWidget::itemCreationTriggered,
								   project, static_cast<bool(Aline::EditableItemManager::*)(QString, QString)>(&Aline::EditableItemManager::createItem));

	_itemSuppresionTrigger = connect(this, &ProjectTreeDockWidget::itemSuppressionTriggered,
									 project, &Aline::EditableItemManager::clearItems);
}

void ProjectTreeDockWidget::reselectProject(Aline::EditableItemManager *project) {

	_internalModel->setSourceModel(project);

}

void ProjectTreeDockWidget::rebuildMenuWithoutProject() {
	rebuildMenu(_mw_parent->currentProject());
}

void ProjectTreeDockWidget::rebuildMenu(Aline::EditableItemManager* project) {

	if (_newItemMenu != nullptr) {
		_newItemMenu->deleteLater();
	}

	if (project == nullptr) {
		return;
	}

	_newItemMenu = new QMenu(this);

	Aline::EditableItemFactoryManager* f = project->factoryManager();

	for (int i = 0; i < f->rowCount(); i++) {

		QString itemType = f->data(f->index(i), Aline::EditableItemFactoryManager::ItemRefRole).toString();

		EditableItemTypeSpecializedAction* action = new EditableItemTypeSpecializedAction(itemType,
																						  QIcon(f->itemIconUrl(itemType)),
																						  f->data(f->index(i),
																								  Qt::DisplayRole).toString(),
																						  _newItemMenu);

		connect(action, &EditableItemTypeSpecializedAction::triggered,
				this, &ProjectTreeDockWidget::onItemCreationTriggered);

		_newItemMenu->addAction(action);

	}

	ui->toolButton_new->setMenu(_newItemMenu);
	ui->toolButton_new->setPopupMode(QToolButton::MenuButtonPopup);

}

void ProjectTreeDockWidget::receiveDoubleClick(const QModelIndex &index) {

	QString ref = _internalModel->data(index, EditableItemManager::ItemRefRole).toString();

	Q_EMIT itemDoubleClicked(ref);

}

void ProjectTreeDockWidget::supprButtonClicked() {

	//TODO: options to have a warning about deletion.

	QModelIndexList smod = ui->treeView->selectionModel()->selectedIndexes();
	QStringList selectedItems;

	for (QModelIndex index : qAsConst(smod)) {
		selectedItems << _internalModel->data(index, Aline::EditableItemFactoryManager::ItemRefRole).toString();
	}

	Q_EMIT itemSuppressionTriggered(selectedItems);

}

void ProjectTreeDockWidget::buildTreeContextMenu(QPoint const& pos) {

	QModelIndex index = ui->treeView->indexAt(pos);

	if (index.isValid()) {

		QMenu menu;

		QVariant data  = index.data(EditableItemManager::ItemTypeRefRole);
		QString itemTypeRef = data.toString();

		if (Aline::EditorFactoryManager::GlobalEditorFactoryManager.hasFactoryInstalledForItem(itemTypeRef) && index.parent() != QModelIndex()) {
			QAction* editAction = menu.addAction(tr("éditer"));
			QString ref = index.data(EditableItemManager::ItemRefRole).toString();

			connect(editAction, &QAction::triggered, this, [this, ref] () {
				Q_EMIT itemDoubleClicked(ref);
			});

			QAction* actionRemove = menu.addAction(tr("supprimer"));

			connect(actionRemove, &QAction::triggered, this, [ref, this] () {
				Q_EMIT itemSuppressionTriggered({ref});
			});

			menu.addSeparator();
		}

		if (_internalModel->flags(index) & Qt::ItemIsEditable) {

			QAction* renameAction = menu.addAction(tr("renommer"));

			connect(renameAction, &QAction::triggered, this, [&index, this] () {ui->treeView->edit(index);});

			menu.addSeparator();

		}

		Aline::EditableItemFactoryManager* f = _mw_parent->currentProject()->factoryManager();

		QString typeName = f->itemTypeName(itemTypeRef);

		EditableItemTypeSpecializedAction* action = new EditableItemTypeSpecializedAction(itemTypeRef,
																						  QIcon(f->itemIconUrl(itemTypeRef)),
																						  QString("ajouter une instance de %1").arg(typeName),
																						  &menu);

		connect(action, &EditableItemTypeSpecializedAction::triggered,
				this, [this] (QString type_id) {

					Q_EMIT itemCreationTriggered(type_id, type_id);

		});

		menu.addAction(action);

		menu.exec(ui->treeView->viewport()->mapToGlobal(pos));

	}

}

} //namespace Sabrina
