#include "AudioVSTHost.h"

#include <string>
#include <showtime/ZstLogging.h>

#include <boost/thread.hpp>
#include <public.sdk/source/vst/utility/stringconvert.h>
#include <pluginterfaces/vst/ivstaudioprocessor.h>
#include <pluginterfaces/gui/iplugview.h>
#include "pluginterfaces/gui/iplugviewcontentscalesupport.h"

#include "platform/iwindow.h"
#include "platform/iplatform.h"
#ifdef WIN32
#include "platform/win32/window.h"
#endif

#include "VSTPlugProvider.h"


using namespace showtime;
using namespace Steinberg;
using namespace Steinberg::Vst::EditorHost;

namespace Steinberg {
	inline bool operator== (const ViewRect& r1, const ViewRect& r2)
	{
		return memcmp(&r1, &r2, sizeof(ViewRect)) == 0;
	}

	//------------------------------------------------------------------------
	inline bool operator!= (const ViewRect& r1, const ViewRect& r2)
	{
		return !(r1 == r2);
	}
}

class WindowController : public IWindowController, public IPlugFrame
{
public:
	WindowController(const IPtr<IPlugView>& plugView) : plugView(plugView) {

	}
	~WindowController() noexcept override {

	}

	void onShow(IWindow& w) override {
		SMTG_DBPRT1("onShow called (%p)\n", (void*)&w);

		window = &w;
		if (!plugView)
			return;

		auto platformWindow = window->getNativePlatformWindow();
		if (plugView->isPlatformTypeSupported(platformWindow.type) != kResultTrue)
		{
			Log::app(Log::Level::error, "PlugView does not support platform type:{}", platformWindow.type);
			return;
		}

		plugView->setFrame(this);

		if (plugView->attached(platformWindow.ptr, platformWindow.type) != kResultTrue)
		{
			Log::app(Log::Level::error, "Attaching PlugView failed");
			return;
		}
	}

	void onClose(Vst::EditorHost::IWindow& w) override {
		SMTG_DBPRT1("onClose called (%p)\n", (void*)&w);

		closePlugView();
	}

	void onResize(Vst::EditorHost::IWindow& w, Vst::EditorHost::Size newSize) override {
		SMTG_DBPRT1("onResize called (%p)\n", (void*)&w);

		if (plugView)
		{
			ViewRect r{};
			r.right = newSize.width;
			r.bottom = newSize.height;
			ViewRect r2{};
			if (plugView->getSize(&r2) == kResultTrue && r != r2)
				plugView->onSize(&r);
		}
	}

	Size constrainSize(Vst::EditorHost::IWindow& w, Vst::EditorHost::Size requestedSize) override {
		SMTG_DBPRT1("constrainSize called (%p)\n", (void*)&w);

		ViewRect r{};
		r.right = requestedSize.width;
		r.bottom = requestedSize.height;
		if (plugView && plugView->checkSizeConstraint(&r) != kResultTrue)
		{
			plugView->getSize(&r);
		}
		requestedSize.width = r.right - r.left;
		requestedSize.height = r.bottom - r.top;
		return requestedSize;
	}

	void onContentScaleFactorChanged(IWindow& window, float newScaleFactor) override {
		SMTG_DBPRT1("onContentScaleFactorChanged called (%p)\n", (void*)&window);

		FUnknownPtr<IPlugViewContentScaleSupport> css(plugView);
		if (css)
		{
			css->setContentScaleFactor(newScaleFactor);
		}
	}

	// IPlugFrame
	tresult PLUGIN_API resizeView(IPlugView* view, ViewRect* newSize) override {
		SMTG_DBPRT1("resizeView called (%p)\n", (void*)view);

		if (newSize == nullptr || view == nullptr || view != plugView)
			return kInvalidArgument;
		if (!window)
			return kInternalError;
		if (resizeViewRecursionGard)
			return kResultFalse;
		ViewRect r;
		if (plugView->getSize(&r) != kResultTrue)
			return kInternalError;
		if (r == *newSize)
			return kResultTrue;

		resizeViewRecursionGard = true;
		Size size{ newSize->right - newSize->left, newSize->bottom - newSize->top };
		window->resize(size);
		resizeViewRecursionGard = false;
		if (plugView->getSize(&r) != kResultTrue)
			return kInternalError;
		if (r != *newSize)
			plugView->onSize(newSize);
		return kResultTrue;
	}

	void closePlugView() {
		if (plugView)
		{
			plugView->setFrame(nullptr);
			if (plugView->removed() != kResultTrue)
			{
				Log::app(Log::Level::error, "Removing PlugView failed");
			}
			plugView = nullptr;
		}
		window = nullptr;
	}

private:
	tresult PLUGIN_API queryInterface(const TUID _iid, void** obj) override
	{
		if (FUnknownPrivate::iidEqual(_iid, IPlugFrame::iid) ||
			FUnknownPrivate::iidEqual(_iid, FUnknown::iid))
		{
			*obj = this;
			addRef();
			return kResultTrue;
		}
		if (window)
			return window->queryInterface(_iid, obj);
		return kNoInterface;
	}
	uint32 PLUGIN_API addRef() override { return 1000; }
	uint32 PLUGIN_API release() override { return 1000; }

