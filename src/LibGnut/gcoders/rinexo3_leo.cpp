/**
*
* @verbatim
History
-1.0 xjhan  2019-11-22  creat the file.

@endverbatim
* Copyright (c) 2018, Wuhan University. All rights reserved.
*
* @file		rinexo3_leo.cpp
* @brief	read rinexo3 containing leo data.
*
* @author   xjhan , Wuhan University
* @version	1.0.0
* @date		2019-11-22
*
*/

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cstring>
#include <memory>
#include <algorithm>

#include "gcoders/rinexo3_leo.h"
#include "gdata/gobsgnss.h"

using namespace std;

namespace gnut {

	// constructor
	// ----------
	t_rinexo3_leo::t_rinexo3_leo(t_gsetbase* s, string version, int sz)
		: t_rinexo2(s, version, sz)
	{
	}


	// OBS-RINEX header
	//   - read individual lines (tmpsize once used and consumed)
	//   - read block of lines at once (tmpsize cummulated and then consumed)
	// ----------
	int t_rinexo3_leo::_decode_head()
	{
		// -------- "SYS / # / OBS TYPES" --------
		if (_line.substr(60, 19).find("SYS / # / OBS TYPES") != string::npos) {

			// read all lines at once
			string sys(1, _line[0]);

			if (sys == " ") {
				mesg(GERROR, "Error: SYS / # / OBS TYPES not recognized SYS!"); _irc++;
			}
			else {

				int num = str2int(_line.substr(3, 3));
				int nlines = (int)ceil(num / 13.0);         // up to 13 values/line
				int addsize = 0; string tmp;
				for (int nl = 0; nl < nlines; ++nl) {
					int irc = t_gcoder::_getline(tmp, _tmpsize + addsize);
					if (irc >= 0) { addsize += irc; }
					else { _complete = false; break; }       // incomplete RECORD
				}

				if (!_complete) return -1;

				for (int ii = 0; ii < num; ++ii) {                          // all linked lines were read
					string go = _line.substr(7 + 4 * (ii % (13)), 3);       // check existing vobstypes
					_fix_band(sys, go);                               // fix band
					GOBS obs = str2gobs(go);

					t_rnxhdr::t_vobstypes::iterator it;
					for (it = _mapobs[sys].begin(); it != _mapobs[sys].end(); ++it) {             // SUBSTITUTE
						if (it->first == obs) { it->second = 1; break; }  // scale fator still 1 (changed later)
					}

					//if( it == _mapobs[sys].end() ) _mapobs[sys].push_back( make_pair(obs, 1) );  // NEW
					_mapobs[sys].push_back(make_pair(obs, 1));  // NEW !! changed by ZHJ Some situations have some obstype Skipping may cause bugs

					if ((ii + 1) % 13 == 0 && ii + 1 < num)
						_tmpsize += t_gcoder::_getline(_line, _tmpsize); // get newline and check!

				} // loop over # observations
			}

			if (_spdlog ) {
				ostringstream lg; lg << "SYS / OBS TYPES: [" + sys + "] ";

				t_rnxhdr::t_vobstypes::const_iterator it;
				for (it = _mapobs[sys].begin(); it != _mapobs[sys].end(); ++it) {
					lg << gobs2str(it->first) << " ";
				}
				SPDLOG_LOGGER_INFO(_spdlog,  "rinexo3", lg.str());
			}


			// -------- "SYS / SCALE FACTOR" --------
		}
		else if (_line.substr(60, 18).find("SYS / SCALE FACTOR") != string::npos) {

			string sys(1, _line[0]);             // read all linked lines at once!

			if (sys == " ") {
				mesg(GERROR, "Error: SYS / SCALE FACTOR not recognized SYS!"); _irc++;
			}
			else {
				double fact = str2dbl(_line.substr(2, 4));
				string nobs = _line.substr(8, 2);

				int num = (nobs == "  ") ? 0 : str2int(nobs);  // blank = 0 (all observations)
				int nlines = (int)ceil(num / 12.0);           // up to 10 values/line
				int addsize = 0; string tmp;
				for (int nl = 0; nl < nlines; ++nl) {
					int cnt = t_gcoder::_getline(tmp, _tmpsize + addsize);
					if (cnt >= 0) { addsize += cnt; }
					else { _complete = false; break; }         // incomplete RECORD
				}

				if (!_complete) return -1;

				if (num == 0) {  // SYSTEM-specific (mapkey => SYS, e.g. 'G','R', ..)
					t_rnxhdr::t_vobstypes::iterator it;
					for (it = _mapobs[sys].begin(); it != _mapobs[sys].end(); ++it)
						it->second = fact;

				}
				else {  // satellite-specific (mapkey => SAT, e.g. 'G10', 'G20', ..)

					for (int ii = 0; ii < num; ++ii) {                       // all linked lines were read
						string go = _line.substr(11 + 4 * (ii % (12)), 3);
						_fix_band(sys, go); GOBS obs = str2gobs(go);  // fix band

						t_rnxhdr::t_vobstypes::iterator it;
						for (it = _mapobs[sys].begin(); it != _mapobs[sys].end(); ++it)
							if (it->first == obs) { it->second = fact; break; }

						if (it == _mapobs[sys].end())
							_mapobs[sys].push_back(make_pair(obs, fact));

						if ((ii + 1) % 12 == 0 && ii + 1 < num)
							_tmpsize += t_gcoder::_getline(_line, _tmpsize); // get newline and check!

					} // loop over # observations
				}

				if (_spdlog ) {
					ostringstream lg;
					lg << "SYS / SCALE FACTOR: [" << sys << "] ";

					t_rnxhdr::t_vobstypes::const_iterator it = _mapobs[sys].begin();
					while (it != _mapobs[sys].end()) {
						lg << " " << gobs2str(it->first);
						it++;
					}
					SPDLOG_LOGGER_INFO(_spdlog,  "rinexo3", lg.str());
				}
			}


			// -------- "SYS / PHASE SHIFT" --------
		}
		else if (_line.substr(60, 17).find("SYS / PHASE SHIFT") != string::npos) {

			string sys(1, _line[0]);             // read all linked lines at once!
			string obs = trim(_line.substr(2, 3));
			GOBS gobs = X;

			if (sys == " ") { mesg(GERROR, "Error: SYS / PHASE SHIFT not recognized SYS!");     _irc++; }
			else if (obs.empty()) { mesg(GERROR, "Error: SYS / PHASE SHIFT not recognized OBS:" + obs); _irc++; }
			else if ((gobs = str2gobs(obs)) == X) { mesg(GERROR, "Error: SYS / PHASE SHIFT not recognized OBS:" + obs); _irc++; }
			else {
				double qcyc = str2dbl(_line.substr(6, 8));
				string nsat = _line.substr(16, 2);
				int num = (nsat == "  ") ? 0 : str2int(nsat);  // blank = 0 (all satellites)
				int nlines = (int)ceil(num / 10.0);           // up to 10 values/line
				int addsize = 0; string tmp;
				for (int nl = 0; nl < nlines; ++nl) {
					int cnt = t_gcoder::_getline(tmp, _tmpsize + addsize);
					if (cnt >= 0) { addsize += cnt; }
					else { _complete = false; break; }         // incomplete RECORD
				}

				if (!_complete) return -1;

				if (num == 0) {  // SYSTEM-specific (mapkey => SYS, e.g. 'G','R', ..)
					t_rnxhdr::t_vobstypes::iterator it;
					for (it = _mapcyc[sys].begin(); it != _mapcyc[sys].end(); ++it) {
						if (it->first == gobs) { it->second = qcyc; break; }
					}

					if (it == _mapcyc[sys].end())
						_mapcyc[sys].push_back(make_pair(gobs, qcyc));

				}
				else {  // satellite-specific (mapkey => SAT, e.g. 'G10', 'G20', ..)

					for (int ii = 0; ii < num; ++ii) {                         // all linked lines were read
						string sat = _line.substr(19 + 4 * (ii % (10)), 3);
						t_rnxhdr::t_vobstypes::iterator it;

						for (it = _mapcyc[sat].begin(); it != _mapcyc[sat].end(); ++it) {
							if (it->first == gobs) { it->second = qcyc; break; }
						}

						if (it == _mapcyc[sat].end())
							_mapcyc[sat].push_back(make_pair(gobs, qcyc));

						if ((ii + 1) % 10 == 0 && ii + 1 < num)
							_tmpsize += t_gcoder::_getline(_line, _tmpsize); // get newline and check!
					}
				}

				// comments
				if (_spdlog) {
					ostringstream lg; lg << "SYS / PHASE SHIFT: [" + sys + "] ";
					t_rnxhdr::t_obstypes::const_iterator it;
					t_rnxhdr::t_vobstypes::const_iterator itOBS;
					for (it = _mapcyc.begin(); it != _mapcyc.end(); ++it) {
						for (itOBS = it->second.begin(); itOBS != it->second.end(); ++itOBS) {
							if (it->first[0] == sys[0] && itOBS->first == gobs) { // [0] => SYS or PRN
								lg << " " << it->first << ":" << gobs2str(itOBS->first) << " " << itOBS->second;
							}
						}
					}
					SPDLOG_LOGGER_INFO(_spdlog,  "rinexo3", lg.str());
				}
			}


			// -------- "GLONASS SLOT / FRQ # --------
		}
		else if (_line.substr(60, 20).find("GLONASS SLOT / FRQ #") != string::npos) {

			int nsat = str2int(_line.substr(0, 3)); // read all linked lines at once!
			GOBS gobs = X;                           // X: ALL OBS!

			if (nsat == 0) {
				mesg(GERROR, "Error: GLONASS SLOT / FRQ zero obs codes!"); _irc++;
			}
			else {
				int nlines = (int)ceil(nsat / 8.0);           // up to 8 values/line
				int addsize = 0; string tmp;
				for (int nl = 0; nl < nlines; ++nl) {
					int cnt = t_gcoder::_getline(tmp, _tmpsize + addsize);
					if (cnt >= 0) { addsize += cnt; }
					else { _complete = false; break; }         // incomplete RECORD
				}

				if (!_complete) return -1;

				for (int ii = 0; ii < nsat; ++ii) {                         // all linked lines were read
					string sat = _line.substr(4 + 7 * (ii % (8)), 3);
					double frq = str2dbl(_line.substr(7 + 7 * (ii % (8)), 3));

					t_rnxhdr::t_vobstypes::iterator it;
					for (it = _glofrq[sat].begin(); it != _glofrq[sat].end(); ++it)
						if (it->first == gobs) { it->second = frq; break; }

					if (it == _glofrq[sat].end())
						_glofrq[sat].push_back(make_pair(gobs, frq));

					if ((ii + 1) % 8 == 0 && ii + 1 < nsat)
						_tmpsize += t_gcoder::_getline(_line, _tmpsize); // get newline and check
				}

				map<string, t_gdata*>::iterator itDAT = _data.begin();
				while (itDAT != _data.end())
				{
					if (itDAT->second->id_type() == t_gdata::ALLOBS)
					{
						for (auto itfrq : _glofrq)
						{
							for (auto itObs : itfrq.second)
							{
								if (itObs.first != GOBS::X) continue;
								((t_gallobs*)itDAT->second)->add_glo_freq(itfrq.first, itObs.second);
							}
						}
					}
					itDAT++;
				}
				// ------------------------------------------------------------------

				// comments
				if (_spdlog ) {
					ostringstream lg; lg << "GLO SLOT/FRQ:";
					t_rnxhdr::t_obstypes::const_iterator it;
					t_rnxhdr::t_vobstypes::const_iterator itOBS;
					for (it = _glofrq.begin(); it != _glofrq.end(); ++it) {
						for (itOBS = it->second.begin(); itOBS != it->second.end(); ++itOBS) {
							if (itOBS->first == gobs) lg << " " << it->first << ":" << itOBS->second;
						}
					}
					SPDLOG_LOGGER_INFO(_spdlog,  "rinexo3", lg.str());
				}
			}


			// -------- "GLONASS COD/PHS/BIS --------
		}
		else if (_line.substr(60, 19).find("GLONASS COD/PHS/BIS") != string::npos) {

			int nobs = 4;
			for (int ii = 0; ii < nobs; ++ii) {                  // single line (4 fields or less)

				string  obs = trim(_line.substr(1 + 13 * (ii % (4)), 3)); if (obs == "") break;
				string  val = trim(_line.substr(5 + 13 * (ii % (4)), 8));
				GOBS   gobs = str2gobs(obs);
				double bias = str2dbl(val);
				if (gobs == X) { mesg(GERROR, "Error: GLONASS COD/PHS/BIS not recognized OBS:" + obs); _irc++; }
				else {
					t_rnxhdr::t_vobstypes::iterator it;
					for (it = _globia.begin(); it != _globia.end(); ++it) {
						if (it->first == gobs) { it->second = bias; break; }
					}

					if (it == _globia.end()) _globia.push_back(make_pair(gobs, bias));
				}
			}

			// comments
			if (_spdlog ) {
				ostringstream lg; lg << "GLO COD/PHA BIAS:";
				t_rnxhdr::t_vobstypes::const_iterator itOBS;
				for (itOBS = _globia.begin(); itOBS != _globia.end(); ++itOBS) {
					lg << " " << gobs2str(itOBS->first) << ":" << itOBS->second;
				}
				SPDLOG_LOGGER_INFO(_spdlog,  "rinexo3", lg.str());
			}


			// -------- "RCV CLOCK OFFS APPL" -------- 
		}
		else if (_line.substr(60, 19).find("RCV CLOCK OFFS APPL") != string::npos) {
			if (_spdlog) SPDLOG_LOGGER_INFO(_spdlog,  "rinexo3", "RCV CLOCK OFFS APPL: " + string("not implemented"));


			// --------"# OF SATELLITES" --------
		}
		else if (_line.substr(60, 15).find("# OF SATELLITES") != string::npos) {

			_rnxhdr.numsats(str2int(_line.substr(0, 6)));
			if (_spdlog) SPDLOG_LOGGER_INFO(_spdlog,  "rinexo3", "# OF SATELLITES: " + int2str(_rnxhdr.numsats()));


			// --------"PRN / # OF OBS" --------
		}
		else if (_line.substr(60, 20).find("PRN / # OF OBS") != string::npos) {
			if (_spdlog) SPDLOG_LOGGER_INFO(_spdlog,  "rinexo3", "PRN / # OF OBS: " + string("not implemented"));


			// -------- "SIGNAL STRENGTH UNIT" --------
		}
		else if (_line.substr(60, 20).find("SIGNAL STRENGTH UNIT") != string::npos) {
			_rnxhdr.strength(trim(_line.substr(0, 20)));
			if (_spdlog) SPDLOG_LOGGER_INFO(_spdlog,  "rinexo3", "SIGNAL STRENGTH UNIT: " + _rnxhdr.strength());


			// -------- "SYS / DCBS APPL" --------
		}
		else if (_line.substr(60, 15).find("SYS / DCBS APPL") != string::npos) {
			if (_spdlog) SPDLOG_LOGGER_INFO(_spdlog,  "rinexo3", "SYS / DCBS APPL: " + string("not implemented"));


			// -------- "SYS / PCVS APPL" --------
		}
		else if (_line.substr(60, 15).find("SYS / PCVS APPL") != string::npos) {
			if (_spdlog) SPDLOG_LOGGER_INFO(_spdlog,  "rinexo3", "SYS / PCVS APPL: " + string("not implemented"));


		}
		else if (t_rinexo2::_decode_head()) {};

		return 1;
	}


