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

namespace avtranscoder
{

InputFile::InputFile( const std::string& filename )
	: _formatContext( filename, AV_OPT_FLAG_DECODING_PARAM )
	, _properties( _formatContext )
	, _filename( filename )
	, _inputStreams()
	, _verbose( false )
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
				VideoProperties properties( _formatContext, streamIndex, progress, level );
				_properties.getVideoProperties().push_back( properties );
				break;
			}
			case AVMEDIA_TYPE_AUDIO:
			{
				AudioProperties properties( _formatContext, streamIndex );
				_properties.getAudioProperties().push_back( properties );
				break;
			}
			case AVMEDIA_TYPE_DATA:
			{
				DataProperties properties( _formatContext, streamIndex );
				_properties.getDataProperties().push_back( properties );
				break;
			}
			case AVMEDIA_TYPE_SUBTITLE:
			{
				SubtitleProperties properties( _formatContext, streamIndex );
				_properties.getSubtitleProperties().push_back( properties );
				break;
			}
			case AVMEDIA_TYPE_ATTACHMENT:
			{
				AttachementProperties properties( _formatContext, streamIndex );
				_properties.getAttachementProperties().push_back( properties );
				break;
			}
			case AVMEDIA_TYPE_UNKNOWN:
			{
				UnknownProperties properties( _formatContext, streamIndex );
				_properties.getUnknownPropertiesProperties().push_back( properties );
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
	double fps = 1;
	// Get Fps from first video stream
	if( _properties.getNbVideoStreams() )
		fps = _properties.getVideoProperties().at( 0 ).getFps();

	uint64_t pos = frame / fps * AV_TIME_BASE;

	// Offset of start time
	if( (int)_formatContext.getStartTime() != AV_NOPTS_VALUE )
		pos += _formatContext.getStartTime();

	if( av_seek_frame( &_formatContext.getAVFormatContext(), -1, pos, AVSEEK_FLAG_BACKWARD ) < 0 )
	{
		std::cerr << "Error during seek at " << frame << " (" << pos << ") in file" << std::endl;
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

void InputFile::setProfile( const ProfileLoader::Profile& profile )
{	
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
			if( _verbose )
				std::cout << "[InputFile] warning: " << e.what() << std::endl;
		}
	}
}

}