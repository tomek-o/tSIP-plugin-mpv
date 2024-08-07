//---------------------------------------------------------------------------
#pragma hdrstop

/**	\note Do not use just "Mplayer" as this module name - link conflict with
	another module installed with IDE.
*/

#include "Mpv.h"
#include "Log.h"
#include <mpv/client.h>
#include <SysUtils.hpp>
#include <math.h>
#include <stdio.h>

#include <Classes.hpp>
#include <Controls.hpp>
#include <ExtCtrls.hpp>

//---------------------------------------------------------------------------

#pragma package(smart_init)
#pragma link "libmpv-2.lib"

MPlayer mplayer;

MPlayer::MPlayer():
	mpv(NULL),
	parent(INVALID_HANDLE_VALUE),
	timer(NULL),
	filePositionValid(false),
	filePosition(0.0),
	fileLengthValid(false),
	fileLength(0.0),
	callbackStopPlaying(NULL),
	callbackMediaInfoUpdate(NULL),
	fileStarted(false),
	useStopCallback(true),
	absoluteSeekRequested(-1)
{
	timer = new TTimer(NULL);
	timer->Interval = 20;
	timer->OnTimer = timerTimer;
	timer->Enabled = true;
}

int MPlayer::configure(const Cfg& cfg)
{
	this->cfg = cfg;
    applyConfiguration();	
	return 0;
}

MPlayer::~MPlayer()
{
	if (timer) {
		timer->Enabled = false;
		delete timer;
		timer = NULL;
	}
	mpvDestroy();
}

void MPlayer::onStopPlayingFn(void)
{
	if (callbackStopPlaying)
		callbackStopPlaying();
}

int MPlayer::create(void)
{
	if (mpv == NULL)
		mpvCreate();
	if (mpv == NULL)
	{
		LOG("Failed to create mpv!");
		return -1;
	}
	return 0;
}

int MPlayer::play(AnsiString filename)
{
	AnsiString cmdLine;
	this->filename = filename;
	useStopCallback = true;

	LOG("play %s", filename.c_str());

	mediaInfo.videoBitrateKnown = false;
	mediaInfo.audioBitrateKnown = false;
	filePositionValid = false;
	fileLengthValid = false;
	if (this->callbackMediaInfoUpdate)
	{
    	callbackMediaInfoUpdate();
	}

	{
		int32_t flag = 0;
		mpv_set_property(mpv, "pause", MPV_FORMAT_FLAG, &flag);
	}
#if 0
	{
		char *value = "udp";
		mpv_set_property(mpv, "rtsp-transport", MPV_FORMAT_STRING, &value);
	}
#endif
	AnsiString utf8name = System::AnsiToUtf8(filename);
	const char *cmd[] = { "loadfile", utf8name.c_str(), NULL };
	int status = mpv_command(mpv, cmd);

	return status;
}

int MPlayer::pause(bool state)
{
	if (mpv == NULL)
		return -1;
	int status;
	int32_t flag = state;
	LOG("pause: %d", flag);
	status = mpv_set_property(mpv, "pause", MPV_FORMAT_FLAG, &flag);
	if (status != 0)
	{
		LOG("pause: error: %d %s", status, mpv_error_string(status));
	}
	return status;
}

int MPlayer::setPropertyString(AnsiString property, AnsiString value)
{
	if (mpv == NULL)
	{
		LOG("mpv instance for player %p not created!", this);
		return -1;
	}
	LOG("setting property string [%s] to [%s]", property.c_str(), value.c_str());
	int status = mpv_set_property_string(mpv, property.c_str(), value.c_str());
	if (status != 0)
	{
		LOG("mpv_set_property_string: error: %d %s", status, mpv_error_string(status));
	}
	return status;
}


int MPlayer::frameStep(void)
{
	if (mpv == NULL)
		return -1;
	LOG("frame-step");
    const char *cmd[] = { "frame-step", NULL };
	return mpv_command(mpv, cmd);
}

