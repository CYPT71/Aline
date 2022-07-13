/*This file is part of the project Aline

Copyright (C) 2022 Paragon <french.paragon@gmail.com>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 3 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with this program. If not, see <https://www.gnu.org/licenses/>.
*/

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "editor.h"
#include "editorfactorymanager.h"
#include "editableitemeditor.h"
#include "model/editableitem.h"
#include "model/editableitemmanager.h"
#include "model/editableitemfactory.h"
#include "model/editableitemmanagerfactory.h"

#include <QTabWidget>
#include <QDockWidget>

namespace Aline {

const QString MainWindow::MENU_FILE_NAME = "file_menu";
const QString MainWindow::MENU_DISPLAY_NAME = "display_menu";

MainWindow::MainWindow(QWidget *parent) :
	QMainWindow(parent),
	_currentProject(nullptr),
	_editorFactoryManager(nullptr),
	ui(new Ui::MainWindow)
{
	ui->setupUi(this);

	//menu

	QMenu* fileMenu = menuBar()->addMenu(tr("fichier"));
	fileMenu->setObjectName(MENU_FILE_NAME);

	QMenu* viewMenu = menuBar()->addMenu(tr("affichage"));
	viewMenu->setObjectName(MENU_DISPLAY_NAME);

	_submenuDock = viewMenu->addMenu(tr("docks"));
	_submenuCreateProject = fileMenu->addMenu(tr("create project"));
	_submenuOpenProject = fileMenu->addMenu(tr("open project"));

	connect(ui->tabWidget, &QTabWidget::tabCloseRequested,
			this, static_cast<void (MainWindow::*)(int)>(&MainWindow::closeEditor));

	connect(ui->tabWidget, &QTabWidget::currentChanged,
			this, &MainWindow::onCurrentEditorChanged);

	_editableItemManagerFactory = new Aline::EditableItemManagerFactory(this);
}

MainWindow::~MainWindow()
{
	delete ui;
}

int MainWindow::nOpenedEditors() const {
	return ui->tabWidget->count();
}

Editor* MainWindow::editorAt(int index) {

	QWidget* widget = ui->tabWidget->widget(index);

	Editor* editor = qobject_cast<Editor*>(widget);

	return editor;

}

Editor* MainWindow::findEditorByName(QString const& name) {

	for (int i = 0; i < nOpenedEditors(); i++) {
		Editor* e = editorAt(i);

		if (e->objectName() == name) {
			return e;
		}
	}

	return nullptr;

}
QVector<Editor*> MainWindow::findAllEditorsOfType(QString const& type) {
	QVector<Editor*> eds = {};

	for (int i = 0; i < nOpenedEditors(); i++) {
		Editor* e = editorAt(i);

		if (e->getTypeId() == type) {
			eds.push_back(e);
		}
	}

	return eds;
}


void MainWindow::switchToEditor(int index) {

	if (index >= 0 and index < ui->tabWidget->count()) {
		ui->tabWidget->setCurrentIndex(index);
	}
}

void MainWindow::switchToEditor(Editor* editor) {
	int index = ui->tabWidget->indexOf(editor);

	if (index >= 0) {
		ui->tabWidget->setCurrentIndex(index);
	}
}


EditableItemManager *MainWindow::currentProject() const
{
	return _currentProject;
}

void MainWindow::setCurrentProject(EditableItemManager *currentProject)
{
	if (currentProject != _currentProject) {

		if (_currentProject != nullptr) {
			disconnect(this, &Aline::MainWindow::editedItemChanged,
					   _currentProject, &EditableItemManager::setActiveItem);

			_currentProject->setEditorManager(nullptr);
		}

		_currentProject = currentProject;

		if (_currentProject != nullptr) {
			connect(this, &Aline::MainWindow::editedItemChanged,
				_currentProject, &EditableItemManager::setActiveItem);

			_currentProject->setEditorManager(_editorFactoryManager);
		}

		Q_EMIT currentProjectChanged(_currentProject);
	}
}

QMenu* MainWindow::findMenuByName(QString const& name, bool createIfNotExist) {

	QMenu* menu = menuBar()->findChild<QMenu*>(name, Qt::FindDirectChildrenOnly);

	if (menu != nullptr) {
		return menu;
	}

	if (createIfNotExist) {
		return menuBar()->addMenu(name);
	}

	return nullptr;

}

void MainWindow::addDockWidget(Qt::DockWidgetArea area, QDockWidget * dockwidget) {

	_submenuDock->addAction(dockwidget->toggleViewAction());

	QMainWindow::addDockWidget(area, dockwidget);

}

void MainWindow::addDockWidget(Qt::DockWidgetArea area, QDockWidget * dockwidget, Qt::Orientation orientation) {

	_submenuDock->addAction(dockwidget->toggleViewAction());

	QMainWindow::addDockWidget(area, dockwidget, orientation);

}

bool MainWindow::isEditingAnItem() const {

	if (ui->tabWidget->count() == 0) {
		return false;
	}

	QWidget* w = ui->tabWidget->currentWidget();

	Aline::EditableItemEditor* eie = qobject_cast<Aline::EditableItemEditor*>(w);

	if (eie != nullptr) {
		return true;
	}

	return false;
}

void MainWindow::registerCreateProjectFunction(QString const& fName,
											   QString const& projectType,
											   QString const& longDescr,
											   std::function<EditableItemManager*(QObject*, MainWindow*)> const& func) {

	registerProjectFunction(fName, projectType, longDescr, func, _submenuCreateProject);

}

void MainWindow::registerOpenProjectFunction(QString const& fName,
											 QString const& projectType,
											 QString const& longDescr,
											 std::function<EditableItemManager*(QObject*, MainWindow*)> const& func) {

	registerProjectFunction(fName, projectType, longDescr, func, _submenuOpenProject);

}

void MainWindow::registerProjectFunction(QString const& fName,
										 QString const& projectType,
										 QString const& longDescr,
										 std::function<EditableItemManager*(QObject*, MainWindow*)> const& func,
										 QMenu* target) {

	EditableItemManagerFactory::funcDescr d;
	d._projectDescr = projectType;
	d._longDescr = longDescr;

	_editableItemManagerFactory->registerGuiAwareFunction(fName, d, func);

	QAction* openProjectAction = new QAction(projectType, this);
	openProjectAction->setToolTip(longDescr);

	connect(openProjectAction, &QAction::triggered, this, [fName, this] () {
		setWindowProjectFromFunc(fName);
	});

	target->addAction(openProjectAction);

}

void MainWindow::setEditorFactoryManager(EditorFactoryManager *editorFactoryManager)
{
	_editorFactoryManager = editorFactoryManager;
}

void MainWindow::addEditor(Editor* editor) {
	ui->tabWidget->addTab(editor, editor->title());

	connect(editor, &Editor::titleChanged, this, &MainWindow::updateTitle);

	switchToEditor(editor);
}


void MainWindow::closeEditor(int index) {

	QWidget* widg = ui->tabWidget->widget(index);

	Editor* editor = qobject_cast<Editor*>(widg);

	if (editor) {
		disconnect(editor, &Editor::titleChanged, this, &MainWindow::updateTitle);
	}

	Q_EMIT editorAboutToBeRemoved(editor);

	QString key = _openedEditors.key(editor);
	_openedEditors.remove(key);

	ui->tabWidget->removeTab(index);
}

void MainWindow::closeEditor(Editor* editor) {

	disconnect(editor, &Editor::titleChanged, this, &MainWindow::updateTitle);

	Q_EMIT editorAboutToBeRemoved(editor);

	QString key = _openedEditors.key(editor);
	_openedEditors.remove(key);

	ui->tabWidget->removeTab(ui->tabWidget->indexOf(editor));

}

void MainWindow::saveCurrentEditor() {

	Editor* editor = qobject_cast<Editor*>(ui->tabWidget->currentWidget());

	if (editor != nullptr) {
		editor->saveAction();
	}

}

void MainWindow::editItem(QString const& itemRef) {

	if (_openedEditors.contains(itemRef)) {
		switchToEditor(_openedEditors.value(itemRef));
		return;
	}

	EditableItem* item = _currentProject->loadItem(itemRef);

	if (item == nullptr) {
		return;
	}

	Editor* editor = nullptr;

	if (_editorFactoryManager != nullptr) {
		editor = _editorFactoryManager->createItemForEditableItem(item, this);
	} else {
		editor = EditorFactoryManager::GlobalEditorFactoryManager.createItemForEditableItem(item, this);
	}

	if (editor != nullptr) {
		addEditor(editor);
		_openedEditors.insert(itemRef, editor);
	}

}

void MainWindow::saveAll() {

	if (_currentProject != nullptr) {
		_currentProject->saveAll();
	}

}

void MainWindow::setWindowProjectFromFunc(QString const& fName) {

	/**
	 * @todo look if it is possible to open a new main windows
	 */
	EditableItemManager* manager = _editableItemManagerFactory->createManagerWithRegistredFunctionAndGuiContext(fName, this, this);

	if (manager != nullptr) {
		Q_EMIT projectLoaded(manager);
		setCurrentProject(manager);
	}
}


void MainWindow::updateTitle(Editor* editor, QString newTitle) {

	int index = ui->tabWidget->indexOf(editor);

	ui->tabWidget->setTabText(index, newTitle);

}

void MainWindow::onCurrentEditorChanged() {

	QWidget* w = ui->tabWidget->currentWidget();

	Aline::EditableItemEditor* eie = qobject_cast<Aline::EditableItemEditor*>(w);

	if (eie != nullptr) {
		Q_EMIT editedItemChanged(eie->getEditedItemRef());
	}

}

QString MainWindow::defaultProjectOpener() const
{
	return _defaultProjectOpener;
}

void MainWindow::setDefaultProjectOpener(const QString &defaultProjectOpener)
{
	_defaultProjectOpener = defaultProjectOpener;
}

QString MainWindow::defaultProjectCreator() const
{
	return _defaultProjectCreator;
}

void MainWindow::setDefaultProjectCreator(const QString &defaultProjectCreator)
{
	_defaultProjectCreator = defaultProjectCreator;
}

} // namespace Aline
