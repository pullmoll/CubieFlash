#ifndef SETUPFILES_H
#define SETUPFILES_H

#include <QDialog>

namespace Ui {
class SetupFiles;
}

class SetupFiles : public QDialog
{
        Q_OBJECT

public:
        explicit SetupFiles(QWidget *parent = 0);
        ~SetupFiles();

private:
        Ui::SetupFiles *ui;
};

#endif // SETUPFILES_H
