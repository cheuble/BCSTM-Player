#pragma once
#include <3ds.h>
#include <cstdio>
#include <string>
#include <string.h>
#include <vector>
#include <arpa/inet.h>
#include "LinearAllocator.hpp"
class MusicBCSTM {
public:
	MusicBCSTM();
	~MusicBCSTM();

	bool openFromFile(const std::string filename);
	void tick();
	void play();
	void pause();
	void stop();

protected:
	void streamData();

	uint8_t read8();
	uint16_t read16();
	uint32_t read32();
	bool fileAdvance(uint64_t byteSize);

	void fillBuffers();

private:
	FILE * m_file;
	enum RefType: uint16_t {
		ByteTable      = 0x0100,
		ReferenceTable = 0x0101,
		SampleData     = 0x1F00,
		DSPADPCMInfo   = 0x0300,
		InfoBlock      = 0x4000,
		SeekBlock      = 0x4001,
		DataBlock      = 0x4002,
		StreamInfo     = 0x4100,
		TrackInfo      = 0x4101,
		ChannelInfo    = 0x4102,
	};

	enum {
		BufferCount = 20,
	};
	
	u32 lastTime = 0;
	u32 currTime = 0;
	
	bool m_isLittleEndian;
	bool m_isStreaming;
	bool m_isPaused;

	bool m_looping;
	uint32_t m_channelCount;
	uint32_t m_sampleRate;

	uint32_t m_blockLoopStart;
	uint32_t m_blockLoopEnd;
	uint32_t m_blockCount;
	uint32_t m_blockSize;
	uint32_t m_blockSampleCount;
	uint32_t m_lastBlockSize;
	uint32_t m_lastBlockSampleCount;
	uint16_t m_adpcmCoefs[2][16];

	uint32_t m_currentBlock;
	uint32_t m_infoOffset;
	uint32_t m_dataOffset;
	
	uint32_t activeNdspChannels = 0;
	
	bool isLoaded = false;
	
	u16 m_channel[2];
	ndspWaveBuf m_waveBuf[2][BufferCount];
	ndspAdpcmData m_adpcmData[2][2];
	std::vector<u8, LinearAllocator<u8>> m_bufferData[2][BufferCount];

};