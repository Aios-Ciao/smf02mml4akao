#include <vector>
#include <string>
#include "appli.h"
#include "midif.h"


int MIDI::File::Load(string filename)
{
	uint8	*smf_raw;
	uint8	*seeker;
	int filesize = getFilesize(filename);

	seeker = smf_raw = new uint8[filesize];

	LoadBuff(filename, smf_raw);
	seeker = LoadHeader(seeker, header);

	// �����g���b�N�`�����N�̃p�[�X
	for (int trk = 0; trk < header._trackcnt; ++trk) {
		MIDI::Track track;
		seeker = LoadTrack(seeker, track);
		tracks.push_back(track);
	}

	delete[] smf_raw;
	_opened = true;

	return(0);
}
int	MIDI::File::getFilesize(string filename)
{
	int file_size = -1;

	// MIDI�t�@�C���T�C�Y�擾
	struct stat statBuf;
	if (stat(filename.c_str(), &statBuf) == 0) {
		file_size = statBuf.st_size;
	}

	return file_size;
}
int MIDI::File::LoadBuff(string filename, uint8 *buffer)
{
	FILE *fp;

	fopen_s(&fp, filename.c_str(), "rb");
	if (fp == NULL) {
		printf("Error : Can't open %s.\n", filename.c_str());
		return(-1);
	}

	// �t�@�C����ǂݍ���
	fread(buffer, 1, getFilesize(filename.c_str()), fp);
	fclose(fp);

	return 0;
}
uint8* MIDI::File::LoadHeader(uint8 *addr, MIDI::Header &header)
{
	addr = header.ChunkLoad(addr);
	header.parse();

	return(addr);
}
uint8* MIDI::File::LoadTrack(uint8 *addr, MIDI::Track &track)
{
	addr = track.ChunkLoad(addr);
	track.parse();

	return(addr);
}
