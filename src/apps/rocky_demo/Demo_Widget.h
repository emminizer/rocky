/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include "helpers.h"
using namespace ROCKY_NAMESPACE;

auto Demo_Widget = [](Application& app)
{
    static entt::entity entity1 = entt::null;
    static entt::entity entity2 = entt::null;
    static Status status;

    if (entity1 == entt::null)
    {
        auto [lock, registry] = app.registry.write();

        // Create a host entity
        entity1 = registry.create();

        // Simple widget with just a text label:
        {
            auto& widget = registry.emplace<Widget>(entity1);
            widget.text = "I'm basic.";

            // Attach a transform to place and move the label:
            auto& transform = registry.emplace<Transform>(entity1);
            transform.setPosition(GeoPoint(SRS::WGS84, 25.0, 25.0, 10.0));
        }

        // Complex imgui rendering widget
        {
            entity2 = registry.create();

            auto& widget = registry.emplace<Widget>(entity2);
            widget.render = [&](WidgetInstance& i)
            {
                static float some_float = 0;
                static int some_int = 0;
                static float some_color[3] = { 255, 0, 0 };

                int flags = i.defaultWindowFlags;
                flags &= ~ImGuiWindowFlags_NoInputs;
                flags &= ~ImGuiWindowFlags_NoBringToFrontOnFocus;

                ImVec2 pos{ i.position.x, i.position.y - i.size.y };
                ImGui::SetNextWindowPos(pos);

                ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 7.0f);

                if (ImGui::Begin(i.uid.c_str(), nullptr, flags))
                {
                    ImGui::Text("I'm not so basic.");
                    ImGui::SliderFloat("Slider", &some_float, 0.0f, 1.0f);
                    ImGui::Separator();
                    if (ImGuiLTable::Begin("table")) {
                        ImGuiLTable::Text("Name", "Value");
                        ImGuiLTable::Text("Property", "Something");
                        ImGuiLTable::SliderInt("Control", &some_int, 100, 50);
                        ImGuiLTable::End();
                    }
                    i.size = ImGui::GetWindowSize();
                    ImGui::End();
                }
                ImGui::PopStyleVar();
            };

            // Attach a transform to place and move the label:
            auto& transform = registry.emplace<Transform>(entity2);
            transform.setPosition(GeoPoint(SRS::WGS84, -25.0, 25.0, 50000.0));

            // Drop line from the widget to the ground, for fun.
            auto& dropline = registry.emplace<Line>(entity2);
            dropline.points = { { 0,0,0 }, { 0, 0, -50000 } };
            dropline.style.color = vsg::vec4{ 0, 1, 0, 1 };
            dropline.style.width = 2;
        }
    }

    if (ImGuiLTable::Begin("widget_demo"))
    {
        auto [lock, registry] = app.registry.read();

        bool visible = ecs::visible(registry, entity1);
        if (ImGuiLTable::Checkbox("Show", &visible))
        {
            ecs::setVisible(registry, entity1, visible);
            ecs::setVisible(registry, entity2, visible);
        }

        auto& widget = registry.get<Widget>(entity1);

        if (widget.text.length() <= 255)
        {
            char buf[256];
            std::copy(widget.text.begin(), widget.text.end(), buf);
            buf[widget.text.length()] = '\0';
            if (ImGuiLTable::InputText("Text", &buf[0], 255))
            {
                widget.text = std::string(buf);
            }
        }

        auto& transform = registry.get<Transform>(entity1);

        if (ImGuiLTable::SliderDouble("Latitude", &transform.position.y, -85.0, 85.0, "%.1lf"))
            transform.dirty();

        if (ImGuiLTable::SliderDouble("Longitude", &transform.position.x, -180.0, 180.0, "%.1lf"))
            transform.dirty();

        if (ImGuiLTable::SliderDouble("Altitude", &transform.position.z, 0.0, 2500000.0, "%.1lf"))
            transform.dirty();

        ImGuiLTable::End();
    }
};
