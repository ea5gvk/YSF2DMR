/*
*   Copyright (C) 2016,2017 by Jonathan Naylor G4KLX
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

#include "YSF2DMR.h"

#if defined(_WIN32) || defined(_WIN64)
#include <Windows.h>
#else
#include <sys/time.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <pwd.h>
#endif

#if defined(_WIN32) || defined(_WIN64)
const char* DEFAULT_INI_FILE = "YSF2DMR.ini";
#else
const char* DEFAULT_INI_FILE = "/etc/YSF2DMR.ini";
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <clocale>

const unsigned char ysfNF[] = {
0x59,0x53,0x46,0x44,0x43,0x41,0x36,0x4A,0x41,0x55,0x20,0x20,0x20,0x20,0x43,0x41,
0x36,0x4A,0x41,0x55,0x20,0x20,0x20,0x20,0x41,0x4C,0x4C,0x20,0x20,0x20,0x20,0x20,
0x20,0x20,0x46};

CAMBEFEC test;

#define COLOR_CODE	1U
#define SRC_ID		7306007U
#define DST_ID		9990U
#define FLCO_LC		FLCO_USER_USER
//#define FLCO_LC		FLCO_GROUP

int main(int argc, char** argv)
{
	const char* iniFile = DEFAULT_INI_FILE;
	if (argc > 1) {
		for (int currentArg = 1; currentArg < argc; ++currentArg) {
			std::string arg = argv[currentArg];
			if ((arg == "-v") || (arg == "--version")) {
				::fprintf(stdout, "YSF2DMR version %s\n", VERSION);
				return 0;
			} else if (arg.substr(0, 1) == "-") {
				::fprintf(stderr, "Usage: YSF2DMR [-v|--version] [filename]\n");
				return 1;
			} else {
				iniFile = argv[currentArg];
			}
		}
	}

	CYSF2DMR* gateway = new CYSF2DMR(std::string(iniFile));

	int ret = gateway->run();

	delete gateway;

	return ret;
}

CYSF2DMR::CYSF2DMR(const std::string& configFile) :
m_callsign(),
m_suffix(),
m_conf(configFile),
m_dmrNetwork(NULL)
{
	::memset(m_ysfFrame, 0U, 200U);
	::memset(m_dmrFrame, 0U, 50U);
	::memcpy(m_ysfFrame, ysfNF, 35U);
}

CYSF2DMR::~CYSF2DMR()
{
}

int CYSF2DMR::run()
{
	bool ret = m_conf.read();
	if (!ret) {
		::fprintf(stderr, "YSF2DMR: cannot read the .ini file\n");
		return 1;
	}

	setlocale(LC_ALL, "C");

	ret = ::LogInitialise(m_conf.getLogFilePath(), m_conf.getLogFileRoot(), m_conf.getLogFileLevel(), m_conf.getLogDisplayLevel());
	if (!ret) {
		::fprintf(stderr, "YSF2DMR: unable to open the log file\n");
		return 1;
	}

#if !defined(_WIN32) && !defined(_WIN64)
	bool m_daemon = m_conf.getDaemon();
	if (m_daemon) {
		// Create new process
		pid_t pid = ::fork();
		if (pid == -1) {
			::LogWarning("Couldn't fork() , exiting");
			return -1;
		} else if (pid != 0)
			exit(EXIT_SUCCESS);

		// Create new session and process group
		if (::setsid() == -1) {
			::LogWarning("Couldn't setsid(), exiting");
			return -1;
		}

		// Set the working directory to the root directory
		if (::chdir("/") == -1) {
			::LogWarning("Couldn't cd /, exiting");
			return -1;
		}

		::close(STDIN_FILENO);
		::close(STDOUT_FILENO);
		::close(STDERR_FILENO);

		//If we are currently root...
		if (getuid() == 0) {
			struct passwd* user = ::getpwnam("mmdvm");
			if (user == NULL) {
				::LogError("Could not get the mmdvm user, exiting");
				return -1;
			}

			uid_t mmdvm_uid = user->pw_uid;
			gid_t mmdvm_gid = user->pw_gid;

			//Set user and group ID's to mmdvm:mmdvm
			if (setgid(mmdvm_gid) != 0) {
				::LogWarning("Could not set mmdvm GID, exiting");
				return -1;
			}

			if (setuid(mmdvm_uid) != 0) {
				::LogWarning("Could not set mmdvm UID, exiting");
				return -1;
			}

			//Double check it worked (AKA Paranoia) 
			if (setuid(0) != -1) {
				::LogWarning("It's possible to regain root - something is wrong!, exiting");
				return -1;
			}
		}
	}
#endif

	m_callsign = m_conf.getCallsign();
	m_suffix   = m_conf.getSuffix();

	bool debug            = m_conf.getDMRNetworkDebug();
	in_addr rptAddress    = CUDPSocket::lookup(m_conf.getRptAddress());
	unsigned int rptPort  = m_conf.getRptPort();
	std::string myAddress = m_conf.getMyAddress();
	unsigned int myPort   = m_conf.getMyPort();

	CNetwork rptNetwork(myAddress, myPort, m_callsign, debug);
	rptNetwork.setDestination(rptAddress, rptPort);

	ret = rptNetwork.open();
	if (!ret) {
		::LogError("Cannot open the repeater network port");
		::LogFinalise();
		return 1;
	}

	ret = createDMRNetwork();
	if (!ret) {
		::LogError("Cannot open DMR Network");
		::LogFinalise();
		return 1;
	}

	CTimer networkWatchdog(100U, 0U, 1500U);

	bool networkEnabled = m_conf.getDMRNetworkEnabled();

	CStopWatch stopWatch;
	CStopWatch ysfWatch;
	CStopWatch dmrWatch;
	stopWatch.start();
	ysfWatch.start();
	dmrWatch.start();
	unsigned char ysf_cnt = 0;
	unsigned char dmr_cnt = 0;
	CDMREmbeddedData	m_EmbeddedLC;

	LogMessage("Starting YSF2DMR-%s", VERSION);

	for (;;) {
		unsigned char buffer[2000U];
		CDMRData tx_dmrdata;
		unsigned int ms = stopWatch.elapsed();

		while (rptNetwork.read(buffer) > 0U) {
			if (::memcmp(buffer, "YSFD", 4U) == 0) {
				CYSFFICH fich;

				bool valid = fich.decode(buffer + 35U);
				if (valid) {
					unsigned char fi = fich.getFI();
					unsigned char cs = fich.getCS();
					unsigned char dev = fich.getDev();
					unsigned char mr = fich.getMR();
					unsigned char sql = fich.getSQL();
					unsigned char sq = fich.getSQ();
					unsigned char dt = fich.getDT();
					unsigned char fn = fich.getFN();
					unsigned char ft = fich.getFT();

					LogMessage("RX YSF: FI:%d CS:%d DEV:%d MR:%d SQL:%d SQ:%d DT:%d FN:%d FT:%d", fi, cs, dev, mr, sql, sq, dt, fn, ft);

					test.regenerateYSFVDT2(buffer + 35U);
				}

			}
		}

		if (dmrWatch.elapsed() > 55U)
			if(test.getDMR(m_dmrFrame)) {
				CDMREMB emb;
				CDMRData rx_dmrdata;
				unsigned int n_dmr = dmr_cnt % 6U;

				rx_dmrdata.setSlotNo(2U);
				rx_dmrdata.setSrcId(SRC_ID);
				rx_dmrdata.setDstId(DST_ID);
				rx_dmrdata.setFLCO(FLCO_LC);
				rx_dmrdata.setN(n_dmr);
				rx_dmrdata.setSeqNo(dmr_cnt);
				rx_dmrdata.setBER(0U);
				rx_dmrdata.setRSSI(0U);
			
				if (!n_dmr) {
					rx_dmrdata.setDataType(DT_VOICE_SYNC);
					// Add sync
					CSync::addDMRAudioSync(m_dmrFrame, 0U);
					// Prepare Full LC data
					CDMRLC dmrLC = CDMRLC(FLCO_LC, SRC_ID, DST_ID);
					// Configure the Embedded LC
					m_EmbeddedLC.setLC(dmrLC);
				}
				else {
					rx_dmrdata.setDataType(DT_VOICE);
					// Generate the Embedded LC
					unsigned char lcss = m_EmbeddedLC.getData(m_dmrFrame, n_dmr);
					// Generate the EMB
					emb.setColorCode(COLOR_CODE);
					emb.setLCSS(lcss);
					emb.getData(m_dmrFrame);
				}

				rx_dmrdata.setData(m_dmrFrame);
				
				//CUtils::dump(1U, "DMR data:", m_dmrFrame, 33U);
				m_dmrNetwork->write(rx_dmrdata);

				dmr_cnt++;
				dmrWatch.start();
			}
		
		while (m_dmrNetwork->read(tx_dmrdata) > 0) {
			if (networkEnabled) { 
		
				unsigned int slotNo = tx_dmrdata.getSlotNo();
				unsigned int SrcId = tx_dmrdata.getSrcId();
				unsigned int DstId = tx_dmrdata.getDstId();
				//FLCO flco_dat = tx_dmrdata.getFLCO();
				unsigned char N = tx_dmrdata.getN();
				unsigned char SeqNo = tx_dmrdata.getSeqNo();
				unsigned char DataType = tx_dmrdata.getDataType();
				unsigned char BER = tx_dmrdata.getBER();
				unsigned char RSSI = tx_dmrdata.getRSSI();
		
				if (!tx_dmrdata.isMissing()) {
					networkWatchdog.start();
				
					LogMessage("DMR Net recv: slotNo:%d, SrcId:%d, DstId:%d, N:%d, SeqNo:%d, DataType:%d, BER:%d, RSSI:%d", slotNo, SrcId, DstId, N, SeqNo, DataType, BER, RSSI);

					if(DataType == DT_VOICE_SYNC || DataType == DT_VOICE) {
						unsigned char dmr_frame[50];
						tx_dmrdata.getData(dmr_frame);
						test.regenerateDMR(dmr_frame); // Add DMR frame for YSF conversion
					}
				}
				else {
					if(DataType == DT_VOICE_SYNC || DataType == DT_VOICE) {
						unsigned char dmr_frame[50];
						tx_dmrdata.getData(dmr_frame);
						test.regenerateDMR(dmr_frame); // Add DMR frame for YSF conversion
					}
					
					networkWatchdog.clock(ms);
					if (networkWatchdog.hasExpired()) {
						LogDebug("Network watchdog has expired");
						m_dmrNetwork->reset(2U);
						networkWatchdog.stop();
					}
				}
			}
		}
		
		if (ysfWatch.elapsed() > 90U)
			if(test.getYSF(m_ysfFrame + 35U)) {
				CYSFFICH fich;

				// Add the YSF Sync
				CSync::addYSFSync(m_ysfFrame + 35U);
				
				// Set the FICH
				fich.setFI(1U);
				fich.setCS(2U);
				fich.setFN(ysf_cnt % 8U);
				fich.setFT(7U);
				fich.setDev(0U);
				fich.setMR(2U);
				fich.setDT(YSF_DT_VD_MODE2);
				fich.setSQL(0U);
				fich.setSQ(0U);
				fich.encode(m_ysfFrame + 35U);

				// Net frame counter
				m_ysfFrame[34U] = (ysf_cnt & 0x7FU) << 1;

				// Send data to MMDVMHost
				rptNetwork.write(m_ysfFrame);
				
				ysf_cnt++;
				ysfWatch.start();
			}

		stopWatch.start();

		rptNetwork.clock(ms);
		m_dmrNetwork->clock(ms);

		if (ms < 5U)
			CThread::sleep(5U);
	}

	rptNetwork.close();
	m_dmrNetwork->close();

	delete m_dmrNetwork;

	::LogFinalise();

	return 0;
}

bool CYSF2DMR::createDMRNetwork()
{
	std::string address  = m_conf.getDMRNetworkAddress();
	unsigned int port    = m_conf.getDMRNetworkPort();
	unsigned int local   = m_conf.getDMRNetworkLocal();
	unsigned int id      = m_conf.getDMRId();
	std::string password = m_conf.getDMRNetworkPassword();
	bool debug           = m_conf.getDMRNetworkDebug();
	unsigned int jitter  = m_conf.getDMRNetworkJitter();
	bool slot1           = m_conf.getDMRNetworkSlot1();
	bool slot2           = m_conf.getDMRNetworkSlot2();
	bool m_duplex        = false;
	HW_TYPE hwType       = HWT_MMDVM;
	
	LogMessage("DMR Network Parameters");
	LogMessage("    Address: %s", address.c_str());
	LogMessage("    Port: %u", port);
	if (local > 0U)
		LogMessage("    Local: %u", local);
	else
		LogMessage("    Local: random");
	LogMessage("    Jitter: %ums", jitter);
	LogMessage("    Slot 1: %s", slot1 ? "enabled" : "disabled");
	LogMessage("    Slot 2: %s", slot2 ? "enabled" : "disabled");

	m_dmrNetwork = new CDMRNetwork(address, port, local, id, password, m_duplex, VERSION, debug, slot1, slot2, hwType, jitter);

	std::string options = m_conf.getDMRNetworkOptions();
	if (!options.empty()) {
		LogMessage("    Options: %s", options.c_str());
		m_dmrNetwork->setOptions(options);
	}

	unsigned int rxFrequency = m_conf.getRxFrequency();
	unsigned int txFrequency = m_conf.getTxFrequency();
	unsigned int power       = m_conf.getPower();
	unsigned int colorCode   = m_conf.getDMRColorCode();
	float latitude           = m_conf.getLatitude();
	float longitude          = m_conf.getLongitude();
	int height               = m_conf.getHeight();
	std::string location     = m_conf.getLocation();
	std::string description  = m_conf.getDescription();
	std::string url          = m_conf.getURL();

	LogMessage("Info Parameters");
	LogMessage("    Callsign: %s", m_callsign.c_str());
	LogMessage("    RX Frequency: %uHz", rxFrequency);
	LogMessage("    TX Frequency: %uHz", txFrequency);
	LogMessage("    Power: %uW", power);
	LogMessage("    Latitude: %fdeg N", latitude);
	LogMessage("    Longitude: %fdeg E", longitude);
	LogMessage("    Height: %um", height);
	LogMessage("    Location: \"%s\"", location.c_str());
	LogMessage("    Description: \"%s\"", description.c_str());
	LogMessage("    URL: \"%s\"", url.c_str());

	m_dmrNetwork->setConfig(m_callsign, rxFrequency, txFrequency, power, colorCode, latitude, longitude, height, location, description, url);

	bool ret = m_dmrNetwork->open();
	if (!ret) {
		delete m_dmrNetwork;
		m_dmrNetwork = NULL;
		return false;
	}

	m_dmrNetwork->enable(true);

	return true;
}
