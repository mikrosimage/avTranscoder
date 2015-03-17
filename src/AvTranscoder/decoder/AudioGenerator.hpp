#ifndef _AV_TRANSCODER_DECODER_AUDIO_GENERATOR_HPP_
#define _AV_TRANSCODER_DECODER_AUDIO_GENERATOR_HPP_

#include "IDecoder.hpp"
#include <AvTranscoder/codec/AudioCodec.hpp>

namespace avtranscoder
{

class AvExport AudioGenerator : public IDecoder
{
public:
	AudioGenerator();
	AudioGenerator( const AudioGenerator& audioGenerator );
	AudioGenerator& operator=( const AudioGenerator& audioGenerator );

	~AudioGenerator();

	void setup() {}

	bool decodeNextFrame( Frame& frameBuffer );
	bool decodeNextFrame( Frame& frameBuffer, const size_t subStreamIndex );

	const AudioFrameDesc& getAudioFrameDesc() const { return _frameDesc; }
	void setAudioFrameDesc( const AudioFrameDesc& frameDesc );
	void setFrame( Frame& inputFrame );

private:
	Frame* _inputFrame;  ///< Has link (no ownership)
	AudioFrame* _silent;   ///< The generated silent (has ownership)
	AudioFrameDesc _frameDesc;  ///< The description of the silent (sample rate...) 
};

}

#endif