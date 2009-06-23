#include "clistoptions.h"
#include <QDebug>

CListOptions::CListOptions(const Settings &settings, QWidget *parent)
	: OptionsPageI(parent), current_settings(settings)
{
	ui.setupUi(this);

        connect(ui.chkHideOffline, SIGNAL(clicked()), this, SIGNAL(changed()));
        connect(ui.chkHideEmptyGroups, SIGNAL(clicked()), this, SIGNAL(changed()));
}

CListOptions::~CListOptions() {

}


bool CListOptions::apply() {
	current_settings.hide_offline = ui.chkHideOffline->isChecked();
	current_settings.hide_empty_groups = ui.chkHideEmptyGroups->isChecked();
	emit applied();
	return true;
}

void CListOptions::reset() {
	ui.chkHideOffline->setChecked(current_settings.hide_offline);
	ui.chkHideEmptyGroups->setChecked(current_settings.hide_empty_groups);
}

