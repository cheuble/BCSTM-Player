#include "MusicBCSTM.hpp"

MusicBCSTM::MusicBCSTM()
: m_isStreaming (false)
, m_isPaused    (false)
, m_channelCount(0)
{
}

MusicBCSTM::~MusicBCSTM()
{
	stop();
}

bool MusicBCSTM::openFromFile(std::string filename)
{
	stop();
	m_file = fopen(filename.c_str(), "rb");
	if (!m_file)
		return false;

	m_isLittleEndian = true; // Default to true so initial reads are not swapped

	// Verify header magic value and get endianness
	uint32_t magic = read32();
	if (!(m_isLittleEndian = (read16() == 0xFEFF)))
		magic = htonl(magic);
	if (magic != 0x4D545343) // "CSTM"
		return false;
	//
	fseek(m_file, 0x10, SEEK_SET);
	uint16_t sectionBlockCount = read16();
	read16();

	m_dataOffset = 0;
	m_infoOffset = 0;

	for (unsigned int i = 0; i < sectionBlockCount; i++)
	{
		int sec = read16();
		read16(); // padding
		uint32_t off = read32();
		read32(); // size
		if (sec == InfoBlock)
			m_infoOffset = off;
		else if (sec == DataBlock)
			m_dataOffset = off;
	}
	if (!m_infoOffset || !m_dataOffset)
		return false;
	
	fseek(m_file, static_cast<size_t>(m_infoOffset + 0x20), SEEK_SET);
	if (read8() != 2)
	{
		//ERROR
		return false;
	}

	m_looping = read8();
	m_channelCount = read8();
	if (m_channelCount > 2)
	{
		//ERROR
		return false;
	}

	fseek(m_file, static_cast<size_t>(m_infoOffset + 0x24), SEEK_SET);
	m_sampleRate = read32();
	uint32_t loopPos = read32();
	uint32_t loopEnd = read32();
	m_blockCount = read32();
	m_blockSize = read32();
	m_blockSampleCount = read32();
	read32(); // last block used bytes
	m_lastBlockSampleCount = read32();
	m_lastBlockSize = read32();

	m_blockLoopStart = loopPos / m_blockSampleCount;
	m_blockLoopEnd = (loopEnd % m_blockSampleCount ? m_blockCount : loopEnd / m_blockSampleCount);

	while (read32() != ChannelInfo);
	fileAdvance(read32() + m_channelCount*8 - 12);

	// Get ADPCM data
	for (unsigned int i = 0; i < m_channelCount; i++)
	{
		fread(m_adpcmCoefs[i], 1, static_cast<size_t>(sizeof(uint16_t) * 16), m_file);
		fread(&m_adpcmData[i][0], 1, static_cast<size_t>(sizeof(ndspAdpcmData)), m_file);// Beginning Context
		fread(&m_adpcmData[i][1], 1, static_cast<size_t>(sizeof(ndspAdpcmData)), m_file); // Loop context
		read16(); // skip padding
	}

	m_currentBlock = 0;
	fseek(m_file, static_cast<size_t>(m_dataOffset + 0x20), SEEK_SET);
	isLoaded = true;
	return true;
}


void MusicBCSTM::play()
{
	if (m_isPaused)
	{
		for (unsigned int i = 0; i < m_channelCount; ++i)
			ndspChnSetPaused(m_channel[i], false);
		m_isPaused = false;
		return;
	}

	if (m_isStreaming)
		stop();

	for (unsigned int i = 0; i < m_channelCount; ++i)
	{
		{
			m_channel[i] = 0;
			while (m_channel[i] < 24 && ((activeNdspChannels >> m_channel[i]) & 1))
				m_channel[i]++;
			if (m_channel[i] == 24)
			{
				//ERROR
				return;
			}
			activeNdspChannels |= 1 << m_channel[i];
			ndspChnWaveBufClear(m_channel[i]);
		}

		static float mix[16];
		ndspChnSetFormat(m_channel[i], NDSP_FORMAT_ADPCM | NDSP_3D_SURROUND_PREPROCESSED);
		ndspChnSetRate(m_channel[i], m_sampleRate);

		if (m_channelCount == 1)
			mix[0] = mix[1] = 0.5f;
		else if (m_channelCount == 2)
		{
			if (i == 0)
			{
				mix[0] = 0.8f;
				mix[1] = 0.0f;
				mix[2] = 0.2f;
				mix[3] = 0.0f;
			} else
			{
				mix[0] = 0.0f;
				mix[1] = 0.8f;
				mix[2] = 0.0f;
				mix[3] = 0.2f;
			}
		}
		ndspChnSetMix(m_channel[i], mix);
		ndspChnSetAdpcmCoefs(m_channel[i], m_adpcmCoefs[i]);

		for (unsigned int j = 0; j < BufferCount; j ++)
		{
			memset(&m_waveBuf[i][j], 0, sizeof(ndspWaveBuf));
			m_waveBuf[i][j].status = NDSP_WBUF_DONE;

			m_bufferData[i][j].resize(m_blockSize);
		}
	}

	m_isStreaming = true;

	
}

