#include "contactlist.h"
#include <options_i.h>
#include <QSettings>
#include <QStack>
#include <QDebug>
#include <QtPlugin>
#include <QMutexLocker>
#include <QInputDialog>

PluginInfo info = {
	0x400,
	"Contact List",
	"Scott Ellis",
	"mail@scottellis.com.au",
	"http://www.scottellis.com.au",
	"Contact List",
	0x00000001
};

ContactList::ContactList() {
}

ContactList::~ContactList() {

}

bool ContactList::load(CoreI *core) {
	core_i = core;
	if((main_win_i = (MainWindowI *)core_i->get_interface(INAME_MAINWINDOW)) == 0) return false;
	icons_i = (IconsI *)core_i->get_interface(INAME_ICONS);
	if((accounts_i = (AccountsI *)core_i->get_interface(INAME_ACCOUNTS)) == 0) return false;
	if((events_i = (EventsI *)core_i->get_interface(INAME_EVENTS)) == 0) return false;

	events_i->add_event_listener(this, UUID_ACCOUNT_CHANGED);
	events_i->add_event_listener(this, UUID_CONTACT_CHANGED);
	events_i->add_event_listener(this, UUID_MSG);

	model = new ContactTreeModel(icons_i, events_i, this);
	sortedModel = new SortedTreeModel(this);
	sortedModel->setDynamicSortFilter(true);
	sortedModel->setModel(model);

	win = new CListWin();
	win->tree()->setModel(sortedModel);
	win->tree()->sortByColumn(0, Qt::AscendingOrder);

	connect(win, SIGNAL(showMenu(const QPoint &, const QModelIndex &)), this, SLOT(aboutToShowMenuSlot(const QPoint &, const QModelIndex &)));

	connect(win->tree(), SIGNAL(expanded(const QModelIndex &)), this, SLOT(treeItemExpanded(const QModelIndex &)));
	connect(win->tree(), SIGNAL(collapsed(const QModelIndex &)), this, SLOT(treeItemCollapsed(const QModelIndex &)));

	connect(win->tree(), SIGNAL(show_tip(const QModelIndex &, const QPoint &)), this, SLOT(treeShowTip(const QModelIndex &, const QPoint &)));
	connect(win->tree(), SIGNAL(hide_tip()), this, SLOT(treeHideTip()));

	connect(win->tree(), SIGNAL(clicked(const QModelIndex &)), this, SLOT(treeItemClicked(const QModelIndex &)));
	connect(win->tree(), SIGNAL(doubleClicked(const QModelIndex &)), this, SLOT(treeItemDoubleClicked(const QModelIndex &)));

	QSettings s;
	current_settings.hide_offline = s.value("CList/hide_offline", false).toBool();
	current_settings.hide_empty_groups = s.value("CList/hide_empty_groups", false).toBool();
	sortedModel->setHideOffline(current_settings.hide_offline);
	sortedModel->setHideEmptyGroups(current_settings.hide_empty_groups);

	if(main_win_i) main_win_i->set_central_widget(win);
	else win->show();

	newGroupAction = add_group_action("New group...");
	deleteGroupAction = add_group_action("Delete group");

	connect(newGroupAction, SIGNAL(triggered()), this, SLOT(newGroup()));
	connect(deleteGroupAction, SIGNAL(triggered()), this, SLOT(deleteGroup()));

	return true;
}

bool ContactList::modules_loaded() {
	OptionsI *options_i = (OptionsI *)core_i->get_interface(INAME_OPTIONS);
	if(options_i) {
		options_i->add_page("Appearance/Contact List", opt = new CListOptions(current_settings));
		connect(opt, SIGNAL(applied()), this, SLOT(options_applied()));
	}

	// test
	//add_contact("test", "test", "test", "offline");
	//add_contact_action("test action", "dot_blue");
	//remove_contact("test");
	return true;
}

bool ContactList::pre_shutdown() {
	events_i->remove_event_listener(this, UUID_ACCOUNT_CHANGED);
	events_i->remove_event_listener(this, UUID_CONTACT_CHANGED);
	events_i->remove_event_listener(this, UUID_MSG);
	return true;
}

bool ContactList::unload() {
	win->deleteLater();
	return true;
}

const PluginInfo &ContactList::get_plugin_info() {
	return info;
}

/////////////////////////////
void ContactList::options_applied() {
	current_settings = opt->currentSettings();
	QSettings settings;
	settings.setValue("CList/hide_offline", current_settings.hide_offline);
	settings.setValue("CList/hide_empty_groups", current_settings.hide_empty_groups);

	sortedModel->setHideOffline(current_settings.hide_offline);
	sortedModel->setHideEmptyGroups(current_settings.hide_empty_groups);
}

void ContactList::newGroup() {
	bool ok;
	QString name = QInputDialog::getText(win, "New Group", "Name", QLineEdit::Normal, QString(), &ok);
	if(ok) {
		model->addGroup(menuGroup << name);
	}
}

void ContactList::deleteGroup() {
	model->removeGroup(menuGroup);
}

/////////////////////////////

bool ContactList::event_fired(EventsI::Event &e) {
	if(e.uuid == UUID_CONTACT_CHANGED) {
		ContactChanged &cc = static_cast<ContactChanged &>(e);
		if(cc.removed) remove_contact(cc.contact);
		else {
			if(!model->has_contact(cc.contact)) add_contact(cc.contact);
			else update_contact(cc.contact);
		}
	} else if(e.uuid == UUID_ACCOUNT_CHANGED) {
		AccountChanged &ac = static_cast<AccountChanged &>(e);
		if(ac.removed) remove_all_contacts(ac.account);
	}
	return true;
}

