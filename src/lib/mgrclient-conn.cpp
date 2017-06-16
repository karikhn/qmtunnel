/*
 Copyright (C) 2017 Nikolay N. Karikh <knn@qmtunnel.com>

 This file is part of qmtunnel and is licensed under GNU General Public License 3.0, with
 the additional special exception to link portions of this program with the OpenSSL library.
 See LICENSE file for more details.
*/

#include "mgrclient-conn.h"
#include "prc_log.h"
#include "sys_util.h"
#include "ssl_helper.h"
#include <QtCore/qmath.h>
#include <QHostAddress>
#include <QSslCipher>
#include <QFile>

// $ openssl genrsa -out server.key 2048
// $ openssl req -new -x509 -key server.key -out server.crt

//---------------------------------------------------------------------------
void MgrClientConnection::init()
{
  if (direction == OUTGOING && params.conn_type == MgrClientParameters::CONN_AUTO)
    beginConnection();
}

//---------------------------------------------------------------------------
void MgrClientConnection::setParameters(const MgrClientParameters *new_params)
{
  bool need_reconnect = false;
  MgrClientParameters old_params = params;
  if (new_params->auth_username != params.auth_username ||
      new_params->auth_password != params.auth_password ||
      new_params->host != params.host ||
      new_params->port != params.port ||
      new_params->ssl_cert_filename != params.ssl_cert_filename ||
      new_params->private_key_filename != params.private_key_filename ||
      new_params->server_ssl_cert_filename != params.server_ssl_cert_filename ||
      new_params->ssl_cert != params.ssl_cert ||
      new_params->private_key != params.private_key ||
      new_params->server_ssl_cert != params.server_ssl_cert ||
      new_params->allowed_ciphers != params.allowed_ciphers ||
      new_params->enabled != params.enabled ||
      new_params->ssl_protocol != params.ssl_protocol ||
      (new_params->flags & MgrClientParameters::FL_ENABLE_HEARTBEATS) != (params.flags & MgrClientParameters::FL_ENABLE_HEARTBEATS) ||
      (new_params->flags & MgrClientParameters::FL_USE_COMPRESSION) != (params.flags & MgrClientParameters::FL_USE_COMPRESSION) ||
      (new_params->flags & MgrClientParameters::FL_DISABLE_ENCRYPTION) != (params.flags & MgrClientParameters::FL_DISABLE_ENCRYPTION) ||
      (new_params->flags & MgrClientParameters::FL_TCP_KEEP_ALIVE) != (params.flags & MgrClientParameters::FL_TCP_KEEP_ALIVE) ||
      (new_params->flags & MgrClientParameters::FL_TCP_NO_DELAY) != (params.flags & MgrClientParameters::FL_TCP_NO_DELAY) ||
      new_params->ping_interval != params.ping_interval)
    need_reconnect = true;
  params = *new_params;
  if (direction == OUTGOING)
  {
    if (need_reconnect)
    {
      emit state_changed(params.enabled ? MgrClientConnection::MGR_ERROR : MgrClientConnection::MGR_NONE);
      if (this->state() != QSslSocket::UnconnectedState)
        this->abort();
      timer_reconnect->setInterval(0);
      timer_reconnect->start();
    }
  }
  if (!need_reconnect)
  {
    if (old_params.read_buffer_size != params.read_buffer_size)
      setReadBufferSize(params.read_buffer_size);
    if (old_params.idle_timeout != params.idle_timeout && timer_idle)
    {
      timer_idle->setInterval(params.idle_timeout);
      if (timer_idle->isActive())
        timer_idle->start();
    }
    if (old_params.rcv_timeout != params.rcv_timeout && timer_heartbeat_check)
    {
      timer_heartbeat_check->setInterval(params.rcv_timeout);
      if (timer_heartbeat_check->isActive())
        timer_heartbeat_check->start();
    }
    if (old_params.reconnect_interval != params.reconnect_interval && timer_reconnect)
      timer_reconnect->setInterval(params.reconnect_interval);
    if (old_params.reconnect_interval_max > params.reconnect_interval_max && timer_reconnect)
    {
      timer_reconnect->setInterval(params.reconnect_interval);
      if (timer_reconnect->isActive())
        timer_reconnect->start();
    }
  }
}

