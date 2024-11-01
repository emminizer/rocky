/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/vsg/Common.h>
#include <rocky/vsg/engine/ViewLocal.h>
#include <rocky/GeoPoint.h>
#include <rocky/Horizon.h>
#include <vsg/nodes/CullGroup.h>
#include <vsg/nodes/Transform.h>

namespace ROCKY_NAMESPACE
{
    template<class T>
    struct PositionedObjectAdapter : public PositionedObject
    {
        vsg::ref_ptr<T> object;
        virtual const GeoPoint& objectPosition() const {
            return object->position;
        }
        static std::shared_ptr<PositionedObjectAdapter<T>> create(vsg::ref_ptr<T> object_) {
            auto r = std::make_shared< PositionedObjectAdapter<T>>();
            r->object = object_;
            return r;
        }
    };

    /**
     * Transform node that accepts geospatial coordinates and creates
     * a local ENU (X=east, Y=north, Z=up) coordinate frame for its children
     * that is tangent to the earth at the transform's geo position.
     */
    class ROCKY_EXPORT GeoTransform :
        public vsg::Inherit<vsg::Group, GeoTransform>,
        PositionedObject
    {
    public:
        GeoPoint position;

        //! Sphere for horizon culling
        vsg::dsphere bound = { };

        //! whether horizon culling is active
        bool horizonCulling = true;

        //! whether frustum culling is active
        bool frustumCulling = true;

        //! Whether the transformation should establish a local tangent plane (ENU)
        //! at the position. Disabling this can increase performance for objects
        //! (like billboards) that don't need tangent plane.
        bool localTangentPlane = true;

    public:
        //! Construct an invalid geotransform
        GeoTransform();

        //! Call this is you change position directly.
        void dirty();

        //! Same as changing position and calling dirty().
        void setPosition(const GeoPoint& p);

    public: // PositionedObject interface

        const GeoPoint& objectPosition() const override {
            return position;
        }

    public:

        //! Disables the copy constructor.
        GeoTransform(const GeoTransform& rhs) = delete;

        void traverse(vsg::RecordTraversal&) const override;

        bool push(vsg::RecordTraversal&, const vsg::dmat4& m) const;

        void pop(vsg::RecordTraversal&) const;

    public:
        struct ViewLocalData
        {
            bool dirty = true;
            vsg::dmat4 matrix;
            vsg::dmat4 local_matrix;
            vsg::dmat4 mvp;
            double aspect_ratio;
            SRS world_srs;
            const Ellipsoid* world_ellipsoid = nullptr;
            SRSOperation pos_to_world;
            std::shared_ptr<Horizon> horizon;
        };
        util::ViewLocal<ViewLocalData> viewLocal;

    };
} // namespace
