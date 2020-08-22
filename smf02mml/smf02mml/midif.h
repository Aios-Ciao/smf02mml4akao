#pragma once
#include <vector>
#include <string>
#include "appli.h"

using namespace std;

class MIDI {
public:
	class Chunk {
	public:
		Bytes		signature;
		uint32		size;
		Bytes		_rawdata;

	public:
		Chunk() : size(0) {
			signature.reserve(4);
		};
		~Chunk() {};

		inline bool chkTrackType(string sign) {
			return (
				(signature[0] == sign[0])
				&& (signature[1] == sign[1])
				&& (signature[2] == sign[2])
				&& (signature[3] == sign[3])
				);
		};
		inline uint32	getChunkSize() {
			return(size);
		};

		// Chunk Setup
		uint8 * ChunkLoad(uint8 *addr) {
			addr = _setChunkSignature(addr);	// シグネチャの読み込み
			addr = _setChunkSize(addr);			// チャンクサイズの読み込み
			addr = _setChunkData(addr);			// チャンクデータの読み込み

			return(addr);
		};
		uint8* _setChunkSignature(uint8 *addr) {
			copy(addr, addr + 4, back_inserter(signature));

			return(addr + 4);
		};
		uint8* _setChunkSize(uint8 *addr) {
			size = MIDI::Util::getValue(addr, 4);
			_rawdata.reserve(size);

			return(addr + 4);
		};
		uint8* _setChunkData(uint8 *addr) {
			copy(addr, addr + size, back_inserter(_rawdata));
			return(addr + size);
		};
	};

	class Event {
	public:
		uint32	deltatime;
		uint32	absolutetime;
		uint16	track;
		Bytes	message;

		Event() : track(0), deltatime(0), absolutetime(0) { };

