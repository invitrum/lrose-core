// *=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=* 
// ** Copyright UCAR (c) 1990 - 2016                                         
// ** University Corporation for Atmospheric Research (UCAR)                 
// ** National Center for Atmospheric Research (NCAR)                        
// ** Boulder, Colorado, USA                                                 
// ** BSD licence applies - redistribution and use in source and binary      
// ** forms, with or without modification, are permitted provided that       
// ** the following conditions are met:                                      
// ** 1) If the software is modified to produce derivative works,            
// ** such modified software should be clearly marked, so as not             
// ** to confuse it with the version available from UCAR.                    
// ** 2) Redistributions of source code must retain the above copyright      
// ** notice, this list of conditions and the following disclaimer.          
// ** 3) Redistributions in binary form must reproduce the above copyright   
// ** notice, this list of conditions and the following disclaimer in the    
// ** documentation and/or other materials provided with the distribution.   
// ** 4) Neither the name of UCAR nor the names of its contributors,         
// ** if any, may be used to endorse or promote products derived from        
// ** this software without specific prior written permission.               
// ** DISCLAIMER: THIS SOFTWARE IS PROVIDED "AS IS" AND WITHOUT ANY EXPRESS  
// ** OR IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED      
// ** WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.    
// *=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=* 
///////////////////////////////////////////////////////////////
// SpolTsMonitor.cc
//
// Mike Dixon, RAP, NCAR, P.O.Box 3000, Boulder, CO, 80307-3000, USA
//
// April 2011
//
///////////////////////////////////////////////////////////////
//
// SpolTsMonitor reads IWRF time-series data from a file message
// queue (FMQ).
// It locates monitoring information in the time series, and
// writes that information out to SPDB, and in a form suitable
// for Nagios.
//
////////////////////////////////////////////////////////////////

#include <iostream>
#include <fstream>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <unistd.h>
#include <sys/stat.h>
#include <ctime>
#include <toolsa/DateTime.hh>
#include <toolsa/uusleep.h>
#include <toolsa/file_io.h>
#include <toolsa/pmu.h>
#include <toolsa/Path.hh>
#include <toolsa/TaXml.hh>
#include <dsserver/DsLdataInfo.hh>
#include "SpolTsMonitor.hh"

using namespace std;

// Constructor

SpolTsMonitor::SpolTsMonitor(int argc, char **argv)
  
{

  isOK = true;
  _pulseLatestTime = time(NULL);
  _prevSpdbTime = time(NULL);
  _prevNagiosTime = time(NULL);
  _iwrfStatusLatestTime = time(NULL);
  _iwrfStatusXmlSeqNum = 0;

  _movementMonitorTime = time(NULL);
  _moveCheckAz = -999;
  _moveCheckEl = -999;

  _nSamplesTestPulse = 0;
  _xmitRcvMode = IWRF_ALT_HV_CO_CROSS;

  _testIqHc = NULL;
  _testIqHx = NULL;
  _testIqVc = NULL;
  _testIqVx = NULL;

  _testPowerDbHc = -9999;
  _testPowerDbHx = -9999;
  _testPowerDbVc = -9999;
  _testPowerDbVx = -9999;

  _testVelHc = -9999;
  _testVelHx = -9999;
  _testVelVc = -9999;
  _testVelVx = -9999;
  
  _nSamplesG0 = 0;
  _g0IqHc = NULL;
  _g0PowerDbHc = -9999;
  _g0VelHc = -9999;

  // set programe name
  
  _progName = "SpolTsMonitor";
  ucopyright(_progName.c_str());

  // get command line args

  if (_args.parse(argc, argv, _progName)) {
    cerr << "ERROR: " << _progName << endl;
    cerr << "Problem with command line args" << endl;
    isOK = false;
    return;
  }

  // get TDRP params
  
  _paramsPath = (char *) "unknown";
  if (_params.loadFromArgs(argc, argv, _args.override.list,
			   &_paramsPath)) {
    cerr << "ERROR: " << _progName << endl;
    cerr << "Problem with TDRP parameters" << endl;
    isOK = false;
    return;
  }
  
  // create the reader from FMQ
  
  IwrfDebug_t iwrfDebug = IWRF_DEBUG_OFF;
  if (_params.debug >= Params::DEBUG_EXTRA) {
    iwrfDebug = IWRF_DEBUG_VERBOSE;
  } else if (_params.debug >= Params::DEBUG_VERBOSE) {
    iwrfDebug = IWRF_DEBUG_NORM;
  } 
  _pulseReader = new IwrfTsReaderFmq(_params.fmq_name, iwrfDebug);
  _pulseReader->setNonBlocking(100);
  
  // init process mapper registration
  
  PMU_auto_init((char *) _progName.c_str(),
                _params.instance,
                PROCMAP_REGISTER_INTERVAL);

  // SPDB if needed

  if (_params.write_to_spdb) {
    if (_params.debug >= Params::DEBUG_VERBOSE) {
      _spdb.setDebug();
    }
    if (_params.compress_spdb) {
      _spdb.setChunkCompressOnPut(Spdb::COMPRESSION_GZIP);
    }
  }

  // allocate arrays for test pulse if needed

  if (_params.monitor_test_pulse) {
    _testIqHc = new RadarComplex_t[_params.test_pulse_n_samples];
    _testIqHx = new RadarComplex_t[_params.test_pulse_n_samples];
    _testIqVc = new RadarComplex_t[_params.test_pulse_n_samples];
    _testIqVx = new RadarComplex_t[_params.test_pulse_n_samples];
  }

  if (_params.monitor_g0_velocity) {
    _g0IqHc = new RadarComplex_t[_params.g0_velocity_n_samples];
  }

  // set startup xmit-rcv mode

  if (_params.alternating_mode) {
    _xmitRcvMode = IWRF_ALT_HV_CO_CROSS;
  } else {
    _xmitRcvMode = IWRF_SIM_HV_FIXED_HV;
  }

  return;
  
}

