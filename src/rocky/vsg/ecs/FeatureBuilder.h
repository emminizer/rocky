/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/ElevationSampler.h>
#include <rocky/Feature.h>
#include <rocky/ecs/Line.h>
#include <rocky/ecs/Mesh.h>
#include <vector>

namespace ROCKY_NAMESPACE
{
    /**
    * FeatureBuilder is a utility that takes a collection of Feature objects
    * and populates (appends to) a geometry component.
    */
    class ROCKY_EXPORT FeatureBuilder
    {
    public:
        //! Reference point (optional) to use for geometry localization.
        //! If you set this, make sure to add a corresponding Transform component
        //! to each of the resulting entities.
        GeoPoint origin;

        //! Elevation sampler (optional) will clamp geometry to the ground.
        ElevationSession clamper;

        //! Function that returns a color for a feature (experimental - may change)
        std::function<Color(const Feature&)> colorFunction;

    public:
        //! Default constructor
        FeatureBuilder() = default;
        
        //! Append line geometry for a collection of features.
        void buildLineGeometry(const std::vector<Feature>& features, const LineStyle& style,
            const SRS& outputSRS, LineGeometry& lineGeom);

        //! Append mesh geometry for a collection of features.
        void buildMeshGeometry(const std::vector<Feature>& features, const MeshStyle& style,
            const SRS& output_srs, MeshGeometry& meshGeom);
    };
}
