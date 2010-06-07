#include "messageformat.h"
#include <QtPlugin>
#include <QTextDocument> // for Qt::escape
#include <QDebug>

#define RX_DOMAIN		"(?:(?:\\w|-)+\\.)+(?:co(?:m)?|org|net|gov|biz|info|travel|ous|[a-z]{2})"
#define RX_PROTOS		"(?:http(?:s)?://|ftp://|mailto:)?"
#define RX_PORT			"(?:\\:\\d{1,5})?"
#define RX_EMAIL		"\\w+@" RX_DOMAIN
#define RX_OTHER		RX_DOMAIN RX_PORT "(?:[/?](?:[a-zA-Z0-9~\\-%;=]|&(?:!apos|quot|gt|lt))+)*"
#define BREAK                   "(?:^|\\n|\\r|\\t|\\s)?"
#define LP				BREAK "(" RX_PROTOS ")(" RX_EMAIL "|" RX_OTHER ")"

QString my_escape(const QString &in) {
    QString out = in;
    return out.replace('&', "&amp;").replace('<', "&lt;").replace('>', "&gt;").replace('\'', "&apos;").replace('\"', "&quot;");
}

QString my_unescape(const QString &in) {
    QString out = in;
    //return out.replace('&', "&amp;").replace('<', "&lt;").replace('>', "&gt;").replace('\'', "&apos;").replace('\"', "&quot;");
    return out.replace("&quot;", "\"").replace("&apos;", "'").replace("&gt;", ">").replace("&lt;", "<").replace("&amp;", "&");
}

PluginInfo info = {
	0x080,
	"MessageFormat",
	"Scott Ellis",
	"mail@scottellis.com.au",
	"http://www.scottellis.com.au",
	"MessageFormat",
	0x00000001
};

MessageFormat::MessageFormat()
{

}

MessageFormat::~MessageFormat()
{

}

bool MessageFormat::load(CoreI *core) {
	core_i = core;
	if((events_i = (EventsI *)core_i->get_interface(INAME_EVENTS)) == 0) return false;

	events_i->add_event_filter(this, 0x200, UUID_MSG, EVENT_TYPE_MASK_INCOMING | EVENT_TYPE_MASK_OUTGOING);
	return true;
}

bool MessageFormat::modules_loaded() {
	return true;
}

bool MessageFormat::pre_shutdown() {
	events_i->removeEventFilter(this);
	return true;
}

bool MessageFormat::unload() {
	return true;
}

const PluginInfo &MessageFormat::get_plugin_info() {
	return info;
}

/////////////////////////////

bool MessageFormat::event_fired(EventsI::Event &e) {
	if(e.uuid == UUID_MSG) {
		Message &m = static_cast<Message &>(e);
		m.text = my_escape(m.text);
		m.text.replace("\n", "<br />\n");
		linkUrls(m.text);
		qDebug() << "Formatted text: " << m.text;
	}
	return true;
}

void MessageFormat::linkUrls(QString &str) {
	//dispMsg.replace(QRegExp(LP), "<a href='http://\\2'>\\1</a>");

	QRegExp rx(LP), rx_email("^" RX_EMAIL);
	int pos = 0, len, len2;
	QString scheme, after;
	bool valid;
	while ((pos = rx.indexIn(str, pos)) != -1) {
		len = rx.matchedLength();
		len2 = rx.cap(1).length() + rx.cap(2).length();

		//rx.cap(0) is whole match, rx.cap(1) is url scheme, rx.cap(2) is the rest

		scheme = rx.cap(1);
		valid = true;
		if(scheme.isEmpty()) {
			if(rx_email.indexIn(rx.cap(2)) != -1)
				scheme = "mailto:";
			else
				scheme = "http://";
		} else {
			if((scheme == "mailto:" && rx_email.indexIn(rx.cap(2)) == -1) || (scheme != "mailto:" && rx_email.indexIn(rx.cap(2)) != -1))
				valid = false;
		}
		if(valid) {
			// encode quotes - leave entities encoded
			after = "<a href=\"" + scheme + rx.cap(2).replace('\"', "\\\"") + "\">" + rx.cap(1) + rx.cap(2) + "</a>";
			str.replace(pos + len - len2, len2, after);
			len = after.length();
		}

		pos += len;
	}
}

/////////////////////////////

Q_EXPORT_PLUGIN2(messageformat, MessageFormat)