		Event(uint32 _dtime, Bytes _msg)
			: deltatime(_dtime), message(_msg), track(0)
		{};
		~Event() {};
		Event(const Event &event) :
			deltatime(event.deltatime),
			absolutetime(event.absolutetime), track(event.track)
		{
			message.reserve(event.message.size());
			copy(event.message.begin(), event.message.end(), back_inserter(message));
		};
	public:
		typedef enum {
			UnknownStatus,
			Note_OFF,
			Note_ON,
			ControlChange,
			ProgramChange,
			PolyKeyPressure,
			ChPressure,
			PitchBend,
			MetaMessage,
			SysExclusive,
		} StatusType;
		typedef enum {
			UnknownMeta,
			Seq_Number,
			Text,
			Copyright,
			Seq_Name,
			Instrument_Name,
			Lyric,
			Marker,
			CuePoint,
			MIDI_Channel_Prefix,
			MIDI_Port_Prefix,
			EndOfTrack,
			SetTempo,
			SMPTE_Offset,
			Time_Signature,
			Key_Signature,
			Sequencer_Specific_MetaEvent
		} MetaType;
	public:
		bool chkCompare(MIDI::Event::StatusType status, uint8 ch, uint8 data1)
		{
			// ステータス、チャネル、データ２つを照合する
			uint8 ech(0);
			getChannel(ech);
			return((getStatusType() == status) && (ech == ch) && (message[1] == data1));
		};
		uint8 getStatusCode() const
		{
			if (message.size() == 0) { return(0xFF); };
			return(message[0] & 0xF0);
		};
		uint8 getChannel() const
		{
			if (message.size() == 0) { return(0xFF); };
			return(message[0] & 0x0F);
		};
		Event::StatusType getStatusType() const
		{
			if (message.size() == 0) { return(UnknownStatus); };
			switch (message[0] & 0xF0) {
			case 0x80:	return(Note_OFF);
			case 0x90:	return(Note_ON);		// including note-off(velo:0h)
			case 0xA0:	return(PolyKeyPressure);
			case 0xB0:	return(ControlChange);
			case 0xE0:	return(PitchBend);
			case 0xC0:	return(ProgramChange);
			case 0xD0:	return(ChPressure);
			case 0xF0:	// System/Meta Message
				switch (message[0] & 0x0F) {
				case 0x00: case 0x07:	return(SysExclusive);
				case 0x0F:				return(MetaMessage);
				default:				return(UnknownStatus);
				} break;
			default:	return(UnknownStatus);
			}
			return(UnknownStatus);
		};
		bool operator < (const Event &ev2) const {
			bool result(false);
			uint8 ne1, ne2, ve1, ve2;
			if (absolutetime == ev2.absolutetime) {
				if (getStatusType() == ev2.getStatusType()) {
					if (getChannel() != ev2.getChannel()) {
						return getChannel() < ev2.getChannel();
					}
					switch (getStatusType()) {
					case Note_OFF: case Note_ON:
						// Statusが同じでNoteイベントの場合はキーナンバーでソート
						getNoteInfo(ne1, ve1);
						ev2.getNoteInfo(ne2, ve2);
						return(ne1 < ne2);
					case ControlChange:
						// 同、Contorol changeイベントの場合はコントローラナンバーでソート
						getControlValue(ne1, ve1);
						ev2.getControlValue(ne2, ve2);
						return(ne1 < ne2);
					default:
						break;
					}
				}
				else
				{
					switch (getStatusType()) {
					case Note_OFF:
						if (ev2.getStatusType() == Note_ON) {
							result = true;
						}
						break;
					case ControlChange:
						switch (ev2.getStatusType()) {
						case Note_OFF: case Note_ON:
							result = true;
							break;
						default:
							break;
						}
						break;
					default:
						break;
					}
				}
			}
			else {
				return(absolutetime < ev2.absolutetime);
			}

			return result;
		};
		MetaType getMetaType()
		{
			if (getStatusType() != MetaMessage)	return(UnknownMeta);
			// message[2] is length
			switch (message[1]) {
			case 0x00:	return(Seq_Number);						// Sequence Number
			case 0x01:	return(Text);							// Text
			case 0x02:	return(Copyright);						// Copyright
			case 0x03:	return(Seq_Name);						// Sequence Name
			case 0x04:	return(Instrument_Name);				// Instrument Name
			case 0x05:	return(Lyric);							// Lyric
			case 0x06:	return(Marker);							// Marker
			case 0x07:	return(CuePoint);						// Cue Point
			case 0x20:	return(MIDI_Channel_Prefix);			// MIDI Channel Prefix
			case 0x21:	return(MIDI_Port_Prefix);				// MIDI Port Prefix
			case 0x2F:	return(EndOfTrack);						// End Of Track
			case 0x51:	return(SetTempo);						// Set Tempo
			case 0x54:	return(SMPTE_Offset);					// SMPTE Offset
			case 0x58:	return(Time_Signature);					// Time Signature
			case 0x59:	return(Key_Signature);					// Key Signature
			case 0x7F:	return(Sequencer_Specific_MetaEvent);	// Sequencer-Specific Meta-Event
			default:	return(UnknownMeta);
			}
			return(UnknownMeta);
		};

		float getTempo()
		{
			float bpm = 0;
			if ((message.size() == 6) && getMetaType() == SetTempo) {
				uint32 usec = Util::getValue(&message[3], 3);
				bpm = 60000000 / (float)usec;
			}
			return(bpm);
		};

		bool getChannel(uint8 &ch)
		{
			bool valid(false);

			switch (getStatusType()) {
			case UnknownStatus:
				break;
			case SysExclusive:
			case MetaMessage:
				ch = 0xFF;
				valid = false;
				break;
			default:
				ch = message[0] & 0x0F;
				valid = true;
				break;
			}

			return(valid);
		};
		bool getNoteInfo(uint8 &notenum, uint8 &velo) const
		{
			bool valid(false);

			switch (getStatusType()) {
			case Note_ON: case Note_OFF:
				notenum = message[1];
				velo = message[2];
				valid = true;
				break;
			default:
				break;
			}

			return(valid);
		};
		uint8 getData1() const
		{
			return message[1];
		};
		uint8 getData2() const
		{
			return message[2];
		};
		bool getAsText(string &text)
		{
			bool valid(false);

			switch (getMetaType()) {
			case Text:
			case Copyright:
			case Seq_Name:
			case Instrument_Name:
			case Lyric:
			case Marker:
			case Sequencer_Specific_MetaEvent:
				text = string(message.begin()+3, message.end());
				valid = true;
				break;
			default:
				break;
			}
			return valid;
		};
		