//---------------------------------------------------------------------------
void MgrClientConnection::beginConnection()
{
  if (!params.enabled)
    return;
  if (this->state() != QSslSocket::UnconnectedState)
    this->abort();

  closing_by_cmd_close = false;

  if (receivers(SIGNAL(password_required())) > 0)
  {
    if (!params.auth_username.isEmpty() && params.auth_password.isEmpty())
    {
      log(LOG_DBG1, QString(": password is empty - waiting for input"));
      if (timer_reconnect->isActive())
        timer_reconnect->stop();
      emit password_required();
      return;
    }
  }

  if (receivers(SIGNAL(passphrase_required())) > 0)
  {
    if (!(params.flags & MgrClientParameters::FL_DISABLE_ENCRYPTION) &&
        !params.private_key_filename.isEmpty() && params.private_key.isNull())
    {
      QFile f(params.private_key_filename);
      if (f.open(QIODevice::ReadOnly))
      {
        QByteArray pkey_data = f.read(1024*1024);
        f.close();
        if (pkey_data.contains("ENCRYPTED"))
        {
          if (!private_key_passphrase.isEmpty())
            params.private_key = QSslKey(pkey_data, QSsl::Rsa, QSsl::Pem, QSsl::PrivateKey, private_key_passphrase.toUtf8());
          if (params.private_key.isNull())
          {
            log(LOG_DBG1, QString(": passphrase is required for private key - waiting for input"));
            if (timer_reconnect->isActive())
              timer_reconnect->stop();
            emit passphrase_required();
            return;
          }
        }
      }
    }
  }

  emit state_changed(MgrClientConnection::MGR_CONNECTING);
  log(LOG_DBG3, QString(": trying to connect..."));
  timer_connect->setInterval(params.conn_timeout);
  timer_connect->start();
  this->connectToHost(params.host, params.port);
}

//---------------------------------------------------------------------------
void MgrClientConnection::socket_connect_timeout()
{
  emit state_changed(MgrClientConnection::MGR_ERROR);
  emit connection_error(QAbstractSocket::SocketTimeoutError);
  log(LOG_DBG2, QString(": connect timeout"));
  if (this->state() != QSslSocket::UnconnectedState)
    this->abort();
  if (params.conn_type == MgrClientParameters::CONN_AUTO)
    socket_initiate_reconnect();
}

//---------------------------------------------------------------------------
void MgrClientConnection::socket_idle_timeout()
{
  emit state_changed(MgrClientConnection::MGR_NONE);
  log(LOG_DBG2, QString(": idle timeout"));
  if (this->state() == QSslSocket::ConnectedState)
  {
    sendPacket(CMD_CLOSE);
//    disconnectFromHost();
  }
}

//---------------------------------------------------------------------------
void MgrClientConnection::socket_phase_timeout()
{
  emit state_changed(MgrClientConnection::MGR_ERROR);
  emit connection_error(QAbstractSocket::SocketTimeoutError);
  log(LOG_DBG2, QString(": phase %4 timed out").arg((int)phase));
  if (this->state() == QSslSocket::ConnectedState)
    disconnectFromHost();
}

//---------------------------------------------------------------------------
void MgrClientConnection::socket_encrypted()
{
  timer_phase->stop();

  log(LOG_DBG3, QString(": SSL handshake established"
                        " (Protocol:%1, Cipher:%2, Auth:%3, Encryption:%4, KeyExchange:%5)")
      .arg(this->sessionCipher().protocolString())
      .arg(this->sessionCipher().name())
      .arg(this->sessionCipher().authenticationMethod())
      .arg(this->sessionCipher().encryptionMethod())
      .arg(this->sessionCipher().keyExchangeMethod()));
  emit state_changed(MgrClientConnection::MGR_AUTH);
  phase = PHASE_AUTH;
  timer_phase->setInterval(PHASE_AUTH_TIMEOUT);
  timer_phase->start();
  if (direction == OUTGOING)
  {
    if (!params.server_ssl_cert.isNull() || !params.server_ssl_cert_filename.isEmpty())
    {
      QSslCertificate cert = params.server_ssl_cert;
      if (cert.isNull())
        cert = loadSSLCertFromFile(params.server_ssl_cert_filename);
      // check server SSL certificate
      if (!cert.isNull())
      {
        if (cert != peerCertificate())
        {
          this->log(LOG_DBG1, QString(": server certificate does not match"));
          emit state_changed(MgrClientConnection::MGR_ERROR);
          emit connection_error(QAbstractSocket::ProxyConnectionClosedError);
          this->abort();
        }
      }
    }
    socket_send_auth();
  }
}

