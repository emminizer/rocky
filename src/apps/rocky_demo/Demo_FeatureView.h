/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include "helpers.h"

using namespace ROCKY_NAMESPACE;

auto Demo_FeatureView = [](Application& app)
    {
        static entt::entity entity = entt::null;

        if (entity == entt::null)
        {
            auto&& [_, reg] = app.registry.write();

            // Let's make a simple graticule.
            // We need to separate meridians from paralles since the interpolation is different
            // (great circle vs rhumb line).

            Feature meridians;
            meridians.geometry.type = Geometry::Type::MultiLineString;
            meridians.interpolation = GeodeticInterpolation::GreatCircle;
            meridians.srs = SRS::WGS84;
            meridians.geometry.parts.reserve(36);
            for (int i = -180; i < 180; i += 10)
            {
                meridians.geometry.parts.emplace_back().points = {
                    { (double)i, -80.0, 0.0 },
                    { (double)i,  80.0, 0.0 },
                };
            }

            Feature parallels;
            parallels.geometry.type = Geometry::Type::MultiLineString;
            parallels.interpolation = GeodeticInterpolation::RhumbLine;
            parallels.srs = SRS::WGS84;
            parallels.geometry.parts.reserve(17);
            for (int i = -80; i <= 80; i += 10)
            {
                parallels.geometry.parts.emplace_back().points = {
                    { -180.0, (double)i, 0.0 },
                    {  -90.0, (double)i, 0.0 },
                    {    0.0, (double)i, 0.0 },
                    {   90.0, (double)i, 0.0 },
                    {  180.0, (double)i, 0.0 }
                };
            }

            entity = reg.create();

            // A Geometry to hold the results. We have to use the "Segments"
            // topology since the data is not one continuous strip.
            auto& lineGeom = reg.emplace<LineGeometry>(entity);
            lineGeom.topology = LineTopology::Segments;

            // And a style:
            auto& lineStyle = reg.emplace<LineStyle>(entity);
            lineStyle.color = StockColor::Gray;
            lineStyle.depthOffset = 50000;
            lineStyle.resolution = 10000.0; // max segment length in meters for tessellation

            FeatureView::generateLine(meridians, lineStyle, GeoPoint{}, ElevationSession{}, app.mapNode->srs(), lineGeom);
            FeatureView::generateLine(parallels, lineStyle, GeoPoint{}, ElevationSession{}, app.mapNode->srs(), lineGeom);

            // The Line ties it all together:
            reg.emplace<Line>(entity, lineGeom, lineStyle);

            app.vsgcontext->requestFrame();
        }

        ImGui::TextWrapped("%s", "FeatureView is a helper utility for turning GIS feature data into geometry (lines and meshes).");
    };
