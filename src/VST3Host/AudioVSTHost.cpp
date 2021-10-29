#include "AudioVSTHost.h"

#include <string>
#include <showtime/ZstLogging.h>

#include <boost/thread.hpp>
#include <boost/range/join.hpp>
#include <public.sdk/source/vst/utility/stringconvert.h>
#include <public.sdk/source/vst/hosting/hostclasses.h>
#include <pluginterfaces/vst/ivstaudioprocessor.h>

#include "WindowController.h"
#include "VSTPlugProvider.h"

using namespace showtime;
using namespace Steinberg;
using namespace Steinberg::Vst;
using namespace Steinberg::Vst::EditorHost;


AudioVSTHost::AudioVSTHost(const char* name, const char* vst_path, Vst::HostApplication* plugin_context) :
	AudioComponentBase(AUDIOVSTHOST_COMPONENT_TYPE, name),
	m_module(nullptr),
	m_plugProvider(nullptr),
	m_processContext(std::make_shared<ProcessContext>()),
	m_elapsed_samples(0)
{
	m_processSetup.processMode = kRealtime;
	m_processSetup.symbolicSampleSize = kSample32;
	m_processSetup.maxSamplesPerBlock = 2048;
	m_processSetup.sampleRate = 44100;

	m_processData.numSamples = 512;
	m_processData.symbolicSampleSize = kSample32;
	m_processData.processContext = m_processContext.get();

	load_VST(vst_path, plugin_context);
}

void AudioVSTHost::load_VST(const std::string& path, Vst::HostApplication* plugin_context) {

	// Load the VST module
	std::string error;
	m_module = VST3::Hosting::Module::create(path, error);
	if (!m_module) {
		Log::entity(Log::Level::error, "Could not create Module for file: {}, {}", path.c_str(), error.c_str());
		return;
	}

	// Get the plugin factory from the VST for constructing an instance of the plugin
	VST3::Hosting::PluginFactory factory = m_module->getFactory();
	for (auto& classInfo : factory.classInfos()) {
		Log::entity(Log::Level::debug, "Found VST Class: {}, Category: {}, Version: {}", classInfo.name().c_str(), classInfo.category().c_str(), classInfo.version().c_str());

		if (classInfo.category() == kVstAudioEffectClass)
		{
			// Get the plugin provider (hosts VST component and VST controller)
			m_plugProvider = owned(new VSTPlugProvider(factory, classInfo, plugin_context));
			if (!m_plugProvider)
			{
				Log::entity(Log::Level::error, "No plugin provider found");
				return;
			}
			Log::entity(Log::Level::notification, "Loaded VST {}", classInfo.name().c_str());

			// Get the audio component for processing audio data
			m_vstPlug = m_plugProvider->getComponent();

			m_audioEffect = FUnknownPtr<Vst::IAudioProcessor>(m_vstPlug);
			//m_vstPlug->queryInterface(IAudioProcessor::iid, (void**)&m_audioEffect);
			if (!m_audioEffect) {
				Log::app(Log::Level::error, "Could not get audio processor from VST");
				return;
			}

			FUnknownPtr<Vst::IProcessContextRequirements> contextRequirements(m_audioEffect);
			if (contextRequirements) {
				auto flags = contextRequirements->getProcessContextRequirements();

#define PRINT_FLAG(x) if (flags & Vst::IProcessContextRequirements::Flags::x) Log::entity(Log::Level::debug, #x);
				PRINT_FLAG(kNeedSystemTime)
				PRINT_FLAG(kNeedContinousTimeSamples)
				PRINT_FLAG(kNeedProjectTimeMusic)
				PRINT_FLAG(kNeedBarPositionMusic)
				PRINT_FLAG(kNeedCycleMusic)
				PRINT_FLAG(kNeedSamplesToNextClock)
				PRINT_FLAG(kNeedTempo)
				PRINT_FLAG(kNeedTimeSignature)
				PRINT_FLAG(kNeedChord)
				PRINT_FLAG(kNeedFrameRate)
				PRINT_FLAG(kNeedTransportState)
#undef PRINT_FLAG
			}


			// Get the edit controller for GUI and parameter control
			auto editController = m_plugProvider->getController();
			editController->release();
			if (editController) {
				createViewAndShow(editController);
			}

			// Query buses
			Log::entity(Log::Level::debug, "VST contains {} input and {} output buses", m_vstPlug->getBusCount(Vst::MediaTypes::kAudio, Vst::BusDirections::kInput), m_vstPlug->getBusCount(Vst::MediaTypes::kAudio, Vst::BusDirections::kOutput));
			BusInfo in_info;
			BusInfo out_info;
			m_vstPlug->getBusInfo(kAudio, kInput, 0, in_info);
			m_vstPlug->getBusInfo(kAudio, kOutput, 0, out_info);
			m_vstPlug->activateBus(kAudio, kInput, 0, true);
			m_vstPlug->activateBus(kAudio, kOutput, 0, true);
			
			SpeakerArrangement input_arr;
			SpeakerArrangement output_arr;
			m_audioEffect->getBusArrangement(kInput, 0, input_arr);
			m_audioEffect->getBusArrangement(kOutput, 0, output_arr);
			
			 tresult res = m_audioEffect->setBusArrangements(&input_arr, 1, &output_arr, 1);
			 if (!res)
				 Log::entity(Log::Level::debug, "Failed to set bus properties");

			prepareProcessing();
			if (m_vstPlug->setActive(true) != kResultTrue)
				Log::entity(Log::Level::error, "Couldn't activate VST component");
		}
	}
}

