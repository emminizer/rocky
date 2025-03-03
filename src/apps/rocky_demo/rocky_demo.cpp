/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */

/**
* THE DEMO APPLICATION is an ImGui-based app that shows off all the features
* of the Rocky Application API. We intend each "Demo_*" include file to be
* both a unit test for that feature, and a reference or writing your own code.
*/

#include <rocky/vsg/Application.h>
#include <rocky/Version.h>

#ifdef ROCKY_HAS_TMS
#include <rocky/TMSImageLayer.h>
#include <rocky/TMSElevationLayer.h>
#endif

#include "vsgImGui/RenderImGui.h"
#include "vsgImGui/SendEventsToImGui.h"

#include <imgui_impl_vulkan.h>
#include <imgui_internal.h>

ROCKY_ABOUT(imgui, IMGUI_VERSION)

using namespace ROCKY_NAMESPACE;

#include "Demo_Map.h"
#include "Demo_Line.h"
#include "Demo_Mesh.h"
#include "Demo_Icon.h"
#include "Demo_Model.h"
#include "Demo_Label.h"
#include "Demo_Widget.h"
#include "Demo_LineFeatures.h"
#include "Demo_PolygonFeatures.h"
#include "Demo_LabelFeatures.h"
#include "Demo_MapManipulator.h"
#include "Demo_Serialization.h"
#include "Demo_Tethering.h"
#include "Demo_Environment.h"
#include "Demo_Views.h"
#include "Demo_RTT.h"
#include "Demo_Stats.h"
#include "Demo_Geocoder.h"
#include "Demo_Rendering.h"
#include "Demo_Simulation.h"
#include "Demo_TrackHistory.h"
#include "Demo_Decluttering.h"

template<class T>
int layerError(T layer)
{
    rocky::Log()->warn("Problem with layer \"" + layer->name() + "\" : " + layer->status().message);
    return -1;
}

auto Demo_About = [](Application& app)
{
    for (auto& about : rocky::ContextImpl::about())
    {
        ImGui::Text(about.c_str());
    }
};

struct Demo
{
    std::string name;
    std::function<void(Application&)> function;
    std::vector<Demo> children;
};

std::vector<Demo> demos =
{
    Demo{ "Map", Demo_Map },
    Demo{ "Basic Components", {},
    {
        Demo{ "Label", Demo_Label },
        Demo{ "Line - absolute", Demo_Line_Absolute },
        Demo{ "Line - relative", Demo_Line_Relative },
        Demo{ "Mesh - absolute", Demo_Mesh_Absolute },
        Demo{ "Mesh - relative", Demo_Mesh_Relative },
        Demo{ "Icon", Demo_Icon },
        Demo{ "Model", Demo_Model },
        Demo{ "Widget", Demo_Widget }
    } },
    Demo{ "GIS Data", {},
    {
        Demo{ "Polygon features", Demo_PolygonFeatures },
        Demo{ "Line features", Demo_LineFeatures },
        Demo{ "Labels from features", Demo_LabelFeatures }
    } },
    Demo{ "Simulation", {},
    {
        Demo{ "Simulated platforms", Demo_Simulation },
        Demo{ "Track histories", Demo_TrackHistory }
    } },
    Demo{ "Decluttering", Demo_Decluttering },
    Demo{ "Geocoding", Demo_Geocoder },
    Demo{ "RTT", Demo_RTT },
    Demo{ "Camera", {},
    {
        Demo{ "Viewpoints", Demo_Viewpoints },
        Demo{ "Tethering", Demo_Tethering }
    } },
    Demo{ "Rendering", Demo_Rendering },
    Demo{ "Views", Demo_Views },
    Demo{ "Environment", Demo_Environment },
    Demo{ "Serialization", Demo_Serialization },
    Demo{ "Stats", Demo_Stats },
    Demo{ "About", Demo_About }
};

struct MainGUI : public vsg::Inherit<vsg::Command, MainGUI>
{
    Application& app;
    MainGUI(Application& app_) : app(app_) { }

    void record(vsg::CommandBuffer& cb) const override
    {
        render();
    }

    void render() const
    {
        ImGui::Begin("Welcome to Rocky");
        {
            for (auto& demo : demos)
            {
                render(demo);
            }
        }
        ImGui::End();
    }

