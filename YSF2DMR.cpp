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

// Sample of DT1 and DT2 from my radio, GPS info + more data, I need to investigate this...
const unsigned char dt1_temp[] = {0x34, 0x22, 0x62, 0x5F, 0x24, 0x53, 0x39, 0x54, 0x38, 0x38};
const unsigned char dt2_temp[] = {0x52, 0x65, 0x2A, 0x3E, 0x6C, 0x22, 0x30, 0x20, 0x03, 0x8B};

const unsigned char dmrFrameSilence[] = {0xB9U, 0xE8U, 0x81U, 0x52U, 0x61U, 0x73U, 0x00U, 0x2AU, 0x6BU, 0xB9U, 0xE8U,
										0x81U, 0x52U, 0x60U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x01U, 0x73U, 0x00U,
										0x2AU, 0x6BU, 0xB9U, 0xE8U, 0x81U, 0x52U, 0x61U, 0x73U, 0x00U, 0x2AU, 0x6BU};

#define DMR_FRAME_PER       55U
#define YSF_FRAME_PER       90U

#if defined(_WIN32) || defined(_WIN64)
const char* DEFAULT_INI_FILE = "YSF2DMR.ini";
#else
const char* DEFAULT_INI_FILE = "/etc/YSF2DMR.ini";
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <clocale>

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
m_conf(configFile),
m_dmrNetwork(NULL),
m_dmrLastDT(0U)
{
	::memset(m_ysfFrame, 0U, 200U);
	::memset(m_dmrFrame, 0U, 50U);
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

	bool debug            = m_conf.getDMRNetworkDebug();
	in_addr dstAddress    = CUDPSocket::lookup(m_conf.getDstAddress());
	unsigned int dstPort  = m_conf.getDstPort();
	std::string localAddress = m_conf.getLocalAddress();
	unsigned int localPort   = m_conf.getLocalPort();

	m_ysfNetwork = new CYSFNetwork(localAddress, localPort, m_callsign, debug);
	m_ysfNetwork->setDestination(dstAddress, dstPort);

	ret = m_ysfNetwork->open();
	if (!ret) {
		::LogError("Cannot open the YSF network port");
		::LogFinalise();
		return 1;
	}

	ret = createDMRNetwork();
	if (!ret) {
		::LogError("Cannot open DMR Network");
		::LogFinalise();
		return 1;
	}
	
	std::string lookupFile  = m_conf.getDMRIdLookupFile();
	unsigned int reloadTime = m_conf.getDMRIdLookupTime();

	m_lookup = new CDMRLookup(lookupFile, reloadTime);
	m_lookup->read();

	FLCO dmrflco;
	if (m_dmrpc)
		dmrflco = FLCO_USER_USER;
	else
		dmrflco = FLCO_GROUP;

	CTimer networkWatchdog(100U, 0U, 1500U);
	CTimer pollTimer(1000U, 5U);

	CStopWatch stopWatch;
	CStopWatch ysfWatch;
	CStopWatch dmrWatch;
	stopWatch.start();
	ysfWatch.start();
	dmrWatch.start();
	unsigned char ysf_cnt = 0;
	unsigned char dmr_cnt = 0;

	CDMREmbeddedData m_EmbeddedLC;

	LogMessage("Starting YSF2DMR-%s", VERSION);

	for (;;) {
		unsigned char buffer[2000U];
		CDMRData tx_dmrdata;
		unsigned int ms = stopWatch.elapsed();

		while (m_ysfNetwork->read(buffer) > 0U) {
			if (::memcmp(buffer, "YSFD", 4U) == 0U) {
				CYSFFICH fich;

				bool valid = fich.decode(buffer + 35U);
				if (valid) {
					unsigned char fi = fich.getFI();
					unsigned char dt = fich.getDT();

					CYSFPayload ysfPayload;

					if (fi == YSF_FI_HEADER) {
						if (ysfPayload.processHeaderData(buffer + 35U)) {
							std::string ysfSrc = ysfPayload.getSource();
							std::string ysfDst = ysfPayload.getDest();
							LogMessage("Received YSF Header: Src: %s Dst: %s", ysfSrc.c_str(), ysfDst.c_str());
							m_srcid = findYSFID(ysfSrc);
							LogMessage("DMR DstID: %s %u", m_dmrpc ? "" : "TG", m_dstid);
							m_conv.putYSFHeader();
						}
					} else if (fi == YSF_FI_TERMINATOR) {
						LogMessage("Received YSF Terminator");
						m_conv.putYSFEOT();
					} else if (fi == YSF_FI_COMMUNICATIONS) {
						if (dt == YSF_DT_VD_MODE2)
							m_conv.putYSF(buffer + 35U);
						else if  (dt == YSF_DT_VD_MODE1)
							LogMessage("YSF Mode V/D Type 1 not supported yet");
					}
				}
			}
		}

		if (dmrWatch.elapsed() > DMR_FRAME_PER) {
			unsigned int dmrFrameType = m_conv.getDMR(m_dmrFrame);

			if(dmrFrameType == TAG_HEADER) {
				CDMRData rx_dmrdata;
				dmr_cnt = 0U;

				rx_dmrdata.setSlotNo(2U);
				rx_dmrdata.setSrcId(m_srcid);
				rx_dmrdata.setDstId(m_dstid);
				rx_dmrdata.setFLCO(dmrflco);
				rx_dmrdata.setN(0U);
				rx_dmrdata.setSeqNo(0U);
				rx_dmrdata.setBER(0U);
				rx_dmrdata.setRSSI(0U);
				rx_dmrdata.setDataType(DT_VOICE_LC_HEADER);

				// Add sync
				CSync::addDMRDataSync(m_dmrFrame, 0);

				// Add SlotType
				CDMRSlotType slotType;
				slotType.setColorCode(m_colorcode);
				slotType.setDataType(DT_VOICE_LC_HEADER);
				slotType.getData(m_dmrFrame);
	
				// Full LC
				CDMRLC dmrLC = CDMRLC(dmrflco, m_srcid, m_dstid);
				CDMRFullLC fullLC;
				fullLC.encode(dmrLC, m_dmrFrame, DT_VOICE_LC_HEADER);
				m_EmbeddedLC.setLC(dmrLC);
				
				rx_dmrdata.setData(m_dmrFrame);
				//CUtils::dump(1U, "DMR data:", m_dmrFrame, 33U);

				for (unsigned int i = 0U; i < 3U; i++) {
					rx_dmrdata.setSeqNo(dmr_cnt);
					m_dmrNetwork->write(rx_dmrdata);
					dmr_cnt++;
				}

				dmrWatch.start();
			}
			else if(dmrFrameType == TAG_EOT) {
				CDMRData rx_dmrdata;
				unsigned int n_dmr = (dmr_cnt - 3U) % 6U;
				unsigned int fill = (6U - n_dmr);
				
				if (n_dmr) {
					for (unsigned int i = 0U; i < fill; i++) {

						CDMREMB emb;
						CDMRData rx_dmrdata;

						rx_dmrdata.setSlotNo(2U);
						rx_dmrdata.setSrcId(m_srcid);
						rx_dmrdata.setDstId(m_dstid);
						rx_dmrdata.setFLCO(dmrflco);
						rx_dmrdata.setN(n_dmr);
						rx_dmrdata.setSeqNo(dmr_cnt);
						rx_dmrdata.setBER(0U);
						rx_dmrdata.setRSSI(0U);
						rx_dmrdata.setDataType(DT_VOICE);

						::memcpy(m_dmrFrame, dmrFrameSilence, DMR_FRAME_LENGTH_BYTES);

						// Generate the Embedded LC
						unsigned char lcss = m_EmbeddedLC.getData(m_dmrFrame, n_dmr);

						// Generate the EMB
						emb.setColorCode(m_colorcode);
						emb.setLCSS(lcss);
						emb.getData(m_dmrFrame);

						rx_dmrdata.setData(m_dmrFrame);
				
						//CUtils::dump(1U, "DMR data:", m_dmrFrame, 33U);
						m_dmrNetwork->write(rx_dmrdata);

						n_dmr++;
						dmr_cnt++;
					}
				}

				rx_dmrdata.setSlotNo(2U);
				rx_dmrdata.setSrcId(m_srcid);
				rx_dmrdata.setDstId(m_dstid);
				rx_dmrdata.setFLCO(dmrflco);
				rx_dmrdata.setN(n_dmr);
				rx_dmrdata.setSeqNo(dmr_cnt);
				rx_dmrdata.setBER(0U);
				rx_dmrdata.setRSSI(0U);
				rx_dmrdata.setDataType(DT_TERMINATOR_WITH_LC);

				// Add sync
				CSync::addDMRDataSync(m_dmrFrame, 0);

				// Add SlotType
				CDMRSlotType slotType;
				slotType.setColorCode(m_colorcode);
				slotType.setDataType(DT_TERMINATOR_WITH_LC);
				slotType.getData(m_dmrFrame);
	
				// Full LC
				CDMRLC dmrLC = CDMRLC(dmrflco, m_srcid, m_dstid);
				CDMRFullLC fullLC;
				fullLC.encode(dmrLC, m_dmrFrame, DT_TERMINATOR_WITH_LC);
				
				rx_dmrdata.setData(m_dmrFrame);
				//CUtils::dump(1U, "DMR data:", m_dmrFrame, 33U);
				m_dmrNetwork->write(rx_dmrdata);

				dmrWatch.start();
			}
			else if(dmrFrameType == TAG_DATA) {
				CDMREMB emb;
				CDMRData rx_dmrdata;
				unsigned int n_dmr = (dmr_cnt - 3U) % 6U;

				rx_dmrdata.setSlotNo(2U);
				rx_dmrdata.setSrcId(m_srcid);
				rx_dmrdata.setDstId(m_dstid);
				rx_dmrdata.setFLCO(dmrflco);
				rx_dmrdata.setN(n_dmr);
				rx_dmrdata.setSeqNo(dmr_cnt);
				rx_dmrdata.setBER(0U);
				rx_dmrdata.setRSSI(0U);
			
				if (!n_dmr) {
					rx_dmrdata.setDataType(DT_VOICE_SYNC);
					// Add sync
					CSync::addDMRAudioSync(m_dmrFrame, 0U);
					// Prepare Full LC data
					CDMRLC dmrLC = CDMRLC(dmrflco, m_srcid, m_dstid);
					// Configure the Embedded LC
					m_EmbeddedLC.setLC(dmrLC);
				}
				else {
					rx_dmrdata.setDataType(DT_VOICE);
					// Generate the Embedded LC
					unsigned char lcss = m_EmbeddedLC.getData(m_dmrFrame, n_dmr);
					// Generate the EMB
					emb.setColorCode(m_colorcode);
					emb.setLCSS(lcss);
					emb.getData(m_dmrFrame);
				}

				rx_dmrdata.setData(m_dmrFrame);
				
				//CUtils::dump(1U, "DMR data:", m_dmrFrame, 33U);
				m_dmrNetwork->write(rx_dmrdata);

				dmr_cnt++;
				dmrWatch.start();
			}
		}

		while (m_dmrNetwork->read(tx_dmrdata) > 0U) {
			unsigned int SrcId = tx_dmrdata.getSrcId();
			unsigned int DstId = tx_dmrdata.getDstId();
			FLCO netflco = tx_dmrdata.getFLCO();
			unsigned char DataType = tx_dmrdata.getDataType();

			if (!tx_dmrdata.isMissing()) {
				networkWatchdog.start();

				if(DataType == DT_TERMINATOR_WITH_LC) {
					LogMessage("DMR received end of voice transmission");
					m_conv.putDMREOT();
					m_dmrNetwork->reset(2U);
					networkWatchdog.stop();
				}

				if((DataType == DT_VOICE_LC_HEADER) && (DataType != m_dmrLastDT)) {
					m_netSrc = m_lookup->findCS(SrcId);
					m_netDst = (netflco == FLCO_GROUP ? "TG " : "") + m_lookup->findCS(DstId);

					m_conv.putDMRHeader();
					LogMessage("DMR Header received from %s to %s", m_netSrc.c_str(), m_netDst.c_str());

					m_netSrc.resize(YSF_CALLSIGN_LENGTH, ' ');
					m_netDst.resize(YSF_CALLSIGN_LENGTH, ' ');
				}

				if(DataType == DT_VOICE_SYNC || DataType == DT_VOICE) {
					unsigned char dmr_frame[50];
					tx_dmrdata.getData(dmr_frame);
					m_conv.putDMR(dmr_frame); // Add DMR frame for YSF conversion
				}
			}
			else {
				if(DataType == DT_VOICE_SYNC || DataType == DT_VOICE) {
					unsigned char dmr_frame[50];
					tx_dmrdata.getData(dmr_frame);
					m_conv.putDMR(dmr_frame); // Add DMR frame for YSF conversion
				}

				networkWatchdog.clock(ms);
				if (networkWatchdog.hasExpired()) {
					LogDebug("Network watchdog has expired");
					m_dmrNetwork->reset(2U);
					networkWatchdog.stop();
				}
			}
			
			m_dmrLastDT = DataType;
		}
		
		if (ysfWatch.elapsed() > YSF_FRAME_PER) {
			unsigned int ysfFrameType = m_conv.getYSF(m_ysfFrame + 35U);

			if(ysfFrameType == TAG_HEADER) {
				//LogMessage("Gen YSF Header");
				ysf_cnt = 0U;
			}
			else if (ysfFrameType == TAG_EOT) {
				//LogMessage("Gen YSF EOT: %u", ysf_cnt % 8U);
			}
			else if (ysfFrameType == TAG_DATA) {
				CYSFFICH fich;
				CYSFPayload ysfPayload;

				unsigned int fn = ysf_cnt % 8U;

				::memcpy(m_ysfFrame + 0U, "YSFD", 4U);
				::memcpy(m_ysfFrame + 4U, m_ysfNetwork->getCallsign().c_str(), YSF_CALLSIGN_LENGTH);
				::memcpy(m_ysfFrame + 14U, m_ysfNetwork->getCallsign().c_str(), YSF_CALLSIGN_LENGTH);
				::memcpy(m_ysfFrame + 24U, "ALL       ", YSF_CALLSIGN_LENGTH);

				// Add the YSF Sync
				CSync::addYSFSync(m_ysfFrame + 35U);

				switch (fn) {
					case 0:
						ysfPayload.writeVDMode2Data(m_ysfFrame + 35U, (const unsigned char*)"**********");
						break;
					case 1:
						ysfPayload.writeVDMode2Data(m_ysfFrame + 35U, (const unsigned char*)m_netSrc.c_str());
						break;
					case 2:
						ysfPayload.writeVDMode2Data(m_ysfFrame + 35U, (const unsigned char*)m_netDst.c_str());
						break;
					case 6:
						ysfPayload.writeVDMode2Data(m_ysfFrame + 35U, dt1_temp);
						break;
					case 7:
						ysfPayload.writeVDMode2Data(m_ysfFrame + 35U, dt2_temp);
						break;
					default:
						ysfPayload.writeVDMode2Data(m_ysfFrame + 35U, (const unsigned char*)"          ");
				}
				
				// Set the FICH
				fich.setFI(1U);
				fich.setCS(2U);
				fich.setFN(fn);
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
				m_ysfNetwork->write(m_ysfFrame);
				
				ysf_cnt++;
				ysfWatch.start();
			}
		}

		stopWatch.start();

		m_ysfNetwork->clock(ms);
		m_dmrNetwork->clock(ms);
		
		pollTimer.clock(ms);
		if (pollTimer.isRunning() && pollTimer.hasExpired()) {
			m_ysfNetwork->writePoll();
			pollTimer.start();
		}

		if (ms < 5U)
			CThread::sleep(5U);
	}

	m_ysfNetwork->close();
	m_dmrNetwork->close();

	delete m_dmrNetwork;
	delete m_ysfNetwork;

	::LogFinalise();

	return 0;
}

