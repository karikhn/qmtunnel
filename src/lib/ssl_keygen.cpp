/*
 Copyright (C) 2017 Nikolay N. Karikh <knn@qmtunnel.com>

 This file is part of qmtunnel and is licensed under GNU General Public License 3.0, with
 the additional special exception to link portions of this program with the OpenSSL library.
 See LICENSE file for more details.
*/

#include "ssl_keygen.h"
#include <QVBoxLayout>
#include <QEvent>
#include <QFile>
#include <QMouseEvent>
#include <QGridLayout>
#include <QSpacerItem>
#include <QComboBox>
#include <QMessageBox>
#include <QApplication>
#include <QFileDialog>
#include <QDateTime>
#include <openssl/pem.h>
#include <openssl/rsa.h>
//#include <openssl/rand.h>
#include "gui_settings.h"

extern "C" {
void RAND_seed(const void *buf, int num);
}

//---------------------------------------------------------------------------
CertGenParamsPage::CertGenParamsPage(QWidget *parent): QWizardPage(parent)
{
  setTitle("Certificate info");

  QVBoxLayout *vLayout = new QVBoxLayout();
  QLabel *label = new QLabel(tr("Please fill in the following info to generate new SSL private key and certificate pair:"));
  label->setWordWrap(true);
  vLayout->addWidget(label);

  QGridLayout *gridLayout = new QGridLayout();
  int row=0;

  label = new QLabel(tr("&Username (CN):"));
  gridLayout->addWidget(label, row, 0, Qt::AlignRight | Qt::AlignVCenter);
  QLineEdit *lineEdit = new QLineEdit();
  gridLayout->addWidget(lineEdit, row, 1);
  label->setBuddy(lineEdit);
  registerField("certSubject*",lineEdit);
  row++;

  label = new QLabel(tr("&Organization:"));
  gridLayout->addWidget(label, row, 0, Qt::AlignRight | Qt::AlignVCenter);
  lineEdit = new QLineEdit();
  gridLayout->addWidget(lineEdit, row, 1);
  label->setBuddy(lineEdit);
  registerField("certOrganization",lineEdit);
  row++;

  label = new QLabel(tr("&E-Mail:"));
  gridLayout->addWidget(label, row, 0, Qt::AlignRight | Qt::AlignVCenter);
  lineEdit = new QLineEdit();
  gridLayout->addWidget(lineEdit, row, 1);
  label->setBuddy(lineEdit);
  registerField("certEMail",lineEdit);
  row++;

  label = new QLabel(tr("Key length:"));
  gridLayout->addWidget(label, row, 0, Qt::AlignRight | Qt::AlignVCenter);
  QComboBox *comboBox = new QComboBox();
  comboBox->addItems(QStringList() << "1024 bits" << "2048 bits" << "4096 bits");
  comboBox->setCurrentIndex(1);
  gridLayout->addWidget(comboBox, row, 1);
  label->setBuddy(comboBox);
  registerField("pkeyLength",comboBox);
  row++;

  gridLayout->setColumnStretch(0, 1);
  gridLayout->setColumnStretch(1, 5);
  vLayout->addLayout(gridLayout);
  setLayout(vLayout);
}

//---------------------------------------------------------------------------
bool RandSeeder::eventFilter(QObject *obj, QEvent *event)
{
  if (event->type() == QEvent::MouseMove)
  {
    uint addition = QDateTime::currentDateTime().toTime_t();
    QMouseEvent *mEvent = static_cast<QMouseEvent *>(event);
    addition *= mEvent->globalX();
    addition *= mEvent->globalY();
    RAND_seed(&addition, sizeof(uint));
    counter++;
    emit step();
    if (counter > maximum)
      stop();
    return true;
  }
  else
    return QObject::eventFilter(obj, event);
}

//---------------------------------------------------------------------------
void RandSeeder::startSeeding(QList<QWidget *> w, int lim)
{
  counter = 0;
  if (lim < 512)
    lim = 512;
  maximum = lim;
  for (int i=0; i < w.count(); i++)
  {
    wdgs.insert(w.at(i), w.at(i)->hasMouseTracking());
    w.at(i)->installEventFilter(this);
    w.at(i)->setMouseTracking(true);
  }
}