    void render(const Demo& demo) const
    {
        if (ImGui::CollapsingHeader(demo.name.c_str()))
        {
            if (demo.function)
                demo.function(app);
    
            if (!demo.children.empty())
            {
                ImGui::Indent();
                for (auto& child : demo.children)
                    render(child);
                ImGui::Unindent();
            }
        }
    }
};

//! Wrapper to render via callbacks.
struct GuiCallbackRunner : public vsg::Inherit<vsg::Node, GuiCallbackRunner>
{
    VSGContext context;

    GuiCallbackRunner(VSGContext context_in) : context(context_in) { }

    void traverse(vsg::RecordTraversal& record) const override
    {
        for (auto& callback : context->guiCallbacks)
        {
            callback(record.getState()->_commandBuffer->viewID, ImGui::GetCurrentContext());
        }
    }
};

//! wrapper for vsgImGui::SendEventsToImGui that restricts ImGui events to a single window.
class SendEventsToImGuiWrapper : public vsg::Inherit<vsgImGui::SendEventsToImGui, SendEventsToImGuiWrapper>
{
public:
    SendEventsToImGuiWrapper(vsg::ref_ptr<vsg::Window> window, rocky::VSGContext& cx) :
        _window(window), _context(cx) { }

    template<typename E>
    void propagate(E& e, bool forceRefresh = false)
    {
        if (e.window.ref_ptr() == _window)
        {
            vsgImGui::SendEventsToImGui::apply(e);
            if (e.handled || forceRefresh)
            {
                _context->requestFrame();
            }
        }
    }

    void apply(vsg::ButtonPressEvent& e) override { propagate(e); }
    void apply(vsg::ButtonReleaseEvent& e) override { propagate(e); }
    void apply(vsg::ScrollWheelEvent& e) override { propagate(e); }
    void apply(vsg::KeyPressEvent& e) override { propagate(e); }
    void apply(vsg::KeyReleaseEvent& e) override { propagate(e); }
    void apply(vsg::MoveEvent& e) override { propagate(e); }
    void apply(vsg::ConfigureWindowEvent& e) override { propagate(e, true); }

private:
    vsg::ref_ptr<vsg::Window> _window;
    VSGContext _context;
};


int main(int argc, char** argv)
{
    // instantiate the application engine.
    rocky::Application app(argc, argv);

    // Exit if the user tries to load a file and failed:
    if (app.commandLineStatus.failed())
    {
        Log()->error(app.commandLineStatus.toString());
        exit(-1);
    }

    // Add some default layers if the user didn't load a file:
    auto& layers = app.mapNode->map->layers();
    if (layers.empty())
    {
#ifdef ROCKY_HAS_TMS
        auto imagery = rocky::TMSImageLayer::create();
        imagery->uri = "https://readymap.org/readymap/tiles/1.0.0/7/";
        layers.add(imagery);

        auto elevation = rocky::TMSElevationLayer::create();
        elevation->uri = "https://readymap.org/readymap/tiles/1.0.0/116/";
        layers.add(elevation);
#endif
    }

    // Create the main window and the main GUI:
    auto window = app.displayManager->addWindow(vsg::WindowTraits::create(1920, 1080, "Main Window"));
    auto imgui = vsgImGui::RenderImGui::create(window);

    // Hook in any embedded GUI renderers:
    imgui->addChild(GuiCallbackRunner::create(app.context));

    // Hook in the main demo gui:
    auto maingui = MainGUI::create(app);
    imgui->addChild(maingui);

    // ImGui likes to live under the main rendergraph, but outside the main view.
    // https://github.com/vsg-dev/vsgExamples/blob/master/examples/ui/vsgimgui_example/vsgimgui_example.cpp#L276
    auto main_view = app.displayManager->windowsAndViews[window].front();
    app.displayManager->getRenderGraph(main_view)->addChild(imgui);

    // Make sure ImGui is the first event handler:
    auto& handlers = app.viewer->getEventHandlers();
    handlers.insert(handlers.begin(), SendEventsToImGuiWrapper::create(window, app.context));

    // In render-on-demand mode, this callback will cause ImGui to handle events
    // TODO: what about the GuiCallbackRunner callbacks...?
    app.noRenderFunction = [&]()
        {
            vsgImGui::RenderImGui::frame([&]()
                {
                    for(auto& render : app.context->guiCallbacks)
                        for(auto& viewID : app.context->activeViewIDs)
                            render(viewID, ImGui::GetCurrentContext());

                    maingui->render();
                });
        };

    // run until the user quits.
    return app.run();
}