// destructor

SpolTsMonitor::~SpolTsMonitor()

{

  // free up

  if (_testIqHc) delete[] _testIqHc;
  if (_testIqHx) delete[] _testIqHx;
  if (_testIqVc) delete[] _testIqVc;
  if (_testIqVx) delete[] _testIqVx;
  if (_g0IqHc) delete[] _g0IqHc;

  // unregister process

  PMU_auto_unregister();

}

//////////////////////////////////////////////////
// Run

int SpolTsMonitor::Run ()
{
  
  PMU_auto_register("Run");
  
  if (_params.debug >= Params::DEBUG_EXTRA) {
    cerr << "Running SpolTsMonitor - extra verbose debug mode" << endl;
  } else if (_params.debug >= Params::DEBUG_VERBOSE) {
    cerr << "Running SpolTsMonitor - verbose debug mode" << endl;
  } else if (_params.debug) {
    cerr << "Running SpolTsMonitor - debug mode" << endl;
  }
  
  while (true) {
    
    PMU_auto_register("Getting next pulse");
    time_t now = time(NULL);

    // clear XML

    int timeSincePulseData = now - _pulseLatestTime;
    if (timeSincePulseData > _params.data_valid_interval_secs) {
      // no data for a while - clear it
      _clearStatus();
    }
    
    // read next pulse
    
    IwrfTsPulse *pulse = _pulseReader->getNextPulse();
    if (pulse == NULL) {
      
      if (_pulseReader->getTimedOut()) {
        // handle time out
        umsleep(100);
      } else {
        return -1;
      }
      
    } else {
      
      // handle this pulse
      
      if (_handlePulse(*pulse)) {
        delete pulse;
        return -1;
      }
      delete pulse;
      
    }

    // update nagios?
    
    if (_params.write_to_nagios) {
      if (now - _prevNagiosTime > _params.nagios_interval_secs) {
        _updateNagios(now);
        _prevNagiosTime = now;
      }
    }
    
    // update spdb?

    if (_params.write_to_spdb) {
      if (now - _prevSpdbTime > _params.spdb_interval_secs) {
        _updateSpdb(now);
        _prevSpdbTime = now;
      }
    }

  } // while
  
  return 0;
  
}

/////////////////////////////
// clear all status

void SpolTsMonitor::_clearStatus()

{
  _iwrfStatusXml.clear();
  _movementXml.clear();
  _testPulseXml.clear();
  _g0Xml.clear();
}

/////////////////////////////
// handle a pulse

int SpolTsMonitor::_handlePulse(IwrfTsPulse &pulse)

{

  _pulseLatestTime = pulse.getTime();

  // write ops info to file, if info has changed since last write
  
  const IwrfTsInfo &info = pulse.getTsInfo();

  // check for new status xml packet

  const iwrf_status_xml_t &xmlHdr = info.getStatusXmlHdr();
  time_t xmlPktTime = xmlHdr.packet.time_secs_utc;
  time_t now = time(NULL);
  double secsSinceXml = (double) now - (double) xmlPktTime;
  if (secsSinceXml < _params.data_valid_interval_secs) {
    _iwrfStatusXmlSeqNum = info.getStatusXmlPktSeqNum();
    _iwrfStatusXml = info.getStatusXmlStr();
    _iwrfStatusLatestTime = time(NULL);
    if (_params.debug >= Params::DEBUG_EXTRA) {
      cerr << "==============================================" << endl;
      cerr << _iwrfStatusXml << endl;
      cerr << "==============================================" << endl;
    }
  }

  // test pulse
  
  if (_params.monitor_test_pulse) {
    _monitorTestPulse(pulse, info);
  }
  
  if (_params.monitor_g0_velocity) {
    _monitorG0(pulse, info);
  }
  
  if (_params.check_for_moving_antenna) {
    _monitorAntennaMovement(pulse);
  }

  return 0;

}

/////////////////////////////
// monitor antenna movement

void SpolTsMonitor::_monitorAntennaMovement(IwrfTsPulse &pulse)

{
  
  int elapsedTime = pulse.getTime() - _movementMonitorTime;
  if (elapsedTime < _params.movement_check_interval) {
    return;
  }

  time_t now = time(NULL);
  double maxAngleChange = _params.stationary_max_angle_change;
  if (fabs(pulse.getAz() - _moveCheckAz) > maxAngleChange ||
      fabs(pulse.getEl() - _moveCheckEl) > maxAngleChange) {
    _isMoving = true;
    _antennaStopTime = 0;
    _antennaStopSecs = 0;
  } else {
    if (_isMoving) {
      _antennaStopTime = now;
    }
    _isMoving = false;
    _antennaStopSecs = now - _antennaStopTime;
  }
  
  _moveCheckAz = pulse.getAz();
  _moveCheckEl = pulse.getEl();
  _movementMonitorTime = pulse.getTime();
  
  _movementXml.clear();
  _movementXml += TaXml::writeStartTag(_params.antenna_movement_xml_tag, 0);
  _movementXml += TaXml::writeTime("Time", 1, _movementMonitorTime);
  _movementXml += TaXml::writeBoolean("AntennaIsMoving", 1, _isMoving);
  _movementXml += TaXml::writeEndTag(_params.antenna_movement_xml_tag, 0);

  if (_params.debug >= Params::DEBUG_VERBOSE) {
    cerr << "======================================" << endl;
    cerr << _movementXml << endl;
    cerr << "======================================" << endl;
  }

}

/////////////////////////////
// monitor the test pulse

void SpolTsMonitor::_monitorTestPulse(IwrfTsPulse &pulse,
                                      const IwrfTsInfo &info)

