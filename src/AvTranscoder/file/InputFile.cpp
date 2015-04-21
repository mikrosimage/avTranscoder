#include "InputFile.hpp"

#include <AvTranscoder/mediaProperty/util.hpp>
#include <AvTranscoder/mediaProperty/VideoProperties.hpp>
#include <AvTranscoder/mediaProperty/AudioProperties.hpp>
#include <AvTranscoder/mediaProperty/DataProperties.hpp>
#include <AvTranscoder/mediaProperty/SubtitleProperties.hpp>
#include <AvTranscoder/mediaProperty/AttachementProperties.hpp>
#include <AvTranscoder/mediaProperty/UnknownProperties.hpp>
#include <AvTranscoder/progress/NoDisplayProgress.hpp>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/pixdesc.h>
}

#include <stdexcept>
#include <sstream>

namespace avtranscoder
{

InputFile::InputFile( const std::string& filename )
	: _formatContext( filename, AV_OPT_FLAG_DECODING_PARAM )
	, _properties( _formatContext )
	, _filename( filename )
	, _inputStreams()
{
	_formatContext.findStreamInfo();

	// Analyse header
	NoDisplayProgress p;
	analyse( p, eAnalyseLevelHeader );

	// Create streams
	for( size_t streamIndex = 0; streamIndex < _formatContext.getNbStreams(); ++streamIndex )
	{
		_inputStreams.push_back( new InputStream( *this, streamIndex ) );
	}
}

InputFile::~InputFile()
{
	for( std::vector< InputStream* >::iterator it = _inputStreams.begin(); it != _inputStreams.end(); ++it )
	{
		delete (*it);
	}
}

void InputFile::analyse( IProgress& progress, const EAnalyseLevel level )
{
	_properties.clearStreamProperties();

	if( level > eAnalyseLevelHeader )
		seekAtFrame( 0 );

	for( size_t streamIndex = 0; streamIndex < _formatContext.getNbStreams(); streamIndex++ )
	{
		switch( _formatContext.getAVStream( streamIndex ).codec->codec_type )
		{
			case AVMEDIA_TYPE_VIDEO:
			{
				const VideoProperties properties( _formatContext, streamIndex, progress, level );
				_properties.addVideoProperties( properties );
				break;
			}
			case AVMEDIA_TYPE_AUDIO:
			{
				const AudioProperties properties( _formatContext, streamIndex );
				_properties.addAudioProperties( properties );
				break;
			}
			case AVMEDIA_TYPE_DATA:
			{
				const DataProperties properties( _formatContext, streamIndex );
				_properties.addDataProperties( properties );
				break;
			}
			case AVMEDIA_TYPE_SUBTITLE:
			{
				const SubtitleProperties properties( _formatContext, streamIndex );
				_properties.addSubtitleProperties( properties );
				break;
			}
			case AVMEDIA_TYPE_ATTACHMENT:
			{
				const AttachementProperties properties( _formatContext, streamIndex );
				_properties.addAttachementProperties( properties );
				break;
			}
			case AVMEDIA_TYPE_UNKNOWN:
			{
				const UnknownProperties properties( _formatContext, streamIndex );
				_properties.addUnknownProperties( properties );
				break;
			}
			case AVMEDIA_TYPE_NB:
			{
				break;
			}
		}
	}

	if( level > eAnalyseLevelHeader )
		seekAtFrame( 0 );
}

FileProperties InputFile::analyseFile( const std::string& filename, IProgress& progress, const EAnalyseLevel level )
{
	InputFile file( filename );
	file.analyse( progress, level );
	return file.getProperties();
}

bool InputFile::readNextPacket( CodedData& data, const size_t streamIndex )
{
	bool nextPacketFound = false;
	while( ! nextPacketFound )
	{
		int ret = av_read_frame( &_formatContext.getAVFormatContext(), &data.getAVPacket() );
		if( ret < 0 ) // error or end of file
		{
			return false;
		}

		// if the packet stream is the expected one
		// return the packet data
		int packetStreamIndex = data.getAVPacket().stream_index;
		if( packetStreamIndex == (int)streamIndex )
		{
			nextPacketFound = true;
		}
		// else add the packet data to the stream cache
		else
		{
			_inputStreams.at( packetStreamIndex )->addPacket( data.getAVPacket() );
			data.clear();
		}
	}
	return true;
}

void InputFile::seekAtFrame( const size_t frame )
{
	seek( frame, AVSEEK_FLAG_FRAME );
}

void InputFile::seekAtTime( const double time )
{
	uint64_t position = time * AV_TIME_BASE;
	seek( position, AVSEEK_FLAG_BACKWARD );
}

void InputFile::seek( uint64_t position, const int flag )
{
	if( (int)_formatContext.getStartTime() != AV_NOPTS_VALUE )
		position += _formatContext.getStartTime();

	if( av_seek_frame( &_formatContext.getAVFormatContext(), -1, position, flag ) < 0 )
	{
		LOG_ERROR( "Error when seek at " << position << " (in AV_TIME_BASE units) in file" )
	}

	for( std::vector<InputStream*>::iterator it = _inputStreams.begin(); it != _inputStreams.end(); ++it )
	{
		(*it)->clearBuffering();
	}
}

void InputFile::activateStream( const size_t streamIndex, bool activate )
{
	getStream( streamIndex ).activate( activate );
}

InputStream& InputFile::getStream( size_t index )
{
	try
	{
		return *_inputStreams.at( index );
	}
	catch( const std::out_of_range& e )
	{
		std::stringstream msg;
		msg << getFilename();
		msg << " has no stream at index ";
		msg << index;
		throw std::runtime_error( msg.str() );
	}
}

double InputFile::getFps()
{
	double fps = 1;
	if( _properties.getNbVideoStreams() )
		fps = _properties.getVideoProperties().at( 0 ).getFps();
	return fps;
}

void InputFile::setProfile( const ProfileLoader::Profile& profile )
{
	LOG_DEBUG( "Set profile of input file with:\n" << profile )

	for( ProfileLoader::Profile::const_iterator it = profile.begin(); it != profile.end(); ++it )
	{
		if( (*it).first == constants::avProfileIdentificator ||
			(*it).first == constants::avProfileIdentificatorHuman ||
			(*it).first == constants::avProfileType )
			continue;
		
		try
		{
			Option& formatOption = _formatContext.getOption( (*it).first );
			formatOption.setString( (*it).second );
		}
		catch( std::exception& e )
		{
			LOG_WARN( "InputFile - can't set option " << (*it).first <<  " to " << (*it).second << ": " << e.what() )
		}
	}
}

}