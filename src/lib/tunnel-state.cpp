/*
 Copyright (C) 2017 Nikolay N. Karikh <knn@qmtunnel.com>

 This file is part of qmtunnel and is licensed under GNU General Public License 3.0, with
 the additional special exception to link portions of this program with the OpenSSL library.
 See LICENSE file for more details.
*/

#include "tunnel-state.h"
#include "../lib/prc_log.h"

//---------------------------------------------------------------------------
int TunnelConnInInfo::parseFromBuffer(const char *buffer, int buf_size)
{
  qint64 msecs;
  quint8 fld_len;

  if ((unsigned int)buf_size < sizeof(bytes_rcv)
                +sizeof(bytes_snd)
                +sizeof(msecs)
                +sizeof(msecs)
                +sizeof(peer_port)
                +sizeof(tun_out_port)
                +sizeof(flags)
                +sizeof(fld_len)
                +sizeof(fld_len)
                +sizeof(fld_len))
    return -1;

  int data_pos = 0;

  bytes_rcv = *((quint64 *)(buffer+data_pos));
  data_pos += sizeof(bytes_rcv);

  bytes_snd = *((quint64 *)(buffer+data_pos));
  data_pos += sizeof(bytes_snd);

  msecs = *((qint64 *)(buffer+data_pos));
  t_connected = QDateTime::fromMSecsSinceEpoch(msecs);
  data_pos += sizeof(msecs);

  msecs = *((qint64 *)(buffer+data_pos));
  if (msecs != 0)
    t_disconnected = QDateTime::fromMSecsSinceEpoch(msecs);
  else
    t_disconnected = QDateTime();
  data_pos += sizeof(msecs);

  peer_port = *((quint16 *)(buffer+data_pos));
  data_pos += sizeof(peer_port);

  tun_out_port = *((quint16 *)(buffer+data_pos));
  data_pos += sizeof(tun_out_port);

  flags = *((quint8 *)(buffer+data_pos));
  data_pos += sizeof(flags);

  fld_len = *((quint8 *)(buffer+data_pos));
  data_pos += sizeof(fld_len);
  if (data_pos+fld_len > buf_size)
    return -1;
  if (fld_len == sizeof(Q_IPV6ADDR))
    peer_address = QHostAddress(*((Q_IPV6ADDR *)(buffer+data_pos)));
  else if (fld_len == sizeof(quint32))
    peer_address = QHostAddress(*((quint32 *)(buffer+data_pos)));
  else
    peer_address = QHostAddress();
  data_pos += fld_len;

  fld_len = *((quint8 *)(buffer+data_pos));
  data_pos += sizeof(fld_len);
  if (data_pos+fld_len > buf_size)
    return -1;
  if (fld_len > 0)
    peer_name = QByteArray(buffer+data_pos, fld_len);
  else
    peer_name.clear();
  data_pos += fld_len;

  fld_len = *((quint8 *)(buffer+data_pos));
  data_pos += sizeof(fld_len);
  if (data_pos+fld_len > buf_size)
    return -1;
  if (fld_len > 0)
    errorstring = QByteArray(buffer+data_pos, fld_len);
  else
    errorstring.clear();
  data_pos += fld_len;

  return data_pos;
}

//---------------------------------------------------------------------------
QByteArray TunnelConnInInfo::printToBuffer() const
{
  QByteArray buffer;
  qint64 msecs;
  quint8 fld_len;

  buffer.reserve(sizeof(TunnelConnInInfo)+16+peer_name.length()+errorstring.length());

  buffer.append((const char *)&bytes_rcv, sizeof(bytes_rcv));
  buffer.append((const char *)&bytes_snd, sizeof(bytes_snd));
  msecs = t_connected.toMSecsSinceEpoch();
  buffer.append((const char *)&msecs, sizeof(msecs));
  if (t_disconnected.isValid())
    msecs = t_disconnected.toMSecsSinceEpoch();
  else
    msecs = 0;
  buffer.append((const char *)&msecs, sizeof(msecs));
  buffer.append((const char *)&peer_port, sizeof(peer_port));
  buffer.append((const char *)&tun_out_port, sizeof(tun_out_port));
  buffer.append((const char *)&flags, sizeof(flags));
  if (peer_address.protocol() == QAbstractSocket::IPv6Protocol)
  {
    Q_IPV6ADDR ipv6 = peer_address.toIPv6Address();
    fld_len = sizeof(ipv6);
    buffer.append((const char *)&fld_len, sizeof(fld_len));
    buffer.append((const char *)&ipv6, fld_len);
  }
  else
  {
    quint32 ipv4 = peer_address.toIPv4Address();
    fld_len = sizeof(ipv4);
    buffer.append((const char *)&fld_len, sizeof(fld_len));
    buffer.append((const char *)&ipv4, fld_len);
  }
  fld_len = peer_name.length();
  buffer.append((const char *)&fld_len, sizeof(fld_len));
  buffer.append((const char *)peer_name.constData(), fld_len);
  fld_len = errorstring.length();
  buffer.append((const char *)&fld_len, sizeof(fld_len));
  buffer.append((const char *)errorstring.constData(), fld_len);

  return buffer;
}

//---------------------------------------------------------------------------
bool TunnelState::parseFromBuffer(const char *buffer, unsigned int buf_size)
{
  quint16 error_str_len = 0;
  if (buf_size < sizeof(flags)
                +sizeof(last_error_code)
                +sizeof(error_str_len)
                +sizeof(latency_ms))
    return false;

  unsigned int data_pos = 0;
  flags = *((quint64 *)(buffer+data_pos));
  data_pos += sizeof(quint64);
  last_error_code = *((quint16 *)(buffer+data_pos));
  data_pos += sizeof(quint16);
  error_str_len = *((quint16 *)(buffer+data_pos));
  data_pos += sizeof(error_str_len);
  latency_ms = *((qint32 *)(buffer+data_pos));
  data_pos += sizeof(qint32);
  if (error_str_len > 0 && buf_size >= data_pos+error_str_len)
  {
    last_error_str = QString::fromUtf8(buffer+data_pos, error_str_len);
    data_pos += error_str_len;
  }
  if (buf_size >= data_pos+sizeof(TunnelStatistics))
  {
    stats = *((TunnelStatistics *)(buffer+data_pos));
    data_pos += sizeof(TunnelStatistics);
  }

  return true;
}

//---------------------------------------------------------------------------
QByteArray TunnelState::printToBuffer(bool include_stat) const
{
  QByteArray error_str = last_error_str.toUtf8();
  quint16 error_str_len = error_str.length();
  QByteArray buffer;
  unsigned int buffer_len = sizeof(flags)
                            +sizeof(last_error_code)
                            +sizeof(error_str_len)
                            +sizeof(latency_ms);
  if (include_stat)
    buffer_len += sizeof(TunnelStatistics);
  buffer.reserve(buffer_len);
  buffer.append((const char *)&flags, sizeof(flags));
  buffer.append((const char *)&last_error_code, sizeof(last_error_code));
  buffer.append((const char *)&error_str_len, sizeof(error_str_len));
  buffer.append((const char *)&latency_ms, sizeof(latency_ms));
  if (error_str_len > 0)
    buffer.append(error_str.constData(), error_str_len);
  if (include_stat)
    buffer.append((const char *)&stats, sizeof(TunnelStatistics));
  return buffer;
}
