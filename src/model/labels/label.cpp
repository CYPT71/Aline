/*
This file is part of the project Sabrina
Copyright (C) 2018  Paragon <french.paragon@gmail.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "label.h"

#include "labelstree.h"

#include "model/editableitem.h"

#include "utils/stringutils.h"

#include <QDebug>

namespace Aline {

const char* Label::PROP_LABELS = "labels";

Label::Label(Label *parent) : QObject(parent),
	_parentLabel(parent),
	_treeParent(parent->parentTree()),
	_hasBeenRefed(false),
	_hasBeenNamed(false)
{
	connect(this, &QObject::objectNameChanged, this, &Label::hasBeenNamed);
}

Label::Label(LabelsTree *parent) : QObject(parent),
	_parentLabel(nullptr),
	_treeParent(parent),
	_hasBeenRefed(false),
	_hasBeenNamed(false)
{
	connect(this, &QObject::objectNameChanged, this, &Label::hasBeenNamed);
}

QString Label::getRef() const {
	return _ref;
}

void Label::setRef(QString ref) {

	if (!_itemsRefs.empty()) {
		return; //don't change ref when items refer to the label.
	}

	QString sref = Aline::StringUtils::simplifyRef(ref);

	if (!_treeParent->makeRefUniq(sref)) {
		return;
	}

	if (_ref != sref) {
		QString oldRef = _ref;
		_ref = sref;

		if (_hasBeenRefed) {
			Q_EMIT refChanged(oldRef, _ref);
		} else {
			_hasBeenRefed = true;
			Q_EMIT gettedRef(_ref);
		}
	}
}
void Label::setParentLabel(Label* parent) {
	_parentLabel = parent;
	setParent(parent);
}

bool Label::addChild(Label* child) {

	if (_childrens.indexOf(child) >= 0) {
		return false; //don't add the same child twice
	}

	_childrens.push_back(child);

	if (child->parentLabel() != this) {
		child->setParentLabel(this);
	}

	return true;

}
void Label::removeChild(Label* child) {

	int index = _childrens.indexOf(child);

	if (index >= 0) {
		_childrens.removeAt(index);
	}

}
bool Label::hasChild(Label* child) const {
	return _childrens.indexOf(child) > 0;
}

bool Label::insertNewLabels(int row, int count) {

	_childrens.insert(row, count, nullptr);

	for (int i = row; i < row + count; i++) {
		_childrens[i] = new Label(this);
	}

	return true;

}

bool Label::insertSubLabels(int row, QVector<Label*> const& labels) {

	int count = labels.size();

	_childrens.insert(row, count, nullptr);

	for (int i = row; i < row + count; i++) {
		_childrens[i] = labels[i-row];
	}

	return true;

}

bool Label::removeLabels(int row, int count) {

	for (int i = row; i < row + count; i++) {
		_childrens[i]->deleteLater();
	}

	_childrens.remove(row, count);

	return true;
}

QVector<Label*> const& Label::subLabels() const {
	return _childrens;
}

bool Label::markItem(Aline::EditableItem* item) {

	if (!_hasBeenRefed) {
		return false;
	}

	bool ok = true;
	bool hasItem = false;

	if (_itemsRefs.indexOf(item->getRef()) >= 0) {
		hasItem = true;
	} else {
		_itemsRefs.push_back(item->getRef());
	}

	bool added = item->addLabel(getRef());

	if (!added) {

		if (hasItem) {
			return false;
		}

		//label is marked in the item, but the item is not marked in the label, this shouldn't happen.

		qDebug() << "Inserted unmarked item: " << item->getRef() << " in label: " << getRef() << " while label was registered in item.";
		return false;

	} else if (hasItem) { //item is marked in the label, but the label is not marked in the item, this shouldn't happen.
		ok = false;
		qDebug() << "Inserted unmarked label: " << getRef() << " in item: " << item->getRef() << " while item was registered in label.";
	}

	if (ok) {
		Q_EMIT itemRefAdded(item->getRef());
	}

	return ok;
}

bool Label::unmarkItem(Aline::EditableItem* item) {

	if (!_hasBeenRefed) {
		return false;
	}

	bool ok = true;
	bool hasItem = false;

	int index = _itemsRefs.indexOf(item->getRef());

	if (index >= 0) {
		hasItem = true;
		_itemsRefs.removeAt(index);
	}

	bool removed = item->removeLabel(getRef());

	if (!removed) {

		if (!hasItem) {
			return false; //try to unmark an item that is not present.
		}

		//label is marked in the item, but the item is not marked in the label, this shouldn't happen.

		qDebug() << "Removed marked item: " << item->getRef() << " in label: " << getRef() << " while label was not registered in item.";
		return false;

	} else if (!hasItem) { //item is marked in the label, but the label is not marked in the item, this shouldn't happen.
		ok = false;
		qDebug() << "Removed marked label: " << getRef() << " in item: " << item->getRef() << " while item was unregistered in label.";
	}

	if (ok) {
		Q_EMIT itemRefRemoved(item->getRef());
	}

	return ok;
}

bool Label::markItem(QString const& itemRef) {
	if (!_hasBeenRefed) {
		return false;
	}

	bool ok = true;

	if (_itemsRefs.indexOf(itemRef) >= 0) {
		return false;
	}

	_itemsRefs.push_back(itemRef);

	return ok;

}

bool Label::unmarkItem(QString const& itemRef) {

	if (!_hasBeenRefed) {
		return false;
	}

	bool ok = true;

	int index = _itemsRefs.indexOf(itemRef);

	if (index >= 0) {
		return false; //try to unmark item that is not present.
	}

	_itemsRefs.removeAt(index);

	return ok;

}

const QVector<QString> &Label::itemsRefs() const {
	return _itemsRefs;
}

Label* Label::parentLabel() const{
	return _parentLabel;
}

LabelsTree* Label::parentTree() const {
	return _treeParent;
}

bool Label::hasBeenRefed() const
{
	return _hasBeenRefed;
}

QStringList Label::refAndSubRefs() const {
	//TODO: look if it is interesting to cache.

	QStringList refs;

	if (!hasBeenRefed()) {
		return refs;
	}

	refs << getRef();

	for (Label* l : _childrens) {
		refs << l->refAndSubRefs();
	}

	return refs;
}


void Label::hasBeenNamed(QString const& name) {

	if (_hasBeenNamed == true) {
		return;
	}

	_hasBeenNamed = true;

	if (!_hasBeenRefed) {
		setRef(name);
	}

	disconnect(this, &QObject::objectNameChanged, this, &Label::hasBeenNamed);
}

} // namespace Aline