//---------------------------------------------------------------------------
void MgrClientConnection::socket_startOperational()
{
  if (direction == OUTGOING)
    timer_reconnect->setInterval(0);
  timer_phase->stop();
  log(LOG_DBG3, QString(": authentication completed successfully"));
  phase = MgrClientConnection::PHASE_OPERATIONAL;
  emit state_changed(MgrClientConnection::MGR_CONNECTED);
  if (params.flags & MgrClientParameters::FL_ENABLE_HEARTBEATS)
  {
    if (params.ping_interval > 0)
    {
      timer_heartbeat->setInterval(params.ping_interval);
      timer_heartbeat->start();
    }
    if (params.rcv_timeout > 0)
    {
      timer_heartbeat_check->setInterval(params.rcv_timeout);
      timer_heartbeat_check->start();
    }
    sendPacket(CMD_HEARTBEAT_REQ);
    t_last_heartbeat_req.restart();
  }
  if (direction == OUTGOING && params.conn_type == MgrClientParameters::CONN_DEMAND)
  {
    if (params.idle_timeout > 0)
    {
      timer_idle->setInterval(params.idle_timeout);
    }
  }
}

//---------------------------------------------------------------------------
void MgrClientConnection::socket_send_auth()
{
  MgrPacket_AuthReq packet_auth_req;
  packet_auth_req.flags = params.flags;
  packet_auth_req.hostname_len = SysUtil::machine_name.toUtf8().length();
  packet_auth_req.username_len = params.auth_username.toUtf8().length();
  packet_auth_req.password_len = params.auth_password.toUtf8().length();
  packet_auth_req.ping_interval = params.ping_interval;

  QByteArray data((const char *)&packet_auth_req, sizeof(MgrPacket_AuthReq));
  if (SysUtil::machine_name.length() > 0)
    data.append(SysUtil::machine_name.toUtf8());
  if (params.auth_username.length() > 0)
    data.append(params.auth_username.toUtf8());
  if (params.auth_password.length() > 0)
    data.append(params.auth_password.toUtf8());

  sendPacket(CMD_AUTH_REQ, data);
}

//---------------------------------------------------------------------------
void MgrClientConnection::socket_connected()
{
  if (direction == OUTGOING && timer_connect && timer_connect->isActive())
    timer_connect->stop();
  input_buffer.clear();
  output_buffer.clear();
  closing_by_cmd_close = false;
  bytes_rcv = 0;
  bytes_snd = 0;
  bytes_rcv_encrypted = 0;
  bytes_snd_encrypted = 0;
  latency = 999999;
  t_connected.restart();
  if (direction == OUTGOING)
  {
    log(LOG_DBG1, QString(": connected"));
    setSocketOption(QSslSocket::KeepAliveOption, (params.flags & MgrClientParameters::FL_TCP_KEEP_ALIVE) ? 1 : 0);
    setSocketOption(QSslSocket::LowDelayOption, (params.flags & MgrClientParameters::FL_TCP_NO_DELAY) ? 1 : 0);
    setReadBufferSize(params.read_buffer_size);
  }

  phase = PHASE_INIT;
  timer_phase->setInterval(PHASE_INIT_TIMEOUT);
  timer_phase->start();

  if (direction == OUTGOING)
  {
    if (params.flags & MgrClientParameters::FL_DISABLE_ENCRYPTION)
    {
      // sending encryption request and waiting for the same answer
      log(LOG_DBG4, QString(": requesting no encryption"));
      output_buffer.append(PHASE_INIT_DECRYPT_CMD);
      sendOutputBuffer();
      timer_phase->stop();
      // starting authentication
      emit state_changed(MgrClientConnection::MGR_AUTH);
      phase = PHASE_AUTH;
      timer_phase->setInterval(PHASE_AUTH_TIMEOUT);
      timer_phase->start();
      socket_send_auth();
    }
    else
    {
      // sending encryption request and waiting for the same answer
      output_buffer.append(PHASE_INIT_ENCRYPT_CMD);
      sendOutputBuffer();
      log(LOG_DBG4, QString(": requesting encryption"));
      if (!params.private_key.isNull())
        this->setPrivateKey(params.private_key);
      else if (!params.private_key_filename.isEmpty())
        this->setPrivateKey(params.private_key_filename, QSsl::Rsa, QSsl::Pem, private_key_passphrase.toUtf8());
      if (!params.ssl_cert.isNull())
        this->setLocalCertificate(params.ssl_cert);
      else if (!params.ssl_cert_filename.isEmpty())
        this->setLocalCertificate(params.ssl_cert_filename);
      this->setProtocol(params.ssl_protocol);
    }
  }
}