void MusicBCSTM::pause()
{
	fclose(m_file);
	if (!m_isStreaming)
		return;
	m_isPaused = true;
	
	for (unsigned int i = 0; i < m_channelCount; ++i)
		ndspChnSetPaused(m_channel[i], true);
}

void MusicBCSTM::stop()
{
	isLoaded = false;
	if (!m_isStreaming)
		return;

	{
		m_isStreaming = false;
	}

	for (unsigned int i = 0; i < m_channelCount; ++i)
	{
		ndspChnWaveBufClear(m_channel[i]);
		activeNdspChannels &= ~(1 << m_channel[i]);
	}
}
void MusicBCSTM::tick(){
	streamData();
}
void MusicBCSTM::streamData()
{
	currTime = svcGetSystemTick();
	if(currTime - lastTime >= 100000000 && isLoaded){
		static bool isPaused = false;
		{
			isPaused = m_isPaused;
			if (!m_isStreaming)
				return;//break;
		}

		if (!isPaused)
		{
			fillBuffers();
		}
	lastTime = currTime;
	}
}
uint32_t MusicBCSTM::read32()
{
	uint32_t v;
	fread(&v, 1, static_cast<size_t>(sizeof(v)), m_file);
	return (m_isLittleEndian ? v : htonl(v));
}

uint16_t MusicBCSTM::read16()
{
	uint16_t v;
	fread(&v, 1, static_cast<size_t>(sizeof(v)), m_file);
	return (m_isLittleEndian ? v : htons(v));
}

uint8_t MusicBCSTM::read8()
{
	uint8_t v;
	fread(&v, 1, static_cast<size_t>(sizeof(v)), m_file);
	return v;
}

void MusicBCSTM::fillBuffers()
{
	for (unsigned int bufIndex = 0; bufIndex < BufferCount; ++bufIndex)
	{
		if (m_waveBuf[0][bufIndex].status != NDSP_WBUF_DONE)
			continue;
		if (m_channelCount == 2 && m_waveBuf[1][bufIndex].status != NDSP_WBUF_DONE)
			continue;

		if (m_currentBlock == m_blockLoopEnd)
		{
			m_currentBlock = m_blockLoopStart;
			fseek(m_file, static_cast<size_t>(m_dataOffset + 0x20 + m_blockSize*m_channelCount*m_blockLoopStart), SEEK_SET);
		}

		for (unsigned int channelIndex = 0; channelIndex < m_channelCount; ++channelIndex)
		{
			ndspWaveBuf *buf = &m_waveBuf[channelIndex][bufIndex];

			memset(buf, 0, sizeof(ndspWaveBuf));
			buf->data_adpcm = m_bufferData[channelIndex][bufIndex].data();
			fread(buf->data_adpcm, 1, static_cast<size_t>((m_currentBlock == m_blockCount-1) ? m_lastBlockSize : m_blockSize), m_file);
			DSP_FlushDataCache(buf->data_adpcm, m_blockSize);

			if (m_currentBlock == 0)
				buf->adpcm_data = &m_adpcmData[channelIndex][0];
			else if (m_currentBlock == m_blockLoopStart)
				buf->adpcm_data = &m_adpcmData[channelIndex][1];
			
			if (m_currentBlock == m_blockCount-1)
				buf->nsamples = m_lastBlockSampleCount;
			else
				buf->nsamples = m_blockSampleCount;

			ndspChnWaveBufAdd(m_channel[channelIndex], buf);
		}


		m_currentBlock++;
	}
}

bool MusicBCSTM::fileAdvance(uint64_t byteSize)
{
	uint64_t seekPosition = ftell(m_file) + byteSize;
	return ((unsigned)fseek(m_file, static_cast<size_t>(seekPosition), SEEK_SET) == seekPosition);
}