		bool getProgramNumber(uint8 &prog) const
		{
			bool valid(false);

			if (getStatusType() == ProgramChange) {
				prog = message[1];
				valid = true;
			}

			return valid;
		};
		bool getControlValue(uint8 &ctrl, uint8 &value) const
		{
			bool valid(false);

			if (getStatusType() == ControlChange) {
				ctrl = message[1];
				value = message[2];
				valid = true;
			}

			return valid;
		};
		bool getPitchwheel(uint16 &value)
		{
			bool valid(false);

			if (getStatusType() == PitchBend) {
				value = (message[1] & 0x7F) << 7;
				value = message[2];
				valid = true;
			}

			return valid;
		};
		uint16 getWord() const
		{
			uint16 value;

			value = (message[1] & 0x7F) << 7;
			value = message[2];

			return value;
		};
	public:
		typedef vector<Event> List;
	};

	class Track : public Chunk {
	public:
		Event::List Events;
	public:
		uint32 EventCount() { return (uint32)Events.size(); };

		void parse() {
			uint8 *seek_limit = &_rawdata[0] + getChunkSize();
			uint8 *seeker = &_rawdata[0];
			uint8 status_last;
			uint32	absolutetime(0);

			// トラックチャンク内はデルタタイム→イベントのセットを繰り返す
			while (seeker < seek_limit) {
				Event		event;

				seeker = Util::getVarValue(seeker, event.deltatime);
				seeker = Util::getMIDIMessage(seeker, event.message, status_last);

				// 対象のチャネルの現在時刻とメッセージを使ってイベントを登録
				absolutetime += event.deltatime;
				event.absolutetime = absolutetime;
				Events.push_back(event);
			}
		};

	public:
		typedef vector<Track> List;
	};

	class Header : public Chunk {
	public:
		uint16	_format;		// from chunkinfo
		uint16	_trackcnt;		// from chunkinfo
		uint16	_timebase;		// from chunkinfo
	public:
		uint16	format() { return(_format); };
		uint16	timebase() { return(_timebase); };

		void parse() {
			_format = (uint16)Util::getValue(&_rawdata[0], 2);
			_trackcnt = (uint16)Util::getValue(&_rawdata[2], 2);
			_timebase = (uint16)Util::getValue(&_rawdata[4], 2);
		}
	};

