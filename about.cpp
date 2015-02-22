#include "about.h"
#include "ui_about.h"

about::about(QWidget *parent) :
        QDialog(parent),
        ui(new Ui::about)
{
        ui->setupUi(this);
        ui->label1->setText(ui->label1->text().replace(QLatin1String("%1"), qApp->applicationName()));
        adjustSize();
}

about::~about()
{
        delete ui;
}
