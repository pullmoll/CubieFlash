#include "setupfiles.h"
#include "ui_setupfiles.h"

SetupFiles::SetupFiles(QWidget *parent) :
        QDialog(parent),
        ui(new Ui::SetupFiles)
{
        ui->setupUi(this);
}

SetupFiles::~SetupFiles()
{
        delete ui;
}