	class Util {
	public:
		static inline uint32	getValue(uint8 *addr, uint8 width) {
			uint32 value = 0;

			while (width) {
				value = (value << 8) + *addr++;
				width--;
			}
			return(value);
		};
		static inline uint8*	getVarValue(uint8 *addr, uint32 &value)
		{
			value = 0;
			do {
				value = (value << 7) + (*addr & 0x7F);
			} while (*addr++ & 0x80);

			return(addr);
		};
		static	uint8*			getMIDIMessage(uint8 *addr, Bytes &message, uint8 &status_last)
		{
			// Use work_status for running-status. work_status will provided by caller scope.
			uint8 status, data;

			status = *addr++;
			if (status & 0x80) {		// ステータスバイト
				data = *addr++;
			}
			else {
				data = status;			// ランニングステータス
				status = status_last;
			}

			message.clear();
			switch (status & 0xF0) {
			case 0x80:	// Note OFF
			case 0x90:	// Note ON
			case 0xA0:	// After touch
			case 0xB0:	// Control Change
			case 0xE0:	// Pitch bend
				addr = parse3ByteEvent(addr, status, data, message, status_last);
				break;
			case 0xC0:	// Program Change
			case 0xD0:	// After touch
				addr = parse2ByteEvent(addr, status, data, message, status_last);
				break;
			case 0xF0:	// System Message
				addr = parseSysMessage(addr, status, data, message, status_last);
				break;
			default:	// Data?
				printf("異常データ検出(0x%02X)\n", status);
				break;
			}

			return(addr);

		};
	public: // for getMIDIMessage
		static uint8* parse3ByteEvent(uint8 *addr, uint8 status, uint8 data1, Bytes &msg, uint8 &status_last)
		{
			status_last = status;

			msg.push_back(status);
			msg.push_back(data1);
			msg.push_back(*addr++);

			return(addr);
		};
		static uint8* parse2ByteEvent(uint8 *addr, uint8 status, uint8 data1, Bytes &msg, uint8 &status_last)
		{
			status_last = status;

			msg.push_back(status);
			msg.push_back(data1);

			return(addr);
		};
		static uint8* parseSysMessage(uint8 *addr, uint8 status, uint8 data, Bytes &msg, uint8 &status_last)
		{
			uint32 msglen;
			switch (status) {

			case 0xF0:		// System Exclusive Message
			case 0xF7:		// End Of Exclusive
				msg.push_back(status);
				addr--;
				addr = getVarValue(addr, msglen);
				for (uint32 cnt = 0; cnt < msglen; ++cnt) {
					msg.push_back(*addr++);
				}
				break;
			case 0xFF:		// Meta Event(on file)
			{
				uint8 kind = data;
				msg.push_back(status);
				msg.push_back(kind);
				addr = parseMetaEvent(addr, kind, msg, status_last);
			}
			break;
			default:		// 下記はリアルタイムメッセージとしての定義
			//	case 0xF0:		// System Exclusive Message
			//	case 0xF1:		// MIDI Timecode Quarter Frame
			//	case 0xF2:		// Song Position Pointer
			//	case 0xF3:		// Song Select
			//	case 0xF4:		// (Reserved)
			//	case 0xF5:		// (Reserved)
			//	case 0xF6:		// Tune Request
			//	case 0xF7:		// End Of Exclusive
			//	case 0xF8:		// Timing Clock
			//	case 0xF9:		// (Reserved)
			//	case 0xFA:		// Start
			//	case 0xFB:		// Continue
			//	case 0xFC:		// Stop
			//	case 0xFD:		// (Reserved)
			//	case 0xFE:		// Active Sensing
			//	case 0xFF:		// System Reset(on wire)
				break;
			}

			return(addr);
		};
		static uint8* parseMetaEvent(uint8 *addr, uint8 kind, Bytes &msg, uint8 &status_last)
		{
			uint32	data_length;
			uint8 *next;

			// メタイベントの処理はこの辺参照
			//https://qiita.com/owts/items/8d641e8a8a423566db71

			switch (kind) {
			case 0x00:		// Sequence Number
			case 0x01:		// Text
			case 0x02:		// Copyright
			case 0x03:		// Sequence Name
			case 0x04:		// Instrument Name
			case 0x05:		// Lyric
			case 0x06:		// Marker
			case 0x07:		// Cue Point
			case 0x20:		// MIDI Channel Prefix
			case 0x21:		// MIDI Port Prefix
			case 0x2F:		// End Of Track
			case 0x51:		// Set Tempo
			case 0x54:		// SMPTE Offset
			case 0x58:		// Time Signature
			case 0x59:		// Key Signature
			case 0x7F:		// Sequencer-Specific Meta-Event
				next = getVarValue(addr, data_length);
				for (; addr < next;) {
					msg.push_back(*addr++);		// Data length (Variable Value Format)
				}
				for (uint32 cnt = 0; cnt < data_length; ++cnt) {
					msg.push_back(*addr++);
				}
				break;
			default:
				break;
			}

			return(addr);
		};
	};

	class File {
	public:
		Header			header;
		Track::List		tracks;
		bool _opened;

		File() :_opened(false) {};
		File(string filename) :_opened(false) { _opened = !Load(filename); };
		~File() {};
		int Load(string filename);
		bool Valid() { return _opened; };
		uint8* LoadHeader(uint8 *addr, Header &header);
		uint8* LoadTrack(uint8 *addr, Track &track);

		uint16	Format() { return header.format(); };
		uint16	TrackCount() { return (uint16)tracks.size(); };
		uint16	Timebase() { return header.timebase(); };
	private:
		int getFilesize(string filename);
		int LoadBuff(string filename, uint8 *buffer);
	};

};
