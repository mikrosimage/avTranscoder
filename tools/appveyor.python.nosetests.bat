set PWD=%~dp0

:: Get avtranscoder library
set PYTHONPATH=%PWD%/build/dist/lib/python2.7.6/site-packages/:%PYTHONPATH%

:: Get avtranscoder profiles
set AVPROFILES=%PWD%/build/dist/share/ressource

:: Get assets
git clone https://github.com/avTranscoder/avTranscoder-data.git
set AVTRANSCODER_TEST_VIDEO_FILE=%PWD%/avTranscoder-data/video/BigBuckBunny/BigBuckBunny_480p_stereo.avi
set AVTRANSCODER_TEST_AUDIO_WAVE_FILE=%PWD%/avTranscoder-data/audio/frequenciesPerChannel.wav
set AVTRANSCODER_TEST_AUDIO_MOV_FILE=%PWD%/avTranscoder-data/video/BigBuckBunny/BigBuckBunny_1080p_5_1.mov

:: Launch tests
cd test/pyTest
nosetests