	// OBS-RINEX body
	//   - read block of lines for each epoch (tmpsize cummulated and then consumed)
	// ----------
	int t_rinexo3_leo::_decode_data()
	{
		int irc = t_rinexo3_leo::_read_epoch();

		//if( _flag != '0' && _flag != '1' ){ // special event handling
		//  if( _log ) SPDLOG_LOGGER_INFO(_spdlog, "rinexo2",_epoch.str_ymdhms("RINEX 3.x special events not handled after "));
		//  else                         cerr << _epoch.str_ymdhms("RINEX 3.x special events not handled after ") << "\n";

		  // not ready to read
		if (irc < 0) { _complete = false; return -1; }

		// skip special lines
		if (irc > 0) {
			for (int i = 0; i < _nsat; ++i) {
				int sz = t_gcoder::_getline(_line, _tmpsize);
				if (sz <= 0) { _complete = false; return -1; }
				_tmpsize += sz;
			}

			t_gcoder::_consume(_tmpsize);
			_consume += _tmpsize;
			_tmpsize = 0;
			return 0;
		}


		// DATA READING 
		// ------------
		// loop over satellits records (x-lines)
		for (int i = 0; i < _nsat; i++) {

			string sat = "";
			string key = "";

			// loop over all observation types for system/satellite key in _mapobs
			if (t_rinexo3_leo::_read_obstypes(sat, key) < 0) { _complete = 0; return -1; }
		}

		if (_complete) {

			_count += this->_fill_data();
			t_gcoder::_consume(_tmpsize);
			_consume += _tmpsize;
			_tmpsize = 0;
		}

		return 0;
	}


