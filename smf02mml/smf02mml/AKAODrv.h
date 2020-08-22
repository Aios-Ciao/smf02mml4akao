#pragma once
#include <vector>
#include <string>
#include <algorithm>

#include "appli.h"
#include "midif.h"
#include "AKAOCMD.h"
#include "MmlWriter.h"

using namespace std;


// Akao driver as MIDI Sequencer
class AKAODRV {

private:
	class MIDITrack {	// MIDIのトラックとほぼ同様。SPCへの変換から見たトラック単位
		class ProgramMng {
			union {
				uint16	word;
				struct {
					uint8	prog;
					uint8	bank;
				} byte;
			}		progmap[128];	// キー別の音色番号
			bool	pnCommon;		// 音色番号をキー別に持つか共通に持つか
		public:
			ProgramMng()
				: pnCommon(true)
			{
				Reset();
			};
			~ProgramMng() {};

			void Reset()
			{
				pnCommon = true;
				for (int pid = 0; pid < 128; ++pid)
					progmap[pid].word = 0;
			};
			void SetAll(uint8 prg, uint8 bank)
			{
				pnCommon = true;
				for (int pid = 0; pid < 128; ++pid) {
					progmap[pid].byte.bank = bank;
					progmap[pid].byte.prog = prg;
				}
			};
			void Set(uint8 key, uint8 prg, uint8 bank)
			{
				pnCommon = false;
				progmap[key].byte.bank = bank;
				progmap[key].byte.prog = prg;
			};
			bool chkCommonProg() const { return(pnCommon); };
			inline uint8 GetTone(uint8 key = 0) const { return progmap[key].byte.prog; };	// キー共通とキー個別
			inline uint8 GetBank(uint8 key = 0) const { return progmap[key].byte.bank; };	// キー共通とキー個別
			uint8 GetProgram(uint8 key = 0) const {
				// バンクは100足す
				return(GetBank(key) * 100 + GetTone(key));
			};
		};
		class NotesMng {
		public:
			MIDI::Event::List	Notes;

			bool isNoteON(MIDI::Event &event) {
				bool result(false);

				if (Notes.size() == 0) return false;
				for (MIDI::Event::List::iterator it = Notes.begin(); it != Notes.end(); ++it) {
					uint8 key, vel, nn, vl;

					it->getNoteInfo(key, vel);
					event.getNoteInfo(nn, vl);
					if (key == nn) {
						result = true;
						break;
					}
				}
				return(result);
			};
			void pushKey(MIDI::Event &event) {
				MIDI::Event ev(event);

				Notes.push_back(ev);
			};
			uint32 releaseKey(MIDI::Event &event, uint32 *ontick = NULL, uint8 *vel = NULL) {
				uint32 duration(0);

				for (MIDI::Event::List::iterator it = Notes.begin(); it != Notes.end(); ++it) {
					uint8 ch(0), key(0), onvel(0);

					it->getChannel(ch);
					it->getNoteInfo(key, onvel);
					if (it->chkCompare(MIDI::Event::Note_ON, ch, key)) {

						duration = event.absolutetime - it->absolutetime;

						if (ontick) *ontick = it->absolutetime;
						if (vel) *vel = onvel;
						it = Notes.erase(it);

						break;
					}
				}

				return(duration);
			};
			bool isSounding() {
				return(Notes.size() > 0);			// 発音していたらtrue
			};
		};
	private:
		uint8			pn[4];			// Parameter Number NRPN/RPN
		uint8			deL, deM;		// Data Entry
		bool			enRPN, enNRPN;	// データがそろっていればtrue
	public:
		NotesMng		NoteStatus;
		ProgramMng		Programs;
		uint8			_midch;			// 当該トラックの基本チャネル(0-15)
		uint8			_spcch;			// 出力先のspcチャネル(1-8)

		MIDITrack() : _spcch(0), _midch(0), enRPN(false), enNRPN(false)
		{
			pn[0] = pn[1] = pn[2] = pn[3] = 0xFF;
		};
		~MIDITrack() {};
	public:
		void AssignCh(uint8 ch) {		// カレントのMIDIトラックで扱う基準chを退避
			if ((1 <= ch) && (ch <= 8)) {
				_spcch = ch;
			}
			else {
				printf("Warn:範囲外のMIDIチャネルが指定されました。\n");
			}
		};
		void AssignCh(string ch) {
			AssignCh(stoi(ch));
		};
		uint8 spcch() const { return _spcch; };

