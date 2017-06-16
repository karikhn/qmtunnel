/*
 Copyright (C) 2017 Nikolay N. Karikh <knn@qmtunnel.com>

 This file is part of qmtunnel and is licensed under GNU General Public License 3.0, with
 the additional special exception to link portions of this program with the OpenSSL library.
 See LICENSE file for more details.
*/

#ifndef SSLKEYGEN_H
#define SSLKEYGEN_H

#include <QDialog>
#include <QSslKey>
#include <QLineEdit>
#include <QPushButton>
#include <QMap>
#include <QSslCertificate>
#include <QWizard>
#include <QWizardPage>
#include <QProgressBar>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFile>
#include <QLabel>
#include "../lib/ssl_helper.h"

//---------------------------------------------------------------------------
class CertificateGeneratorWizard: public QWizard
{
  Q_OBJECT

public:
  CertificateGeneratorWizard(const QString &default_cn, QWidget * parent=0);
  QSslKey getPrivateKey() { return pkey; }
  QSslCertificate getCertificate() { return cert; }
  QString getPrivateKeyFilename() { return pkey_filename; }
  QString getCertificateFilename() { return cert_filename; }
  void accept();

private slots:
  void setDefaultFilenames();

private:
  QSslKey pkey;
  QSslCertificate cert;
  QString pkey_filename;
  QString cert_filename;
};

//---------------------------------------------------------------------------
class RandSeeder: public QObject
{
  Q_OBJECT

public:
  void startSeeding(QList<QWidget *> w, int lim);
  void stop();
  bool isCompleted() const{return (counter>=maximum);}
protected:
  QMap<QWidget *, bool> wdgs;
  int counter;
  int maximum;
  bool eventFilter(QObject *obj, QEvent *event);
signals:
  void step();
  void stopped();
};

//---------------------------------------------------------------------------
class CertGenParamsPage : public QWizardPage
{
  Q_OBJECT

public:
  CertGenParamsPage(QWidget *parent=0);
};

//---------------------------------------------------------------------------
class CertGenRandomizePage : public QWizardPage
{
  Q_OBJECT

public:
  CertGenRandomizePage(QWidget *parent=0);
  virtual void cleanupPage();
  virtual void initializePage();
  virtual bool isComplete() const;
private:
  QProgressBar * progressBar;
  QLabel * label;
  RandSeeder seeder;
  private slots:
  void seedStep();
  void seedStopped();
};


//---------------------------------------------------------------------------
class CertGenFilenamesPage : public QWizardPage
{
  Q_OBJECT

public:
  CertGenFilenamesPage(QWidget *parent=0);
private slots:
  void btnBrowseCertFile_clicked();
  void btnBrowsePKeyFile_clicked();
private:
  QLineEdit *edtCertFile;
  QLineEdit *edtPKeyFile;
};

#endif // SSLKEYGEN_H
