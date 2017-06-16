/*
 Copyright (C) 2017 Nikolay N. Karikh <knn@qmtunnel.com>

 This file is part of qmtunnel and is licensed under GNU General Public License 3.0, with
 the additional special exception to link portions of this program with the OpenSSL library.
 See LICENSE file for more details.
*/

#include "aboutdialog.h"
#include "ui_aboutdialog.h"
#include "mainwindow.h"

AboutDialog::AboutDialog(QWidget *parent) :
  QDialog(parent),
  ui(new Ui::AboutDialog)
{
  ui->setupUi(this);
  QString txt = ui->label->text();
  txt.replace(QString("v.X.Y"), QString("v.%1.%2").arg(GUI_MAJOR_VERSION).arg(GUI_MINOR_VERSION));
  ui->label->setText(txt);
}

AboutDialog::~AboutDialog()
{
  delete ui;
}