		uint8	BankSelectM, BankSelectL;

		uint8 getNRPN_MSB() const { return(pn[1]); };
		uint8 getNRPN_LSB() const { return(pn[0]); };
		void setNRPN_MSB(uint8 msb) {
			if (pn[0] != 0xFF) { enNRPN = true; enRPN = false; }
			pn[1] = msb & 0x7F;
//			std::printf("Set NRPN MSB %d\n", pn[1]);
		};
		void setNRPN_LSB(uint8 lsb) {
			if (pn[1] != 0xFF) { enNRPN = true; enRPN = false; }
			pn[0] = lsb & 0x7F;
//			std::printf("Set NRPN LSB %d\n", pn[0]);
		};
		uint8 getRPN_MSB() const { return(pn[3]); };
		uint8 getRPN_LSB() const { return(pn[2]); };
		void setRPN_MSB(uint8 msb) {
			if (pn[2] != 0xFF) { enNRPN = false; enRPN = true; }
			pn[3] = msb & 0x7F;
//			std::printf("Set RPN MSB %d\n", pn[3]);
		};
		void setRPN_LSB(uint8 lsb) {
			if (pn[3] != 0xFF) { enNRPN = false; enRPN = true; }
			pn[2] = lsb & 0x7F;
//			std::printf("Set RPN LSB %d\n", pn[2]);
		};
		uint8 getDEtry_MSB() const { return(deM); };
		uint8 getDEtry_LSB() const { return(deL); };
		void setDEtry_MSB(uint8 msb) {
			deM = msb & 0x7F;
//			std::printf("Data Entry MSB %d\n", deM);
		};
		void setDEtry_LSB(uint8 lsb) {
			deL = lsb & 0x7F;
//			std::printf("Data Entry LSB %d\n", deL);
		};
		bool isNRPN_Valid() const { return enNRPN; };
		bool isRPN_Valid() const { return enRPN; };
	public:
		typedef vector<MIDITrack> List;
	};

	string d2h(uint8 val);
	string d2s(uint8 value);

public:
	class SPCCh {		// Control Change for MIDI
	public:
		uint8	Volume;				// [C4]	00-7F
		uint8	Pan;				// [C6]	00-7F
		uint8	VibDelay;			// [C9/A]	0-FF
		uint8	VibCyc;				// ↑		0-FF
		uint8	VibPrm;				//			0-FF
		uint8	TrmDelay;			// [CB/C]	0-FF
		uint8	TrmCyc;				// ↑		0-FF
		uint8	TrmPrm;				//			0-FF
		bool	PitchModulation;	// [D2/3]
		bool	NoiseSw;			// [D0/1]
		bool	EchoSw;				// [D4/5]
		uint8	Transpose;			// [D9] (DA)
		char	Detune;				// [DB]		0x80-0-0x7F
		uint8	AttackR;			// [DD] 0-F
		uint8	DecayR;				// [DE] 0-7
		uint8	SustainLv;			// [DF] 0-7
		uint8	SustainR;			// [E0] 0-1F
		bool	Slur;				// [E4/5]
		bool	Legato;				// [E6/7]

		bool	bOctOut;			// オクターブ情報出力済みか
		uint8	lastNote;			// 最後に出力したノート情報
		uint8	lastVel;			// 最後に出力したベロシティ情報
		bool	bPrgOut;			// 音色情報出力済みか
		uint16	lastProg;			// 最後に出力した音色情報
	public:

		SPCCh() :
			Volume(0), Pan(64), Transpose(0), Detune(0),
			VibDelay(0), VibCyc(0), VibPrm(0),
			TrmDelay(0), TrmCyc(0), TrmPrm(0),
			NoiseSw(false), PitchModulation(false), EchoSw(false),
			AttackR(0xF), DecayR(0x7), SustainLv(0), SustainR(0x0),
			Slur(false), Legato(false),
			bOctOut(false), lastNote(0xFF), lastVel(0),
			bPrgOut(false), lastProg(0xFFFF)
		{

		};
	};
	class Master {
	public:
		uint8	MasterVolume;	// [F4]	0-0xFF
		uint8	EchoVolume;		// [F2] 90-7E
		uint8	NoiseClk;		// [CF]	0-0x1F