//---------------------------------------------------------------------------
void RandSeeder::stop()
{
  QList<QWidget *> w = wdgs.keys();
  for (int i=0; i < w.count(); i++)
  {
    bool mt = wdgs.value(w.at(i));
    wdgs.remove(w.at(i));
    w.at(i)->setMouseTracking(mt);
    w.at(i)->removeEventFilter(this);
  }
  emit stopped();
}

//---------------------------------------------------------------------------
CertGenRandomizePage::CertGenRandomizePage(QWidget *parent): QWizardPage(parent)
{
  setTitle("Random seed generation");

  QVBoxLayout *vLayout = new QVBoxLayout();
  label = new QLabel(tr("Move the mouse cursor randomly in the area below in different directions to generate random seed."));
  label->setWordWrap(true);
  vLayout->addWidget(label);
  progressBar = new QProgressBar();
  progressBar->setMaximum(1024);
  progressBar->setValue(0);
  vLayout->addWidget(progressBar);

  setLayout(vLayout);

  connect(&seeder, SIGNAL(step()), this, SLOT(seedStep()));
  connect(&seeder, SIGNAL(stopped()), this, SLOT(seedStopped()));
}

//---------------------------------------------------------------------------
void CertGenRandomizePage::seedStep()
{
  progressBar->setValue(progressBar->value()+1);
}

//---------------------------------------------------------------------------
void CertGenRandomizePage::cleanupPage()
{
  seeder.stop();
  progressBar->setValue(0);
}

//---------------------------------------------------------------------------
void CertGenRandomizePage::initializePage()
{
  progressBar->setValue(0);
  progressBar->setMaximum(1024);
  QList<QWidget *> wlst, chwdg;
  wlst << this << wizard() << progressBar << label;
  chwdg = wizard()->findChildren<QWidget *>();
  for(int i=0; i < chwdg.count(); i++)
  {
    if (!wlst.contains(chwdg.at(i)))
      wlst.append(chwdg.at(i));
  }
  seeder.startSeeding(wlst, 1024);
}

//---------------------------------------------------------------------------
void CertGenRandomizePage::seedStopped()
{
  if (seeder.isCompleted())
    emit completeChanged();
}

//---------------------------------------------------------------------------
bool CertGenRandomizePage::isComplete() const
{
  if(seeder.isCompleted())
    return true;
  return false;
}

//---------------------------------------------------------------------------
CertGenFilenamesPage::CertGenFilenamesPage(QWidget *parent): QWizardPage(parent)
{
  setTitle("Save certificate and private key");

  setFinalPage(true);

  QVBoxLayout *vLayout = new QVBoxLayout();
  QLabel *label = new QLabel(tr("Please enter or select file names to save generated certificate and private key:"));
  label->setWordWrap(true);
  vLayout->addWidget(label);

  QHBoxLayout *hLayout = new QHBoxLayout();
  label = new QLabel(tr("&Certificate filename:"));
  hLayout->addWidget(label, 1, Qt::AlignRight | Qt::AlignVCenter);
  edtCertFile = new QLineEdit();
  hLayout->addWidget(edtCertFile, 8);
  label->setBuddy(edtCertFile);
  QPushButton *btnBrowse = new QPushButton(tr("Browse"));
  connect(btnBrowse, SIGNAL(clicked()), this, SLOT(btnBrowseCertFile_clicked()));
  hLayout->addWidget(btnBrowse, 1);
  vLayout->addLayout(hLayout);
  label = new QLabel(tr("You will need to provide your certificate to the servers you want to connect to."));
  label->setEnabled(false);
  label->setWordWrap(true);
  vLayout->addWidget(label);
  registerField("certFilename*",edtCertFile);

  hLayout = new QHBoxLayout();
  label = new QLabel(tr("&Private key filename:"));
  hLayout->addWidget(label, 1, Qt::AlignRight | Qt::AlignVCenter);
  edtPKeyFile = new QLineEdit();
  hLayout->addWidget(edtPKeyFile, 8);
  label->setBuddy(edtPKeyFile);
  btnBrowse = new QPushButton(tr("Browse"));
  connect(btnBrowse, SIGNAL(clicked()), this, SLOT(btnBrowsePKeyFile_clicked()));
  hLayout->addWidget(btnBrowse, 1);
  vLayout->addLayout(hLayout);
  label = new QLabel(tr("Keep your private key in secure place and do not allow others to get it."));
  label->setEnabled(false);
  label->setWordWrap(true);
  vLayout->addWidget(label);
  registerField("pkeyFilename*",edtPKeyFile);

  setLayout(vLayout);
}