	// correct reading stop
	// ----------
	int t_rinexo3_leo::_check_head()
	{
		t_rinexo2::_check_head();

		if (_rnxhdr.rnxsys() == ' ') { mesg(GERROR, "Error: RINEX SYS not available!");       _irc++; } // mandatory
		if (_rnxhdr.mapcyc().size() == 0) { mesg(GERROR, "Error: SYS PHASE SHIFT not available!"); _irc++; } // mandatory
		if (_rnxhdr.glofrq().size() == 0) { mesg(GERROR, "Error: GLO SLOT/FREQ not available!");   _irc++; } // mandatory

		// only if not GEODETIC or NON_GEODETIC (i.e. to identify moving platform!)
	  //if( _rnxhdr.marktype().empty() ){ mesg(GERROR,"Error: MARKER TYPE not available!");     _irc++; } // optional

		if (_version >= "3.02" &&
			_rnxhdr.globia().size() == 0) {
			mesg(GERROR, "Error: GLO BIASES not available!");      _irc++;
		} // mandatory

		return _irc;
	}


	// read epoch & number of satellites, return flag
	// ----------
	int t_rinexo3_leo::_read_epoch()
	{
		_nsat = 0;
		_flag = 'X';


		// not enough data to recognize standard epoch or special event
		if (_line.length() < 35) return _stop_read();
		_flag = _line[31]; 
		_nsat = str2int(_line.substr(32, 3));

		if (_line.substr(1, 6) == "      ") {
			switch (_flag) {
			case '2':
				if (_spdlog) SPDLOG_LOGGER_INFO(_spdlog,  "rinexo3", "Warning: start moving antenna identified, but not yet implemented!");
				mesg(GWARNING, "Warning: start moving antenna identified, but not yet implemented!");
				return 1;

			case '3':
				if (_spdlog) SPDLOG_LOGGER_INFO(_spdlog,  "rinexo3", "Warning: new site occupation identified, but not yet implemented!");
				mesg(GWARNING, "Warning: new site occupation identified, but not yet implemented!");
				return 1;

			case '4':
				if (_spdlog) SPDLOG_LOGGER_INFO(_spdlog,  "rinexo3", "Warning: new header information identified, but not fully implemented!");
				mesg(GWARNING, "Warning: new header information identified, but not fully implemented!");
				return 1;

			case '5':
				if (_spdlog) SPDLOG_LOGGER_INFO(_spdlog,  "rinexo3", "Warning: external event identified, but not yet implemented!");
				mesg(GWARNING, "Warning: external event identified, but not yet implemented!");
				return 1;
			}
		}
		string dummy;
		int yr, mn, dd, hr, mi;
		double sc;
		istringstream is(_line);
		is.clear();

		is >> dummy >> yr >> mn >> dd >> hr >> mi >> sc >> _flag >> _nsat;

		// check success
		if (is.fail()) { 
			return _stop_read();
		}

		_epoch.from_ymdhms(yr, mn, dd, hr, mi, sc);

		// filter out data
		if (_flag != '0' &&
			_flag != '1') {
			return  1;
		}
		if (_epoch > _end) { if (_epoch > _epo_end) { _epo_end = _epoch; } _xend++; return  2; }
		if (_epoch < _beg) { if (_epoch < _epo_beg) { _epo_beg = _epoch; } _xbeg++; return  2; }

		if (!_filter_epoch(_epoch)) { _xsmp++; return 2; }

		return 0;
	}


