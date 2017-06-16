/*
 Copyright (C) 2017 Nikolay N. Karikh <knn@qmtunnel.com>

 This file is part of qmtunnel and is licensed under GNU General Public License 3.0, with
 the additional special exception to link portions of this program with the OpenSSL library.
 See LICENSE file for more details.
*/

#include "ssl_helper.h"

//---------------------------------------------------------------------------
bool isCorrectKeyCertificatePair(const QSslKey &key, const QSslCertificate &cert)
{
  X509 *x=(X509 *)cert.handle();
  EVP_PKEY *k = EVP_PKEY_new();
  if (key.algorithm() == QSsl::Rsa)
    EVP_PKEY_assign_RSA(k, (RSA *)key.handle());
  else
    EVP_PKEY_assign_DSA(k, (DSA *)key.handle());
  if (X509_check_private_key(x,k) == 1)
    return true;
  return false;
}

//---------------------------------------------------------------------------
QSslCertificate loadSSLCertFromFile(const QString &filename)
{
  QList<QSslCertificate> certificateList = QSslCertificate::fromPath(filename, QSsl::Pem);

  QSslCertificate crt;
  for (int i=certificateList.count()-1; i >= 0; i--)
  {
    if (certificateList.at(i).isNull())
      certificateList.removeAt(i);
    else if (crt.isNull())
      crt = certificateList.at(i);
  }
  return crt;
}

//---------------------------------------------------------------------------
QByteArray QByteArray_from_X509(X509 *x509)
{
  if (!x509)
    return QByteArray();

  // Use i2d_X509 to convert the X509 to an array.
  int length = i2d_X509(x509, 0);
  QByteArray array;
  array.resize(length);
  char *data = array.data();
  char **dataP = &data;
  unsigned char **dataPu = (unsigned char **)dataP;
  if (i2d_X509(x509, dataPu) < 0)
    return QByteArray();

  // Convert to Base64 - wrap at 64 characters.
  array = array.toBase64();
  QByteArray tmp;
  for (int i = 0; i < array.size() - 64; i += 64) {
    tmp += QByteArray::fromRawData(array.data() + i, 64);
    tmp += "\n";
  }
  if (int remainder = array.size() % 64) {
    tmp += QByteArray::fromRawData(array.data() + array.size() - remainder, remainder);
    tmp += "\n";
  }

  return "-----BEGIN CERTIFICATE-----\n" + tmp + "-----END CERTIFICATE-----\n";
}
