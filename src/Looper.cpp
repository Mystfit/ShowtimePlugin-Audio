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

//class LogInputSignal : public ZstComponent {
//public:
//	LogInputSignal() {}
//	LogInputSignal(const char* name) :
//		ZstComponent("LOGSIGNAL", name),
//		input_signal(std::make_shared<ZstInputPlug>("in_signal", ZstValueType::FloatList))
//	{
//	}
//
//	void on_registered() override {
//		add_child(input_signal.get());
//	
//
//	void compute(ZstInputPlug* plug) override {
//		if (plug == input_signal.get()) {
//			std::string buffer_str;
//			//buffer_str += "<";
//			float total = 0.0f;
//			for (size_t idx = 0; idx < input_signal->size(); ++idx) {
//				total += abs(input_signal->float_at(idx));
//				//buffer_str += std::to_string(input_signal->float_at(idx));
//				//buffer_str += ",";
//			}
//			total /= input_signal->size();
//			int numblocks = total * 100;
//			for (size_t block_idx = 0; block_idx < numblocks; ++block_idx) {
//				buffer_str += "\xDB";
//			}
//			
//			//buffer_str += ">";
//			Log::app(Log::Level::debug, buffer_str.c_str());
//		}
//	}
//
//	std::shared_ptr<ZstInputPlug> input_signal;
//};

class Looper {
public:
	Looper() {		
		m_server.init("LooperServer");
		auto vst_search_path = fs::path("C:\\Program Files\\Common Files\\VST3");
		m_client.set_plugin_data_path(vst_search_path.string().c_str());
		m_client.init("Looper", true);
		m_client.auto_join_by_name("stage");

		ZstOutputPlug* recording_plug;
		ZstInputPlug* playback_plug;

		if (auto audio_factory = dynamic_cast<ZstEntityFactory*>(m_client.find_entity(m_client.get_root()->URI() + ZstURI("audio_ports")))) {
			ZstURIBundle bundle;
			audio_factory->get_creatables(&bundle);
			for (auto c : bundle) {
				Log::app(Log::Level::warn, c.path());
			}

			// Create audio devices
			ZstURI virtualaudioin("Looper/audio_ports/Microphone (Realtek(R) Audio)");  //VoiceMeeter Output (VB-Audio VoiceMeeter VAIO)");
			auto input_device = m_client.create_entity(virtualaudioin, (std::string(virtualaudioin.last().path()) + "_looper").c_str());
			recording_plug = dynamic_cast<ZstOutputPlug*>(m_client.find_entity(input_device->URI() + ZstURI("audio_from_device")));

			ZstURI virtualaudioout("Looper/audio_ports/Speakers (Realtek(R) Audio)");  //VoiceMeeter Output (VB-Audio VoiceMeeter VAIO)");
			auto output_device = m_client.create_entity(virtualaudioout, (std::string(virtualaudioout.last().path()) + "_looper").c_str());
			playback_plug = dynamic_cast<ZstInputPlug*>(m_client.find_entity(output_device->URI() + ZstURI("audio_to_device")));
			

			//// Get output plug of recording audio device
			//ZstOutputPlug* recording_plug = nullptr;
			//ZstEntityBundle output_plug_bundle;
			//input_device->get_child_entities(&output_plug_bundle, false, false, ZstEntityType::PLUG);
			//for (auto p : output_plug_bundle) {
			//	auto plug = dynamic_cast<ZstPlug*>(p);
			//	if (plug->direction() == ZstPlugDirection::OUT_JACK) {
			//		recording_plug = dynamic_cast<ZstOutputPlug*>(plug);
			//		break;
			//	}
			//}
			//
			//// Get input plug of playback audio device
			//ZstInputPlug* playback_plug = nullptr;
			//ZstEntityBundle input_plug_bundle;
			//output_device->get_child_entities(&input_plug_bundle, false, false, ZstEntityType::PLUG);
			//for (auto p : input_plug_bundle) {
			//	auto plug = dynamic_cast<ZstPlug*>(p);
			//	if (plug->direction() == ZstPlugDirection::IN_JACK) {
			//		playback_plug = dynamic_cast<ZstInputPlug*>(plug);
			//		break;
			//	}
			//}
			
			//auto cable = m_client.connect_cable(playback_plug, recording_plug);
		}

		if(auto vst_factory = dynamic_cast<ZstEntityFactory*>(m_client.find_entity(m_client.get_root()->URI() + ZstURI("vsts")))){
			ZstURIBundle bundle;
			vst_factory->get_creatables(&bundle);
			for (auto creatable : bundle) {
				Log::app(Log::Level::debug, creatable.path());
				//m_client.create_entity(creatable, (std::string(creatable.last().path()) + "_looper").c_str());
			}
		}

		ZstURI filter_path("Looper/vsts/OrilRiver"); //Looper/vsts/TAL-Filter-2 //Looper/vsts/ValhallaSupermassive //Looper/vsts/VST3 Host Checker
		auto filter_device = m_client.create_entity(filter_path, (std::string(filter_path.last().path()) + "_looper").c_str());
		ZstInputPlug* filter_in_plug = dynamic_cast<ZstInputPlug*>(m_client.find_entity(filter_device->URI() + ZstURI("audio_to_device")));
		ZstOutputPlug* filter_out_plug = dynamic_cast<ZstOutputPlug*>(m_client.find_entity(filter_device->URI() + ZstURI("audio_from_device")));

		auto filter_in_cable = m_client.connect_cable(filter_in_plug, recording_plug);
		auto filter_out_cable = m_client.connect_cable(playback_plug, filter_out_plug);
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

