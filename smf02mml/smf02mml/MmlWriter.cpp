
#include <algorithm>
#include <sstream>
#include <iomanip>

#include "appli.h"
#include "MmlWriter.h"

void MmlWriter::addEvent(uint8 spc_ch, string mml, uint32 tick, uint32 duration, bool defer)
{
	MmlWriter::MMLEvent ev(tick, duration * 48 / timebase, mml);

	if (defer) {
		// 発音中は登録を保留する
		deferlist.push_back(ev);
	}
	else {
		// deferがつかないイベント登録が行われたら、保留していたイベントを直後にまとめてフラッシュする
		mmltrk[spc_ch].push_back(ev);

		uint32 delayedtick = ev.tick + ev.duration;
		for (MMLEvent::List::iterator it = deferlist.begin(); it != deferlist.end(); it++) {
			it->tick = delayedtick;
			it->duration = 0;
			mmltrk[spc_ch].push_back(*it);
		}
		deferlist.clear();
	}
}

string MmlWriter::octchgmml(uint8 lastkey, uint8 newkey)
{
	uint8 lastoct = lastkey / 12;
	uint8 newoct = newkey / 12;
	bool inc = (lastoct < newoct);	// 次ノートのオクターブが上
	string mml("");

	if (inc) {
		for (int diff = 0; diff < (newoct - lastoct); ++diff) {
			mml += ">";
		}
	}
	else {
		for (int diff = 0; diff < (lastoct - newoct); ++diff) {
			mml += "<";
		}
	}
	return(mml);
}

string MmlWriter::d2h(int value)
{
	stringstream ss;

	ss << std::uppercase << std::hex << std::setfill('0') << std::setw(2) << int(value);

	return ss.str();
};

string MmlWriter::getSongInfoMML()
{
	string outbuf("");

	outbuf += "#songname " + songinfo.songtitle + "\n";
	outbuf += "#gametitle " + songinfo.gametitle + "\n";
	outbuf += "#artist " + songinfo.artist + "\n";
	outbuf += "#dumper " + songinfo.gametitle + "\n";
	if (songinfo.comment != "") {
		outbuf += "#comment " + songinfo.comment + "\n";
	}
	if (songinfo.brr_chk_echo_overlap) {
		outbuf += "#brr_echo_overcheck\n";
	}
	if (songinfo.surround) {
		outbuf += "#surround\n";
	}

	return outbuf;
}

string MmlWriter::getToneInfoMML()
{
	string outbuf("");

	for (vector<string>::iterator it = songinfo.tones.begin(); it != songinfo.tones.end(); ++it)
	{
		outbuf += "#tone " + *it + "\n";
	}

	return outbuf;
}


class MMLRest
{
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
	string makeblank(uint32 duration, uint16 timebase = 48) const
	{
		uint32 dul = duration * 48 / timebase;
		string notemml = "";

		if (dul >= 3) {		// 0tickの場合は出力不要
			makeRest(dul, 0xB6, notemml);
		}
		else {
			printf("rest duration is too short!!(%d, equal or more than 3)\n", dul);
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
			mml += MmlWriter::d2h(cr + keynumber);			// ノートを出力
		}
	};
public:
	MMLRest()
	{
		//			      1     2     4.    3     4     8.    6     8    12    16    24    32    48    64 [ｎ分音符]
		uint8 val[] = { 0xC0, 0x60, 0x48, 0x40, 0x30, 0x24, 0x20, 0x18, 0x10, 0x0C, 0x08, 0x06, 0x04, 0x03 };
		dur.assign(val, val + sizeof(val));
	};

	string toString(uint32 _duration, uint16 _timebase = 48) const
	{
		string buf;

		buf = makeblank(_duration, _timebase);

//		makeRest(_duration * 48 / _timebase, 0xB6, buf);

		return buf;
	};
};




string MmlWriter::getTrackMML(uint8 spcch)
{
	string outbuf("");
	uint32	trktime(0);

	std::sort(mmltrk[spcch].begin(), mmltrk[spcch].end());
	for (MMLEvent::List::iterator it = mmltrk[spcch].begin(); it != mmltrk[spcch].end(); ++it)
	{
		uint32	evttime = it->tick;

		if ((evttime - trktime) > 0) {
			MMLRest	rest;
			printf("track %d tick %d\n", spcch, evttime);
			outbuf += rest.toString(evttime - trktime, timebase);
		}
		outbuf += it->text;
		trktime = evttime + it->duration;
	}

	return outbuf;
}

int MmlWriter::WriteOut(string filename)
{
	FILE *fp;

	fopen_s(&fp, filename.c_str(), "w");
	if (fp == NULL) {
		printf("Error : Can't open %s.\n", filename.c_str());
		return(-1);
	}

	fprintf_s(fp, "%s\n", getSongInfoMML().c_str());		// SPC情報の出力
	printf("%s\n", getSongInfoMML().c_str());

	fprintf_s(fp, "%s\n", getToneInfoMML().c_str());		// 音色設定情報の出力
	printf("%s\n", getToneInfoMML().c_str());		// 音色設定情報の出力

	for (int spch = 0; spch < 8; ++spch)					// チャネルごとに出力
	{
		fprintf(fp,"#track %d\n", spch + 1);
		printf("#track %d\n", spch + 1);
		fprintf(fp,"%s\n", getTrackMML(spch).c_str());
		printf("%s\n", getTrackMML(spch).c_str());
	}

	fclose(fp);

	return 0;
}