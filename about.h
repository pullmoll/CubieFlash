#ifndef ABOUTCUBIEFLASHER_H
#define ABOUTCUBIEFLASHER_H

#include <QDialog>

namespace Ui {
class about;
}

class about : public QDialog
{
        Q_OBJECT

public:
        explicit about(QWidget *parent = 0);
        ~about();

private:
        Ui::about *ui;
};

#endif // ABOUTCUBIEFLASHER_H