//---------------------------------------------------------------------------
void MgrClientConnection::socket_initiate_reconnect()
{
  closing_by_cmd_close = false;
  if (timer_connect->isActive())
    timer_connect->stop();
  if (timer_reconnect->interval() == 0 && params.reconnect_interval > 0)
    timer_reconnect->setInterval(params.reconnect_interval);
  else
  {
    if (params.reconnect_interval_increment > 0)
      timer_reconnect->setInterval(timer_reconnect->interval()+params.reconnect_interval_increment);
    else if (params.reconnect_interval_multiplicator > 1)
      timer_reconnect->setInterval((quint32)qCeil((double)qMax(timer_reconnect->interval(),1)*params.reconnect_interval_multiplicator));
    if (params.reconnect_interval_max > 0 && (quint32)timer_reconnect->interval() > params.reconnect_interval_max)
      timer_reconnect->setInterval(params.reconnect_interval_max);
  }
  timer_reconnect->start();
}

//---------------------------------------------------------------------------
void MgrClientConnection::socket_disconnected()
{
  if (timer_connect && timer_connect->isActive())
    timer_connect->stop();
  if (timer_phase->isActive())
    timer_phase->stop();
  if (timer_idle->isActive())
    timer_idle->stop();
  if (timer_heartbeat->isActive())
    timer_heartbeat->stop();
  if (timer_heartbeat_check->isActive())
    timer_heartbeat_check->stop();
  phase = PHASE_NONE;
  input_buffer.clear();
  output_buffer.clear();
  log(LOG_DBG1, QString(": disconnected"));
  if (direction == OUTGOING && params.conn_type == MgrClientParameters::CONN_AUTO)
    socket_initiate_reconnect();
  else if (direction == INCOMING)
    emit socket_finished();
}

//---------------------------------------------------------------------------
void MgrClientConnection::socket_error(QAbstractSocket::SocketError error)
{
  if (timer_connect && timer_connect->isActive())
    timer_connect->stop();
  log(LOG_DBG2, QString(" error: ")+this->errorString());
  emit state_changed(MgrClientConnection::MGR_ERROR);
  emit connection_error(error);
  if (this->state() != QSslSocket::UnconnectedState)
    this->abort();
  else if (direction == OUTGOING && params.conn_type == MgrClientParameters::CONN_AUTO)
    socket_initiate_reconnect();
}

//---------------------------------------------------------------------------
void MgrClientConnection::socket_sslError(const QList<QSslError> &errors)
{
  for (int i=0; i < errors.count(); i++)
  {
    if (errors[i].error() == QSslError::SelfSignedCertificate ||
        errors[i].error() == QSslError::SelfSignedCertificateInChain ||
        errors[i].error() == QSslError::HostNameMismatch ||
        errors[i].error() == QSslError::CertificateExpired ||
        errors[i].error() == QSslError::CertificateNotYetValid)
      continue;
    log(LOG_DBG2, QString(" SSL error: ")+errors[i].errorString());
    if (this->state() != QSslSocket::UnconnectedState)
    {
      emit state_changed(MgrClientConnection::MGR_ERROR);
      emit connection_error(QAbstractSocket::SslHandshakeFailedError);
      this->abort();
      return;
    }
  }
}

