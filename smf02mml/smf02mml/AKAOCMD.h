#pragma once
#include <string>
#include <vector>
#include "appli.h"
#include <sstream>		// for to_string(x16)

using namespace std;

class MMLCMD {
public:
	uint32	tick;
	MMLCMD(uint32 _tick) : tick(_tick) {};
	virtual string	toString() const { return ""; };		// MMLコマンドとして取得するメソッドは個別定義

	string i2h(int value) const {
		stringstream ss;

		ss << std::hex << std::uppercase << value;
		string res = ss.str();
		if (res.size() == 1) res = "0" + res;		// 0サフィックス
		return res;
	};

	typedef vector<MMLCMD *>	List;
};

class AKAOCMD {
public:
	enum eAKAOCMD {
		EVENT_NULL
		, EVENT_VOLUME
		, EVENT_ECHO_VOLUME
		, EVENT_PAN
		, EVENT_ADSR_DEFAULT
		, EVENT_NOTE
		, EVENT_TEMPO
		, EVENT_PROGCHANGE
		, EVENT_MASTER_VOLUME
		, EVENT_TRANSPOSE_ABS
		, EVENT_TRANSPOSE_REL
		, EVENT_TUNING
		, EVENT_VOLUME_ENVELOPE
		, EVENT_VOLUME_ALT
		, EVENT_VOLUME_FADE
		, EVENT_ADSR_AR
		, EVENT_ADSR_DR
		, EVENT_ADSR_SL
		, EVENT_ADSR_SR
		, EVENT_ECHO_FIR_RS3
		, EVENT_TEMPO_FADE
		, EVENT_PAN_FADE
		, EVENT_GAIN_RELEASE
		, EVENT_DURATION_RATE
		, EVENT_ONETIME_DURATION
		, EVENT_PITCH_SLIDE
		, EVENT_PITCH_SLIDE_ON
		, EVENT_VIBRATO_ON
		, EVENT_TREMOLO_ON
		, EVENT_PAN_LFO_ON
		, EVENT_NOISE_ON
		, EVENT_PITCHMOD_ON
		, EVENT_ECHO_ON
		, EVENT_SLUR_ON
		, EVENT_LEGATO_ON
		, EVENT_PERC_ON
		, EVENT_PITCH_SLIDE_OFF
		, EVENT_VIBRATO_OFF
		, EVENT_TREMOLO_OFF
		, EVENT_PAN_LFO_OFF
		, EVENT_NOISE_OFF
		, EVENT_PITCHMOD_OFF
		, EVENT_ECHO_OFF
		, EVENT_SLUR_OFF
		, EVENT_LEGATO_OFF
		, EVENT_PERC_OFF
		, EVENT_PAN_LFO_ON_WITH_DELAY
		, EVENT_NOISE_FREQ
		, EVENT_IGNORE_MASTER_VOLUME
		, EVENT_IGNORE_MASTER_VOLUME_BROKEN
		, EVENT_IGNORE_MASTER_VOLUME_BY_PROGNUM
		, EVENT_ECHO_VOLUME_FADE
		, EVENT_ECHO_FIR_FADE
		, EVENT_ECHO_FEEDBACK_FIR
		, EVENT_ECHO_FEEDBACK_FADE
	};

public:
	
};