{

  // get xmit/rcv mode

  string statusXml = info.getStatusXmlStr();
  string transmitStatusStr;
  if (TaXml::readString(statusXml, "SpolTransmitStatus",
                        transmitStatusStr) == 0) {
    string xmitModeStr;
    if (TaXml::readString(transmitStatusStr, "XmitMode", xmitModeStr) == 0) {
      if (xmitModeStr == "Alternating") {
        _xmitRcvMode = IWRF_ALT_HV_CO_CROSS;
      } else if (xmitModeStr == "Simultaneous") {
        _xmitRcvMode = IWRF_SIM_HV_FIXED_HV;
      }
    }
  }

  // compute gate number

  int nGates = pulse.getNGates();
  double startRange = pulse.get_start_range_km();
  double gateSpacing = pulse.get_gate_spacing_km();

  int gateNumHc = (int)
    ((_params.test_pulse_range_km_hc - startRange) / gateSpacing + 0.5);
  if (gateNumHc < 0) gateNumHc = 0;
  if (gateNumHc > nGates - 1) gateNumHc = nGates - 1;
  
  int gateNumHx = (int)
    ((_params.test_pulse_range_km_hx - startRange) / gateSpacing + 0.5);
  if (gateNumHx < 0) gateNumHx = 0;
  if (gateNumHx > nGates - 1) gateNumHx = nGates - 1;
  
  int gateNumVc = (int)
    ((_params.test_pulse_range_km_vc - startRange) / gateSpacing + 0.5);
  if (gateNumVc < 0) gateNumVc = 0;
  if (gateNumVc > nGates - 1) gateNumVc = nGates - 1;
  
  int gateNumVx = (int)
    ((_params.test_pulse_range_km_vx - startRange) / gateSpacing + 0.5);
  if (gateNumVx < 0) gateNumVx = 0;
  if (gateNumVx > nGates - 1) gateNumVx = nGates - 1;
  
  // load IQ values for test pulse
  
  if (_xmitRcvMode == IWRF_ALT_HV_CO_CROSS) {
    bool isHoriz = pulse.isHoriz();
    if (_params.switching_receivers) {
      if (isHoriz) {
        _loadTestPulseIq(pulse, 0, gateNumHc, _testIqHc);
        _loadTestPulseIq(pulse, 1, gateNumVx, _testIqVx);
      } else {
        _loadTestPulseIq(pulse, 0, gateNumVc, _testIqVc);
        _loadTestPulseIq(pulse, 1, gateNumHx, _testIqHx);
      }
    } else {
      if (isHoriz) {
        _loadTestPulseIq(pulse, 0, gateNumHc, _testIqHc);
        _loadTestPulseIq(pulse, 1, gateNumVx, _testIqVx);
      } else {
        _loadTestPulseIq(pulse, 1, gateNumVc, _testIqVc);
        _loadTestPulseIq(pulse, 0, gateNumHx, _testIqHx);
      }
    }
  } else {
    _loadTestPulseIq(pulse, 0, gateNumHc, _testIqHc);
    _loadTestPulseIq(pulse, 1, gateNumVc, _testIqVc);
  }
  _nSamplesTestPulse++;

  // compute mean power if ready

  if (_nSamplesTestPulse < _params.test_pulse_n_samples) {
    return;
  }

  int nSamples = _nSamplesTestPulse;
  if (_xmitRcvMode == IWRF_ALT_HV_CO_CROSS) {
    nSamples /= 2;
  }
  double meanPowerHc = RadarComplex::meanPower(_testIqHc, nSamples);
  double meanPowerHx = RadarComplex::meanPower(_testIqHx, nSamples);
  double meanPowerVc = RadarComplex::meanPower(_testIqVc, nSamples);
  double meanPowerVx = RadarComplex::meanPower(_testIqVx, nSamples);

  if (meanPowerHc > 0) {
    _testPowerDbHc = 10.0 * log10(meanPowerHc);
  }
  if (meanPowerHx > 0) {
    _testPowerDbHx = 10.0 * log10(meanPowerHx);
  }
  if (meanPowerVc > 0) {
    _testPowerDbVc = 10.0 * log10(meanPowerVc);
  }
  if (meanPowerVx > 0) {
    _testPowerDbVx = 10.0 * log10(meanPowerVx);
  }

  RadarComplex_t lag1Hc =
    RadarComplex::meanConjugateProduct(_testIqHc + 1, _testIqHc,
                                       nSamples-1);
  RadarComplex_t lag1Hx =
    RadarComplex::meanConjugateProduct(_testIqHx + 1, _testIqHx,
                                       nSamples-1);
  RadarComplex_t lag1Vc =
    RadarComplex::meanConjugateProduct(_testIqVc + 1, _testIqVc,
                                       nSamples-1);
  RadarComplex_t lag1Vx =
    RadarComplex::meanConjugateProduct(_testIqVx + 1, _testIqVx,
                                       nSamples-1);

  double argVelHc = RadarComplex::argRad(lag1Hc);
  double argVelHx = RadarComplex::argRad(lag1Hx);
  double argVelVc = RadarComplex::argRad(lag1Vc);
  double argVelVx = RadarComplex::argRad(lag1Vx);

  double wavelengthM = info.get_radar_wavelength_cm() / 100.0;
  double prt = pulse.getPrt();
  if (_xmitRcvMode == IWRF_ALT_HV_CO_CROSS) {
    prt *= 2;
  }
  double nyquist = (wavelengthM / prt) / 4.0;

  _testVelHc = (argVelHc / M_PI) * nyquist;
  _testVelHx = (argVelHx / M_PI) * nyquist;
  _testVelVc = (argVelVc / M_PI) * nyquist;
  _testVelVx = (argVelVx / M_PI) * nyquist;

  // clear the test pulse stats to prepare for next average

  _nSamplesTestPulse = 0;

  // create test pulse XML

  _testPulseLatestTime = time(NULL);
  _testPulseXml.clear();
  _testPulseXml += TaXml::writeStartTag(_params.test_pulse_xml_tag, 0);
  _testPulseXml += TaXml::writeTime("Time", 1, _testPulseLatestTime);
  _testPulseXml += TaXml::writeDouble("RangeKmHc", 1, _params.test_pulse_range_km_hc);
  _testPulseXml += TaXml::writeInt("GateNumHc", 1, gateNumHc);
  _testPulseXml += TaXml::writeDouble("RangeKmHx", 1, _params.test_pulse_range_km_hx);
  _testPulseXml += TaXml::writeInt("GateNumHx", 1, gateNumHx);
  _testPulseXml += TaXml::writeDouble("RangeKmVc", 1, _params.test_pulse_range_km_vc);
  _testPulseXml += TaXml::writeInt("GateNumVc", 1, gateNumVc);
  _testPulseXml += TaXml::writeDouble("RangeKmVx", 1, _params.test_pulse_range_km_vx);
  _testPulseXml += TaXml::writeInt("GateNumVx", 1, gateNumVx);

  if (_testPowerDbHc > -9990) {
    _testPulseXml +=
      TaXml::writeDouble("TestPulsePowerDbHc", 1, _testPowerDbHc);
  }
  if (_testPowerDbVc > -9990) {
    _testPulseXml +=
      TaXml::writeDouble("TestPulsePowerDbVc", 1, _testPowerDbVc);
  }

  if (_xmitRcvMode == IWRF_ALT_HV_CO_CROSS) {
    if (_testPowerDbHx > -9990) {
      _testPulseXml +=
        TaXml::writeDouble("TestPulsePowerDbHx", 1, _testPowerDbHx);
    }
    if (_testPowerDbVx > -9990) {
      _testPulseXml +=
        TaXml::writeDouble("TestPulsePowerDbVx", 1, _testPowerDbVx);
    }
  }

  if (_testPowerDbHc > -9990) {
    _testPulseXml += TaXml::writeDouble("TestPulseVelHc", 1, _testVelHc);
  }
  if (_testPowerDbVc > -9990) {
    _testPulseXml += TaXml::writeDouble("TestPulseVelVc", 1, _testVelVc);
  }
  if (_xmitRcvMode == IWRF_ALT_HV_CO_CROSS) {
    if (_testPowerDbHx > -9990) {
      _testPulseXml += TaXml::writeDouble("TestPulseVelHx", 1, _testVelHx);
    }
    if (_testPowerDbVx > -9990) {
      _testPulseXml += TaXml::writeDouble("TestPulseVelVx", 1, _testVelVx);
    }
  }

  _testPulseXml += TaXml::writeEndTag(_params.test_pulse_xml_tag, 0);
  
  if (_params.debug >= Params::DEBUG_NORM) {
    cerr << "============= TEST PULSE POWERS ==========================" << endl;
    if (_xmitRcvMode == IWRF_ALT_HV_CO_CROSS) {
      cerr << "  Alternating mode" << endl;
    } else if (_xmitRcvMode == IWRF_SIM_HV_FIXED_HV) {
      cerr << "  Simultaneous mode" << endl;
    }
    cerr << "     nGates: " << nGates << endl;
    cerr << "     startRange: " << startRange << endl;
    cerr << "     gateSpacing: " << gateSpacing << endl;
    cerr << "     hcRange: " << _params.test_pulse_range_km_hc << endl;
    cerr << "     vcRange: " << _params.test_pulse_range_km_vc << endl;
    cerr << "     hxRange: " << _params.test_pulse_range_km_hx << endl;
    cerr << "     vxRange: " << _params.test_pulse_range_km_vx << endl;
    cerr << "     hcGateNum: " << gateNumHc << endl;
    cerr << "     vcGateNum: " << gateNumVc << endl;
    cerr << "     hxGateNum: " << gateNumHx << endl;
    cerr << "     vxGateNum: " << gateNumVx << endl;
    cerr << "     testPowerDbHc: " << _testPowerDbHc << endl;
    cerr << "     testPowerDbVc: " << _testPowerDbVc << endl;
    cerr << "     testPowerDbHx: " << _testPowerDbHx << endl;
    cerr << "     testPowerDbVx: " << _testPowerDbVx << endl;
    cerr << "==========================================================" << endl;
  }
  
  if (_params.debug >= Params::DEBUG_VERBOSE) {
    cerr << "======================================" << endl;
    cerr << _testPulseXml << endl;
    cerr << "======================================" << endl;
  }

}

