#include "filetransferoptions.h"

FileTransferOptions::FileTransferOptions(QWidget *parent)
	: OptionsPageI(parent)
{
	ui.setupUi(this);

}

FileTransferOptions::~FileTransferOptions()
{

}

bool FileTransferOptions::apply() {
	emit applied();
	return true;
}

void FileTransferOptions::reset() {
}