QTreeWidgetItem *findGroup(QTreeWidgetItem *parent, const QString &name) {
	for(int i = 0; i < parent->childCount(); i++) {
		if(parent->child(i)->type() == TWIT_GROUP && parent->child(i)->text(0) == name)
			return parent->child(i);
	}
	return 0;
}

QAction *ContactList::add_contact_action(const QString &label, const QString &icon) {
	QAction *action = new QAction(QIcon(icons_i->get_icon(icon)), label, 0);
	win->contact_menu()->addAction(action);

	return action;
}

QAction *ContactList::add_group_action(const QString &label, const QString &icon) {
	QAction *action = new QAction(QIcon(icons_i->get_icon(icon)), label, 0);
	win->group_menu()->addAction(action);
	return action;
}

void ContactList::add_contact(Contact *contact) {
	model->addContact(contact);
}

void ContactList::remove_contact(Contact *contact) {
	model->removeContact(contact);
}

void ContactList::remove_all_contacts(Account *account) {
	model->remove_all_contacts(account);
}

void ContactList::update_contact(Contact *contact) {
	model->update_contact(contact);
}


void ContactList::aboutToShowMenuSlot(const QPoint &pos, const QModelIndex &i) {
	Contact *contact = model->getContact(sortedModel->mapToSource(i));
	if(contact) {
		events_i->fire_event(ShowContactMenu(contact, this));
		win->contact_menu()->exec(pos);
	} else {
		menuGroup = model->getGroup(sortedModel->mapToSource(i));
		int contactCount = model->contactCount(menuGroup);
		
		deleteGroupAction->setEnabled(menuGroup.size() && contactCount == 0);

		events_i->fire_event(ShowGroupMenu(menuGroup, contactCount, this));
		win->group_menu()->exec(pos);
	}
}

void ContactList::treeItemExpanded(const QModelIndex &i) {
	QSettings settings;
	QStringList full_gn = model->getGroup(sortedModel->mapToSource(i));
	settings.setValue("CList/group_expand/" + full_gn.join(">"), true);
}

void ContactList::treeItemCollapsed(const QModelIndex &i) {
	QSettings settings;
	QStringList full_gn = model->getGroup(sortedModel->mapToSource(i));
	settings.setValue("CList/group_expand/" + full_gn.join(">"), false);
}

void ContactList::treeItemClicked(const QModelIndex &i) {
	Contact *contact = model->getContact(sortedModel->mapToSource(i));
	if(contact) 
		events_i->fire_event(ContactClicked(contact, this));
}

void ContactList::treeItemDoubleClicked(const QModelIndex &i) {
	Contact *contact = model->getContact(sortedModel->mapToSource(i));
	if(contact) 
		events_i->fire_event(ContactDblClicked(contact, this));
}

void ContactList::treeShowTip(const QModelIndex &i, const QPoint &pos) {
	Contact *contact = model->getContact(sortedModel->mapToSource(i));
	if(contact) 
		events_i->fire_event(ShowTip(contact, this));
}

void ContactList::treeHideTip() {
	events_i->fire_event(HideTip(this));
}

/////////////////////////////

SortedTreeModel::SortedTreeModel(QObject *parent): QSortFilterProxyModel(parent) {
}

void SortedTreeModel::resort() {
	invalidateFilter();
}

void SortedTreeModel::setModel(ContactTreeModel *model) {
	QSortFilterProxyModel::setSourceModel(model);
}

void SortedTreeModel::setHideOffline(bool f) {
	hideOffline = f;
	invalidateFilter();
}

void SortedTreeModel::setHideEmptyGroups(bool f) {
	hideEmptyGroups = f;
	invalidateFilter();
}

bool SortedTreeModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const {
	ContactTreeModel *model = static_cast<ContactTreeModel *>(sourceModel());
	TreeItemType type = model->getType(model->index(source_row, 0, source_parent));
	if(type == TIT_CONTACT && hideOffline) {
		Contact *c = model->getContact(model->index(source_row, 0, source_parent));
		if(c->status == ST_OFFLINE) return false;
	}
	if(type == TIT_GROUP && hideEmptyGroups) {
		//return hasChildren(mapFromSource(model->index(source_row, 0, source_parent)));
		QStringList group = model->getGroup(model->index(source_row, 0, source_parent));
		int count = model->onlineCount(group);
		return count > 0;
	}
	return true;
}

bool SortedTreeModel::lessThan(const QModelIndex &left, const QModelIndex &right) const {
	ContactTreeModel *model = static_cast<ContactTreeModel *>(sourceModel());

	TreeItemType ltype = model->getType(left), rtype = model->getType(right);
	if(ltype != rtype) return ltype < rtype;

	if(ltype == TIT_GROUP)
		return model->getGroup(left).last() < model->getGroup(right).last();
	
	Contact *cleft = model->getContact(left),
		*cright = model->getContact(right);

	if(cleft->status != cright->status)
		return cleft->status > cright->status;

	return ContactTreeModel::getNick(cleft) < ContactTreeModel::getNick(cright);
}

/////////////////////////////

Q_EXPORT_PLUGIN2(contactList, ContactList)