///////////////////////////////////////////////////
// load test pulse IQ

void SpolTsMonitor::_loadTestPulseIq(IwrfTsPulse &pulse,
                                     int channelNum,
                                     int gateNum,
                                     RadarComplex_t *iq)
  
{
  fl32 ival, qval;
  pulse.getIq(channelNum, gateNum, ival, qval);
  int index = _nSamplesTestPulse;
  if (_xmitRcvMode == IWRF_ALT_HV_CO_CROSS) {
    index /= 2;
  }
  iq[index].re = ival;
  iq[index].im = qval;
}

/////////////////////////////
// monitor g0

void SpolTsMonitor::_monitorG0(IwrfTsPulse &pulse,
                               const IwrfTsInfo &info)
  
{
  
  // compute gate number
  
  int nGates = pulse.getNGates();
  double startRange = pulse.get_start_range_km();
  double gateSpacing = pulse.get_gate_spacing_km();
  
  int gateNum = (int)
    ((_params.g0_velocity_range_km - startRange) / gateSpacing + 0.5);
  if (gateNum < 0) gateNum = 0;
  if (gateNum > nGates - 1) gateNum = nGates - 1;
  
  // load IQ values
  
  _loadG0Iq(pulse, 0, gateNum, _g0IqHc);
  _nSamplesG0++;
  
  // compute if ready
  
  if (_nSamplesG0 < _params.g0_velocity_n_samples) {
    return;
  }
  
  int nSamples = _nSamplesG0;
  double meanPowerHc = RadarComplex::meanPower(_g0IqHc, nSamples);

  _g0PowerDbHc = -9999;
  if (meanPowerHc > 0) {
    _g0PowerDbHc = 10.0 * log10(meanPowerHc);
  }

  RadarComplex_t lag1Hc =
    RadarComplex::meanConjugateProduct(_g0IqHc + 1, _g0IqHc,
                                       nSamples-1);

  double argVelHc = RadarComplex::argRad(lag1Hc);
  double wavelengthM = info.get_radar_wavelength_cm() / 100.0;
  double prt = pulse.getPrt();
  double nyquist = (wavelengthM / prt) / 4.0;
  _g0VelHc = (argVelHc / M_PI) * nyquist;

  // clear the test pulse stats to prepare for next average

  _nSamplesG0 = 0;

  // create test pulse XML

  _g0LatestTime = time(NULL);
  _g0Xml.clear();
  _g0Xml += TaXml::writeStartTag(_params.g0_velocity_xml_tag, 0);
  _g0Xml += TaXml::writeTime("Time", 1, _g0LatestTime);
  _g0Xml += TaXml::writeDouble("RangeKm", 1, _params.g0_velocity_range_km);
  _g0Xml += TaXml::writeInt("GateNum", 1, gateNum);
  _g0Xml += TaXml::writeDouble("G0PowerDbHc", 1, _g0PowerDbHc);
  _g0Xml += TaXml::writeDouble("G0VelHc", 1, _g0VelHc);
  _g0Xml += TaXml::writeEndTag(_params.g0_velocity_xml_tag, 0);
  
  if (_params.debug >= Params::DEBUG_VERBOSE) {
    cerr << "======================================" << endl;
    cerr << _g0Xml << endl;
    cerr << "======================================" << endl;
  }

}