		Master() : MasterVolume(0), EchoVolume(0), NoiseClk(0) {};
		~Master() {};
	};
	class DSP {
	public:
		Master	All;
		SPCCh	Ch[8];

		DSP() {};
		~DSP() {};
	};
public:
	DSP					dsp;
	uint16				timebase;
	MIDITrack::List		miditrk;
	MmlWriter			mml;

	AKAODRV(MmlWriter &_mml) :mml(_mml) {};
	~AKAODRV() {};

	bool Parse(MIDI::File &smf)
	{
		MIDI::Event::List	merged;

		uint16	trkcnt = smf.TrackCount();
		vector<uint32> evtidx(trkcnt, 0);

		timebase = smf.Timebase();

		mml.setTimebase(timebase);
//		mml.setTrackSize(trkcnt);		// 各種シーケンサパラメータ初期化
		miditrk.clear();
		for (int i = 0; i < trkcnt; ++i) {
			miditrk.push_back(MIDITrack());
		}

		while (true) {
			int16 curtrk(-1);
			uint32 curtime(0);
			int16 nextEventTrack;

			// 全トラックのイベントを１つの時間軸上にマージする
			nextEventTrack = getNearestEventTrack(smf.tracks, evtidx, curtrk, curtime);
			if (nextEventTrack == -1) break;
			MIDI::Event &event = smf.tracks[nextEventTrack].Events[evtidx[nextEventTrack]];
			event.track = nextEventTrack;
			merged.push_back(event);

			curtime = event.absolutetime;
			evtidx[nextEventTrack]++;
			curtrk = nextEventTrack;
		}
//		std::sort(merged.begin(), merged.end());

		for (MIDI::Event::List::iterator it = merged.begin(); it != merged.end(); ++it) {
			EventProc(*it);
		}
		return true;
	};
	// 直近のイベントがあるトラックを検索する
	int16 getNearestEventTrack(MIDI::Track::List &tracks, vector<uint32> &evidxs, uint16 curtrk, uint32 curtime)
	{
		uint32 tm = 0xFFFFFFFF;
		uint16 trkcnt = (uint16)tracks.size();
		int16 result = -1;	// みつからない

		for (uint16 trk = 0; trk < trkcnt; ++trk) {
			if (++curtrk >= trkcnt) curtrk = 0;			// 次のトラックから巡回検索
			MIDI::Track &track = tracks[curtrk];
			if (evidxs[curtrk] >= track.EventCount()) continue;	// 最後のイベントまで到達したら次トラックへスキップ

			MIDI::Event &event = track.Events[evidxs[curtrk]];
			if (tm > (event.absolutetime - curtime)) {
				tm = (event.absolutetime - curtime);
				result = curtrk;
			}
		}

		return(result);
	};

	void EventProc(MIDI::Event &event)
	{
		switch (event.getStatusType()) {
		case MIDI::Event::Note_ON:			EventNoteON(event);			break;
		case MIDI::Event::Note_OFF:			EventNoteOFF(event);		break;
		case MIDI::Event::ProgramChange:	EventProgramChange(event);	break;
		case MIDI::Event::ControlChange:	EventControlChange(event);	break;
		case MIDI::Event::MetaMessage:		EventMetaMessage(event);	break;
		case MIDI::Event::SysExclusive:		EventSysExMessage(event);	break;
		default:														break;
		}
	};

