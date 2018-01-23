/*
*   Copyright (C) 2016 by Jonathan Naylor G4KLX
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

#if !defined(YSF2DMR_H)
#define	YSF2DMR_H

#include "ModeConv.h"
#include "JitterBuffer.h"
#include "DMRNetwork.h"
#include "DMREmbeddedData.h"
#include "DMRLC.h"
#include "DMRFullLC.h"
#include "DMREMB.h"
#include "UDPSocket.h"
#include "StopWatch.h"
#include "Version.h"
#include "YSFPayload.h"
#include "Network.h"
#include "YSFFICH.h"
#include "Thread.h"
#include "Timer.h"
#include "Sync.h"
#include "Utils.h"
#include "Conf.h"
#include "Log.h"

#include <string>

class CYSF2DMR
{
public:
	CYSF2DMR(const std::string& configFile);
	~CYSF2DMR();

	int run();

private:
	std::string    m_callsign;
	CConf          m_conf;
	CDMRNetwork*   m_dmrNetwork;
	CNetwork*      m_ysfNetwork;
	CModeConv      m_conv;
	unsigned int   m_srcid;
	unsigned int   m_colorcode;
	unsigned int   m_dstid;
	bool           m_dmrpc;
	unsigned char  m_ysfFrame[200U];
	unsigned char  m_dmrFrame[50U];
	
	bool createDMRNetwork();
};

#endif