///////////////////////////////////////////////////
// load G0 IQ

void SpolTsMonitor::_loadG0Iq(IwrfTsPulse &pulse,
                              int channelNum,
                              int gateNum,
                              RadarComplex_t *iq)
  
{
  fl32 ival, qval;
  pulse.getIq(channelNum, gateNum, ival, qval);
  int index = _nSamplesG0;
  iq[index].re = ival;
  iq[index].im = qval;
}

/////////////////////////////////////
// get the concatenated status XML
//
// Puts together the iwrf status, test pulse and movement xml
// strings as appropriate

string SpolTsMonitor::_getCombinedXml(time_t now)

{

  time_t validTime = now - _params.data_valid_interval_secs;

  if (_params.debug >= Params::DEBUG_VERBOSE) {
    cerr << "_getCombinedXml(), time: " << DateTime::strm(now) << endl;
    cerr << "_getCombinedXml(), looking back to: " << DateTime::strm(validTime) << endl;
  }

  string combinedXml;

  if (_iwrfStatusLatestTime >= validTime) {
    combinedXml += _iwrfStatusXml;
    if (_params.debug >= Params::DEBUG_VERBOSE) {
      cerr << "Adding iwrfStatusXml: " << endl;
      cerr << _iwrfStatusXml << endl;
    }
  }
  if (_testPulseLatestTime >= validTime) {
    combinedXml += _testPulseXml;
    if (_params.debug >= Params::DEBUG_VERBOSE) {
      cerr << "Adding testPulseXml: " << endl;
      cerr << _testPulseXml << endl;
    }
  }
  if (_g0LatestTime >= validTime) {
    combinedXml += _g0Xml;
    if (_params.debug >= Params::DEBUG_VERBOSE) {
      cerr << "Adding g0Xml: " << endl;
      cerr << _g0Xml << endl;
    }
  }
  if (_movementMonitorTime >= validTime) {
    combinedXml += _movementXml;
    if (_params.debug >= Params::DEBUG_VERBOSE) {
      cerr << "Adding movementXml: " << endl;
      cerr << _movementXml << endl;
    }
  }

  if (_params.debug >= Params::DEBUG_VERBOSE) {
    cerr << "======== Combined XML: ============" << endl;
    cerr << combinedXml << endl;
    cerr << "===================================" << endl;
  }

  return combinedXml;

}

/////////////////////////////
// update the SPDB data base

int SpolTsMonitor::_updateSpdb(time_t now)

{

  if (_params.debug) {
    cerr << "==>> updating SPDB" << endl;
  }

  // get the concatenated xml string

  string xml = _getCombinedXml(now);
  
  if (xml.size() == 0) {
    return 0;
  }

  _spdb.clearPutChunks();
  _spdb.addPutChunk(0, now, now + _params.spdb_interval_secs,
                    xml.size() + 1, xml.c_str());

  if (_spdb.put(_params.spdb_url,
                SPDB_XML_ID, SPDB_XML_LABEL)) {
    cerr << "ERROR - SpolTsMonitor::_updateSpdb" << endl;
    cerr << _spdb.getErrStr() << endl;
    return -1;
  }

  if (_params.debug) {
    cerr << "Wrote SPDB data to: " << _params.spdb_url << endl;
  }

  return 0;

}

/////////////////////////////
// update the NAGIOS file

int SpolTsMonitor::_updateNagios(time_t now)