//---------------------------------------------------------------------------
void CertGenFilenamesPage::btnBrowseCertFile_clicked()
{
  QFileDialog fileDialog(this);
  fileDialog.setFileMode(QFileDialog::AnyFile);
  fileDialog.setAcceptMode(QFileDialog::AcceptSave);
  fileDialog.setDefaultSuffix("crt");
  fileDialog.setNameFilters(QStringList() << "CRT files (*.crt)" << "PEM format (*.pem)" << "Any files (*)");

  cJSON *j_fileDialogs = cJSON_GetObjectItem(j_gui_settings, "fileDialogs");
  if (!j_fileDialogs)
  {
    j_fileDialogs = cJSON_CreateObject();
    cJSON_AddItemToObject(j_gui_settings, "fileDialogs", j_fileDialogs);
  }
  cJSON *j_fileDialog_SSLCert = cJSON_GetObjectItem(j_fileDialogs, "SSLCert");
  if (j_fileDialog_SSLCert)
  {
    cJSON *j;
    j = cJSON_GetObjectItem(j_fileDialog_SSLCert, "geometry");
    if (j && j->type == cJSON_String)
      fileDialog.restoreGeometry(QByteArray::fromBase64(j->valuestring));
    j = cJSON_GetObjectItem(j_fileDialog_SSLCert, "state");
    if (j && j->type == cJSON_String)
      fileDialog.restoreState(QByteArray::fromBase64(j->valuestring));
    j = cJSON_GetObjectItem(j_fileDialog_SSLCert, "filter");
    if (j && j->type == cJSON_String)
      fileDialog.selectNameFilter(QString::fromUtf8(j->valuestring));
  }
  if (!fileDialog.exec())
    return;

  if (j_fileDialog_SSLCert)
    cJSON_DeleteItemFromObject(j_fileDialogs, "SSLCert");
  j_fileDialog_SSLCert = cJSON_CreateObject();
  cJSON_AddItemToObject(j_fileDialogs, "SSLCert", j_fileDialog_SSLCert);

  cJSON_AddStringToObject(j_fileDialog_SSLCert, "geometry", fileDialog.saveGeometry().toBase64());
  cJSON_AddStringToObject(j_fileDialog_SSLCert, "state", fileDialog.saveState().toBase64());
  cJSON_AddStringToObject(j_fileDialog_SSLCert, "filter", fileDialog.selectedNameFilter().toUtf8());

  ui_save();

  if (fileDialog.selectedFiles().isEmpty())
    return;

  edtCertFile->setText(fileDialog.selectedFiles().at(0));
}


