#include "history.h"
#include <QtPlugin>
#include <QDebug>
#include <QSqlError>

#define DB_FILE_NAME		"message_history.db"

PluginInfo info = {
	0x280,
	"History",
	"Scott Ellis",
	"mail@scottellis.com.au",
	"http://www.scottellis.com.au",
	"History",
	0x00000001
};

History::History()
{

}

History::~History()
{

}

bool History::load(CoreI *core) {
	core_i = core;
	if((events_i = (EventsI *)core_i->get_interface(INAME_EVENTS)) == 0) return false;
	if((contact_info_i = (ContactInfoI*)core_i->get_interface(INAME_CONTACTINFO)) == 0) return false;
	if((accounts_i = (AccountsI*)core_i->get_interface(INAME_ACCOUNTS)) == 0) return false;

	events_i->add_event_listener(this, UUID_MSG);

	db = QSqlDatabase::addDatabase("QSQLITE", "History");
	db.setDatabaseName(core_i->get_config_dir() + "/" + DB_FILE_NAME);
    if(!db.open()) return false;

	QSqlQuery q(db);
	if(!q.exec("CREATE TABLE messages ("
		"  protocol varchar(256),"
		"  account varchar(256),"
		"  contact_id varchar(256),"
		"  timestamp number,"
		"  incomming boolean,"
		"  msg_read boolean,"
		"  message text);"))
	{
		qWarning() << "History db error:" << q.lastError().text();
	}
	
	return true;
}

bool History::modules_loaded() {
	QSqlQuery unread(db);
	if(!unread.exec("SELECT protocol, account, contact_id, message, incomming, timestamp FROM messages WHERE msg_read='false';"))
		qWarning() << "History read unread failed:" << unread.lastError().text();

	while(unread.next()) {
		Account *account = accounts_i->account_info(unread.value(0).toString(), unread.value(1).toString());
		if(account) {
			Contact *contact = contact_info_i->get_contact(account, unread.value(2).toString());
			ContactChanged cc(contact, this);
			events_i->fire_event(cc);
			Message m(contact, unread.value(3).toString(), unread.value(4).toBool(), 0, this);
			m.timestamp = QDateTime::fromTime_t(unread.value(5).toUInt());
			events_i->fire_event(m);
		}
	}
	return true;
}

bool History::pre_shutdown() {
	events_i->remove_event_listener(this, UUID_MSG);
	return true;
}

bool History::unload() {
	db.close();
	return true;
}

const PluginInfo &History::get_plugin_info() {
	return info;
}

/////////////////////////////

bool History::event_fired(EventsI::Event &e) {
	if(e.uuid == UUID_MSG && e.source != this) {
		Message &m = static_cast<Message &>(e);
		if(m.contact->has_property("DisableHistory"))
			return true;

		writeQuery = new QSqlQuery(db);
		writeQuery->prepare("INSERT INTO messages VALUES(?, ?, ?, ?, ?, ?, ?);");

		writeQuery->addBindValue(m.contact->account->proto->name());
		writeQuery->addBindValue(m.contact->account->account_id);
		writeQuery->addBindValue(m.contact->contact_id);
		writeQuery->addBindValue(m.timestamp.toTime_t());
		writeQuery->addBindValue(m.type == EventsI::ET_INCOMMING);
		writeQuery->addBindValue(m.read);
		writeQuery->addBindValue(m.text);

		if(!writeQuery->exec()) {
			qWarning() << "History write failed:" << writeQuery->lastError().text();
		}

		delete writeQuery;
	}
	return true;
}

QList<Message> History::get_latest_events(Contact *contact, QDateTime earliest, bool mark_read) {
	QList<Message> ret;

	readQueryTime = new QSqlQuery(db);
	readQueryTime->prepare("SELECT message, incomming, timestamp FROM messages WHERE protocol=:proto AND account=:account AND contact_id=:contact_id AND timestamp>=:timestamp;");

	readQueryTime->bindValue(":proto", contact->account->proto->name());
	readQueryTime->bindValue(":account", contact->account->account_id);
	readQueryTime->bindValue(":contact_id", contact->contact_id);
	readQueryTime->bindValue(":timestamp", earliest.toTime_t());

	if(!readQueryTime->exec()) {
		qWarning() << "History read failed:" << readQueryTime->lastError().text();
	}

	while(readQueryTime->next()) {
		Message m(contact, readQueryTime->value(0).toString(), readQueryTime->value(1).toBool(), 0, this);
		m.timestamp = QDateTime::fromTime_t(readQueryTime->value(2).toUInt());
		ret << m;
		if(mark_read) mark_as_read(contact, m.timestamp);
	}

	delete readQueryTime;

	return ret;
}

QList<Message> History::get_latest_events(Contact *contact, int count, bool mark_read) {
	QList<Message> ret;

	readQueryCount = new QSqlQuery(db);
	readQueryCount->prepare("SELECT message, incomming, timestamp FROM messages WHERE protocol=:proto AND account=:account AND contact_id=:contact_id ORDER BY timestamp DESC LIMIT :count;");

	readQueryCount->bindValue(":proto", contact->account->proto->name());
	readQueryCount->bindValue(":account", contact->account->account_id);
	readQueryCount->bindValue(":contact_id", contact->contact_id);
	readQueryCount->bindValue(":count", count);

	if(!readQueryCount->exec()) {
		qWarning() << "History read failed:" << readQueryCount->lastError().text();
	}

	while(readQueryCount->next()) {
		Message m(contact, readQueryCount->value(0).toString(), readQueryCount->value(1).toBool(), 0, this);
		m.timestamp = QDateTime::fromTime_t(readQueryCount->value(2).toUInt());
		ret.prepend(m);
		if(mark_read) mark_as_read(contact, m.timestamp);
	}

	delete readQueryCount;

	return ret;
}
	
void History::mark_as_read(Contact *contact, QDateTime timestamp) {
	QSqlQuery mrq(db);
	QString query_text = "UPDATE messages SET msg_read='true' WHERE protocol='" + contact->account->proto->name() + "'"
		+ " AND account='" + contact->account->account_id + "'"
		+ " AND contact_id='" + contact->contact_id + "'" + 
		+ " AND timestamp=" + QString("%1").arg(timestamp.toTime_t()) + ";";
	if(!mrq.exec(query_text))
		qWarning() << "History mark as read failed:" << mrq.lastError().text();
}

void History::enable_history(Contact *contact, bool enable) {
	if(enable) contact->remove_property("DisableHistory");
	else contact->set_property("DisableHistory", true);

	events_i->fire_event(ContactChanged(contact, this));
}

/////////////////////////////

Q_EXPORT_PLUGIN2(history, History)
