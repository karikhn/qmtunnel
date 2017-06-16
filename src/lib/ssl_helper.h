/*
 Copyright (C) 2017 Nikolay N. Karikh <knn@qmtunnel.com>

 This file is part of qmtunnel and is licensed under GNU General Public License 3.0, with
 the additional special exception to link portions of this program with the OpenSSL library.
 See LICENSE file for more details.
*/

#ifndef SSL_HELPER_H
#define SSL_HELPER_H

#include <QSslCertificate>
#include <QSslKey>
#include <openssl/evp.h>
#include <openssl/x509.h>

bool isCorrectKeyCertificatePair(const QSslKey &key, const QSslCertificate &cert);
QSslCertificate loadSSLCertFromFile(const QString &filename);
QByteArray QByteArray_from_X509(X509 *x509);

#endif // SSL_HELPER_H