int MPlayer::seekRelative(int seconds)
{
	if (mpv == NULL)
		return -1;
	LOG("seekRelative %d");
	AnsiString secondsStr = seconds;
	const char *cmd[] = { "seek", secondsStr.c_str(), "relative", NULL };
	return mpv_command(mpv, cmd);
}

int MPlayer::seekAbsolute(double seconds)
{
	if (mpv == NULL)
		return -1;
	if (!fileStarted)
	{
		LOG("Delaying seekAbsolute to %.1f", seconds);
		absoluteSeekRequested = seconds;
		return 0;
	}
	if (fileLengthValid)
	{
		if (seconds > fileLength)
		{
			absoluteSeekRequested = -1;
			return -1;
		}
	}
	LOG("seekAbsolute to %.1f", seconds);
	absoluteSeekRequested = -1;
	AnsiString msg;
	msg.sprintf("%f", seconds);
	AnsiString secondsStr = seconds;
	const char *cmd[] = { "seek", msg.c_str(), "absolute", NULL };
	return mpv_command(mpv, cmd);
}

int MPlayer::setOsdLevel(int level)
{
	if (mpv == NULL)
		return -1;
	int64_t val = level;
	if (mpv_set_property(mpv, "osd-level", MPV_FORMAT_INT64, &val) < 0) {
		LOG("failed to set mpv osd level");
		return -2;
	}
	return 0;
}

int MPlayer::changeVolume(int delta)
{
	if (mpv == NULL)
		return -1;

	if (cfg.softVolLevel + delta < 0 || cfg.softVolLevel + delta > cfg.softVolMax)
		return 0;
	cfg.softVolLevel += delta;

	int64_t val = cfg.softVolLevel;
	if (mpv_set_property(mpv, "volume", MPV_FORMAT_INT64, &val) < 0) {
		LOG("failed to set mpv volume");
		return -2;
	}
	return 0;
}

int MPlayer::changeVolumeAbs(int val)
{
	if (mpv == NULL)
	{
		LOG("changeVolumeAbs: mpv not created!");
		return -1;
	}

	AnsiString msg;
	cfg.softVolLevel = val;
	int64_t val64 = cfg.softVolLevel;
	if (mpv_set_property(mpv, "volume", MPV_FORMAT_INT64, &val64) < 0) {
		LOG("failed to set mpv volume");
		return -2;
	}
	return 0;
}

int MPlayer::osdShowText(AnsiString text, int duration)
{
	if (mpv == NULL)
		return -1;

	AnsiString durationStr = duration;
	const char* cmd[] = { "show-text", text.c_str(), durationStr.c_str(), "0", NULL };
	return mpv_command(mpv, cmd);
}

int MPlayer::setSubVisibility(bool state)
{
	int val = state ? 1 : 0;
	if (mpv_set_property(mpv, "sub-visibility", MPV_FORMAT_FLAG, &val) < 0) {
		LOG("failed to set mpv sub-visibility");
		return -1;
	}
	return 0;
}

int MPlayer::stop(bool useCallback)
{
	if (mpv == NULL)
		return -1;
	if (fileStarted == false)
	{
		LOG("stop: file not started");
		return -1;
	}
	LOG("Stopping");
	useStopCallback = useCallback;
	const char *cmd[] = { "stop", NULL };
	int status = mpv_command(mpv, cmd);
	int cnt = 0;
	while (fileStarted)
	{
		Application->ProcessMessages();
		Sleep(20);
		if (cnt > 500)
		{
			LOG("Timed out stopping...");
			return -3;
		}
	}
	return status;
}

void __fastcall MPlayer::timerTimer(TObject *Sender)
{
    while (mpv) {
        const mpv_event *e = mpv_wait_event(mpv, 0);
        if (e->event_id == MPV_EVENT_NONE)
            break;
        onMpvEvent(*e);
    }
}