	void EventNoteON(MIDI::Event &event)
	{
		uint8 midich(0), notekey(0), velo(0);

		uint16 evttrk_mid = event.track;
		uint8 spctrk = miditrk[evttrk_mid]._spcch;
		if (!spctrk) { printf("Unspecified at Track #%d\n", evttrk_mid); return; }
		spctrk--;

		if (event.getData2() == 0x00) {
			std::printf("Velzero Note On\n");
			EventNoteOFF(event);			// Velocity == 0 as Note off
			return;
		}

		midich = event.getChannel();
		event.getNoteInfo(notekey, velo);
		// 音色切り替え出力
		if (!dsp.Ch[spctrk].bPrgOut) {
			if (miditrk[evttrk_mid].Programs.chkCommonProg())		// キー共通音色
			{
				mml.addEvent(spctrk, "\n@" + d2s(miditrk[evttrk_mid].Programs.GetProgram()) + "\n", event.absolutetime);
				dsp.Ch[spctrk].lastProg = miditrk[evttrk_mid].Programs.GetProgram();
			}
			else
			{
				mml.addEvent(spctrk, "\n@" + d2s(miditrk[evttrk_mid].Programs.GetProgram(notekey)) + "\n", event.absolutetime);
				dsp.Ch[spctrk].lastProg = miditrk[evttrk_mid].Programs.GetProgram(notekey);
			}
			dsp.Ch[spctrk].bPrgOut = true;		// 初回出力
		}
		else {
			if (miditrk[evttrk_mid].Programs.chkCommonProg())		// キー共通音色
			{
				// 直前までに出力した音色番号と異なる場合、出力する
				if(dsp.Ch[spctrk].lastProg != miditrk[evttrk_mid].Programs.GetProgram())
				{
					mml.addEvent(spctrk, "\n@" + d2s(miditrk[evttrk_mid].Programs.GetProgram()) + "\n", event.absolutetime);
					dsp.Ch[spctrk].lastProg = miditrk[evttrk_mid].Programs.GetProgram();
				}
			}
			else
			{
				// 直前までに出力した音色番号と異なる場合、出力する
				if (dsp.Ch[spctrk].lastProg != miditrk[evttrk_mid].Programs.GetProgram(notekey))
				{
					printf("prog #%d for key #%d\n", miditrk[evttrk_mid].Programs.GetProgram(notekey), notekey);
					mml.addEvent(spctrk, "\n@" + d2s(miditrk[evttrk_mid].Programs.GetProgram(notekey)) + "\n", event.absolutetime);
					dsp.Ch[spctrk].lastProg = miditrk[evttrk_mid].Programs.GetProgram(notekey);
				}
			}
		}

		miditrk[evttrk_mid].NoteStatus.pushKey(event);

	};

	void EventNoteOFF(MIDI::Event &event)
	{
		uint8 midich(0), notekey(0), velo(0);
		uint32 dul_midtb, onTiming;

		uint16 trknum = event.track;
		uint8 spctrk = miditrk[trknum]._spcch;		// トラックと結び付けられたSPCチャネルを確認
		if (!spctrk) { return; }					// 未アサインなら何もしない

		spctrk--;
		if (miditrk[trknum].NoteStatus.isSounding())	// 当該トラックが発音中
		{
			event.getNoteInfo(notekey, velo);
			dul_midtb = miditrk[trknum].NoteStatus.releaseKey(event, &onTiming, &velo);	// duration on timebase

			// オクターブ情報初期値出力
			if (dsp.Ch[spctrk].bOctOut) {
				mml.addEvent(spctrk, mml.octchgmml(dsp.Ch[spctrk].lastNote, notekey), onTiming);
			}
			else {
				mml.addEvent(spctrk, string("D6") + d2h(notekey / 12) + " // Octave\n", onTiming);
				dsp.Ch[spctrk].bOctOut = true;
			}

			string nof = MMLNote(onTiming, dul_midtb, notekey, velo, timebase).toString();
			printf("NoF dul%d %s\n", dul_midtb, nof.c_str());
			mml.addEvent(spctrk, nof, onTiming, dul_midtb);
			//			mml.addEvent(spctrk, MMLNote(onTiming, dul_midtb, notekey, velo, timebase).toString(), onTiming, dul_midtb);

			dsp.Ch[spctrk].lastNote = notekey;
			dsp.Ch[spctrk].lastVel = velo;
		}
	};

	void EventProgramChange(MIDI::Event &event)
	{
		uint8 ch = event.getChannel();
		uint16 trk = event.track;

		miditrk[trk]._midch = ch;		// 基準チャネルを設定
	};

