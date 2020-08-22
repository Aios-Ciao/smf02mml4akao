#include <stdio.h>
#include <memory.h>
#pragma warning( disable : 4786)
#include <string>
#include <map>
#include <vector>
#include "appli.h"
#include "midif.h"
#include "AKAODrv.h"
#include "MmlWriter.h"

using namespace std;

class CmdWriter {
	static string val2hex(int value) {
		stringstream ss;
		ss << std::hex << std::uppercase << value;
		string res = ss.str();
		if (res.size() == 1) res = "0" + res;		// 0サフィックス
		return res;
	};
public:
	static string Volume(uint8 vv)
	{
		return(val2hex(0xC4) + val2hex(vv & 0x7F));
	};
	static string VolumeFade(uint8 tick, uint8 vv)
	{
		return(val2hex(0xC5) + val2hex(tick & 0xFF) + val2hex(vv & 0x7F));
	};
	static string Pan(uint8 pp)
	{
		return(val2hex(0xC6) + val2hex(pp & 0x7F));
	};
	static string PanFade(uint8 tick, uint8 pp)
	{
		return(val2hex(0xC7) + val2hex(tick & 0xFF) + val2hex(pp & 0x7F));
	};
	static string PitchSlide(uint8 tick, uint8 pp)
	{
		return(val2hex(0xC8) + val2hex(tick & 0xFF) + val2hex(pp & 0x7F));
	};
	static string Vibrate(uint8 delay, uint8 cyc, uint8 pp)
	{
		return(val2hex(0xC9) + val2hex(delay) + val2hex(cyc) + val2hex(pp));
	};
};

#if 0

class MID2MML {

public:
	class Command {

	public:
		typedef enum {
			NOP,
			NOTE_ON,	// 00-A7
			TIE,		// A8-B5
			REST,		// B6-C3
			VOL
		} CodeMnemonic;
	public:
		CodeMnemonic code;

	public:
		typedef vector<Command> List;
	};

	// Manage program assign for multi-timber (eg. drum part)
	class ProgramMng {
		uint8	progmap[8][128];
	public:
		ProgramMng() {
			Reset();
		};
		~ProgramMng() {};

		void Reset()
		{
			for (int ch = 0; ch < 8; ++ch)
				for (int pid = 0; pid < 128; ++pid)
					progmap[ch][pid] = 0;
		};
		void SetAll(uint8 prg, uint8 ch)
		{
			for (int pid = 0; pid < 128; ++pid)
				progmap[ch][pid] = prg;
		};
	};

	// Manage control parameters for track
	class ControlManage {			// Trigger event
		uint8	_trackvolume;		// CC:expression
		uint8	_panpot;			// CC:
		uint8	_echovol;

	};
public:
	MMLWriter::SongInfo	songinfo;
	MIDI::Event::List	ProgramEvents;		// プログラムチェンジのみ抜粋のイベントリスト
	Bytes dulations;

	MID2MML() {
		//    1     2     4.    3     4     8.    6     8    12    16    24    32    48    64 [ｎ分音符]
		uint8 val[] = { 0xC0, 0x60, 0x48, 0x40, 0x30, 0x24, 0x20, 0x18, 0x10, 0x0C, 0x08, 0x06, 0x04, 0x03 };
		dulations.assign(val, val + sizeof(val));
	};

	bool Load(MIDI::File &smf);

	void parseMetaEvent(MIDI::File &smf);
	//	void parseProgramEvent(MIDI::File &smf);
	void parseTrackEvent(MIDI::File &smf);

	uint32 timebase2resttick(uint32 dul, uint16 timebase);
	uint8  dulsel(uint16 dul);
	void makeRest(uint32 dulation, uint8 keynumber, string &mml);
	void makeNote(uint32 dulation, uint8 keynumber, string &mml);
	string val2hex(int value) {
		stringstream ss;
		ss << std::hex << std::uppercase << value;
		string res = ss.str();
		if (res.size() == 1) res = "0" + res;		// 0サフィックス
		return res;
	};
	string makeblank(uint32 tickpos, uint16 timebase) {
		uint32 dul = timebase2resttick(tickpos, timebase);
		string notemml = "";
		if (dul > 0) {		// 0tickの場合は出力不要
			makeRest(dul, 0xB6, notemml);
		}

		return(notemml);
	};
	uint32	getNoteOffEvIdx(MIDI::Event::List events, int32 &offevidx, uint32 searchbase, uint8 keyn, uint8 ch);
	void note2mml(MIDI::Event &event, uint8 &note, uint8 &octave, uint8 &velo);