{

  if (_params.debug) {
    cerr << "==>> updating Nagios" << endl;
  }

  // get the concatenated xml string

  string xml = _getCombinedXml(now);
  
  if (_params.debug) {
    cerr << "statusXml for nagios: " << endl;
    cerr << xml << endl;
  }

  // create directory as needed

  Path path(_params.nagios_file_path);
  if (ta_makedir_recurse(path.getDirectory().c_str())) {
    int errNum = errno;
    cerr << "ERROR - SpolTsMonitor::_updateNagios" << endl;
    cerr << "  Cannot make directory for output file: " << _params.nagios_file_path << endl;
    cerr << strerror(errNum) << endl;
    return -1;
  }

  // write to tmp nagios file

  string tmpPath(_params.nagios_file_path);
  tmpPath += ".tmp";
  
  FILE *tmpFile = fopen(tmpPath.c_str(), "w");
  if (tmpFile == NULL) {
    int errNum = errno;
    cerr << "ERROR - SpolTsMonitor::_updateNagios" << endl;
    cerr << "  Cannot open tmp file for writing: " << tmpPath << endl;
    cerr << strerror(errNum) << endl;
    return -1;
  }

  // write to nagios file

  for (int ii = 0; ii < _params.xml_entries_n; ii++) {

    const Params::xml_entry_t &entry = _params._xml_entries[ii];
    
    string sectionStr;
    if (TaXml::readString(xml, entry.xml_outer_tag, sectionStr)) {
      if (_params.debug >= Params::DEBUG_EXTRA) { 
        cerr << "WARNING - SpolTsMonitor::_updateNagios" << endl;
        cerr << " Cannot find main tag: " << entry.xml_outer_tag << endl;
      }
    }

    switch (entry.entry_type) {
      
      case Params::XML_ENTRY_BOOLEAN:
        _handleBooleanNagios(sectionStr, entry, tmpFile);
        break;
        
      case Params::XML_ENTRY_BOOLEAN_TO_INT:
        _handleBooleanToIntNagios(sectionStr, entry, tmpFile);
        break;
        
      case Params::XML_ENTRY_INT:
        _handleIntNagios(sectionStr, entry, tmpFile);
        break;
        
      case Params::XML_ENTRY_DOUBLE:
        _handleDoubleNagios(sectionStr, entry, tmpFile);
        break;
        
      case Params::XML_ENTRY_STRING:
      default:
        _handleStringNagios(sectionStr, entry, tmpFile);
        break;
        
    } // switch (entry.entry_type)
        
  } // ii

  if (_params.nagios_monitor_antenna_movement) {
    _addMovementToNagios(tmpFile);
  }

  fclose(tmpFile);
  
  // rename file

  if (rename(tmpPath.c_str(), _params.nagios_file_path)) {
    int errNum = errno;
    cerr << "ERROR - SpolTsMonitor::_updateNagios" << endl;
    cerr << "  Cannot rename tmp file to final path" << endl;
    cerr << "  tmp path: " << tmpPath << endl;
    cerr << "  final path: " << _params.nagios_file_path << endl;
    cerr << strerror(errNum) << endl;
    return -1;
  }

  if (_params.debug) {
    cerr << "Wrote Nagios data to: " << _params.nagios_file_path << endl;
  }

  return 0;

}

///////////////////////////////////////////
// handle a boolean entry in the status xml

int SpolTsMonitor::_handleBooleanNagios(const string &xml,
                                        const Params::xml_entry_t &entry,
                                        FILE *nagiosFile)
  
{
  
  string label = entry.label;
  if (label.size() == 0) {
    label = entry.xml_inner_tag;
  }

  // get the substring

  string sval;
  if (TaXml::readString(xml, entry.xml_inner_tag, sval)) {
    _handleMissingEntry(xml, entry, nagiosFile);
    return 0;
  }

  // get the boolean value

  bool bval;
  if (TaXml::readBoolean(xml, entry.xml_inner_tag, bval)) {
    cerr << "ERROR - SpolTsMonitor::_handleBooleanNagios" << endl;
    cerr << " Cannot find sub tag: " << entry.xml_inner_tag << endl;
    return -1;
  }
  
  int warnLevel = 0;
  string warnStr = "OK";
  if (bval == entry.ok_boolean) {
    warnLevel = 0;
    warnStr = "OK";
  } else {
    if (entry.boolean_failure_is_critical) {
      warnLevel = 2;
      warnStr = "CRITICAL";
    } else {
      warnLevel = 1;
      warnStr = "WARN";
    }
  }
  int warnLimit = 1;
  int critLimit = 1;
  if (entry.ok_boolean) {
    critLimit = 0;
    warnLimit = 0;
  }

  char comment[1024];
  if (strlen(entry.comment) > 0) {
    sprintf(comment, " (%s)", entry.comment);
  } else {
    comment[0] = '\0';
  }

  char text[4096];
  sprintf(text, "%d %s_%s %s=%d;%d;%d;%g;%g; %s - %s = %s %s\n",
          warnLevel,
          entry.xml_outer_tag,
          entry.xml_inner_tag,
          label.c_str(),
          bval,
          warnLimit,
          critLimit,
          entry.graph_min_val,
          entry.graph_max_val,
          warnStr.c_str(),
          label.c_str(),
          sval.c_str(),
          comment);
  
  fprintf(nagiosFile, "%s", text);
  if (_params.debug) {
    fprintf(stderr, "Adding nagios entry: ");
    fprintf(stderr, "%s", text);
  }

  return 0;

}

///////////////////////////////////////////
// handle a boolean entry, convert to int
// in the status xml

int SpolTsMonitor::_handleBooleanToIntNagios(const string &xml,
                                             const Params::xml_entry_t &entry,
                                             FILE *nagiosFile)
  
