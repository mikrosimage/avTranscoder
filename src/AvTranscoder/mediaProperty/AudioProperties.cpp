#include "AudioProperties.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/pixdesc.h>
#include <libavutil/channel_layout.h>
}

#include <stdexcept>

namespace avtranscoder
{

AudioProperties::AudioProperties( const FormatContext& formatContext, const size_t index )
	: _formatContext( &formatContext.getAVFormatContext() )
	, _codecContext( NULL )
	, _codec( NULL )
	, _streamIndex( index )
{
	if( _formatContext )
		_codecContext = _formatContext->streams[index]->codec;

	if( _formatContext && _codecContext )
		_codec = avcodec_find_decoder( _codecContext->codec_id );

	if( _formatContext )
		detail::fillMetadataDictionnary( _formatContext->streams[index]->metadata, _metadatas );
}

size_t AudioProperties::getStreamId() const
{
	if( ! _formatContext )
		throw std::runtime_error( "unknown format context" );
	return _formatContext->streams[_streamIndex]->id;
}

std::string AudioProperties::getCodecName() const
{
	if( _codec && _codec->name )
		return std::string( _codec->name );
	return "unknown codec";
}

std::string AudioProperties::getCodecLongName() const
{
	if( _codec && _codec->long_name )
		return std::string( _codec->long_name );
	return "unknown codec";
}

std::string AudioProperties::getSampleFormatName() const
{
	if( ! _codecContext )
		return "unknown codec context";

	const char* fmtName = av_get_sample_fmt_name( _codecContext->sample_fmt );
	if( fmtName )
		return std::string( fmtName );
	return "unknown sample format";
}

std::string AudioProperties::getSampleFormatLongName() const
{
	if( ! _codecContext )
		return "unknown codec context";

	switch( _codecContext->sample_fmt )
	{
		case AV_SAMPLE_FMT_NONE:
			return "none";
		case AV_SAMPLE_FMT_U8:
			return "unsigned 8 bits";
		case AV_SAMPLE_FMT_S16:
			return "signed 16 bits";
		case AV_SAMPLE_FMT_S32:
			return "signed 32 bits";
		case AV_SAMPLE_FMT_FLT:
			return "float";
		case AV_SAMPLE_FMT_DBL:
			return "double";
		case AV_SAMPLE_FMT_U8P:
			return "unsigned 8 bits, planar";
		case AV_SAMPLE_FMT_S16P:
			return "signed 16 bits, planar";
		case AV_SAMPLE_FMT_S32P:
			return "signed 32 bits, planar";
		case AV_SAMPLE_FMT_FLTP:
			return "float, planar";
		case AV_SAMPLE_FMT_DBLP:
			return "double, planar";
		case AV_SAMPLE_FMT_NB:
			return "number of sample formats";
	}
	return "unknown sample format";
}

std::string AudioProperties::getChannelLayout() const
{
	if( ! _codecContext )
		return "unknown codec context";

	char buf1[1024];
	av_get_channel_layout_string( buf1, sizeof( buf1 ), -1, _codecContext->channel_layout );
	return std::string( buf1 );
}

std::string AudioProperties::getChannelName() const
{
	if( ! _codecContext )
		return "unknown codec context";

	const char* channelName = av_get_channel_name( _codecContext->channel_layout );
	if( channelName )
		return std::string( channelName );
	return "unknown channel name";
}

std::string AudioProperties::getChannelDescription() const
{
	if( ! _codecContext )
		return "unknown codec context";

#ifdef FF_RESAMPLE_LIBRARY
	const char* channelDescription = av_get_channel_description( _codecContext->channel_layout );
	if( channelDescription )
		return std::string( channelDescription );
	return "unknown channel description";
#else
	return "can't access channel description";
#endif
}

size_t AudioProperties::getCodecId() const 
{
	if( ! _codecContext )
		throw std::runtime_error( "unknown codec context" );
	return _codecContext->codec_id;
}

size_t AudioProperties::getSampleRate() const 
{
	if( ! _codecContext )
		throw std::runtime_error( "unknown codec context" );
	return _codecContext->sample_rate;
}

size_t AudioProperties::getChannels() const 
{
	if( ! _codecContext )
		throw std::runtime_error( "unknown codec context" );
	return _codecContext->channels;
}

size_t AudioProperties::getBitRate() const
{
	if( ! _codecContext )
		throw std::runtime_error( "unknown codec context" );

	// return bit rate of stream
	if( _codecContext->bit_rate )
		return _codecContext->bit_rate;

	// else get computed bit rate from our computation (warning: way to compute bit rate of PCM audio data)
	int bitsPerSample = av_get_bits_per_sample( _codecContext->codec_id ); // 0 if unknown for the given codec
	return _codecContext->sample_rate * _codecContext->channels * bitsPerSample;
}

size_t AudioProperties::getNbSamples() const
{
	if( ! _formatContext )
		throw std::runtime_error( "unknown format context" );
	return _formatContext->streams[_streamIndex]->nb_frames;
}

size_t AudioProperties::getTicksPerFrame() const
{
	if( ! _codecContext )
		throw std::runtime_error( "unknown codec context" );
	return _codecContext->ticks_per_frame;
}

Rational AudioProperties::getTimeBase() const
{
	if( ! _formatContext )
		throw std::runtime_error( "unknown format context" );

	Rational timeBase = {
		_formatContext->streams[_streamIndex]->time_base.num,
		_formatContext->streams[_streamIndex]->time_base.den,
	};
	return timeBase;
}

double AudioProperties::getDuration() const
{
	Rational timeBase = getTimeBase();
	double duration = ( timeBase.num / (double) timeBase.den ) * _formatContext->streams[_streamIndex]->duration;
	return duration;
}

PropertiesMap AudioProperties::getPropertiesAsMap() const
{
	PropertiesMap dataMap;

	detail::add( dataMap, "streamId", getStreamId() );
	detail::add( dataMap, "codecId", getCodecId() );
	detail::add( dataMap, "codecName", getCodecName() );
	detail::add( dataMap, "codecLongName", getCodecLongName() );
	detail::add( dataMap, "sampleFormatName", getSampleFormatName() );
	detail::add( dataMap, "sampleFormatLongName", getSampleFormatLongName() );
	detail::add( dataMap, "sampleRate", getSampleRate() );
	detail::add( dataMap, "bitRate", getBitRate() );
	detail::add( dataMap, "nbSamples", getNbSamples() );
	detail::add( dataMap, "channels", getChannels() );
	detail::add( dataMap, "channelLayout", getChannelLayout() );
	detail::add( dataMap, "channelName", getChannelName() );
	detail::add( dataMap, "channelDescription", getChannelDescription() );
	detail::add( dataMap, "ticksPerFrame", getTicksPerFrame() );
	detail::add( dataMap, "timeBase", getTimeBase() );
	detail::add( dataMap, "duration", getDuration() );

	for( size_t metadataIndex = 0; metadataIndex < _metadatas.size(); ++metadataIndex )
	{
		detail::add( dataMap, _metadatas.at( metadataIndex ).first, _metadatas.at( metadataIndex ).second );
	}

	return dataMap;
}

}