	//	bool isProgChgEvent(MIDI::Event &ev);
	//	uint16 getNearestEventCh(vector<MIDI::Event::List> &tracks, vector<uint32> &evidx, uint32 orign);
};

bool MID2MML::Load(MIDI::File &smf)
{
	if (!smf.Valid()) return false;

	std::printf("Format      :%d\n", smf.Format());
	std::printf("Track Count :%d\n", smf.TrackCount());
	std::printf("Time Base   :%d\n", smf.Timebase());

	parseMetaEvent(smf);
	parseTrackEvent(smf);

	return true;
}
void MID2MML::parseMetaEvent(MIDI::File &smf)
{
	// 全トラックを対象にメタ情報を確認し、タイトルなどを抜き出して格納する
	for (int tid = 0; tid < smf.TrackCount(); ++tid) {
		MIDI::Track &track = smf.tracks[tid];
		for (uint32 eid = 0; eid < track.EventCount(); ++eid) {
			MIDI::Event &event = track.Events[eid];
			if (event.getStatusType() == MIDI::Event::StatusType::MetaMessage) {
				string text;

				switch (event.getMetaType()) {
				case MIDI::Event::MetaType::Text:
					if (event.getAsText(text)) {
						if (text.substr(0, 9 + 1) == "#songname ") {
							songinfo.songtitle = text.substr(10);
						}
						if (text.substr(0, 10 + 1) == "#gametitle ") {
							songinfo.gametitle = text.substr(11);
						}
						if (text.substr(0, 7 + 1) == "#artist ") {
							songinfo.artist = text.substr(8);
						}
						if (text.substr(0, 7 + 1) == "#dumper ") {
							songinfo.dumper = text.substr(8);
						}
						if (text.substr(0, 8 + 1) == "#comment ") {
							songinfo.dumper = text.substr(9);
						}
						if (text.substr(0, 7 + 1) == "#length ") {
							songinfo.length = text.substr(8);
						}
						if (text.substr(0, 11 + 1) == "#brr_offset ") {
							songinfo.brr_offset = text.substr(12);
						}
						if (text.substr(0, 19) == "#brr_echo_overcheck") {
							songinfo.brr_chk_echo_overlap = true;
						}
						if (text.substr(0, 11 + 1) == "#echo_depth ") {
							songinfo.echo_depth = text.substr(12);
						}
						if (text.substr(0, 9 + 1) == "#surround ") {
							songinfo.surround = true;
						}
						if (text.substr(0, 5 + 1) == "#tone ") {
							songinfo.tones.push_back(text.substr(6));
						}
						/* 以下は非対応
							#track		: トラック別出力なので自動出力
							#macro		: 何かに使えるかもしれないが現状未対応
						*/
					}
					break;
				default:
					break;
				}
			}
		}
	}

	printf("#songname %s\n", songinfo.songtitle.c_str());
	printf("#gametitle %s\n", songinfo.gametitle.c_str());
	printf("#artist %s\n", songinfo.artist.c_str());
	printf("#dumper %s\n", songinfo.dumper.c_str());
	printf("#comment %s\n", songinfo.comment.c_str());
	if (songinfo.surround) printf("#surrond\n");
	if (songinfo.brr_chk_echo_overlap) printf("#brr_echo_overcheck\n");
	for (uint32 toneidx = 0; toneidx < songinfo.tones.size(); ++toneidx) {
		printf("#tone %s\n", songinfo.tones[toneidx].c_str());
	}

}