{
  
  string label = entry.label;
  if (label.size() == 0) {
    label = entry.xml_inner_tag;
  }

  // get the substring

  string sval;
  if (TaXml::readString(xml, entry.xml_inner_tag, sval)) {
    _handleMissingEntry(xml, entry, nagiosFile);
    return 0;
  }

  // get the boolean value

  bool bval;
  if (TaXml::readBoolean(xml, entry.xml_inner_tag, bval)) {
    cerr << "ERROR - SpolTsMonitor::_handleBooleanNagios" << endl;
    cerr << " Cannot find sub tag: " << entry.xml_inner_tag << endl;
    return -1;
  }
  
  int warnLevel = 0;
  string warnStr = "OK";
  if (bval == entry.ok_boolean) {
    warnLevel = 0;
    warnStr = "OK";
  } else {
    if (entry.boolean_failure_is_critical) {
      warnLevel = 2;
      warnStr = "CRITICAL";
    } else {
      warnLevel = 1;
      warnStr = "WARN";
    }
  }
  int warnLimit = 1;
  int critLimit = 1;
  if (entry.ok_boolean) {
    critLimit = 0;
    warnLimit = 0;
  }

  char comment[1024];
  if (strlen(entry.comment) > 0) {
    sprintf(comment, " (%s)", entry.comment);
  } else {
    comment[0] = '\0';
  }

  char text[4096];
  sprintf(text, "%d %s_%s %s=%d;%d;%d;%g;%g; %s - %s = %d %s\n",
          warnLevel,
          entry.xml_outer_tag,
          entry.xml_inner_tag,
          label.c_str(),
          bval,
          warnLimit,
          critLimit,
          entry.graph_min_val,
          entry.graph_max_val,
          warnStr.c_str(),
          label.c_str(),
          bval,
          comment);
  
  fprintf(nagiosFile, "%s", text);
  if (_params.debug) {
    fprintf(stderr, "Adding nagios entry: ");
    fprintf(stderr, "%s", text);
  }

  return 0;

}

///////////////////////////////////////////
// handle a int entry in the status xml

int SpolTsMonitor::_handleIntNagios(const string &xml,
                                    const Params::xml_entry_t &entry,
                                    FILE *nagiosFile)
  
{

  // get the substring

  string sval;
  if (TaXml::readString(xml, entry.xml_inner_tag, sval)) {
    _handleMissingEntry(xml, entry, nagiosFile);
    return 0;
  }

  int ival;
  if (TaXml::readInt(xml, entry.xml_inner_tag, ival)) {
    if (_params.debug) { 
      cerr << "WARNING - SpolTsMonitor::_handleBooleanNagios" << endl;
      cerr << " Cannot find sub tag: " << entry.xml_inner_tag << endl;
    }
    return -1;
  }
  
  int warnLevel = 0;
  string warnStr = "OK";
  if (ival >= entry.ok_value_lower_limit &&
      ival <= entry.ok_value_upper_limit) {
    warnLevel = 0;
    warnStr = "OK";
  } else if (ival >= entry.impaired_value_lower_limit &&
             ival <= entry.impaired_value_upper_limit) {
    warnLevel = 1;
    warnStr = "WARN";
  } else {
    warnLevel = 2;
    warnStr = "CRITICAL";
  }

  double warnLimit = entry.impaired_value_lower_limit;
  double critLimit = entry.impaired_value_upper_limit + 1;
  if (entry.impaired_value_lower_limit < entry.ok_value_lower_limit) {
    warnLimit = entry.impaired_value_upper_limit;
    critLimit = entry.impaired_value_lower_limit - 1;
  }

  string label = entry.label;
  if (label.size() == 0) {
    label = entry.xml_inner_tag;
  }

  char comment[1024];
  if (strlen(entry.comment) > 0) {
    sprintf(comment, " (%s)", entry.comment);
  } else {
    comment[0] = '\0';
  }

  char text[4096];
  sprintf(text, "%d %s_%s %s=%s;%g;%g;%g;%g; %s - %s = %s %s%s\n",
          warnLevel,
          entry.xml_outer_tag,
          entry.xml_inner_tag,
          label.c_str(),
          sval.c_str(),
          warnLimit,
          critLimit,
          entry.graph_min_val,
          entry.graph_max_val,
          warnStr.c_str(),
          label.c_str(),
          sval.c_str(),
          entry.units,
          comment);
  
  fprintf(nagiosFile, "%s", text);
  if (_params.debug) {
    fprintf(stderr, "Adding nagios entry: ");
    fprintf(stderr, "%s", text);
  }

  return 0;

}

///////////////////////////////////////////
// handle a double entry in the status xml

int SpolTsMonitor::_handleDoubleNagios(const string &xml,
                                       const Params::xml_entry_t &entry,
                                       FILE *nagiosFile)
  
