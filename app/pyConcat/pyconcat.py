import sys

from pyAvTranscoder import avtranscoder as av

av.preloadCodecsAndFormats()
av.setLogLevel( av.AV_LOG_QUIET )


def parseConfigFile( inputConfigFile ):
	"""
	Parse a config file with for each lines:
	create an output stream
	"""
	outputVideoStreamCreated = False
	outputAudioStreamCreated = False

	file = open( inputConfigFile, 'r' )
	for line in file:
		line = line.strip( '\n' )

		# input file
		inputFiles.append( av.InputFile( line ) )
		# input stream
		inputStreams.append( av.InputStream( inputFiles[-1], 0 ) )
		inputStreams[-1].activate()
		# output stream
		if inputStreams[-1].getStreamType() == av.AVMEDIA_TYPE_VIDEO and not outputVideoStreamCreated:
			outputFile.addVideoStream( inputStreams[-1].getVideoCodec() )
			outputVideoStreamCreated = True
		elif inputStreams[-1].getStreamType() == av.AVMEDIA_TYPE_AUDIO and not outputAudioStreamCreated:
			outputFile.addAudioStream( inputStreams[-1].getAudioCodec() )
			outputAudioStreamCreated = True

# output
outputFile = av.OutputFile( sys.argv[2] );
outputFile.setup();
inputFiles = []
inputStreams = []
outputVideoStreamIndex = 0
outputAudioStreamIndex = 1

# parse configuration file
inputConfigFile = sys.argv[1]
parseConfigFile( inputConfigFile )

# process
outputFile.beginWrap()

data = av.Frame()
for inputFile in inputFiles:
	print "add file", inputFile.getFilename()
	packetRead = True

	while packetRead:
		# read
		packetRead = inputFile.readNextPacket( data, 0 )
		# wrap
		if inputFile.getStream( 0 ).getStreamType() == av.AVMEDIA_TYPE_VIDEO:
			outputFile.wrap( data, outputVideoStreamIndex )
		elif inputFile.getStream( 0 ).getStreamType() == av.AVMEDIA_TYPE_AUDIO:
			outputFile.wrap( data, outputAudioStreamIndex )

outputFile.endWrap()
