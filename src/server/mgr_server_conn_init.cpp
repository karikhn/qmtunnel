/*
 Copyright (C) 2017 Nikolay N. Karikh <knn@qmtunnel.com>

 This file is part of qmtunnel and is licensed under GNU General Public License 3.0, with
 the additional special exception to link portions of this program with the OpenSSL library.
 See LICENSE file for more details.
*/

#include "mgr_server.h"
#include "../lib/prc_log.h"
#include "../lib/sys_util.h"
#include <QStringList>
#include <QMap>
#include <QUrl>
#include <time.h>

#define HTTP_MAX_HEADERS_SIZE         16*1024
#define HTTP_MAX_READ_BUFFER_SIZE     64*1024

const char *html_error_template = "<html>\n"
    "<head><title>%d %s</title></head>\n"
    "<body bgcolor=\"white\">\n"
    "<center><h1>%d %s</h1></center>\n"
    "<hr><center><h1>%s</h1></center>\n"
    "</body>\n"
    "</html>\n";

const char *http_server_name = "qmtunnel/http";

//---------------------------------------------------------------------------
void http_error(MgrClientConnection *conn, int http_code, const char *http_error)
{
  char current_time[50];
  time_t t = time(NULL);
  strftime(current_time, sizeof(current_time), "%a, %d %b %Y %H:%M:%S GMT", gmtime(&t));

  char html_buf[4096];
  sprintf(html_buf, html_error_template, http_code, http_error, http_code, http_error, http_server_name);
  conn->log(LOG_DBG2, QString(": HTTP error %1 %2")
          .arg(http_code).arg(QString::fromUtf8(http_error)));
  conn->write(QString("HTTP/1.1 %1 %2\r\n"
                      "Server: %3\r\n"
                      "Date: %4\r\n"
                      "Content-Type: text/html\r\n"
                      "Content-Length: %5\r\n"
                      "Connection: close\r\n"
                      "\r\n").arg(http_code)
                             .arg(QString::fromUtf8(http_error))
                             .arg(QString::fromUtf8(http_server_name))
                             .arg(QString::fromUtf8(current_time))
                             .arg((int)strlen(html_buf)).toUtf8()+QByteArray(html_buf));
  conn->disconnectFromHost();
}


//---------------------------------------------------------------------------
#if QT_VERSION >= 0x050000
void MgrServer::incomingConnection(qintptr socketDescriptor)
#else
void MgrServer::incomingConnection(int socketDescriptor)
#endif
{
  MgrClientConnection *socket = new MgrClientConnection(MgrClientConnection::INCOMING);
  socket->setSocketDescriptor(socketDescriptor);
  connect(socket, SIGNAL(socket_finished()), this, SLOT(socket_finished()));
  if (params.flags & MgrServerParameters::FL_ENABLE_HTTP)
    connect(socket, SIGNAL(init_inputParsing()), this, SLOT(socket_parseInitBuffer()));
  connect(socket, SIGNAL(packetReceived(MgrPacketCmd,QByteArray)), this, SLOT(packetReceived(MgrPacketCmd,QByteArray)));
  mgrconn_list_in.append(socket);
  MgrClientState *socket_state = new MgrClientState;
  mgrconn_state_list_in.insert(socket, socket_state);
  socket->setPrivateKey(params.private_key_filename);
  socket->setLocalCertificate(params.ssl_cert_filename);
  socket->params.name.clear();
  socket->params.host = socket->peerAddress().toString();
  socket->params.port = socket->peerPort();
  socket->socket_connected();
  prc_log(LOG_DBG1, QString("New incoming connection from %1:%2").arg(socket->peerAddress().toString()).arg(socket->peerPort()));

  if (params.max_incoming_mgrconn > 0 && (quint32)mgrconn_list_in.count() > params.max_incoming_mgrconn)
  {
    for (int i=0; i < mgrconn_list_in.count(); i++)
    {
      if (mgrconn_list_in[i]->phase != MgrClientConnection::PHASE_OPERATIONAL)
      {
        mgrconn_list_in[i]->log(LOG_HIGH, QString(": closing non-operational/non-authorized connection due to max incoming connections limit (%1 connections)").arg(params.max_incoming_mgrconn));
        mgrconn_list_in[i]->abort();
        break;
      }
    }
  }
}

//---------------------------------------------------------------------------
void MgrServer::socket_parseInitBuffer()
{
  MgrClientConnection *socket = qobject_cast<MgrClientConnection *>(sender());
  if (!socket)
    return;

  QRegExp http_req_line_rx("^([^ ]+) (.+) HTTP/([0-9\\.]+)$");
  int pos_header_end = socket->input_buffer.indexOf("\r\n\r\n");
  while (pos_header_end >= 0)
  {
    if (pos_header_end > HTTP_MAX_HEADERS_SIZE)
    {
      http_error(socket, 431, "Request Header Fields Too Large");
      return;
    }
    QStringList header_lines = QString::fromUtf8(socket->input_buffer.left(pos_header_end)).split("\n", QString::SkipEmptyParts);
    QString http_req_line = header_lines.takeFirst().trimmed();
    QMap<QString, QString> req_headers;
    for (int i=0; i < header_lines.count(); i++)
    {
      int pos_colon = header_lines[i].indexOf(":");
      if (pos_colon >= 0)
        req_headers.insert(header_lines[i].left(pos_colon).trimmed(), header_lines[i].mid(pos_colon+1).trimmed());
    }
    int content_length = req_headers.value("Content-Length").toInt();
    if (content_length > HTTP_MAX_READ_BUFFER_SIZE-pos_header_end-4)
    {
      http_error(socket, 413, "Request Entity Too Large");
      return;
    }
    if (content_length > socket->input_buffer.length()-pos_header_end-4)
    {
      // request is not fully buffered yet
      return;
    }
    socket->input_buffer.remove(0, pos_header_end+4);
    QByteArray req_data;
    if (content_length > 0)
    {
      req_data = socket->input_buffer.left(content_length);
      socket->input_buffer.remove(0, content_length);
    }
    if (!http_req_line_rx.exactMatch(http_req_line))
    {
      http_error(socket, 400, "Bad Request");
      return;
    }
    QString http_method = http_req_line_rx.cap(1);
    if (http_method != QString("GET"))
    {
      http_error(socket, 501, "Not Implemented");
      return;
    }
    QString http_version = http_req_line_rx.cap(3);
    if (http_version != QString("1.0") && http_version != QString("1.1"))
    {
      http_error(socket, 505, "HTTP Version Not Supported");
      return;
    }
/*
    QUrl http_uri = http_req_line_rx.cap(2);
    if (http_method == QString("GET") && http_uri == QUrl("/test"))
    {
    }
    else
    {
    */
      http_error(socket, 404, "Not Found");
      return;
//    }

    pos_header_end = socket->input_buffer.indexOf("\r\n\r\n");
  }

  // check maximum read buffer size
  if (socket->input_buffer.length() > HTTP_MAX_READ_BUFFER_SIZE)
  {
    socket->log(LOG_DBG2, QString(" error: read buffer overflow (phase %1, len=%2), dropping connection")
            .arg((int)socket->phase).arg(socket->input_buffer.length()));
    socket->input_buffer.clear();
    socket->abort();
    return;
  }
}