//---------------------------------------------------------------------------
void CertGenFilenamesPage::btnBrowsePKeyFile_clicked()
{
  QFileDialog fileDialog(this);
  fileDialog.setFileMode(QFileDialog::AnyFile);
  fileDialog.setAcceptMode(QFileDialog::AcceptSave);
  fileDialog.setDefaultSuffix("key");
  fileDialog.setNameFilters(QStringList() << "KEY files (*.key)" << "PEM format (*.pem)" << "Any files (*)");

  cJSON *j_fileDialogs = cJSON_GetObjectItem(j_gui_settings, "fileDialogs");
  if (!j_fileDialogs)
  {
    j_fileDialogs = cJSON_CreateObject();
    cJSON_AddItemToObject(j_gui_settings, "fileDialogs", j_fileDialogs);
  }
  cJSON *j_fileDialog_PrivateKey = cJSON_GetObjectItem(j_fileDialogs, "PrivateKey");
  if (j_fileDialog_PrivateKey)
  {
    cJSON *j;
    j = cJSON_GetObjectItem(j_fileDialog_PrivateKey, "geometry");
    if (j && j->type == cJSON_String)
      fileDialog.restoreGeometry(QByteArray::fromBase64(j->valuestring));
    j = cJSON_GetObjectItem(j_fileDialog_PrivateKey, "state");
    if (j && j->type == cJSON_String)
      fileDialog.restoreState(QByteArray::fromBase64(j->valuestring));
    j = cJSON_GetObjectItem(j_fileDialog_PrivateKey, "filter");
    if (j && j->type == cJSON_String)
      fileDialog.selectNameFilter(QString::fromUtf8(j->valuestring));
  }
  if (!fileDialog.exec())
    return;

  if (j_fileDialog_PrivateKey)
    cJSON_DeleteItemFromObject(j_fileDialogs, "PrivateKey");
  j_fileDialog_PrivateKey = cJSON_CreateObject();
  cJSON_AddItemToObject(j_fileDialogs, "PrivateKey", j_fileDialog_PrivateKey);

  cJSON_AddStringToObject(j_fileDialog_PrivateKey, "geometry", fileDialog.saveGeometry().toBase64());
  cJSON_AddStringToObject(j_fileDialog_PrivateKey, "state", fileDialog.saveState().toBase64());
  cJSON_AddStringToObject(j_fileDialog_PrivateKey, "filter", fileDialog.selectedNameFilter().toUtf8());

  ui_save();

  if (fileDialog.selectedFiles().isEmpty())
    return;

  edtPKeyFile->setText(fileDialog.selectedFiles().at(0));
}

//---------------------------------------------------------------------------
CertificateGeneratorWizard::CertificateGeneratorWizard(const QString &default_cn, QWidget * parent): QWizard(parent)
{
  setOption(QWizard::NoCancelButton, false);
  setOption(QWizard::NoDefaultButton, false);
  setOption(QWizard::CancelButtonOnLeft, true);
  CertGenParamsPage *first_page = new CertGenParamsPage;
  addPage(first_page);
  connect(first_page, SIGNAL(completeChanged()), this, SLOT(setDefaultFilenames()));
  addPage(new CertGenRandomizePage);
  addPage(new CertGenFilenamesPage);

  setWindowTitle(tr("Certificate and private key generator"));

  setField("certSubject", default_cn);
}

//---------------------------------------------------------------------------
void CertificateGeneratorWizard::setDefaultFilenames()
{
  QWizardPage *page = qobject_cast<QWizardPage *>(sender());
  if (page && page->isComplete())
  {
    setField("certFilename",QDir::current().filePath(field("certSubject").toString()+QString(".crt")));
    setField("pkeyFilename",QDir::current().filePath(field("certSubject").toString()+QString(".key")));
  }
}

//---------------------------------------------------------------------------
/*
  BIGNUM *bne = BN_new();
  ok = BN_set_word(bne, RSA_F4) == 1;
  if (!ok)
  {
    BN_free(bne);
    QApplication::restoreOverrideCursor();
    QMessageBox::critical(this, tr("SSL error"), tr("BN_set_word() error"));
    QWizard::reject();
    return;
  }
  RSA *rsa_new = RSA_new();
  ok = RSA_generate_key_ex(rsa_new, bits, bne, NULL) == 1;
  BN_free(bne);
  if (!ok)
  {
    RSA_free(rsa_new);
    QApplication::restoreOverrideCursor();
    QMessageBox::critical(this, tr("SSL error"), tr("RSA_generate_key_ex() error"));
    QWizard::reject();
    return;
  }
  */
