#include "OutputFile.hpp"

#include <AvTranscoder/util.hpp>

#include <stdexcept>

namespace avtranscoder
{

OutputFile::OutputFile( const std::string& filename )
	: _formatContext( AV_OPT_FLAG_ENCODING_PARAM )
	, _outputStreams()
	, _frameCount()
	, _previousProcessedStreamDuration( 0.0 )
	, _profile()
{
	_formatContext.setFilename( filename );
	_formatContext.setOutputFormat( filename );
}

OutputFile::~OutputFile()
{
	for( std::vector< OutputStream* >::iterator it = _outputStreams.begin(); it != _outputStreams.end(); ++it )
	{
		delete (*it);
	}
}

IOutputStream& OutputFile::addVideoStream( const VideoCodec& videoDesc )
{
	AVStream& stream = _formatContext.addAVStream( videoDesc.getAVCodec() );

	// copy video codec settings from input
	avcodec_copy_context( stream.codec, &videoDesc.getAVCodecContext() );

	// need to set the time_base on the AVCodecContext and the AVStream
	// compensating the frame rate with the ticks_per_frame and keeping
	// a coherent reading speed.
	av_reduce(
		&stream.codec->time_base.num,
		&stream.codec->time_base.den,
		videoDesc.getAVCodecContext().time_base.num * videoDesc.getAVCodecContext().ticks_per_frame,
		videoDesc.getAVCodecContext().time_base.den,
		INT_MAX );

	stream.time_base = stream.codec->time_base;

	OutputStream* avOutputStream = new OutputStream( *this, _formatContext.getNbStreams() - 1 );
	_outputStreams.push_back( avOutputStream );

	return *_outputStreams.back();
}

IOutputStream& OutputFile::addAudioStream( const AudioCodec& audioDesc )
{
	AVStream& stream = _formatContext.addAVStream( audioDesc.getAVCodec() );

	// copy audio codec settings from input
	avcodec_copy_context( stream.codec, &audioDesc.getAVCodecContext() );

	OutputStream* avOutputStream = new OutputStream( *this, _formatContext.getNbStreams() - 1 );
	_outputStreams.push_back( avOutputStream );

	return *_outputStreams.back();
}

IOutputStream& OutputFile::addDataStream( const DataCodec& dataDesc )
{
	AVStream& stream = _formatContext.addAVStream( dataDesc.getAVCodec() );

	// copy data codec settings from input
	avcodec_copy_context( stream.codec, &dataDesc.getAVCodecContext() );

	OutputStream* avOutputStream = new OutputStream( *this, _formatContext.getNbStreams() - 1 );
	_outputStreams.push_back( avOutputStream );

	return *_outputStreams.back();
}

IOutputStream& OutputFile::getStream( const size_t streamId )
{
	if( streamId >= _outputStreams.size() )
		throw std::runtime_error( "unable to get output stream (out of range)" );
	return *_outputStreams.at( streamId );
}

std::string OutputFile::getFilename() const
{
	return std::string( _formatContext.getAVFormatContext().filename );
}

std::string OutputFile::getFormatName() const
{
	if( _formatContext.getAVOutputFormat().name == NULL )
	{
		LOG_WARN("Unknown muxer format name of '" << getFilename() << "'.")
		return "";
	}
	return std::string(_formatContext.getAVOutputFormat().name);
}

std::string OutputFile::getFormatLongName() const
{
	if( _formatContext.getAVOutputFormat().long_name == NULL )
	{
		LOG_WARN("Unknown muxer format long name of '" << getFilename() << "'.")
		return "";
	}
	return std::string(_formatContext.getAVOutputFormat().long_name);
}

std::string OutputFile::getFormatMimeType() const
{
	if( _formatContext.getAVOutputFormat().mime_type == NULL )
	{
		LOG_WARN("Unknown muxer format mime type of '" << getFilename() << "'.")
		return "";
	}
	return std::string(_formatContext.getAVOutputFormat().mime_type);
}

bool OutputFile::beginWrap( )
{
	LOG_DEBUG( "Begin wrap of OutputFile" )

	_formatContext.openRessource( getFilename(), AVIO_FLAG_WRITE );
	_formatContext.writeHeader();

	// set specific wrapping options
	setupRemainingWrappingOptions();

	_frameCount.clear();
	_frameCount.resize( _outputStreams.size(), 0 );

	return true;
}

IOutputStream::EWrappingStatus OutputFile::wrap( const CodedData& data, const size_t streamId )
{
	if( ! data.getSize() )
		return IOutputStream::eWrappingSuccess;

	LOG_DEBUG( "Wrap on stream " << streamId << " (" << data.getSize() << " bytes for frame " << _frameCount.at( streamId ) << ")" )

	AVPacket& packetToWrap = const_cast<AVPacket&>( data.getAVPacket() );
	packetToWrap.stream_index = streamId;
	_formatContext.writeFrame( packetToWrap );

	const double currentStreamDuration = _outputStreams.at( streamId )->getStreamDuration();
	if( currentStreamDuration < _previousProcessedStreamDuration )
	{
		// if the current stream is strictly shorter than the previous, wait for more data
		return IOutputStream::eWrappingWaitingForData;
	}

	_previousProcessedStreamDuration = currentStreamDuration;
	_frameCount.at( streamId )++;

	return IOutputStream::eWrappingSuccess;
}

bool OutputFile::endWrap( )
{
	LOG_DEBUG( "End wrap of OutputFile" )

	_formatContext.writeTrailer();
	_formatContext.closeRessource();
	return true;
}

void OutputFile::addMetadata( const PropertyVector& data )
{
	for( PropertyVector::const_iterator it = data.begin(); it != data.end(); ++it )
	{
		addMetadata( it->first, it->second );
	}
}

void OutputFile::addMetadata( const std::string& key, const std::string& value )
{
	_formatContext.addMetaData( key, value );
}

void OutputFile::setupWrapping( const ProfileLoader::Profile& profile )
{
	// check the given profile
	const bool isValid = ProfileLoader::checkFormatProfile( profile );
	if( ! isValid )
	{
		const std::string msg( "Invalid format profile to setup wrapping." );
		LOG_ERROR( msg )
		throw std::runtime_error( msg );
	}

	LOG_INFO( "Setup wrapping with:\n" << profile )

	// check if output format indicated is valid with the filename extension
	if( ! matchFormat( profile.find( constants::avProfileFormat )->second, getFilename() ) )
	{
		throw std::runtime_error( "Invalid format according to the file extension." );
	}
	// set output format
	_formatContext.setOutputFormat( getFilename(), profile.find( constants::avProfileFormat )->second );

	// set common wrapping options
	setupWrappingOptions( profile );
}

void OutputFile::setupWrappingOptions( const ProfileLoader::Profile& profile )
{
	// set format options
	for( ProfileLoader::Profile::const_iterator it = profile.begin(); it != profile.end(); ++it )
	{
		if( (*it).first == constants::avProfileIdentificator ||
			(*it).first == constants::avProfileIdentificatorHuman ||
			(*it).first == constants::avProfileType ||
			(*it).first == constants::avProfileFormat )
			continue;

		try
		{
			Option& formatOption = _formatContext.getOption( (*it).first );
			formatOption.setString( (*it).second );
		}
		catch( std::exception& e )
		{
			LOG_INFO( "OutputFile - option " << (*it).first <<  " will be saved to be called when beginWrap" )
			_profile[ (*it).first ] = (*it).second;
		}
	}
}

void OutputFile::setupRemainingWrappingOptions()
{
	// set format options
	for( ProfileLoader::Profile::const_iterator it = _profile.begin(); it != _profile.end(); ++it )
	{
		if( (*it).first == constants::avProfileIdentificator ||
			(*it).first == constants::avProfileIdentificatorHuman ||
			(*it).first == constants::avProfileType ||
			(*it).first == constants::avProfileFormat )
			continue;

		try
		{
			Option& formatOption = _formatContext.getOption( (*it).first );
			formatOption.setString( (*it).second );
		}
		catch( std::exception& e )
		{
			LOG_WARN( "OutputFile - can't set option " << (*it).first <<  " to " << (*it).second << ": " << e.what() )
		}
	}
}

}