//---------------------------------------------------------------------------
void MgrClientConnection::socket_bytesWritten(qint64 bytes)
{
  log(LOG_DBG4, QString(": %1 bytes sent").arg(bytes));
  bytes_snd += bytes;
  emit stat_bytesSent(bytes);
  int bytes_to_write = bytesToWrite();
  if (bytes_to_write > 0 && output_buffer.length() > 0)
    log(LOG_DBG4, QString(": %1 bytes written (%2 more in internal buffer, %3 in output buffer)").arg(bytes).arg(bytes_to_write).arg(output_buffer.length()));
  else if (bytes_to_write > 0)
    log(LOG_DBG4, QString(": %1 bytes written (%2 more in internal buffer)").arg(bytes).arg(bytes_to_write));
  else if (output_buffer.length() > 0)
    log(LOG_DBG4, QString(": %1 bytes written (%3 more in output buffer)").arg(bytes).arg(output_buffer.length()));
  else
    log(LOG_DBG4, QString(": %1 bytes written").arg(bytes));
  t_last_snd.restart();
  if (timer_heartbeat->isActive())
    timer_heartbeat->start();
  if (!output_buffer.isEmpty())
    sendOutputBuffer();
  else if (bytes_to_write == 0 && receivers(SIGNAL(output_buffer_empty())) > 0)
    emit output_buffer_empty();
}

//---------------------------------------------------------------------------
void MgrClientConnection::socket_encryptedBytesWritten(qint64 bytes)
{
  emit stat_bytesSentEncrypted(bytes);
  bytes_snd_encrypted += bytes;
  t_last_snd.restart();
  if (timer_heartbeat->isActive())
    timer_heartbeat->start();
}

//-----------------------------------------------------------------------------
void MgrClientConnection::sendOutputBuffer()
{
  quint64 bytes_to_write;
  if (mode() == QSslSocket::UnencryptedMode)
    bytes_to_write = bytesToWrite();
  else
    bytes_to_write = encryptedBytesToWrite();
  int max_new_bytes_to_write = output_buffer.length();
  if (params.write_buffer_size > 0 && bytes_to_write+max_new_bytes_to_write > params.write_buffer_size)
    max_new_bytes_to_write = params.write_buffer_size-bytes_to_write;
  if (max_new_bytes_to_write <= 0)
    return;

  int send_len = this->write(output_buffer, max_new_bytes_to_write);
  if (send_len > 0)
  {
    output_buffer.remove(0, send_len);
    t_last_snd.restart();
  }
  if (timer_heartbeat->isActive())
    timer_heartbeat->start();
}

//---------------------------------------------------------------------------
QByteArray MgrClientConnection::socket_read(quint64 max_len)
{
  QByteArray data = this->read(max_len);
  if (prc_log_level >= LOG_DBG4)
  {
    qint64 bytes_avail = bytesAvailable();
    if (bytes_avail == 0)
      log(LOG_DBG4, QString(": %1 bytes received").arg(data.length()));
    else
      log(LOG_DBG4, QString(": %1 bytes received (%2 more available)").arg(data.length()).arg(bytes_avail));
  }
  bytes_rcv += data.length();
  emit stat_bytesReceived(data.length());
  return data;
}

//---------------------------------------------------------------------------
void MgrClientConnection::socket_readyRead()
{
  t_last_rcv.restart();
  if (timer_heartbeat_check->isActive())
    timer_heartbeat_check->start();
  // check minimum read buffer size
  if (phase == PHASE_INIT && input_buffer.length()+bytesAvailable() < (int)strlen(PHASE_INIT_ENCRYPT_CMD))
    return;

  if (params.read_buffer_size > 0 && (quint32)input_buffer.length() >= params.read_buffer_size)
  {
    // input buffer overflows, slow down and reschedule
    QTimer::singleShot(5, this, SLOT(socket_readyRead()));
    return;
  }

  // allow client to start SSL handshake phase
  if (phase == PHASE_INIT)
  {
    int len = strlen(PHASE_INIT_ENCRYPT_CMD)-input_buffer.length();
    input_buffer.append(socket_read(len));
    if (input_buffer == QByteArray(PHASE_INIT_ENCRYPT_CMD))
    {
      input_buffer.clear();
      timer_phase->stop();
      phase = PHASE_SSL_HANDSHAKE;
      log(LOG_DBG3, QString(": starting SSL handshake"));
      if (direction == INCOMING)
      {
        // sending encryption request and waiting for the same answer
        output_buffer.append(PHASE_INIT_ENCRYPT_CMD);
        sendOutputBuffer();
      }
      timer_phase->setInterval(PHASE_SSL_HANDSHAKE_TIMEOUT);
      timer_phase->start();
      if (direction == INCOMING)
        startServerEncryption();
      else
        startClientEncryption();
      return;
    }
    else if (input_buffer == QByteArray(PHASE_INIT_DECRYPT_CMD) && direction == INCOMING)
    {
      input_buffer.clear();
      timer_phase->stop();
      emit state_changed(MgrClientConnection::MGR_AUTH);
      phase = PHASE_AUTH;
      log(LOG_DBG3, QString(": starting authentication"));
      timer_phase->setInterval(PHASE_AUTH_TIMEOUT);
      timer_phase->start();
    }
  }
  // we don't want to mess with SSL handshake, so ignore all data received in this phase
  // and let it happen in QSslSocket
  if (phase == PHASE_SSL_HANDSHAKE)
  {
    bytes_rcv += bytesAvailable();
    bytes_rcv_encrypted += encryptedBytesAvailable();
    return;
  }
  int bytes_to_read = max_bytes_to_read_at_once > 0 ? max_bytes_to_read_at_once : bytesAvailable();
  if (params.read_buffer_size > 0 && (quint32)input_buffer.length()+bytes_to_read > params.read_buffer_size)
    bytes_to_read = params.read_buffer_size-input_buffer.length();
  QByteArray buffer = this->socket_read(bytes_to_read);
  input_buffer.append(buffer);
  parseInputBuffer();
  if (((mode() == QSslSocket::UnencryptedMode) ? bytesAvailable() : encryptedBytesAvailable()) > 0)
  {
    // have more data? Schedule to read later. Give other connections a chance!
    QTimer::singleShot(0, this, SLOT(socket_readyRead()));
  }
}

