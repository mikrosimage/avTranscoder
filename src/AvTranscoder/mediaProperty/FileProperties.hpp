#ifndef _AV_TRANSCODER_MEDIA_PROPERTY_FILE_PROPERTIES_HPP
#define _AV_TRANSCODER_MEDIA_PROPERTY_FILE_PROPERTIES_HPP

#include <AvTranscoder/common.hpp>
#include <AvTranscoder/mediaProperty/util.hpp>
#include <AvTranscoder/file/FormatContext.hpp>
#include <AvTranscoder/mediaProperty/StreamProperties.hpp>
#include <AvTranscoder/mediaProperty/VideoProperties.hpp>
#include <AvTranscoder/mediaProperty/AudioProperties.hpp>
#include <AvTranscoder/mediaProperty/DataProperties.hpp>
#include <AvTranscoder/mediaProperty/SubtitleProperties.hpp>
#include <AvTranscoder/mediaProperty/AttachementProperties.hpp>
#include <AvTranscoder/mediaProperty/UnknownProperties.hpp>

#include <string>
#include <vector>

namespace avtranscoder
{

class AvExport FileProperties
{
public:
	FileProperties( const FormatContext& formatContext );

	std::string getFilename() const;
	std::string getFormatName() const;  ///< A comma separated list of short names for the format
	std::string getFormatLongName() const;

	void addVideoProperties( const VideoProperties& properties );
	void addAudioProperties( const AudioProperties& properties );
	void addDataProperties( const DataProperties& properties );
	void addSubtitleProperties( const SubtitleProperties& properties );
	void addAttachementProperties( const AttachementProperties& properties );
	void addUnknownProperties( const UnknownProperties& properties );

	size_t getProgramsCount() const;
	double getStartTime() const;
	double getDuration() const;  ///< in seconds
	size_t getBitRate() const;  ///< total stream bitrate in bit/s, 0 if not available (result of a computation by ffmpeg)
	size_t getPacketSize() const;

	PropertyVector& getMetadatas() { return _metadatas; }

	size_t getNbStreams() const;
	size_t getNbVideoStreams() const { return _videoStreams.size(); }
	size_t getNbAudioStreams() const { return _audioStreams.size(); }
	size_t getNbDataStreams() const { return _dataStreams.size(); }
	size_t getNbSubtitleStreams() const { return _subtitleStreams.size(); }
	size_t getNbAttachementStreams() const { return _attachementStreams.size(); }
	size_t getNbUnknownStreams() const { return _unknownStreams.size(); }

	// @brief Get the properties with the indicated stream index
	const avtranscoder::StreamProperties& getPropertiesWithStreamIndex( const size_t streamIndex ) const;

	//@{
	// @brief Get the list of properties for a given type (video, audio...)
	const std::vector< avtranscoder::StreamProperties* >& getStreamProperties() const  { return  _streams; }
	const std::vector< avtranscoder::VideoProperties >& getVideoProperties() const  { return  _videoStreams; }
	const std::vector< avtranscoder::AudioProperties >& getAudioProperties() const  { return  _audioStreams; }
	const std::vector< avtranscoder::DataProperties >& getDataProperties() const  { return  _dataStreams; }
	const std::vector< avtranscoder::SubtitleProperties >& getSubtitleProperties() const  { return  _subtitleStreams; }
	const std::vector< avtranscoder::AttachementProperties >& getAttachementProperties() const  { return  _attachementStreams; }
	const std::vector< avtranscoder::UnknownProperties >& getUnknownPropertiesProperties() const  { return  _unknownStreams; }
	//@}

#ifndef SWIG
	const AVFormatContext& getAVFormatContext() { return *_formatContext; }
#endif

	PropertyVector getPropertiesAsVector() const;  ///< Return all file properties as a vector (name of property: value)

	void clearStreamProperties();  ///< Clear all array of stream properties

private:
#ifndef SWIG
	template<typename T>
	void addProperty( PropertyVector& data, const std::string& key, T (FileProperties::*getter)(void) const ) const
	{
		try
		{
			detail::add( data, key, (this->*getter)() );
		}
		catch( const std::exception& e )
		{
			detail::add( data, key, e.what() );
		}
	}
#endif

private:
	const AVFormatContext* _formatContext;  ///< Has link (no ownership)

	std::vector< StreamProperties* > _streams;  ///< Array of properties per stream (of all types) - only references to the following properties
	std::vector< VideoProperties > _videoStreams;  ///< Array of properties per video stream
	std::vector< AudioProperties >  _audioStreams;  ///< Array of properties per audio stream
	std::vector< DataProperties > _dataStreams;  ///< Array of properties per data stream
	std::vector< SubtitleProperties > _subtitleStreams;  ///< Array of properties per subtitle stream
	std::vector< AttachementProperties > _attachementStreams;  ///< Array of properties per attachement stream
	std::vector< UnknownProperties > _unknownStreams;  ///< Array of properties per unknown stream

	PropertyVector _metadatas;
};

}

#endif