void MPlayer::onMpvEvent(const mpv_event &e)
{
	/** \note mpv_event_id is defined in header as event, which is not quite portable
		event_id type should not be changed to mpv_event_id below!
	*/
	int event_id = e.event_id;
#ifdef __BORLANDC__
#pragma warn -8006	// disable "Initializing mpv_event_id with int" warning
#endif
	const char *event_name = mpv_event_name(event_id);
#ifdef __BORLANDC__
#pragma warn .8006
#endif

	if (event_id != MPV_EVENT_LOG_MESSAGE && event_id != MPV_EVENT_PROPERTY_CHANGE)
	{
		LOG("Event: id = %d (%s)", static_cast<int>(event_id), event_name);
	}
	switch (event_id)
	{
	case MPV_EVENT_LOG_MESSAGE: {
		const mpv_event_log_message *msg = (const mpv_event_log_message *)e.data;
		AnsiString logLine = msg->text;
		logLine = logLine.TrimRight();
		LOG("%s", logLine.c_str());
		break;
	}
	case MPV_EVENT_PROPERTY_CHANGE: {
		const mpv_event_property *prop = (mpv_event_property *)e.data;
		// ignore logging of some properties
		if (strcmp(prop->name, "time-pos") &&
			strcmp(prop->name, "video-bitrate") &&
			strcmp(prop->name, "audio-bitrate") &&
			strcmp(prop->name, "volume") &&
			strcmp(prop->name, "duration")	/* ignored because of constant updates when receiving RTSP */
			) {
			LOG("Event: id = %d (%s: %s)", static_cast<int>(event_id), event_name, prop->name);
		}
        if (strcmp(prop->name, "media-title") == 0) {
            char *data = NULL;
			if (mpv_get_property(mpv, prop->name, MPV_FORMAT_OSD_STRING, &data) < 0) {
				//SetTitle("mpv");
			} else {
			#if 0
				wxString title = wxString::FromUTF8(data);
				if (!title.IsEmpty())
					title += " - ";
				title += "mpv";
				SetTitle(title);
			#endif
				mpv_free(data);
            }
		} else if (strcmp(prop->name, "video-bitrate") == 0) {
			double data = 0;
			if (mpv_get_property(mpv, prop->name, MPV_FORMAT_DOUBLE, &data) == 0) {
				if (mediaInfo.videoBitrateKnown == false) {
					mediaInfo.videoBitrateKnown = true;
				}
				//LOG("Video: %d bps", static_cast<int>(data));
				mediaInfo.videoBitrate = static_cast<int>(data);
				if (this->callbackMediaInfoUpdate)
				{
					callbackMediaInfoUpdate();
				}
			}
		} else if (strcmp(prop->name, "audio-bitrate") == 0) {
			double data = 0;
			if (mpv_get_property(mpv, prop->name, MPV_FORMAT_DOUBLE, &data) == 0) {
				if (mediaInfo.audioBitrateKnown == false) {
					mediaInfo.audioBitrateKnown = true;
				}
				//LOG("Audio: %d bps", static_cast<int>(data));
				mediaInfo.audioBitrate = static_cast<int>(data);
				if (this->callbackMediaInfoUpdate)
				{
					callbackMediaInfoUpdate();
				}
			}
		} else if (strcmp(prop->name, "duration") == 0) {
			double data = 0;
			if (mpv_get_property(mpv, prop->name, MPV_FORMAT_DOUBLE, &data) == 0) {
				if (data > 0.0001) {
					fileLength = data;
					fileLengthValid = true;
					if (this->callbackMediaInfoUpdate)
					{
						callbackMediaInfoUpdate();
					}
				} else {
					fileLengthValid = false;
				}
			}
		} else if (strcmp(prop->name, "time-pos") == 0) {
			double data = 0;
			if (mpv_get_property(mpv, prop->name, MPV_FORMAT_DOUBLE, &data) == 0) {
				filePosition = data;
				filePositionValid = true;
			}
		} else if (strcmp(prop->name, "volume") == 0) {
			double data = 0;
			if (mpv_get_property(mpv, prop->name, MPV_FORMAT_DOUBLE, &data) == 0) {
				LOG("volume = %d", static_cast<int>(data));
			}
		} else if (strcmp(prop->name, "pause") == 0) {
			bool state = (bool)*(unsigned*)prop->data;
			LOG("pause state changed to %s", state?"true":"false");
		}
        break;
	}
	case MPV_EVENT_START_FILE:
		break;
	case MPV_EVENT_FILE_LOADED:
		fileStarted = true;
		if (absoluteSeekRequested > 0) {
			seekAbsolute(absoluteSeekRequested);
		}
		break;
	case MPV_EVENT_END_FILE:
		fileStarted = false;
		if (callbackStopPlaying && useStopCallback)
			callbackStopPlaying();	
		break;
    case MPV_EVENT_SHUTDOWN:
        mpvDestroy();
        break;
    default:
        break;
	}
}