	IPtr<IPlugView> plugView;
	Vst::EditorHost::IWindow* window{ nullptr };
	bool resizeViewRecursionGard{ false };
};


AudioVSTHost::AudioVSTHost(const char* name, const char* vst_path) : 
	ZstComponent(AUDIOVSTHOST_COMPONENT_TYPE, name),
	m_module(nullptr),
	m_plugProvider(nullptr)
{
	load_VST(vst_path);
}

void AudioVSTHost::load_VST(const std::string& path) {
	std::string error;
	m_module = VST3::Hosting::Module::create(path, error);
	if (!m_module) {
		Log::entity(Log::Level::error, "Could not create Module for file: {}, {}", path.c_str(), error.c_str());
		return;
	}

	VST3::Hosting::PluginFactory factory = m_module->getFactory();
	for (auto& classInfo : factory.classInfos()) {
		Log::entity(Log::Level::debug, "Found VST Class: {}, Category: {}, Version: {}", classInfo.name().c_str(), classInfo.category().c_str(), classInfo.version().c_str());

		if (classInfo.category() == kVstAudioEffectClass)
		{

			/*  
				TODO: This thread needs to both create the VST plugprovider, as well as pump the windows 
			    UI message queue.
			    This should be moved to our inherited PlugProvider and that can be responsible for creating
			    the GUI well as managing GUI events.
			*/
			m_events = boost::thread([this, factory, classInfo, path]()
			{
				m_plugProvider = owned(new VSTPlugProvider(factory, classInfo, false));
				if (!m_plugProvider)
				{
					Log::entity(Log::Level::error, "No plugin provider found");
					return;
				}
				Log::entity(Log::Level::notification, "Loaded VST {}", classInfo.name().c_str());
				Log::entity(Log::Level::debug, "VST contains {} input and {} output buses", m_plugProvider->getComponent()->getBusCount(Vst::MediaTypes::kAudio, Vst::BusDirections::kInput), m_plugProvider->getComponent()->getBusCount(Vst::MediaTypes::kAudio, Vst::BusDirections::kOutput));

				for (size_t idx = 0; idx < m_plugProvider->getComponent()->getBusCount(Vst::MediaTypes::kAudio, Vst::BusDirections::kInput); ++idx) {
					Vst::BusInfo info;
					m_plugProvider->getComponent()->getBusInfo(Vst::MediaTypes::kAudio, Vst::BusDirections::kInput, idx, info);
					auto result = m_plugProvider->getComponent()->activateBus(Vst::MediaTypes::kAudio, Vst::BusDirections::kInput, idx, true);
					Log::entity(Log::Level::debug, "Bus name: {}, type: {}, channels: {}, direction: {}, result: {}", VST3::StringConvert::convert(info.name), info.busType ? "aux" : "main", info.channelCount, info.direction ? "output" : "input", result);
				}

				for (size_t idx = 0; idx < m_plugProvider->getComponent()->getBusCount(Vst::MediaTypes::kAudio, Vst::BusDirections::kOutput); ++idx) {
					Vst::BusInfo info;
					m_plugProvider->getComponent()->getBusInfo(Vst::MediaTypes::kAudio, Vst::BusDirections::kOutput, idx, info);
					auto result = m_plugProvider->getComponent()->activateBus(Vst::MediaTypes::kAudio, Vst::BusDirections::kOutput, idx, true);
					Log::entity(Log::Level::debug, "Bus name: {}, type: {}, channels: {}, direction: {}, result: {}", VST3::StringConvert::convert(info.name), info.busType ? "aux" : "main", info.channelCount, info.direction ? "output" : "input", result);
				}

				auto editController = m_plugProvider->getController();
				editController->release();
				if (editController) {
					createViewAndShow(editController);
				}

				MSG msg;
				HWND native_window = nullptr;
				auto native_window_info = m_window->getNativePlatformWindow();
				if (strcmp(native_window_info.type, kPlatformTypeHWND) == 0) {
					native_window = static_cast<HWND>(m_window->getNativePlatformWindow().ptr);
				}
				UpdateWindow(native_window);

				if (native_window) {
					m_window->show();
					while (GetMessage(&msg, native_window, 0, 0))
					{
						TranslateMessage(&msg);
						DispatchMessage(&msg);
					}
				}
			});
		}
	}
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
	/*m_window = IPlatform::instance().createWindow(
		"Editor", viewRect.size, view->canResize() == kResultTrue, m_windowController);*/
	
	if (!m_window)
	{
		Log::app(Log::Level::error, "Could not create window");
		return;
	}
}


void AudioVSTHost::on_registered()
{
}