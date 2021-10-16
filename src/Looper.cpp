#include <showtime/ShowtimeClient.h>
#include <showtime/ShowtimeServer.h>
#include <showtime/ZstLogging.h>
#include <showtime/ZstFilesystemUtils.h>
#include <signal.h>

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
	SignalHandlerPointer previousHandler;
	previousHandler = signal(SIGSEGV, &s_signal_handler);
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
		m_server.init("LooperServer");
		auto vst_search_path = fs::path("C:\\Program Files\\Common Files\\VST3");
		m_client.set_plugin_data_path(vst_search_path.string().c_str());
		m_client.init("Looper", true);
		m_client.auto_join_by_name("LooperServer");

		ZstURI audio_factory_path = m_client.get_root()->URI() + ZstURI("audio_ports");
		auto audio_factory = dynamic_cast<ZstEntityFactory*>(m_client.find_entity(audio_factory_path));
		if (audio_factory) {
			ZstURIBundle bundle;
			audio_factory->get_creatables(&bundle);
			for (auto creatable : bundle) {
				m_client.create_entity(creatable, (std::string(creatable.last().path()) + "_looper").c_str());
			}
		}

		ZstURI vst_factory_path = m_client.get_root()->URI() + ZstURI("vsts");
		auto vst_factory = dynamic_cast<ZstEntityFactory*>(m_client.find_entity(vst_factory_path));
		if (vst_factory) {
			ZstURIBundle bundle;
			vst_factory->get_creatables(&bundle);
			for (auto creatable : bundle) {
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
};

int main(int argc, char** argv){
	s_catch_signals();

	Looper looper;

	Log::app(Log::Level::notification, "Listening for audio...");
	while (!s_interrupted) {
		looper.get_client().poll_once();
	}

	Log::app(Log::Level::notification, "Quitting");
	looper.get_client().destroy();
	looper.get_server().destroy();
	return 0;
}

//------------------------------------------------------------------------
int APIENTRY wWinMain (_In_ HINSTANCE instance, _In_opt_ HINSTANCE /*prevInstance*/,
                       _In_ LPWSTR lpCmdLine, _In_ int /*nCmdShow*/)
{
#if !SMTG_OS_WINDOWS_ARM
	HRESULT hr = CoInitialize (nullptr);
	if (FAILED (hr))
		return FALSE;
#endif

	//Steinberg::Vst::EditorHost::Platform::instance ().run (lpCmdLine, instance);
	s_catch_signals();

	//TODO: This blocks and needs to happen in its own thread!

	boost::thread gui_thread = boost::thread([]() {
		MSG msg;
		while (GetMessage(&msg, nullptr, 0, 0))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	});

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

