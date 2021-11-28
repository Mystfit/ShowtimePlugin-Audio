#include "AudioVSTHost.h"

#include <string>
#include <showtime/ZstLogging.h>

#include <boost/thread.hpp>
#include <boost/range/join.hpp>
#include <public.sdk/source/vst/utility/stringconvert.h>
#include <public.sdk/source/vst/hosting/hostclasses.h>
#include <public.sdk/source/vst/hosting/eventlist.h>
#include <pluginterfaces/vst/ivstaudioprocessor.h>

#include "WindowController.h"
#include "VSTPlugProvider.h"

using namespace showtime;
using namespace Steinberg;
using namespace Steinberg::Vst;
using namespace Steinberg::Vst::EditorHost;


AudioVSTHost::AudioVSTHost(const char* name, const char* vst_path, Vst::HostApplication* plugin_context) :
	AudioComponentBase(0, 0, AUDIOVSTHOST_COMPONENT_TYPE, name),
	m_module(nullptr),
	m_plugProvider(nullptr),
	m_processContext(std::make_shared<ProcessContext>()),
	m_elapsed_samples(0),
	m_midi_in(std::make_unique<ZstInputPlug>("IN_midi", ZstValueType::ByteList, 1, false))
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

void AudioVSTHost::on_registered() {
	AudioComponentBase::on_registered();

	// TODO: Only add the midi input plug if the VST supports input event buses
	//add_child(m_midi_in.get());
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

			// Query VST requirements
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

			// Query audio buses
			Log::entity(Log::Level::debug, "VST contains {} input audio and {} output audio buses", m_vstPlug->getBusCount(Vst::MediaTypes::kAudio, Vst::BusDirections::kInput), m_vstPlug->getBusCount(Vst::MediaTypes::kAudio, Vst::BusDirections::kOutput));
			BusInfo out_info;
			for (size_t bus_idx = 0; bus_idx < m_vstPlug->getBusCount(Vst::MediaTypes::kAudio, Vst::BusDirections::kInput); ++bus_idx) {
				BusInfo in_info;
				m_vstPlug->getBusInfo(kAudio, kInput, bus_idx, in_info);
				m_vstPlug->activateBus(kAudio, kInput, bus_idx, true);
			}

			for (size_t bus_idx = 0; bus_idx < m_vstPlug->getBusCount(Vst::MediaTypes::kAudio, Vst::BusDirections::kOutput); ++bus_idx) {
				BusInfo out_info;
				m_vstPlug->getBusInfo(kAudio, kOutput, bus_idx, out_info);
				m_vstPlug->activateBus(kAudio, kOutput, bus_idx, true);
			}

			// Query event busses
			for (size_t bus_idx = 0; bus_idx < m_vstPlug->getBusCount(Vst::MediaTypes::kEvent, Vst::BusDirections::kInput); ++bus_idx) {
				BusInfo in_event_info;
				m_vstPlug->getBusInfo(kEvent, kInput, bus_idx, in_event_info);
				m_vstPlug->activateBus(kEvent, kInput, bus_idx, true);
			}

			// Setup speakers
			SpeakerArrangement input_arr;
			SpeakerArrangement output_arr;
			m_audioEffect->getBusArrangement(kInput, 0, input_arr);
			m_audioEffect->getBusArrangement(kOutput, 0, output_arr);
			tresult res = m_audioEffect->setBusArrangements(&input_arr, 1, &output_arr, 1);
			if (!res)
				 Log::entity(Log::Level::debug, "Failed to set bus properties");

			// Prepare VST for audio processing
			prepareProcessing();
			if (m_vstPlug->setActive(true) != kResultTrue)
				Log::entity(Log::Level::error, "Couldn't activate VST component");

			// Setup plugs for VST channels
			init_plugs(m_vstPlug->getBusCount(Vst::MediaTypes::kAudio, Vst::BusDirections::kInput), m_vstPlug->getBusCount(Vst::MediaTypes::kAudio, Vst::BusDirections::kOutput));
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
	if (m_audioEffect){

		// Read floats from plug into VST buffer
		if (m_processData.inputs) {
			for (size_t channel_idx = 0; channel_idx < num_input_channels(); ++channel_idx) {
				std::copy(
					incoming_audio(channel_idx)->raw_value()->float_buffer(),
					incoming_audio(channel_idx)->raw_value()->float_buffer() + incoming_audio(channel_idx)->size(),
					m_processData.inputs->channelBuffers32[channel_idx]
				);
			}
		}

		// Set process context info
		int samplesize = floor(double((num_input_channels()) ? incoming_audio(0)->size() : 0) * 0.5);
		m_elapsed_samples += samplesize;//floor(incoming_audio()->size() * 0.5);
		
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

		// Add incoming notes
		NoteOnEvent noteOn;
		noteOn.channel = 0;
		noteOn.pitch = 64;
		
		Event event;
		event.busIndex = 0;
		event.type = Event::EventTypes::kNoteOnEvent;
		event.noteOn = noteOn;

		EventList midi_events;
		midi_events.addEvent(event);
		m_processData.inputEvents = &midi_events;

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
			for (size_t channel_idx = 0; channel_idx < num_output_channels(); ++channel_idx) {
				outgoing_audio(channel_idx)->raw_value()->assign(m_processData.outputs->channelBuffers32[channel_idx], m_processData.numSamples);
				outgoing_audio(channel_idx)->fire();
			}
		}
		else {
			Log::entity(Log::Level::error, "Can't publish output VST samples. Output buffer is null");
		}
	}

	ZstComponent::compute(plug);
}
