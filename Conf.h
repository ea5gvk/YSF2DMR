/*
 *   Copyright (C) 2015,2016,2017 by Jonathan Naylor G4KLX
 *   Copyright (C) 2018 by Andy Uribe CA6JAU
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#if !defined(CONF_H)
#define	CONF_H

#include <string>
#include <vector>

class CConf
{
public:
  CConf(const std::string& file);
  ~CConf();

  bool read();

  // The General section
  std::string  getCallsign() const;
  std::string  getSuffix() const;
  std::string  getRptAddress() const;
  unsigned int getRptPort() const;
  std::string  getMyAddress() const;
  unsigned int getMyPort() const;
  bool         getDaemon() const;

  // The Info section
  unsigned int getRxFrequency() const;
  unsigned int getTxFrequency() const;
  unsigned int getPower() const;
  float        getLatitude() const;
  float        getLongitude() const;
  int          getHeight() const;
  std::string  getLocation() const;
  std::string  getDescription() const;
  std::string  getURL() const;
  
  // The DMR Network section
  bool         getDMRNetworkEnabled() const;
  unsigned int getDMRId() const;
  unsigned int getDMRColorCode() const;
  std::string  getDMRNetworkAddress() const;
  unsigned int getDMRNetworkPort() const;
  unsigned int getDMRNetworkLocal() const;
  std::string  getDMRNetworkPassword() const;
  std::string  getDMRNetworkOptions() const;
  bool         getDMRNetworkDebug() const;
  unsigned int getDMRNetworkJitter() const;
  bool         getDMRNetworkSlot1() const;
  bool         getDMRNetworkSlot2() const;

  // The Log section
  unsigned int getLogDisplayLevel() const;
  unsigned int getLogFileLevel() const;
  std::string  getLogFilePath() const;
  std::string  getLogFileRoot() const;

private:
  std::string  m_file;
  std::string  m_callsign;
  std::string  m_suffix;
  std::string  m_rptAddress;
  unsigned int m_rptPort;
  std::string  m_myAddress;
  unsigned int m_myPort;
  bool         m_daemon;

  unsigned int m_rxFrequency;
  unsigned int m_txFrequency;
  unsigned int m_power;
  float        m_latitude;
  float        m_longitude;
  int          m_height;
  std::string  m_location;
  std::string  m_description;
  std::string  m_url;
  
  bool         m_dmrNetworkEnabled;
  unsigned int m_dmrId;
  unsigned int m_dmrColorCode;
  std::string  m_dmrNetworkAddress;
  unsigned int m_dmrNetworkPort;
  unsigned int m_dmrNetworkLocal;
  std::string  m_dmrNetworkPassword;
  std::string  m_dmrNetworkOptions;
  bool         m_dmrNetworkDebug;
  unsigned int m_dmrNetworkJitter;
  bool         m_dmrNetworkSlot1;
  bool         m_dmrNetworkSlot2;

  unsigned int m_logDisplayLevel;
  unsigned int m_logFileLevel;
  std::string  m_logFilePath;
  std::string  m_logFileRoot;

};

#endif