void CertificateGeneratorWizard::accept()
{
  EVP_PKEY *pk;
  X509 *x;
  X509_NAME *name=NULL;
  bool ok;
  //parameters
  int bits = 4096;
  if (field("pkeyLength").toInt() == 0)
    bits = 1024;
  else if (field("pkeyLength").toInt() == 1)
    bits = 2048;
  long serial=42;
  long days=1895;

  QApplication::setOverrideCursor(Qt::WaitCursor);
  //create private key
  pk = EVP_PKEY_new();
  RSA *rsa = RSA_generate_key(bits, RSA_F4, NULL, NULL);
  EVP_PKEY_assign_RSA(pk, rsa);
  {
    //save it to QSslKey
    BIO *bio = BIO_new(BIO_s_mem());
    PEM_write_bio_RSAPrivateKey(bio, rsa, (const EVP_CIPHER *)0, NULL, 0, 0, 0);
    QByteArray pem;
    char *data;
    long size = BIO_get_mem_data(bio, &data);
    pem = QByteArray(data, size);
    BIO_free(bio);
    pkey = QSslKey(pem,QSsl::Rsa);
    ok = !pkey.isNull();
    if (!ok)
    {
      QApplication::restoreOverrideCursor();
      QMessageBox::critical(this, tr("SSL error"), tr("pkey.isNull"));
      QWizard::reject();
      return;
    }
  }
  x = X509_new();
  X509_set_version(x, 2);
  ASN1_INTEGER_set(X509_get_serialNumber(x), serial);
  X509_gmtime_adj(X509_get_notBefore(x), (long)60*60*24*(-2));
  X509_gmtime_adj(X509_get_notAfter(x), (long)60*60*24*days);
  X509_set_pubkey(x, pk);

  name = X509_get_subject_name(x);

  QVariant fldVal = field("certSubject");
  X509_NAME_add_entry_by_txt(name,"CN", MBSTRING_ASC,
                 (unsigned char *)fldVal.toString().toLatin1().data(), -1, -1, 0);

  fldVal = field("certOrganization");
  if (!fldVal.isNull())
  {
    X509_NAME_add_entry_by_txt(name,"O", MBSTRING_ASC,
                   (unsigned char *)fldVal.toString().toLatin1().data(), -1, -1, 0);
  }
  fldVal = field("certEMail");
  if (!fldVal.isNull())
  {
    X509_NAME_add_entry_by_txt(name,"emailAddress", MBSTRING_ASC,
                   (unsigned char *)fldVal.toString().toLatin1().data(), -1, -1, 0);
  }

  X509_set_issuer_name(x,name);

  X509_sign(x,pk,EVP_md5());
  {
    cert = QSslCertificate(QByteArray_from_X509(x));
    ok = !cert.isNull();
    if (!ok)
    {
      QApplication::restoreOverrideCursor();
      QMessageBox::critical(this, tr("SSL error"), tr("cert.isValid"));
      QWizard::reject();
      return;
    }
  }

  ok = isCorrectKeyCertificatePair(pkey, cert);
  if (!ok)
  {
    QApplication::restoreOverrideCursor();
    QMessageBox::critical(this, tr("SSL error"), tr("isCorrectKeyCertificatePair"));
    QWizard::reject();
    return;
  }

  QApplication::restoreOverrideCursor();
  if (ok)
  {
    cert_filename = field("certFilename").toString();
    pkey_filename = field("pkeyFilename").toString();
    if (!cert_filename.isEmpty())
    {
      QFile f(cert_filename);
      if (f.open(QIODevice::WriteOnly))
      {
        f.write(cert.toPem());
        f.close();
      }
      else
      {
        QMessageBox::critical(this, tr("Unable to open file"), tr("Unable to open file %1 for writing").arg(f.fileName()));
        ok = false;
      }
    }
    if (!pkey_filename.isEmpty())
    {
      QFile f(pkey_filename);
      if (f.open(QIODevice::WriteOnly))
      {
        f.write(pkey.toPem());
        f.close();
      }
      else
      {
        QMessageBox::critical(this, tr("Unable to open file"), tr("Unable to open file %1 for writing").arg(f.fileName()));
        ok = false;
      }
    }

    if (ok)
      QWizard::accept();
  }
  if (!ok)
    QWizard::reject();
}