unsigned int CYSF2DMR::findYSFID(std::string cs)
{
	size_t first = cs.find_first_not_of(' ');
	size_t last = cs.find_last_not_of(' ');

	std::string cstrim = cs.substr(first, (last - first + 1));

	unsigned int id = m_lookup->findID(cstrim);

	if (id == 0) {
		id = m_defsrcid;
		LogMessage("Not DMR ID found, using default ID: %u", id);
	}
	else
		LogMessage("DMR ID of %s: %u", cstrim.c_str(), id);

	return id;
}

bool CYSF2DMR::createDMRNetwork()
{
	std::string address  = m_conf.getDMRNetworkAddress();
	unsigned int port    = m_conf.getDMRNetworkPort();
	unsigned int local   = m_conf.getDMRNetworkLocal();
	std::string password = m_conf.getDMRNetworkPassword();
	bool debug           = m_conf.getDMRNetworkDebug();
	unsigned int jitter  = m_conf.getDMRNetworkJitter();
	bool slot1           = false;
	bool slot2           = true;
	bool duplex          = false;
	HW_TYPE hwType       = HWT_MMDVM;

	m_srcHS = m_conf.getDMRId();
	m_colorcode = 1U;
	m_dstid = m_conf.getDMRDstId();
	m_dmrpc = m_conf.getDMRPC();

	if (m_srcHS > 99999999U)
		m_defsrcid = m_srcHS / 100U;
	else if (m_srcHS > 9999999U)
		m_defsrcid = m_srcHS / 10U;
	else
		m_defsrcid = m_srcHS;

	m_srcid = m_defsrcid;

	LogMessage("DMR Network Parameters");
	LogMessage("    ID: %u", m_srcHS);
	LogMessage("    Default SrcID: %u", m_defsrcid);
	LogMessage("    Startup DstID: %s%u", m_dmrpc ? "" : "TG", m_dstid);
	LogMessage("    Address: %s", address.c_str());
	LogMessage("    Port: %u", port);
	if (local > 0U)
		LogMessage("    Local: %u", local);
	else
		LogMessage("    Local: random");
	LogMessage("    Jitter: %ums", jitter);

	m_dmrNetwork = new CDMRNetwork(address, port, local, m_srcHS, password, duplex, VERSION, debug, slot1, slot2, hwType, jitter);

	std::string options = m_conf.getDMRNetworkOptions();
	if (!options.empty()) {
		LogMessage("    Options: %s", options.c_str());
		m_dmrNetwork->setOptions(options);
	}

	unsigned int rxFrequency = m_conf.getRxFrequency();
	unsigned int txFrequency = m_conf.getTxFrequency();
	unsigned int power       = m_conf.getPower();
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

	m_dmrNetwork->setConfig(m_callsign, rxFrequency, txFrequency, power, m_colorcode, latitude, longitude, height, location, description, url);

	bool ret = m_dmrNetwork->open();
	if (!ret) {
		delete m_dmrNetwork;
		m_dmrNetwork = NULL;
		return false;
	}

	m_dmrNetwork->enable(true);

	return true;
}
