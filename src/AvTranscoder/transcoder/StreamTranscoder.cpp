
#include "StreamTranscoder.hpp"

#include <AvTranscoder/stream/InputStream.hpp>

#include <AvTranscoder/decoder/VideoDecoder.hpp>
#include <AvTranscoder/decoder/AudioDecoder.hpp>
#include <AvTranscoder/decoder/VideoGenerator.hpp>
#include <AvTranscoder/decoder/AudioGenerator.hpp>
#include <AvTranscoder/encoder/VideoEncoder.hpp>
#include <AvTranscoder/encoder/AudioEncoder.hpp>

#include <AvTranscoder/transform/AudioTransform.hpp>
#include <AvTranscoder/transform/VideoTransform.hpp>

#include <cassert>
#include <limits>
#include <sstream>

namespace avtranscoder
{

StreamTranscoder::StreamTranscoder(
		IInputStream& inputStream,
		IOutputFile& outputFile
	)
	: _inputStream( &inputStream )
	, _outputStream( NULL )
	, _sourceBuffer( NULL )
	, _frameBuffer( NULL )
	, _inputDecoder( NULL )
	, _generator( NULL )
	, _currentDecoder( NULL )
	, _outputEncoder( NULL )
	, _transform( NULL )
	, _subStreamIndex( -1 )
	, _offset( 0 )
	, _canSwitchToGenerator( false )
{
	// create a re-wrapping case
	switch( _inputStream->getStreamType() )
	{
		case AVMEDIA_TYPE_VIDEO :
		{
			VideoFrameDesc inputFrameDesc( _inputStream->getVideoCodec().getVideoFrameDesc() );

			// generator decoder
			VideoGenerator* generatorVideo = new VideoGenerator();
			generatorVideo->setVideoFrameDesc( inputFrameDesc );
			_generator = generatorVideo;

			// buffers to process
			_sourceBuffer = new VideoFrame( inputFrameDesc );
			_frameBuffer = new VideoFrame( inputFrameDesc );

			// transform
			_transform = new VideoTransform();

			// output encoder
			VideoEncoder* outputVideo = new VideoEncoder( _inputStream->getVideoCodec().getCodecName() );
			outputVideo->getVideoCodec().setImageParameters( inputFrameDesc );
			outputVideo->setup();
			_outputEncoder = outputVideo;

			// output stream
			_outputStream = &outputFile.addVideoStream( _inputStream->getVideoCodec() );

			break;
		}
		case AVMEDIA_TYPE_AUDIO :
		{
			AudioFrameDesc inputFrameDesc( _inputStream->getAudioCodec().getAudioFrameDesc() );

			// generator decoder
			AudioGenerator* generatorAudio = new AudioGenerator();
			generatorAudio->setAudioFrameDesc( inputFrameDesc );
			_generator = generatorAudio;

			// buffers to process
			_sourceBuffer = new AudioFrame( inputFrameDesc );
			_frameBuffer  = new AudioFrame( inputFrameDesc );

			// transform
			_transform = new AudioTransform();

			// output encoder
			AudioEncoder* outputAudio = new AudioEncoder( _inputStream->getAudioCodec().getCodecName()  );
			outputAudio->getAudioCodec().setAudioParameters( inputFrameDesc );
			outputAudio->setup();
			_outputEncoder = outputAudio;

			// output stream
			_outputStream = &outputFile.addAudioStream( _inputStream->getAudioCodec() );

			break;
		}
		case AVMEDIA_TYPE_DATA :
		{
			// @warning: rewrap a data stream can't be lengthen by a generator (end of rewrapping will end the all process)
			_outputStream = &outputFile.addDataStream( _inputStream->getDataCodec() );
			break;
		}
		default:
			break;
	}
}

StreamTranscoder::StreamTranscoder(
		IInputStream& inputStream,
		IOutputFile& outputFile,
		const ProfileLoader::Profile& profile,
		const int subStreamIndex,
		const double offset
	)
	: _inputStream( &inputStream )
	, _outputStream( NULL )
	, _sourceBuffer( NULL )
	, _frameBuffer( NULL )
	, _inputDecoder( NULL )
	, _generator( NULL )
	, _currentDecoder( NULL )
	, _outputEncoder( NULL )
	, _transform( NULL )
	, _subStreamIndex( subStreamIndex )
	, _offset( offset )
	, _canSwitchToGenerator( false )
{
	// create a transcode case
	switch( _inputStream->getStreamType() )
	{
		case AVMEDIA_TYPE_VIDEO :
		{
			// input decoder
			VideoDecoder* inputVideo = new VideoDecoder( *static_cast<InputStream*>( _inputStream ) );
			// set decoder options with empty profile to set some key options to specific values (example: threads to auto)
			inputVideo->setProfile( ProfileLoader::Profile() );
			inputVideo->setup();
			_inputDecoder = inputVideo;
			_currentDecoder = _inputDecoder;

			// output encoder
			VideoEncoder* outputVideo = new VideoEncoder( profile.at( constants::avProfileCodec ) );
			_outputEncoder = outputVideo;

			VideoFrameDesc outputFrameDesc = _inputStream->getVideoCodec().getVideoFrameDesc();
			outputFrameDesc.setParameters( profile );
			outputVideo->setProfile( profile, outputFrameDesc );

			// output stream
			_outputStream = &outputFile.addVideoStream( outputVideo->getVideoCodec() );

			// buffers to process
			_sourceBuffer = new VideoFrame( _inputStream->getVideoCodec().getVideoFrameDesc() );
			_frameBuffer = new VideoFrame( outputVideo->getVideoCodec().getVideoFrameDesc() );

			// transform
			_transform = new VideoTransform();

			// generator decoder
			VideoGenerator* generatorVideo = new VideoGenerator();
			generatorVideo->setVideoFrameDesc( outputVideo->getVideoCodec().getVideoFrameDesc() );
			_generator = generatorVideo;

			break;
		}
		case AVMEDIA_TYPE_AUDIO :
		{
			// input decoder
			AudioDecoder* inputAudio = new AudioDecoder( *static_cast<InputStream*>( _inputStream ) );
			// set decoder options with empty profile to set some key options to specific values (example: threads to auto)
			inputAudio->setProfile( ProfileLoader::Profile() );
			inputAudio->setup();
			_inputDecoder = inputAudio;
			_currentDecoder = _inputDecoder;

			// output encoder
			AudioEncoder* outputAudio = new AudioEncoder( profile.at( constants::avProfileCodec )  );
			_outputEncoder = outputAudio;

			AudioFrameDesc outputFrameDesc( _inputStream->getAudioCodec().getAudioFrameDesc() );
			outputFrameDesc.setParameters( profile );
			if( subStreamIndex > -1 )
			{
				// @todo manage downmix ?
				outputFrameDesc.setChannels( 1 );
			}
			outputAudio->setProfile( profile, outputFrameDesc );

			// output stream
			_outputStream = &outputFile.addAudioStream( outputAudio->getAudioCodec() );

			// buffers to process
			AudioFrameDesc inputFrameDesc( _inputStream->getAudioCodec().getAudioFrameDesc() );
			if( subStreamIndex > -1 )
				inputFrameDesc.setChannels( 1 );

			_sourceBuffer = new AudioFrame( inputFrameDesc );
			_frameBuffer  = new AudioFrame( outputAudio->getAudioCodec().getAudioFrameDesc() );

			// transform
			_transform = new AudioTransform();

			// generator decoder
			AudioGenerator* generatorAudio = new AudioGenerator();
			generatorAudio->setAudioFrameDesc( outputAudio->getAudioCodec().getAudioFrameDesc() );
			_generator = generatorAudio;

			break;
		}
		default:
		{
			throw std::runtime_error( "unupported stream type" );
			break;
		}
	}
	if( offset )
		switchToGeneratorDecoder();
}

StreamTranscoder::StreamTranscoder(
		const ICodec& inputCodec,
		IOutputFile& outputFile,
		const ProfileLoader::Profile& profile
	)
	: _inputStream( NULL )
	, _outputStream( NULL )
	, _sourceBuffer( NULL )
	, _frameBuffer( NULL )
	, _inputDecoder( NULL )
	, _generator( NULL )
	, _currentDecoder( NULL )
	, _outputEncoder( NULL )
	, _transform( NULL )
	, _subStreamIndex( -1 )
	, _offset( 0 )
	, _canSwitchToGenerator( false )
{
	if( profile.find( constants::avProfileType )->second == constants::avProfileTypeVideo )
	{
		// generator decoder
		VideoGenerator* generatorVideo = new VideoGenerator();
		const VideoCodec& inputVideoCodec = static_cast<const VideoCodec&>( inputCodec );
		generatorVideo->setVideoFrameDesc( inputVideoCodec.getVideoFrameDesc() );
		_currentDecoder = generatorVideo;

		// buffers to process
		VideoFrameDesc inputFrameDesc = inputVideoCodec.getVideoFrameDesc();
		VideoFrameDesc outputFrameDesc = inputFrameDesc;
		outputFrameDesc.setParameters( profile );
		_sourceBuffer = new VideoFrame( inputFrameDesc );
		_frameBuffer  = new VideoFrame( outputFrameDesc );

		// transform
		_transform = new VideoTransform();

		// output encoder
		VideoEncoder* outputVideo = new VideoEncoder( profile.at( constants::avProfileCodec ) );
		outputVideo->setProfile( profile, outputFrameDesc );
		_outputEncoder = outputVideo;

		// output stream
		_outputStream = &outputFile.addVideoStream( outputVideo->getVideoCodec() );
	}
	else if( profile.find( constants::avProfileType )->second == constants::avProfileTypeAudio )
	{
		// generator decoder
		AudioGenerator* generatorAudio = new AudioGenerator();
		const AudioCodec& inputAudioCodec = static_cast<const AudioCodec&>( inputCodec );
		generatorAudio->setAudioFrameDesc( inputAudioCodec.getAudioFrameDesc() );
		_currentDecoder = generatorAudio;

		// buffers to process
		AudioFrameDesc inputFrameDesc = inputAudioCodec.getAudioFrameDesc();
		AudioFrameDesc outputFrameDesc = inputFrameDesc;
		outputFrameDesc.setParameters( profile );
		_sourceBuffer = new AudioFrame( inputFrameDesc );
		_frameBuffer  = new AudioFrame( outputFrameDesc );

		// transform
		_transform = new AudioTransform();

		// output encoder
		AudioEncoder* outputAudio = new AudioEncoder( profile.at( constants::avProfileCodec ) );
		outputAudio->setProfile( profile, outputFrameDesc );
		_outputEncoder = outputAudio;

		// output stream
		_outputStream = &outputFile.addAudioStream( outputAudio->getAudioCodec() );
	}
	else
	{
		throw std::runtime_error( "unupported stream type" );
	}
}

StreamTranscoder::~StreamTranscoder()
{
	delete _sourceBuffer;
	delete _frameBuffer;
	delete _generator;
	delete _outputEncoder;
	delete _transform;
	delete _inputDecoder;
}

void StreamTranscoder::preProcessCodecLatency()
{
	// rewrap case: no need to take care of the latency of codec
	if( ! _currentDecoder )
		return;

	int latency = _outputEncoder->getCodec().getLatency();

	LOG_DEBUG( "Latency of stream: " << latency )

	if( ! latency ||
		latency < _outputEncoder->getCodec().getAVCodecContext().frame_number )
		return;

	while( ( latency-- ) > 0 )
	{
		processFrame();
	}
}

bool StreamTranscoder::processFrame()
{
	if( ! _currentDecoder )
	{
		return processRewrap();
	}

	if( _subStreamIndex < 0 )
	{
		return processTranscode();
	}
	return processTranscode( _subStreamIndex );	
}

bool StreamTranscoder::processRewrap()
{
	assert( _inputStream  != NULL );
	assert( _outputStream != NULL );
	
	LOG_DEBUG( "Rewrap a frame" )

	CodedData data;
	if( ! _inputStream->readNextPacket( data ) )
	{
		if( _canSwitchToGenerator )
		{
			switchToGeneratorDecoder();
			return processTranscode();
		}
		return false;
	}

	IOutputStream::EWrappingStatus wrappingStatus = _outputStream->wrap( data );

	switch( wrappingStatus )
	{
		case IOutputStream::eWrappingSuccess:
			return true;
		case IOutputStream::eWrappingWaitingForData:
			// the wrapper needs more data to write the current packet
			return processRewrap();
		case IOutputStream::eWrappingError:
			return false;
	}

	return true;
}

bool StreamTranscoder::processTranscode( const int subStreamIndex )
{
	assert( _outputStream   != NULL );
	assert( _currentDecoder != NULL );
	assert( _outputEncoder  != NULL );
	assert( _sourceBuffer   != NULL );
	assert( _frameBuffer    != NULL );
	assert( _transform      != NULL );

	LOG_DEBUG( "Transcode a frame" )

	// check offset
	if( _offset )
	{
		bool endOfOffset = _outputStream->getStreamDuration() >= _offset;
		if( endOfOffset )
		{
			// switch to essence from input stream
			switchToInputDecoder();
			// reset offset
			_offset = 0;
		}
	}

	bool decodingStatus = false;
	if( subStreamIndex == -1 )
		decodingStatus = _currentDecoder->decodeNextFrame( *_sourceBuffer );
	else
		decodingStatus = _currentDecoder->decodeNextFrame( *_sourceBuffer, subStreamIndex );

	CodedData data;
	if( decodingStatus )
	{
		LOG_DEBUG( "convert (" << _sourceBuffer->getSize() << " bytes)" )
		_transform->convert( *_sourceBuffer, *_frameBuffer );

		LOG_DEBUG( "encode (" << _frameBuffer->getSize() << " bytes)" )
		_outputEncoder->encodeFrame( *_frameBuffer, data );
	}
	else
	{
		LOG_DEBUG( "encode last frame(s)" )
		if( ! _outputEncoder->encodeFrame( data ) )
		{
			if( _canSwitchToGenerator )
			{
				switchToGeneratorDecoder();
				return processTranscode();
			}
			return false;
		}
	}

	LOG_DEBUG( "wrap (" << data.getSize() << " bytes)" )

	IOutputStream::EWrappingStatus wrappingStatus = _outputStream->wrap( data );
	switch( wrappingStatus )
	{
		case IOutputStream::eWrappingSuccess:
			return true;
		case IOutputStream::eWrappingWaitingForData:
			// the wrapper needs more data to write the current packet
			return processTranscode( subStreamIndex );
		case IOutputStream::eWrappingError:
			return false;
	}

	return true;
}

void StreamTranscoder::switchToGeneratorDecoder()
{
	_currentDecoder = _generator;
	assert( _currentDecoder != NULL );
}

void StreamTranscoder::switchToInputDecoder()
{
	_currentDecoder = _inputDecoder;
	assert( _currentDecoder != NULL );
}

double StreamTranscoder::getDuration() const
{	
	if( _inputStream )
	{
		double totalDuration = _inputStream->getDuration() + _offset;
		return totalDuration;
	}
	else
		return std::numeric_limits<double>::max();
}

}