// あとで作る
//void MID2MML::parseProgramEvent(MIDI::File &smf)
//{
//	uint16	trackc = smf.TrackCount();
//
//	vector<MIDI::Event::List>	trk;
//	vector<uint32> curtime(trackc, 0);
//	vector<uint32> eventidx(trackc, 0);
//
//	// プログラムチェンジ関連のイベントだけ抽出
//	for (int tid = 0; tid < smf.TrackCount(); ++tid) {
//		MIDI::Event::List	work;
//		MIDI::Track &track = smf.tracks[tid];
//		uint32	tickpos = 0;	// 現在位置
//
//		for (uint32 eid = 0; eid < track.EventCount(); ++eid) {
//			MIDI::Event &event = track.Events[eid];
//			tickpos += event.deltatime;
//			if (isProgChgEvent(event)) {
//				MIDI::Event newevent(tickpos, event.message);
//				trk[tid].push_back(newevent);
//			}
//		}
//		trk.push_back(work);
//	}
//
//
//}

// あとで作る
//uint16 getNearestEventCh(vector<MIDI::Event::List> &tracks, vector<uint32> &evidx, uint32 orign)
//{ }

// あとで作る
//bool MID2MML::isProgChgEvent(MIDI::Event &ev)
//{
//	bool result(false);
//	uint8	cc;
//	uint8	value;
//
//	switch (ev.getStatusType()) {
//	case MIDI::Event::StatusType::ProgramChange:
//		result = true;
//		break;
//	case MIDI::Event::StatusType::ControlChange:
//		ev.getControlValue(cc, value);
//		switch (cc) {
//		case 0:		// Bank select MSB
//		case 32:	// Bank select LSB
//		case 98:	// NRPN LSB
//		case 99:	// NRPN MSB
//			result = true;
//			break;
//		default:
//			break;
//		}
//	default:
//		break;
//	}
//
//	return (result);
//}

// MIDIのデルタタイムからMML(音符・休符)向けのticksに変換する
uint32 MID2MML::timebase2resttick(uint32 dul, uint16 timebase)
{
	uint32 ticks = dul * 48 / timebase;
	return(ticks);
}
void MID2MML::makeRest(uint32 dulation, uint8 keynumber, string &mml)
{
	mml = "";
	int32 rest = dulation;

	while (rest > 0) {
		uint8 cr = dulsel(rest);					// 音長を選ぶ
		rest -= dulations[cr];						// 音長さを減らす
		mml += val2hex(cr + keynumber) + " ";		// ノートを出力
	}
}
void MID2MML::makeNote(uint32 dulation, uint8 keynumber, string &mml)
{
	mml = "";
	int32 rest = dulation;

	while (rest > 0) {
		uint8 cr = dulsel(rest);	// 音長を選ぶ
		rest -= dulations[cr];		// 音長さを減らす
		mml += val2hex(cr + (keynumber * 14)) + " ";		// ノートを出力
		if (rest > 0) {				// まだ続くなら次からタイに変更
			keynumber = 0xA8;
		}
	}
}
uint8  MID2MML::dulsel(uint16 dul)
{
	if (dul >= dulations[0]) return(0);

	for (uint32 idx = 0; idx < dulations.size(); ++idx) {
		if (dul >= dulations[idx]) return((uint8)idx);
	}
	return 0;
}

// 指定のイベントに対応するNoteOFFを検索する
uint32	MID2MML::getNoteOffEvIdx(MIDI::Event::List events, int32 &offevidx, uint32 searchbase, uint8 keyn, uint8 ch)
{
	int32	result = -1;
	uint32	totaldul = 0;

	for (uint32 idx = searchbase + 1; idx < events.size(); ++idx) {
		MIDI::Event &evt = events[idx];
		totaldul += evt.deltatime;
		if (evt.chkCompare(MIDI::Event::Note_ON, ch, keyn)
			|| evt.chkCompare(MIDI::Event::Note_OFF, ch, keyn)
			) {
			result = idx;
			break;
		}
	}
	offevidx = result;

	return totaldul;
}

// ノートイベントから音高情報を取得
void MID2MML::note2mml(MIDI::Event &event, uint8 &note, uint8 &octave, uint8 &velo)
{
	uint8 keynumber, velocity;

	event.getNoteInfo(keynumber, velocity);
	// #C4 = 3Ch = 60

	octave = keynumber / 12;
	note = keynumber - (octave * 12);
	velo = velocity;
}