	// read satellite observation types
	// ----------
	int t_rinexo3_leo::_read_obstypes(const string& sat, const string& sys)
	{
		int ii = 0;
		int addsize = 0;
		unsigned int idx = 0;

		// general sys- or sat- specific group of observations
		string key = sat;
		t_rnxhdr::t_obstypes::const_iterator itMAP = _mapobs.find(sat);
		if (itMAP == _mapobs.end()) { key = sys; }

		string tmpsat("");

		// read new line for individual satellite (complete observations)
		if ((addsize = t_gcoder::_getline(_line, _tmpsize)) >= 3) {
			_tmpsize += addsize;
			tmpsat = t_gsys::eval_sat_addleo(_line.substr(0, 3), t_gsys::char2gsys(_line[0]));//add leo

		}
		else  return _stop_read();

		//add leo
		t_spt_gobs obs;
		obs = make_shared<t_gobsgnss>(_spdlog,_site, tmpsat, _epoch);

		//add leo
		// loop over sys-defined observation types
		t_rnxhdr::t_vobstypes obstypes;
		if (tmpsat[0] < 'a')
			obstypes = _mapobs["G"];
		else
			obstypes = _mapobs[tmpsat.substr(0, 1)];

		t_rnxhdr::t_vobstypes::const_iterator it = obstypes.begin();
		unsigned int len;
		while (_complete && (it != obstypes.end())) {

			idx = 3 + 16 * ii;
			len = _line.length();

			// getline succeed only if 'EOL' found
			if (len <= 0 && len < (idx + 14)) return _stop_read();

			// check completness (excluding last SNR+LLI - sometimes missing)
			if (len < (idx + 14)) { _null_log(tmpsat, gobs2str(it->first)); }
			else { _read_obs(idx, it, obs); }

			ii++; it++;

		} // while (over observation types)

		_vobs.push_back(obs);

		return 1;
	}


	// fix band
	// ----------
	int t_rinexo3_leo::_fix_band(string sys, string& go)
	{
		// EXCEPTION FOR BEIDOU (B1 --> B2)
		if (t_gsys::str2gsys(sys) == BDS)
		{
			if (go[1] == '1') {
				go[1] = '2'; // change B1 -> B2 !!!
				if (_version != "3.02") {
					if (_spdlog) 
						SPDLOG_LOGGER_INFO(_spdlog, "rinexo3", "Warning: BDS band changed: B1->B2");
					else               cerr << "rinexo3 - Warning: BDS band changed: B1->B2\n";
					mesg(GWARNING, "Warning: BDS band changed: B1->B2 for RINEX " + _version);
				}
			}
			if (go[1] == '3') {
				go[1] = '6'; // for B3 (change C3I -> C6I or so on !!!)
				if (_version != "3.02") {
					if (_spdlog) SPDLOG_LOGGER_INFO(_spdlog, "rinexo3", "Warning: BDS band changed: 3->6");
					else               cerr << "rinexo3 - Warning: BDS band changed: 3->6\n";
					mesg(GWARNING, "Warning: BDS band changed: B1->B2 for RINEX " + _version);
				}
			}
		}
		return 1;
	}

} // namespace