class MMLRest : public MMLCMD {
	Bytes dur;
public:
	uint8  dulsel(uint16 dul) const
	{
		if (dul >= dur[0]) return(0);

		for (uint32 idx = 0; idx < dur.size(); ++idx) {
			if (dul >= dur[idx]) return((uint8)idx);
		}
		return 0;
	}
	string makeblank(uint32 duration, uint16 timebase = 48) {
		uint32 dul = duration * 48 / timebase;
		string notemml = "";
		if (dul > 0) {		// 0tickの場合は出力不要
			makeRest(dul, 0xB6, notemml);
		}

		return(notemml);
	};
	void makeRest(uint32 dulation, uint8 keynumber, string &mml) const
	{
		mml = "";
		int32 rest = dulation;

		while (rest > 0) {
			uint8 cr = dulsel(rest);			// 音長を選ぶ
			rest -= dur[cr];					// 音長さを減らす
			mml += i2h(cr + keynumber);			// ノートを出力
		}
	}
public:
	MMLRest(uint32 _tick, uint32 _duration, uint8 _key, uint8 _vel, uint16 _timebase = 48)
		: MMLCMD(_tick), key(_key), vel(_vel)
	{
		//    1     2     4.    3     4     8.    6     8    12    16    24    32    48    64 [ｎ分音符]
		uint8 val[] = { 0xC0, 0x60, 0x48, 0x40, 0x30, 0x24, 0x20, 0x18, 0x10, 0x0C, 0x08, 0x06, 0x04, 0x03 };
		dur.assign(val, val + sizeof(val));
		duration = _duration * 48 / _timebase;
	};
	uint32	duration;
	uint8	key;
	uint8	vel;

	string toString() const {
		string buf;
		makeRest(duration, key, buf);
		return buf;
	};
};


class MMLNote : public MMLCMD {
	Bytes dur;

	uint8  dulsel(uint16 dul) const
	{
		if (dul >= dur[0]) return(0);

		if (dul == 18) return 9;	// 18は16+32として扱う
		for (uint32 idx = 0; idx < dur.size(); ++idx) {
			if (dul >= dur[idx]) return((uint8)idx);
		}

		return 0;
	}
	void makeNote(uint32 dulation, uint8 keynumber, string &mml) const
	{
		mml = "";
		int32 rest = dulation;			// 長さはtimebase=48相当にしておくこと
		uint8 oct = keynumber / 12;
		uint8 key = keynumber - (oct * 12);

		while (rest > 0) {
			uint8 cr = dulsel(rest);	// 音長を選ぶ

			rest -= dur[cr];			// 音長を減らす
			mml += i2h(cr + (key * 14));		// ノートを出力
			if (rest > 0) {				// まだ続くなら次からタイに変更
				key = 12;				// key*14が"A8"になるようにしておく
			}
		}
	}

public:
	MMLNote(uint32 _tick, uint32 _duration, uint8 _key, uint8 _vel, uint16 _timebase = 48)
		: MMLCMD(_tick), key(_key), vel(_vel)
	{
		//				  1     2     4.    3     4     8.    6     8    12    16    24    32    48    64 [ｎ分音符]
		uint8 val[] = { 0xC0, 0x60, 0x48, 0x40, 0x30, 0x24, 0x20, 0x18, 0x10, 0x0C, 0x08, 0x06, 0x04, 0x03 };
		dur.assign(val, val + sizeof(val));
		duration = _duration * 48 / _timebase;
	};
	uint32	duration;
	uint8	key;
	uint8	vel;

	string toString() const {
		string nm[] = { "C", "C#", "D", "D#","E", "F", "F#", "G", "G#", "A", "A#", "B" };
		string buf;
		makeNote(duration, key, buf);
//		printf("Note %d %s\n", key, nm[key % 12].c_str());
		return buf;
	};
};
class MMLVolume : public MMLCMD {
public:
	MMLVolume(uint32 _tick, uint8 vol)
		: MMLCMD(_tick), volume(vol) {};
	uint8	volume;
	string toString() {
		return "C4 " + i2h(volume);
	};
};
class MMLPan : public MMLCMD {
public:
	MMLPan(uint32 _tick, uint8 _pan)
		: MMLCMD(_tick), pan(_pan) {};
	uint8	pan;
	string toString() {
		return "C6 " + i2h(pan);
	};
};
class MMLTempo : public MMLCMD {
public:
	MMLTempo(uint32 _tick, uint8 _tempo)
		: MMLCMD(_tick), tempo(_tempo) {};
	uint8	tempo;
	string toString() {
		return "F0 " + i2h(tempo);
	};
};