//---------------------------------------------------------------------------
void MgrClientConnection::parseInputBuffer()
{
  if (phase == PHASE_INIT)
  {
    if (receivers(SIGNAL(init_inputParsing())) > 0)
      emit init_inputParsing();
    else
      input_buffer.clear();
    return;
  }

  QTime t_processing;
  t_processing.start();
  int pos = 0;
  while (input_buffer.length() >= pos+MGR_PACKET_HEADER_LEN)
  {
    if ((unsigned int)qAbs(t_processing.elapsed()) > max_processing_time_ms)
    {
      // parsing is taking too long? Give other connections a chance!
      QTimer::singleShot(0, this, SLOT(parseInputBuffer()));
      break;
    }
    MgrPacketCmd orig_cmd = *(MgrPacketCmd *)(input_buffer.data()+pos);
    MgrPacketLen orig_len = *(MgrPacketLen *)(input_buffer.data()+pos+sizeof(MgrPacketCmd));

    if (orig_len > MGR_PACKET_MAX_LEN)
    {
      log(LOG_DBG3, QString(": received packet with incorrect len (%1) - dropping").arg(orig_len));
      input_buffer.clear();
      emit state_changed(MgrClientConnection::MGR_ERROR);
      emit connection_error(QAbstractSocket::ProxyProtocolError);
      this->abort();
      return;
    }
    MgrPacketCmd cmd = orig_cmd & ~MGR_PACKET_FLAG_COMPRESSED;
    if (cmd >= CMD_MAX)
    {
      log(LOG_DBG3, QString(": received packet with incorrect cmd - dropping"));
      input_buffer.clear();
      emit state_changed(MgrClientConnection::MGR_ERROR);
      emit connection_error(QAbstractSocket::ProxyProtocolError);
      this->abort();
      return;
    }
    // if we have not received all packet data yet - exit
    if (input_buffer.length() < pos+MGR_PACKET_HEADER_LEN+(int)orig_len)
      break;

    if (cmd > CMD_MAX_INTERNAL)
    {
      QByteArray data;
      if (orig_cmd & MGR_PACKET_FLAG_COMPRESSED)
        data = qUncompress(input_buffer.mid(pos+MGR_PACKET_HEADER_LEN,orig_len));
      else
        data = input_buffer.mid(pos+MGR_PACKET_HEADER_LEN,orig_len);
      log(LOG_DBG4, QString(": received packet cmd=%1, len=%2").arg(mgrPacket_cmdString(cmd)).arg(orig_len));
      emit packetReceived(cmd, data);
      if (this->state() != QAbstractSocket::ConnectedState)
      {
        input_buffer.clear();
        return;
      }
    }
    else if (cmd == CMD_HEARTBEAT_REQ)
      sendPacket(CMD_HEARTBEAT_REP);
    else if (cmd == CMD_HEARTBEAT_REP)
    {
      if (latency != (unsigned int)qAbs(t_last_heartbeat_req.elapsed()))
      {
        latency = qAbs(t_last_heartbeat_req.elapsed());
        emit latency_changed(latency);
      }
    }
    else if (cmd == CMD_CLOSE)
    {
      closing_by_cmd_close = true;
      this->abort();
    }

    pos += MGR_PACKET_HEADER_LEN+orig_len;
  }
  if (pos >= input_buffer.length())
    input_buffer.clear();
  else if (pos > 0)
    input_buffer.remove(0, pos);
}