bool AudioVSTHost::prepareProcessing()
{
	if (!m_vstPlug || !m_audioEffect)
		return false;

	tresult setupResult = m_audioEffect->setupProcessing(m_processSetup);
	if (setupResult == kResultOk)
	{
		m_processData.prepare(*m_vstPlug, m_processData.numSamples, m_processSetup.symbolicSampleSize);
	}
	return false;
}


void AudioVSTHost::createViewAndShow(Vst::IEditController* controller)
{
	auto view = owned(controller->createView(Vst::ViewType::kEditor));
	if (!view)
	{
		Log::app(Log::Level::error, "EditController does not provide its own editor");
		return;
	}

	ViewRect plugViewSize{};
	auto result = view->getSize(&plugViewSize);
	if (result != kResultTrue)
	{
		Log::app(Log::Level::error, "Could not get editor window size");
		return;
	}

	auto viewRect =  Vst::EditorHost::ViewRectToRect(plugViewSize);
	m_windowController = std::make_shared<WindowController>(view);
	m_window = Window::make("Editor", viewRect.size, view->canResize() == kResultTrue, m_windowController, nullptr);
	
	if (!m_window){
		Log::app(Log::Level::error, "Could not create window");
		return;
	}
	m_window->show();
}

void AudioVSTHost::compute(showtime::ZstInputPlug* plug)
{
	bool processed_VST = false;
	if (plug == incoming_audio()) {
		if (!m_audioEffect)
			return;

		// Read floats from plug into VST buffer

		if (m_processData.inputs) {
			for (size_t channel = 0; channel < 2; ++channel) {
				size_t channel_start_offset = floor(plug->size() * 0.5) * channel;
				size_t channel_sample = channel_start_offset + floor(plug->size() * 0.5);
				std::copy(plug->raw_value()->float_buffer() + channel_start_offset, plug->raw_value()->float_buffer() + channel_sample, m_processData.inputs->channelBuffers32[channel]);
			}
		}

		// Set process context info
		int samplesize = floor(double(plug->size()) * 0.5);
		m_elapsed_samples += samplesize;//floor(plug->size() * 0.5);
		
		m_processContext->state = ProcessContext::kPlaying;// | ProcessContext::kRecording | ProcessContext::kCycleActive;
		m_processContext->sampleRate = m_processSetup.sampleRate;
		m_processContext->projectTimeSamples = m_elapsed_samples;

		m_processContext->state |= ProcessContext::kSystemTimeValid;
		m_processContext->systemTime = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

		m_processContext->state |= ProcessContext::kContTimeValid;
		m_processContext->continousTimeSamples += m_elapsed_samples;

		m_processContext->state |= ProcessContext::kTempoValid;
		m_processContext->tempo = 120;

		m_processContext->state |= ProcessContext::kTimeSigValid;
		m_processContext->timeSigNumerator = 4;
		m_processContext->timeSigDenominator = 4;

		m_processContext->state |= ProcessContext::kProjectTimeMusicValid;
		m_processContext->projectTimeMusic = double(m_processContext->projectTimeSamples) / (60.0 / double(m_processContext->tempo)) * double(m_processContext->sampleRate);

		// Start processing VST data
		m_audioEffect->setProcessing(true);
		tresult result = m_audioEffect->process(m_processData);
		if (result != kResultOk){
			if (m_processSetup.symbolicSampleSize == kSample32)
				Log::entity(Log::Level::error, "IAudioProcessor::process (..with kSample32..) failed.");
			else
				Log::entity(Log::Level::error, "IAudioProcessor::process (..with kSample64..) failed.");
		}
		m_audioEffect->setProcessing(false);
		
		// Copy VST data into plug
		if (m_processData.outputs) {
			processed_VST = true;
			for (size_t channel = 0; channel < 2; ++channel) {
				for (size_t out_sample = 0; out_sample < m_processData.numSamples; out_sample++) {
					outgoing_audio()->append_float(m_processData.outputs->channelBuffers32[channel][out_sample]);
				}
			}
		}
		else {
			Log::entity(Log::Level::error, "Can't publish output VST samples. Output buffer is null");
		}
		
		// Only publish to the performance if we did work
		if(processed_VST)
			outgoing_audio()->fire();
	}
}
