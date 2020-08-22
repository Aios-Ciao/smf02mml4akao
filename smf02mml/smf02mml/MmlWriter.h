#pragma once
#include <vector>
#include <string>
#include "appli.h"

using namespace std;

class MmlWriter {
public:
	static const int SPC_CH = 8;
	struct SongInfo {
		string	songtitle;
		string	gametitle;
		string	artist;
		string	dumper;
		string	comment;
		string	length;
		string	brr_offset;
		bool	brr_chk_echo_overlap;
		string	echo_depth;
		bool	surround;
		vector<string>	tones;
	public:
		SongInfo() :
			songtitle("\"No SongTitle\""),
			gametitle("\"No GameTitle\""),
			artist("\"No info\""),
			dumper("\"No info\""),
			comment(""),
			length(""),
			surround(false),
			echo_depth(""),
			brr_offset(""),
			brr_chk_echo_overlap(true)
		{

		};
		~SongInfo() {};
	};
	struct MMLEvent {
		uint32		tick;
		uint32		duration;
		string		text;

		MMLEvent(uint32 _tick, uint32 _dur, string _text)
			: tick(_tick), duration(_dur), text(_text)
		{	};

		bool operator < (const MMLEvent &ev2) const {
			return(tick < ev2.tick);	// éûä‘èáÇ…É\Å[Ég
		};

		typedef vector<MMLEvent> List;
	};

	MMLEvent::List	mmltrk[8];
	MMLEvent::List	deferlist;

public:
	SongInfo		songinfo;		// ã»èÓïÒ
	uint16			timebase;

	MmlWriter(uint16 _timebase = 0) : timebase(_timebase) {};
	~MmlWriter() {};

	int WriteOut(string fname);

	void setTimebase(uint16 _timebase) { timebase = _timebase; };
	void addEvent(uint8 spc_ch, string mml, uint32 tick, uint32 duration = 0, bool defer = false);

	string octchgmml(uint8 lastkey, uint8 newkey);

	static string d2h(int value);
	string getSongInfoMML();
	string getToneInfoMML();
	string getTrackMML(uint8 spcch);
};


