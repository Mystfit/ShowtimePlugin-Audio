#include "AudioVSTHost.h"

#include <string>
#include <showtime/ZstLogging.h>
#include <fmt/xchar.h>

#include <map>
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

tresult AudioVSTHost::beginEdit(ParamID id)
{
	try {
		auto param = getParameterInfoByID(id);
		Log::entity(Log::Level::debug, "VST EditController: {} | {} | beginEdit", this->URI().last().path(), ParameterTitleToString(param.title));
	} catch(std::out_of_range e){}
	return kNotImplemented;
}

tresult AudioVSTHost::performEdit(ParamID id, ParamValue valueNormalized)
{
	try {
		try {
			auto plug = m_param_output_plug_lookup.left.at(id);
			plug->append_float(valueNormalized);
			plug->fire();
		}
		catch (std::out_of_range e){}

		auto param = getParameterInfoByID(id);
		Log::entity(Log::Level::debug, "VST EditController: {} | {} | performEdit", this->URI().last().path(), ParameterTitleToString(param.title));
	}
	catch (std::out_of_range e) {}
	return kNotImplemented;
}

tresult AudioVSTHost::endEdit(ParamID id)
{
	try {
		auto param = getParameterInfoByID(id);
		Log::entity(Log::Level::debug, "VST EditController: {} | {} | endEdit", this->URI().last().path(), ParameterTitleToString(param.title));
	}
	catch (std::out_of_range e) {}
	return kNotImplemented;
}

tresult AudioVSTHost::restartComponent(int32 flags)
{
	Log::entity(Log::Level::debug, "VST EditController: {} | {} | restartComponent", this->URI().path());
	return kNotImplemented;
}

tresult AudioVSTHost::setDirty(TBool state)
{
	Log::entity(Log::Level::debug, "VST EditController: {} | setDirty", this->URI().path());
	return kNotImplemented;
}

tresult AudioVSTHost::requestOpenEditor(FIDString name)
{
	Log::entity(Log::Level::debug, "VST EditController: {} | requestOpenEditor", this->URI().path());
	return kNotImplemented;
}

tresult AudioVSTHost::startGroupEdit()
{
	Log::entity(Log::Level::debug, "VST EditController: {} | startGroupEdit", this->URI().path());
	return kNotImplemented;
}

tresult AudioVSTHost::finishGroupEdit()
{
	Log::entity(Log::Level::debug, "VST EditController: {} | finishGroupEdit", this->URI().path());
	return kNotImplemented;
}

void AudioVSTHost::on_registered() {
	AudioComponentBase::on_registered();

	// TODO: Only add the midi input plug if the VST supports input event buses
	//add_child(m_midi_in.get());
	for (size_t param_idx = 0; param_idx < m_editController->getParameterCount(); ++param_idx) {
		
		// Get Vst parameter info
		Vst::ParameterInfo param;
		m_editController->getParameterInfo(param_idx, param);		
		std::string param_title = ParameterTitleToString(param.title);
		bool flag_hidden = (param.flags & Vst::ParameterInfo::ParameterFlags::kIsHidden) == Vst::ParameterInfo::ParameterFlags::kIsHidden;
		bool flag_readonly = (param.flags & Vst::ParameterInfo::ParameterFlags::kIsReadOnly) == Vst::ParameterInfo::ParameterFlags::kIsReadOnly;
		bool is_cc = param_title.rfind("MIDI CC", 0) == 0;

		if (!flag_hidden && !is_cc) {
		
			// Create output plug
			auto output_plug = std::make_unique<ZstOutputPlug>(("OUT_" + param_title).c_str(), ZstValueType::FloatList);
			this->add_child(output_plug.get());
			m_param_output_plug_lookup.insert(ParamOutputPlugBimap::value_type(param.id, output_plug.get()));
			m_vst_plugs.push_back(std::move(output_plug));
			
			// Create input plug (if writable)
			if (!flag_readonly) {
				auto input_plug = std::make_unique<ZstInputPlug>(("IN_" + param_title).c_str(), ZstValueType::FloatList);
				this->add_child(input_plug.get());
				m_param_input_plug_lookup.insert(ParamInputPlugBimap::value_type(param.id, input_plug.get()));
				m_vst_plugs.push_back(std::move(input_plug));
			}
		}
	}
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
			m_editController = m_plugProvider->getController();
			//m_editController->release();
			if (m_editController) {
				createViewAndShow(m_editController);
				m_editController->setComponentHandler(this);
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

Steinberg::Vst::ParameterInfo AudioVSTHost::getParameterInfoByID(Steinberg::Vst::ParamID paramID)
{
	ParameterInfo param;
	for (size_t idx = 0; idx < m_editController->getParameterCount(); ++idx) {
		m_editController->getParameterInfo(idx, param);
		if (param.id == paramID)
			return param;
	}
	throw std::out_of_range("No parameter found");
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
	// Local dependency graph execution will compute audio
	if (plug == get_upstream_compute_plug()) {
		ProcessAudio();
	}
	else {
		try {
			auto paramID = m_param_input_plug_lookup.right.at(plug);
			m_editController->setParamNormalized(paramID, plug->float_at(0));
		} catch(std::out_of_range e){}
	}

	ZstComputeComponent::compute(plug);
}

void AudioVSTHost::ProcessAudio()
{
	if (!m_audioEffect)
		return;

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
	if (result != kResultOk) {
		if (m_processSetup.symbolicSampleSize == kSample32)
			Log::entity(Log::Level::error, "IAudioProcessor::process (..with kSample32..) failed.");
		else
			Log::entity(Log::Level::error, "IAudioProcessor::process (..with kSample64..) failed.");
	}
	m_audioEffect->setProcessing(false);

	// Copy VST data into plug
	bool processed_VST = false;
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