{

  // get the substring

  string sval;
  if (TaXml::readString(xml, entry.xml_inner_tag, sval)) {
    _handleMissingEntry(xml, entry, nagiosFile);
    return 0;
  }

  double dval;
  if (TaXml::readDouble(xml, entry.xml_inner_tag, dval)) {
    if (_params.debug) { 
      cerr << "WARNING - SpolTsMonitor::_handleBooleanNagios" << endl;
      cerr << " Cannot find sub tag: " << entry.xml_inner_tag << endl;
    }
    return -1;
  }
  
  int warnLevel = 0;
  string warnStr = "OK";
  if (dval >= entry.ok_value_lower_limit &&
      dval <= entry.ok_value_upper_limit) {
    warnLevel = 0;
    warnStr = "OK";
  } else if (dval >= entry.impaired_value_lower_limit &&
             dval <= entry.impaired_value_upper_limit) {
    warnLevel = 1;
    warnStr = "WARN";
  } else {
    warnLevel = 2;
    warnStr = "CRITICAL";
  }

  double warnLimit = entry.ok_value_upper_limit;
  double critLimit = entry.impaired_value_upper_limit;
  if (entry.impaired_value_lower_limit < entry.ok_value_lower_limit) {
    warnLimit = entry.ok_value_lower_limit;
    critLimit = entry.impaired_value_lower_limit;
  }

  string label = entry.label;
  if (label.size() == 0) {
    label = entry.xml_inner_tag;
  }

  char comment[1024];
  if (strlen(entry.comment) > 0) {
    sprintf(comment, " (%s)", entry.comment);
  } else {
    comment[0] = '\0';
  }

  char text[4096];
  sprintf(text, "%d %s_%s %s=%s;%g;%g;%g;%g; %s - %s = %s %s%s\n",
          warnLevel,
          entry.xml_outer_tag,
          entry.xml_inner_tag,
          label.c_str(),
          sval.c_str(),
          warnLimit,
          critLimit,
          entry.graph_min_val,
          entry.graph_max_val,
          warnStr.c_str(),
          label.c_str(),
          sval.c_str(),
          entry.units,
          comment);
  
  fprintf(nagiosFile, "%s", text);
  if (_params.debug) {
    fprintf(stderr, "Adding nagios entry: ");
    fprintf(stderr, "%s", text);
  }

  return 0;

}

///////////////////////////////////////////
// handle a string entry in the status xml

int SpolTsMonitor::_handleStringNagios(const string &xml,
                                       const Params::xml_entry_t &entry,
                                       FILE *nagiosFile)
  
{
  
  // get the substring

  string sval;
  if (TaXml::readString(xml, entry.xml_inner_tag, sval)) {
    _handleMissingEntry(xml, entry, nagiosFile);
    return 0;
  }

  // set the label

  string label = entry.label;
  if (label.size() == 0) {
    label = entry.xml_inner_tag;
  }

  char comment[1024];
  if (strlen(entry.comment) > 0) {
    sprintf(comment, " (%s)", entry.comment);
  } else {
    comment[0] = '\0';
  }

  char text[1024];
  sprintf(text, "0 %s_%s %s=%s OK - %s = %s %s%s\n",
          entry.xml_outer_tag,
          entry.xml_inner_tag,
          label.c_str(),
          sval.c_str(),
          label.c_str(),
          sval.c_str(),
          entry.units,
          comment);
  
  fprintf(nagiosFile, "%s", text);
  if (_params.debug) {
    fprintf(stderr, "Adding nagios entry: ");
    fprintf(stderr, "%s", text);
  }

  return 0;

}

///////////////////////////////////////////
// handle a missing entry

int SpolTsMonitor::_handleMissingEntry(const string &xml,
                                       const Params::xml_entry_t &entry,
                                       FILE *nagiosFile)
  
{

  if (_params.debug >= Params::DEBUG_EXTRA) { 
    cerr << "WARNING - SpolTsMonitor" << endl;
    cerr << " Cannot find entry: " << entry.xml_outer_tag
         << "_" << entry.xml_inner_tag << endl;
  }

  time_t now = time(NULL);
  int secsSinceData = now - _pulseLatestTime;

  string label = entry.label;
  if (label.size() == 0) {
    label = entry.xml_inner_tag;
  }

  char comment[1024];
  if (strlen(entry.comment) > 0) {
    sprintf(comment, " (%s)", entry.comment);
  } else {
    comment[0] = '\0';
  }

  char text[1024];
  if (secsSinceData >= _params.nagios_nsecs_missing_for_critical) {
    sprintf(text, "2 %s_%s %s=MISSING CRITICAL - no data for %d secs\n",
            entry.xml_outer_tag,
            entry.xml_inner_tag,
            label.c_str(),
            secsSinceData);
  } else if (secsSinceData >= _params.nagios_nsecs_missing_for_warning) {
    sprintf(text, "1 %s_%s %s=MISSING WARN - no data for %d secs\n",
            entry.xml_outer_tag,
            entry.xml_inner_tag,
            label.c_str(),
            secsSinceData);
  } else {
    sprintf(text, "1 %s_%s %s=MISSING OK - no data for %d secs\n",
            entry.xml_outer_tag,
            entry.xml_inner_tag,
            label.c_str(),
            secsSinceData);
  }
  
  fprintf(nagiosFile, "%s", text);
  if (_params.debug) {
    fprintf(stderr, "Adding nagios entry: ");
    fprintf(stderr, "%s", text);
  }

  return 0;

}

///////////////////////////////////////////
// handle a missing entry

int SpolTsMonitor::_addMovementToNagios(FILE *nagiosFile)
  
{

  int warnLevel = 0;
  string warnStr = "OK";
  if (!_isMoving) {
    if (_antennaStopSecs > _params.nagios_antenna_movement_crit_secs) {
      warnLevel = 2;
      warnStr = "WARN";
    } else if (_antennaStopSecs > _params.nagios_antenna_movement_warn_secs) {
      warnLevel = 1;
      warnStr = "CRITICAL";
    }
  }

  double warnLimit = _params.nagios_antenna_movement_warn_secs;
  double critLimit = _params.nagios_antenna_movement_crit_secs + 1;
  string label = _params.nagios_antenna_movement_label;
  string name = "stationaryPeriod";

  char text[4096];
  sprintf(text, "%d %s_%s %s=%d;%g;%g;%g;%g; %s - %s = %d sec\n",
          warnLevel,
          label.c_str(),
          name.c_str(),
          name.c_str(),
          _antennaStopSecs,
          warnLimit,
          critLimit,
          0.0,
          critLimit,
          warnStr.c_str(),
          name.c_str(),
          _antennaStopSecs);
  
  fprintf(nagiosFile, "%s", text);
  if (_params.debug) {
    fprintf(stderr, "Adding nagios entry: ");
    fprintf(stderr, "%s", text);
  }

  return 0;

}