	void EventControlChange(MIDI::Event &event)
	{
		uint8 cc, vl, ch;
		uint16 trk = event.track;

		uint8 dtrk = miditrk[trk].spcch();
		if (!dtrk) { return; }
		dtrk--;

		ch = event.getChannel();		// MIDI Channel
		trk = event.track;				// MIDI Track
		event.getControlValue(cc, vl);


		SPCCh	&DSPCurCh = dsp.Ch[dtrk];
		uint32	eventtime = event.absolutetime;

		bool sounding = miditrk[trk].NoteStatus.isSounding();


		switch (cc) {
		case 0:		miditrk[trk].BankSelectM = vl;		break;
		case 32:	miditrk[trk].BankSelectL = vl;		break;
		case 1: 	DSPCurCh.VibPrm = vl;			break;
		case 10:
			DSPCurCh.Pan = vl;
			mml.addEvent(dtrk, "C6" + d2h(vl), event.absolutetime, sounding);
			break;
		case 11:
			DSPCurCh.Volume = vl;
			mml.addEvent(dtrk, "C4" + d2h(vl) + " // Volume\n", event.absolutetime, sounding);
			break;
		case 0x48:
			DSPCurCh.Slur = vl >= 0x40;
			mml.addEvent(dtrk, (vl >= 0x40) ? "E4" : "E5", event.absolutetime, sounding);
			break;
		case 0x49:
			DSPCurCh.Legato = vl >= 0x40;
			mml.addEvent(dtrk, (vl >= 0x40) ? "E6" : "E7", event.absolutetime, sounding);
			break;
		case 0x50:
			DSPCurCh.PitchModulation = vl >= 0x40;
			mml.addEvent(dtrk, (vl >= 0x40) ? "D2" : "D3", event.absolutetime, sounding);
			break;
		case 0x51:
			DSPCurCh.NoiseSw = vl >= 0x40;
			mml.addEvent(dtrk, (vl >= 0x40) ? "D0" : "D1", event.absolutetime, sounding);
			break;
		case 0x54:	DSPCurCh.AttackR = vl & 0x0F;	break;
		case 0x55:	DSPCurCh.DecayR = vl & 0x07;	break;
		case 0x56:	DSPCurCh.SustainLv = vl & 0x07;	break;
		case 0x57:	DSPCurCh.SustainR = vl & 0x1F;	break;
		case 0x5C:	DSPCurCh.TrmPrm = vl & 0x3F;	break;
		case 0x5E:
			DSPCurCh.EchoSw = vl >= 0x40;
			mml.addEvent(dtrk, (vl >= 0x40) ? "D4" : "D5", event.absolutetime, sounding);
			break;
		case 0x73:	/* $E1 ADSR Reset */ break;
		case 6:		miditrk[trk].setDEtry_MSB(vl);
			DataEntry(miditrk[trk]);
			break;
		case 38:	miditrk[trk].setDEtry_LSB(vl);
			DataEntry(miditrk[trk]);
			break;
		case 98:	miditrk[trk].setNRPN_LSB(vl);		break;
		case 99:	miditrk[trk].setNRPN_MSB(vl);		break;
		case 100:	miditrk[trk].setRPN_LSB(vl);		break;
		case 101:	miditrk[trk].setRPN_MSB(vl);		break;
		default:									break;
		}

		if (miditrk[trk].NoteStatus.isSounding())	// 当該トラックが発音中
		{
			mml.addEvent(dtrk, "// <-Overlap event\n", eventtime);
		}

	};
	void EventMetaMessage(MIDI::Event &event)
	{
		string text;

		switch (event.getMetaType()) {
		case MIDI::Event::MetaType::Text:
			if (event.getAsText(text)) {
				if (text.substr(0, 9 + 1) == "#songname ") {
					mml.songinfo.songtitle = text.substr(10);
				}
				if (text.substr(0, 10 + 1) == "#gametitle ") {
					mml.songinfo.gametitle = text.substr(11);
				}
				if (text.substr(0, 7 + 1) == "#artist ") {
					mml.songinfo.artist = text.substr(8);
				}
				if (text.substr(0, 7 + 1) == "#dumper ") {
					mml.songinfo.dumper = text.substr(8);
				}
				if (text.substr(0, 8 + 1) == "#comment ") {
					mml.songinfo.dumper = text.substr(9);
				}
				if (text.substr(0, 7 + 1) == "#length ") {
					mml.songinfo.length = text.substr(8);
				}
				if (text.substr(0, 11 + 1) == "#brr_offset ") {
					mml.songinfo.brr_offset = text.substr(12);
				}
				if (text.substr(0, 19) == "#brr_echo_overcheck") {
					mml.songinfo.brr_chk_echo_overlap = true;
				}
				if (text.substr(0, 11 + 1) == "#echo_depth ") {
					mml.songinfo.echo_depth = text.substr(12);
				}
				if (text.substr(0, 9 + 1) == "#surround ") {
					mml.songinfo.surround = true;
				}
				if (text.substr(0, 5 + 1) == "#tone ") {
					mml.songinfo.tones.push_back(text.substr(6));
				}
				if (text.substr(0, 6 + 1) == "#track ") {
					{
						uint16 trk = event.track;
						string tn(text.substr(7));

						std::printf("Assign SPC Ch.%s to Track #%d\n", tn.c_str(), event.track);
						miditrk[trk]._spcch = atoi(tn.c_str());
					}
				}
			}
			break;
		case MIDI::Event::MetaType::SetTempo:
			printf("%s // Tempo %d\n", MMLTempo(event.absolutetime, (uint8)event.getTempo()).toString().c_str(), (uint8)event.getTempo());
//			mml.addEvent(miditrk[event.track]._spcch == 0 ? 0 : miditrk[event.track]._spcch,
//							"F0" + d2h((uint8)event.getTempo()) + " // Tempo\n", event.absolutetime);
			mml.addEvent(0, "F0" + d2h((uint8)event.getTempo()) + " // Tempo\n", event.absolutetime);
			break;
		default:
			break;
		}
	};
	void EventSysExMessage(MIDI::Event &event)
	{
		// GM Master Volume
		if ((event.message[1] == 0x7F)
		 && (event.message[2] == 0x7F)		// Device ID
		 && (event.message[3] == 0x04)
		 && (event.message[4] == 0x01)		// Function Master volume
		 //  event.message[5]                  Volume LSB
		 //  event.message[6]                  Volume MSB
		 && (event.message[7] == 0xF7)) {								// End of Exclusive

			uint16 mvol = ((event.message[6] & 0x7F) << 7) | (event.message[5] & 0x7F);		// Data
			uint16 trknum = event.track;
			uint8 spctrk = miditrk[trknum]._spcch;		// トラックと結び付けられたSPCチャネルを確認

			mml.addEvent(spctrk, "F4" + d2h((uint8)mvol), event.absolutetime);

			printf("F4 d%d // Master volume\n", mvol >> 6);
		}
	};
	void DataEntry(AKAODRV::MIDITrack &miditrk)
	{
		if (miditrk.isNRPN_Valid()) {	// 直前にNRPNが指定されていたら
			std::printf("Data Entry MSB:LSB %d:%d\n", miditrk.getNRPN_MSB(), miditrk.getNRPN_LSB());
			switch (miditrk.getNRPN_MSB()) {
			case 0x01:	// Vibrate setting
				switch (miditrk.getNRPN_LSB()) {
				case 0x08:	// Vibrate rate
				case 0x09:	// Vibrate depth
				case 0x0A:	// Vibrate delay
				case 0x63:	// Attack rate
				case 0x64:	// Decay rate
				case 0x65:	// Sustain level
				case 0x66:	// Sustain rate
					break;
				default:
					break;
				}
				break;
			case 0x20:	// Prog assgin to keynote
				switch (miditrk.getNRPN_LSB()) {
				case 0:// LSB = 0: Tone map for allkey
					miditrk.Programs.SetAll(miditrk.getDEtry_MSB(), miditrk.getDEtry_LSB());
					break;
				default:
					break;
				}
				break;
			case 0x21:	// Prog assign to key individual
				// LSB : keynumber(0-7F)
				std::printf("Program assgin #%d to keynumber %d\n", miditrk.getDEtry_MSB(), miditrk.getNRPN_LSB());
				miditrk.Programs.Set(miditrk.getNRPN_LSB(), miditrk.getDEtry_MSB(), miditrk.getDEtry_LSB());
				break;
			default:
				break;
			}
		}
	};

};