int MPlayer::mpvCreate(void)
{
	mpvDestroy();
	
	mpv = mpv_create();
	if (!mpv) {
		LOG("failed to create mpv instance");
		return -1;
	}

	mpv_request_log_messages(mpv, "info");	

	int64_t wid = reinterpret_cast<int64_t>(parent);
	if (mpv_set_property(mpv, "wid", MPV_FORMAT_INT64, &wid) < 0) {
		LOG("failed to set mpv wid");
		mpvDestroy();
		return -2;
	}

	if (mpv_initialize(mpv) < 0) {
		LOG("failed to initialize mpv");
		mpvDestroy();
		return -3;
	}

	{
		int64_t val = 1;
		if (mpv_set_property(mpv, "osd-level", MPV_FORMAT_INT64, &val) < 0) {
			LOG("failed to set mpv osd level");
			return -2;
		}
	}

	{
		int val = 1;
		if (mpv_set_property(mpv, "osd-bar", MPV_FORMAT_FLAG, &val) < 0) {
			LOG("failed to set mpv osd-bar");
		} else {
        	LOG("mpv osd-bar enabled");
		}
	}
#if 0
	{
        // subtitle track id
		int val = 1;
		if (mpv_set_property(mpv, "sid", MPV_FORMAT_INT64, &val) < 0) {
			LOG("failed to set mpv sid");
		} else {
			LOG("mpv sid set");
		}
	}
#endif

	mpv_observe_property(mpv, 0, "media-title", MPV_FORMAT_NONE);
	mpv_observe_property(mpv, 0, "video-bitrate", MPV_FORMAT_DOUBLE);
	mpv_observe_property(mpv, 0, "audio-bitrate", MPV_FORMAT_DOUBLE);
	mpv_observe_property(mpv, 0, "duration", MPV_FORMAT_DOUBLE);
	mpv_observe_property(mpv, 0, "time-pos", MPV_FORMAT_DOUBLE);
	//mpv_observe_property(mpv, 0, "volume", MPV_FORMAT_DOUBLE);
    mpv_observe_property(mpv, 0, "pause", MPV_FORMAT_FLAG);

    applyConfiguration();

	return 0;
}

void MPlayer::mpvDestroy(void)
{
	if (mpv) {
		mpv_terminate_destroy(mpv);
		mpv = NULL;
	}
}

void MPlayer::applyConfiguration(void)
{
	if (mpv == NULL)
		return;

	{
		int64_t val = cfg.softVolMax;
		if (mpv_set_property(mpv, "volume-max", MPV_FORMAT_INT64, &val) < 0) {
			LOG("failed to set mpv volume-max");
		} else {
			LOG("mpv volume-max set to %d", cfg.softVolMax);
		}
	}

	changeVolumeAbs(cfg.softVolLevel);
}

AnsiString MPlayer::getApiVersion(void)
{
	unsigned long v = mpv_client_api_version();
	unsigned int major = v >> 16;
	unsigned int minor = v & 0xFFFF;
    AnsiString str;
	str.sprintf("%u.%u", major, minor);
	return str;
}