//---------------------------------------------------------------------------
void MgrClientConnection::log(LogPriority prio, const QString &text)
{
  if (!params.name.isEmpty())
    prc_log(prio, log_prefix+QString("Connection with %1 (%2:%3)").arg(params.name).arg(params.host).arg(params.port)+text);
  else
    prc_log(prio, log_prefix+QString("Connection with %1:%2").arg(params.host).arg(params.port)+text);
}

//---------------------------------------------------------------------------
bool MgrClientConnection::sendPacket(MgrPacketCmd _cmd, const QByteArray &_data)
{
  if (!(this->phase == PHASE_OPERATIONAL || (this->phase == PHASE_AUTH && (_cmd == CMD_AUTH_REQ || _cmd == CMD_AUTH_REP))))
    return false;

  MgrPacketLen len = _data.length();
  if (params.max_io_buffer_size > 0 && output_buffer.length()+len > params.max_io_buffer_size)
  {
    log(LOG_DBG1, QString("output buffer overflow - dropping"));
    output_buffer.clear();
    emit state_changed(MgrClientConnection::MGR_ERROR);
    emit connection_error(QAbstractSocket::ProxyProtocolError);
    this->abort();
    return false;
  }
  bool compressed = false;
  if ((params.flags & MgrClientParameters::FL_USE_COMPRESSION) && len >= MGR_PACKET_MIN_LEN_FOR_COMPRESSION)
  {
    QByteArray data = qCompress(_data);
    if (data.length() < _data.length())
    {
      MgrPacketLen cmd = _cmd | MGR_PACKET_FLAG_COMPRESSED;
      len = data.length();
      if (len > MGR_PACKET_MAX_LEN)
        return false;
      output_buffer.reserve(output_buffer.length()+sizeof(MgrPacketCmd)+sizeof(MgrPacketLen)+data.length());
      output_buffer.append(QByteArray((const char *)&cmd, sizeof(MgrPacketCmd)));
      output_buffer.append(QByteArray((const char *)&len, sizeof(MgrPacketLen)));
      output_buffer.append(data);
      compressed = true;
    }
  }
  if (!compressed)
  {
    if (len > MGR_PACKET_MAX_LEN)
      return false;
    output_buffer.reserve(output_buffer.length()+sizeof(MgrPacketCmd)+sizeof(MgrPacketLen)+_data.length());
    output_buffer.append(QByteArray((const char *)&_cmd, sizeof(MgrPacketCmd)));
    output_buffer.append(QByteArray((const char *)&len, sizeof(MgrPacketLen)));
    output_buffer.append(_data);
  }
  if ((_cmd != CMD_HEARTBEAT_REQ && _cmd != CMD_HEARTBEAT_REP) || prc_log_level >= LOG_DBG4)
    log(LOG_DBG4, QString(": sending packet cmd=%1, len=%2").arg(mgrPacket_cmdString(_cmd)).arg(len));
  sendOutputBuffer();
  return true;
}

//---------------------------------------------------------------------------
void MgrClientConnection::socket_heartbeat()
{
//  if ((unsigned int)qAbs(t_last_snd.elapsed()) >= params.ping_interval)
  sendPacket(CMD_HEARTBEAT_REQ, QByteArray());
  t_last_heartbeat_req.restart();
}

//---------------------------------------------------------------------------
void MgrClientConnection::socket_heartbeat_check()
{
  log(LOG_DBG3, QString(": no heartbeat received in %1 s").arg((double)params.rcv_timeout/1000, 0, 'f', 1));
  emit state_changed(MgrClientConnection::MGR_ERROR);
  emit connection_error(QAbstractSocket::SocketTimeoutError);
  this->abort();
}
