//---------------------------------------------------------------------------

#ifndef MpvH
#define MpvH
//---------------------------------------------------------------------------
#include <System.hpp>
#include <Graphics.hpp>

struct mpv_handle;
struct mpv_event;

namespace System
{
	class TObject;
}

namespace Extctrls
{
	class TTimer;
}

class MPlayer
{
private:
	mpv_handle *mpv;
	HANDLE parent;
	Extctrls::TTimer *timer;	
	int process_priority;

	AnsiString filename;
	bool filePositionValid;
	double filePosition;
	bool fileLengthValid;
	double fileLength;

	typedef void (__closure *CallbackStopPlaying)(void);
	typedef void (__closure *CallbackMediaInfoUpdate)(void);

	void __fastcall timerTimer(System::TObject *Sender);
	void onMpvEvent(const mpv_event &e);
	int mpvCreate(void);
	void mpvDestroy(void);
	void applyConfiguration(void);

public:
	MPlayer(void);
	~MPlayer();
	void setParent(HANDLE parent)
	{
    	this->parent = parent;
	}
	struct Cfg
	{
		int softVolLevel;
		int softVolMax;
		Cfg(void):
			softVolLevel(100),
			softVolMax(100)
		{}
	};
	int configure(const Cfg& cfg);
	const Cfg& getCfg(void)
	{
    	return cfg;
	}
	void __fastcall setCmdLine(AnsiString cmdline);
	void __fastcall lineReceived(AnsiString line);
	void __fastcall playerExited();

	int create(void);
	int play(AnsiString filename);
	AnsiString getFilename(void) const
	{
    	return filename;
	}
	int frameStep(void);
	int pause(bool state);
	int seekRelative(int seconds);
	int seekAbsolute(double pos);
	int setOsdLevel(int level);
	int changeVolume(int delta);
	int changeVolumeAbs(int val);
	int osdShowText(AnsiString text, int duration);
	/** \brief Enable/disable subtitles (e.g. from mkv)
	*/
	int setSubVisibility(bool state);
	int stop(bool useCallback = true);
	int setPropertyString(AnsiString property, AnsiString value);

	void onStopPlayingFn(void);
	CallbackStopPlaying callbackStopPlaying;
	CallbackMediaInfoUpdate callbackMediaInfoUpdate;

	struct MediaInfo
	{
		bool videoBitrateKnown;
		int videoBitrate;
		bool audioBitrateKnown;
		int audioBitrate;
		MediaInfo(void):
			videoBitrateKnown(false),
			videoBitrate(0),
			audioBitrateKnown(false),
			audioBitrate(0)
		{}
	} mediaInfo;

	double getFilePosition(void) const {
		if (filePositionValid == false)
			return -1;
		return filePosition;
	}

	double getFileLength(void) const {
		if (fileLengthValid == false)
			return -1;
		return fileLength;
	}

	static AnsiString getApiVersion(void);

private:
	Cfg cfg;
	bool fileStarted;
	bool useStopCallback;
	double absoluteSeekRequested;
};

extern class MPlayer mplayer;

#endif