void MID2MML::parseTrackEvent(MIDI::File &smf)
{
	ProgramMng	prg;

	for (int tid = 0; tid < smf.TrackCount(); ++tid) {
		MIDI::Track &track = smf.tracks[tid];
		uint32	tickpos = 0;	// 現在位置
		uint8	defaultch = 0;	// デフォルトの音色番号
		uint8	lastoctave = 0;	// 前回のオクターブ
		uint8	lastvelo = 0;	// 前回のベロシティ
		bool oct_out = false;	// トラック最初のオクターブ情報出力

		prg.Reset();
		if (track.EventCount() == 0) continue;
		std::printf("#track %d\n", tid + 1);

		for (uint32 eid = 0; eid < track.EventCount(); ++eid) {
			MIDI::Event &event = track.Events[eid];
			tickpos += event.deltatime;

			// 各イベントについて
			switch (event.getStatusType()) {
			case MIDI::Event::StatusType::ProgramChange: {
				uint8 progno;
				event.getChannel(defaultch);		// 当該トラックのチャネル番号取り出し
				event.getProgramNumber(progno);
				prg.SetAll(progno, defaultch);
				if (tickpos > 0) {
					string w = makeblank(tickpos, smf.Timebase());
					std::printf("%s\t// Rest\n", w.c_str());
					tickpos = 0;
				}
				std::printf("@%d\n", progno);
			}
														 break;
			case MIDI::Event::StatusType::ControlChange: {
				uint8 cc, value;
				event.getControlValue(cc, value);
				switch (cc) {
				case 1:		// Modulation depth		実際の深さや周期は別設定
					if (tickpos > 0) {
						string w = makeblank(tickpos, smf.Timebase());
						std::printf("%s\t// Rest\n", w.c_str());
						tickpos = 0;
					}
					if (value >= 0x40)	std::printf("C9\t// Vibrato on\n");
					else				std::printf("CA\t// Vibrato off\n");
					break;
				case 10:	// Pan
					if (tickpos > 0) {
						string w = makeblank(tickpos, smf.Timebase());
						std::printf("%s\t// Rest\n", w.c_str());
						tickpos = 0;
					}
					std::printf("C6 d%d\t// Pan\n", value);
					break;
				case 11:	// Expression Controller
					if (tickpos > 0) {
						string w = makeblank(tickpos, smf.Timebase());
						std::printf("%s\t// Rest\n", w.c_str());
						tickpos = 0;
					}
					std::printf("C4 d%d\t// Volume\n", value);
					break;
				case 12:	// Effect Control1
					if (tickpos > 0) {
						string w = makeblank(tickpos, smf.Timebase());
						std::printf("%s\t// Rest\n", w.c_str());
						tickpos = 0;
					}
					if (value >= 0x40)	std::printf("D2\t// P.Mod on\n");
					else				std::printf("D3\t// P.Mod off\n");
					break;
				case 13:	// Effect Control2
					if (tickpos > 0) {
						string w = makeblank(tickpos, smf.Timebase());
						std::printf("%s\t// Rest\n", w.c_str());
						tickpos = 0;
					}
					if (value >= 0x40)	std::printf("D0\t// Noise on\n");
					else				std::printf("D1\t// Noise off\n");
					break;
				case 68:	// Legato footswitch
					if (tickpos > 0) {
						string w = makeblank(tickpos, smf.Timebase());
						std::printf("%s\t// Rest\n", w.c_str());
						tickpos = 0;
					}
					if (value >= 0x40)	std::printf("E4\t// Sler on\n");
					else				std::printf("E5\t// Sler off\n");
					break;
				case 64:	// Hold footswitch
					if (tickpos > 0) {
						string w = makeblank(tickpos, smf.Timebase());
						std::printf("%s\t// Rest\n", w.c_str());
						tickpos = 0;
					}
					if (value >= 0x40)	std::printf("E6\t// Legato on\n");
					else				std::printf("E7\t// Legato off\n");
					break;
				case 92:	// Tremolo depth
					if (tickpos > 0) {
						string w = makeblank(tickpos, smf.Timebase());
						std::printf("%s\t// Rest\n", w.c_str());
						tickpos = 0;
					}
					if (value >= 0x40)	std::printf("CB\t// Toremolo on\n");
					else				std::printf("CC\t// Toremolo off\n");
					break;
				case 94:	// Echo depth
					if (tickpos > 0) {
						string w = makeblank(tickpos, smf.Timebase());
						std::printf("%s\t// Rest\n", w.c_str());
						tickpos = 0;
					}
					if (value >= 0x40)	std::printf("D4\t// Echo on\n");
					else				std::printf("D5\t// Echo off\n");
					break;
				}
				break;
			case MIDI::Event::StatusType::MetaMessage:
				switch (event.getMetaType()) {
				case MIDI::Event::MetaType::SetTempo:
					if (tickpos > 0) {
						string w = makeblank(tickpos, smf.Timebase());
						std::printf("%s\t// Rest\n", w.c_str());
						tickpos = 0;
					}
					std::printf("F0 d%d\t// Tempo\n", (uint8)event.getTempo());
					break;
				}
				break;
			case MIDI::Event::StatusType::Note_ON:
			{
				uint8 key, vel, ch(0);
				int32 noffevt;
				event.getNoteInfo(key, vel);
				event.getChannel(ch);

				if (tickpos > 0) {
					string w = makeblank(tickpos, smf.Timebase());
					std::printf("%s\t// Rest\n", w.c_str());
					tickpos = 0;
				}

				uint32 notedul = getNoteOffEvIdx(track.Events, noffevt, eid, key, ch);
				if (noffevt != -1) {
					string mml("");
					uint8 mmlkey, mmloct, mmlvel;

					note2mml(event, mmlkey, mmloct, mmlvel);
					if (oct_out) {
						// オクターブ合わせ込み
						if (lastoctave != mmloct) {
							bool inc = (lastoctave < mmloct);	// 次ノートのオクターブが上
							if (inc) {
								for (int diff = 0; diff < (mmloct - lastoctave); ++diff) {
									std::printf(">");
								}
							}
							else {
								for (int diff = 0; diff < (lastoctave - mmloct); ++diff) {
									std::printf("<");
								}
							}
							lastoctave = mmloct;
						}
					}
					else {
						std::printf("D6 d%d ", mmloct);		// 初回オクターブ指定
						lastoctave = mmloct;
						oct_out = true;
					}
					if (lastvelo != mmlvel) {
						printf("C4 %02X ", mmlvel);
						lastvelo = mmlvel;
					}

					makeNote(timebase2resttick(notedul, smf.Timebase()), mmlkey, mml);
					std::printf("%s\t// Note\n", mml.c_str());
				}
				tickpos = 0;
			}
			break;
			case MIDI::Event::StatusType::Note_OFF:
				break;
			}
														 break;
			default:
				break;
			}

		}
	}

}

