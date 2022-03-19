#include <showtime/ShowtimeClient.h>
#include <showtime/ShowtimeServer.h>
#include <showtime/ZstLogging.h>
#include <showtime/ZstFilesystemUtils.h>
#include <signal.h>
#include <math.h>

#include <boost/thread.hpp>
#ifdef WIN32
#include <wtypes.h>
#endif

using namespace showtime;

int s_interrupted = 0;
typedef void (*SignalHandlerPointer)(int);

void s_signal_handler(int signal_value) {
	switch (signal_value) {
	case SIGINT:
		s_interrupted = 1;
	case SIGTERM:
		s_interrupted = 1;
	case SIGABRT:
		s_interrupted = 1;
	case SIGSEGV:
		s_interrupted = 1;
		signal(signal_value, SIG_DFL);
		raise(SIGABRT);
	default:
		break;
	}
}
 
void s_catch_signals() {
#ifdef WIN32
	//SignalHandlerPointer previousHandler;
	//previousHandler = signal(SIGSEGV, &s_signal_handler);
	signal(SIGINT, &s_signal_handler);
	signal(SIGTERM, &s_signal_handler);
	signal(SIGABRT, &s_signal_handler);
	signal(SIGSEGV, &s_signal_handler);
#else
	struct sigaction action;
	action.sa_handler = s_signal_handler;
	action.sa_flags = 0;
	sigemptyset(&action.sa_mask);
	sigaction(SIGINT, &action, NULL);
	sigaction(SIGTERM, &action, NULL);
	sigaction(SIGSEGV, &action, NULL);
#endif
}


class Looper {
public:
	Looper() {		
		//m_server.init("LooperServer");
		auto vst_search_path = fs::path("C:\\Program Files\\Common Files\\VST3");
		m_client.set_plugin_data_path(vst_search_path.string().c_str());
		m_client.init("Looper", true);
		m_client.auto_join_by_name("stage");

		ZstOutputPlug* recording_plug = nullptr;
		ZstInputPlug* playback_plug = nullptr;

		if (auto audio_factory = dynamic_cast<ZstEntityFactory*>(m_client.find_entity(m_client.get_root()->URI() + ZstURI("audio_ports")))) {
			ZstURIBundle bundle;
			audio_factory->get_creatables(&bundle);
			for (auto c : bundle) {
				Log::app(Log::Level::warn, c.path());
			}

			// Create audio devices
			ZstURI audioin("Looper/audio_ports/Microphone (Rode Podcaster)");  //VoiceMeeter Output (VB-Audio VoiceMeeter VAIO)"); //CABLE-A Output (VB-Audio Cable A) //Microphone (Realtek(R) Audio)
			auto input_device = m_client.create_entity(audioin, "Realtek Microphone");
			if(input_device)
				recording_plug = dynamic_cast<ZstOutputPlug*>(m_client.find_entity(input_device->URI() + ZstURI("audio_from_device")));

			ZstURI audioout("Looper/audio_ports/Realtek HD Audio 2nd output (Realtek(R) Audio)");  //VoiceMeeter Output (VB-Audio VoiceMeeter VAIO)"); //Speakers (Realtek(R) Audio) //CABLE-A Input (VB-Audio Cable A)
			auto output_device = m_client.create_entity(audioout, "Realtek Speaker");
			if(output_device)
				playback_plug = dynamic_cast<ZstInputPlug*>(m_client.find_entity(output_device->URI() + ZstURI("audio_to_device")));

			ZstURI virtualaudioout("Looper/audio_ports/CABLE-A Input (VB-Audio Cable A)");
			auto phys_output_device = m_client.create_entity(virtualaudioout, "CABLE-A Input");

			// Create ASIO device
			//ZstURI asio_path("Looper/audio_ports/ASIO4ALL v2");  //VoiceMeeter Output (VB-Audio VoiceMeeter VAIO)"); //CABLE-A Output (VB-Audio Cable A) //Microphone (Realtek(R) Audio)
			//auto asio_device = m_client.create_entity(asio_path, (std::string(asio_path.last().path())).c_str());
		}

		if(auto vst_factory = dynamic_cast<ZstEntityFactory*>(m_client.find_entity(m_client.get_root()->URI() + ZstURI("vsts")))){
			ZstURIBundle bundle;
			vst_factory->get_creatables(&bundle);
			for (auto creatable : bundle) {
				Log::app(Log::Level::debug, creatable.path());
				m_client.create_entity(creatable, (std::string(creatable.last().path()) + "_looper").c_str());
			}
		}
	}

	~Looper() {
		m_server.destroy();
		m_client.destroy();
	}

	ShowtimeClient& get_client() {
		return m_client;
	}


	ShowtimeServer& get_server() {
		return m_server;
	}

private:
	ShowtimeClient m_client;
	ShowtimeServer m_server;
	//std::shared_ptr<LogInputSignal> m_signal_logger;
};
//
//int main(int argc, char** argv){
//	s_catch_signals();
//
//	Looper looper;
//
//	Log::app(Log::Level::notification, "Listening for audio...");
//	while (!s_interrupted) {
//		looper.get_client().poll_once();
//	}
//
//	Log::app(Log::Level::notification, "Quitting");
//	looper.get_client().destroy();
//	looper.get_server().destroy();
//	return 0;
//}

//------------------------------------------------------------------------
int APIENTRY wWinMain (_In_ HINSTANCE instance, _In_opt_ HINSTANCE /*prevInstance*/,
                       _In_ LPWSTR lpCmdLine, _In_ int /*nCmdShow*/)
{
	s_catch_signals();

	// create the console
	if (AllocConsole()) {
		FILE* pCout;
		freopen_s(&pCout, "CONOUT$", "w", stdout);
		SetConsoleTitle("Debug Console");
		SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_RED);
	}

	// set std::cout to use my custom streambuf
	//outbuf ob;
	//std::streambuf* sb = std::cout.rdbuf(&ob);


	Looper looper;
	Log::app(Log::Level::notification, "Listening for audio...");
	while (!s_interrupted) {
		looper.get_client().poll_once();
	}

	Log::app(Log::Level::notification, "Quitting");
	looper.get_client().destroy();
	looper.get_server().destroy();

#if !SMTG_OS_WINDOWS_ARM
	CoUninitialize ();
#endif

	return 0;
}

