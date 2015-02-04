#ifndef _AV_TRANSCODER_FRAME_AUDIO_FRAME_HPP_
#define _AV_TRANSCODER_FRAME_AUDIO_FRAME_HPP_

#include "Frame.hpp"
#include <AvTranscoder/ProfileLoader.hpp>

extern "C" {
#include <libavutil/samplefmt.h>
}

#include <stdexcept>

namespace avtranscoder
{

/// @brief Description of a number of samples, which corresponds to one video frame
class AvExport AudioFrameDesc
{
public:
	/**
	 * @warning FPS value is set to 25 by default
	 */
	AudioFrameDesc( const size_t sampleRate = 0, const size_t channels = 0, const AVSampleFormat sampleFormat = AV_SAMPLE_FMT_NONE )
		: _sampleRate( sampleRate )
		, _channels( channels )
		, _sampleFormat( sampleFormat )
		, _fps( 1. )
	{}
	/**
	 * @warning FPS value is set to 25 by default
	 */
	AudioFrameDesc( const size_t sampleRate, const size_t channels, const std::string& sampleFormat )
		: _sampleRate( sampleRate )
		, _channels( channels )
		, _sampleFormat( av_get_sample_fmt( sampleFormat.c_str() ) )
		, _fps( 1. )
	{}

	size_t getSampleRate() const { return _sampleRate; }
	size_t getChannels() const { return _channels; }
	AVSampleFormat getSampleFormat() const { return _sampleFormat; }
	std::string getSampleFormatName() const
	{
		const char* formatName = av_get_sample_fmt_name( _sampleFormat );
		return formatName ? std::string( formatName ) : "unknown sample format";
	}
	double getFps() const { return _fps; }

	size_t getDataSize() const
	{
		if( _sampleFormat == AV_SAMPLE_FMT_NONE )
			throw std::runtime_error( "incorrect sample format" );

		size_t size = ( _sampleRate / _fps ) * _channels * av_get_bytes_per_sample( _sampleFormat );
		if( size == 0 )
			throw std::runtime_error( "unable to determine audio buffer size" );

		return size;
	}
	
	void setSampleRate( const size_t sampleRate ) { _sampleRate = sampleRate; }
	void setChannels( const size_t channels ) { _channels = channels; }
	void setSampleFormat( const std::string& sampleFormatName ) { _sampleFormat = av_get_sample_fmt( sampleFormatName.c_str() ); }
	void setSampleFormat( const AVSampleFormat sampleFormat ) { _sampleFormat = sampleFormat; }
	void setFps( const double fps ) { _fps = fps; }
	
	void setParameters( const ProfileLoader::Profile& profile )
	{
		// sample rate
		if( profile.count( constants::avProfileSampleRate ) )
			setSampleRate( atoi( profile.find( constants::avProfileSampleRate )->second.c_str() ) );
		// channels
		if( profile.count( constants::avProfileChannel ) )
			setChannels( atoi( profile.find( constants::avProfileChannel )->second.c_str() ) );
		// sample format	
		if( profile.count( constants::avProfileSampleFormat ) )
			setSampleFormat( profile.find( constants::avProfileSampleFormat )->second );
		// fps
		if( profile.count( constants::avProfileFrameRate ) )
			setFps( atof( profile.find( constants::avProfileFrameRate )->second.c_str() ) );
	}

private:
	size_t _sampleRate;
	size_t _channels;
	AVSampleFormat _sampleFormat;
	double _fps;
};

class AvExport AudioFrame : public Frame
{
public:
	AudioFrame( const AudioFrameDesc& ref )
		: Frame( ref.getDataSize() )
		, _audioFrameDesc( ref )
		, _nbSamples( 0 )
	{}

	const AudioFrameDesc& desc() const    { return _audioFrameDesc; }
	
	size_t getNbSamples() const { return _nbSamples; }
	void setNbSamples( size_t nbSamples ) { _nbSamples = nbSamples; }

private:
	const AudioFrameDesc _audioFrameDesc;
	size_t _nbSamples;
};

}

#endif