#endif

int main(int argc, char *argv[])
{
	std::printf("[smf0 to spcmake(akao4) 20200204 ]\n\n");

	if (argc < 5) {
		std::printf("usage smfspcmake.exe -i input.mid -o output.txt\n");
		getchar();
		return (-1);
	}

	char *fname_in = NULL;
	char *fname_out = NULL;

	// コマンドライン引数処理
	int argidx;
	for (argidx = 1; argidx < argc; argidx++) {
		if (strncmp(argv[argidx], "-i", 2) == 0) {
			fname_in = argv[argidx + 1];
			if (strncmp(fname_in + strlen(fname_in) - 4, ".mid", 4) != 0) {
				printf("Error : -i %s 入力ファイル名が.midではありません\n", fname_in);
			}
			argidx++;
		}
		else
			if (strncmp(argv[argidx], "-o", 2) == 0) {
				fname_out = argv[argidx + 1];
				if (strncmp(fname_out + strlen(fname_out) - 4, ".txt", 4) != 0) {
					printf("Error : -o %s 出力ファイル名が.txtではありません\n", fname_out);
				}
				argidx++;
			}
			else
			{
				printf("Error : 不明なオプション %s\n", argv[argidx]);
				getchar();
				return(-1);
			}
	}

	MIDI::File	smf;
	MmlWriter	mml;
	AKAODRV	asd(mml);

	std::printf("Load %s as SMF\n", fname_in);
	smf.Load(fname_in);

	asd.Parse(smf);
	asd.mml.WriteOut(fname_out);

	std::printf("Done. Press enter\n");
	getchar();

	return (0);
}

