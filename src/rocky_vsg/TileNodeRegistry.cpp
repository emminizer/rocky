/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "TileNodeRegistry.h"
#include "TerrainSettings.h"
#include "TerrainContext.h"
#include "RuntimeContext.h"
#include "Utils.h"

#include <rocky/ElevationLayer.h>
#include <rocky/ImageLayer.h>
#include <rocky/Map.h>
#include <rocky/TerrainTileModelFactory.h>

#include <vsg/nodes/QuadGroup.h>
#include <vsg/ui/FrameStamp.h>

using namespace ROCKY_NAMESPACE;

#define LC "[TileNodeRegistry] "

//#define LOAD_ELEVATION_SEPARATELY

//----------------------------------------------------------------------------

TileNodeRegistry::TileNodeRegistry(TerrainTileHost* in_host) :
    _host(in_host)
{
    //nop
}

TileNodeRegistry::~TileNodeRegistry()
{
    releaseAll();
}

void
TileNodeRegistry::releaseAll()
{
    std::scoped_lock lock(_mutex);

    _tiles.clear();
    _tracker.reset();
    _loadChildren.clear();
    _loadElevation.clear();
    _mergeElevation.clear();
    _loadData.clear();
    _mergeData.clear();
    _updateData.clear();
}

void
TileNodeRegistry::ping(TerrainTileNode* tile, const TerrainTileNode* parent, vsg::RecordTraversal& rv)
{
    // first, update the tracker to keep this tile alive.
    TileTable::iterator i = _tiles.find(tile->key);

    if (i == _tiles.end())
    {
        // new entry:
        auto& entry = _tiles[tile->key];
        entry._tile = tile;
        entry._trackerToken = _tracker.use(tile, nullptr);
    }
    else
    {
        _tracker.use(tile, i->second._trackerToken);
    }

    // next, see if the tile needs anything.
    // 
    // "progressive" means do not load LOD N+1 until LOD N is complete.
    const bool progressive = true;

    if (progressive)
    {

#if 0
        auto tileHasData = tile->dataMerger.available();

        if (tileHasData && tile->_needsChildren)
            _loadChildren.push_back(tile->key);

        if (parentHasData && tile->dataLoader.empty())
            _loadData.push_back(tile->key);
#else

        auto tileHasData = tile->dataMerger.available();

#ifdef LOAD_ELEVATION_SEPARATELY
        auto tileHasElevation = tile->elevationMerger.available();
#else
        auto tileHasElevation = tileHasData;
#endif
        
        if (tileHasData && tileHasElevation && tile->_needsChildren)
            _loadChildren.push_back(tile->key);

        bool parentHasElevation = (parent == nullptr || parent->elevationMerger.available());
        if (parentHasElevation && tile->elevationLoader.empty())
            _loadElevation.push_back(tile->key);

        bool parentHasData = (parent == nullptr || parent->dataMerger.available());
        if (parentHasData && tile->dataLoader.empty())
            _loadData.push_back(tile->key);
#endif

    }
    else
    {
        //// free for all
        //if (tile->_needsChildren)
        //    _needsChildren.push_back(tile->key);

        //if (tile->dataLoader.empty())
        //    _needsLoad.push_back(tile->key);
    }

    if (tile->elevationLoader.available() && tile->elevationMerger.empty())
        _mergeElevation.push_back(tile->key);

    // This will only queue one merge per frame, to prevent overloading
    // the (synchronous) update cycle in VSG.
    if (tile->dataLoader.available() && tile->dataMerger.empty())
        _mergeData.push_back(tile->key);

    if (tile->_needsUpdate)
        _updateData.push_back(tile->key);
}

void
TileNodeRegistry::update(
    const vsg::FrameStamp* fs,
    const IOOptions& io,
    shared_ptr<TerrainContext> terrain)
{
    std::scoped_lock lock(_mutex);

    //Log::info()
    //    << "Frame " << fs->frameCount << ": "
    //    << "tiles=" << _tracker._list.size() << " "
    //    << "needsChildren=" << _loadChildren.size() << " "
    //    << "needsLoad=" << _loadData.size() << " "
    //    << "needsMerge=" << _mergeData.size() << std::endl;

    // update any tiles that asked for it
    for (auto& key : _updateData)
    {
        auto iter = _tiles.find(key);
        if (iter != _tiles.end())
        {
            iter->second._tile->update(fs, io);
        }
    }
    _updateData.clear();

    // launch any "new children" requests
    for (auto& key : _loadChildren)
    {
        auto iter = _tiles.find(key);
        if (iter != _tiles.end())
        {
            requestLoadChildren(
                iter->second._tile, // parent
                terrain);  // context

            iter->second._tile->_needsChildren = false;
        }
    }
    _loadChildren.clear();

#ifdef LOAD_ELEVATION_SEPARATELY
    // launch any data loading requests
    for (auto& key : _loadElevation)
    {
        auto iter = _tiles.find(key);
        if (iter != _tiles.end())
        {
            requestLoadElevation(iter->second._tile, io, terrain);
        }
    }
    _loadElevation.clear();

    // schedule any data merging requests
    for (auto& key : _mergeElevation)
    {
        auto iter = _tiles.find(key);
        if (iter != _tiles.end())
        {
            requestMergeElevation(iter->second._tile, io, terrain);
        }
    }
    _mergeElevation.clear();
#endif

    // launch any data loading requests
    for (auto& key : _loadData)
    {
        auto iter = _tiles.find(key);
        if (iter != _tiles.end())
        {
            requestLoadData(iter->second._tile, io, terrain);
        }
    }
    _loadData.clear();

    // schedule any data merging requests
    for (auto& key : _mergeData)
    {
        auto iter = _tiles.find(key);
        if (iter != _tiles.end())
        {
            requestMergeData(iter->second._tile, io, terrain);
        }
    }
    _mergeData.clear();

    // Flush unused tiles (i.e., tiles that failed to ping) out of
    // the system. Tiles ping their children all at once; this
    // should in theory prevent a child from expiring without its
    // siblings.
    // TODO: track frames, times, and resident requirements so we 
    // are not thrashing tiles in and out of memory. Perhaps a simple
    // L2 cache of disposed tiles would be appropriate instead of
    // all these limits.
    const auto dispose = [&](TerrainTileNode* tile)
    {
        if (!tile->doNotExpire)
        {
            auto key = tile->key;
            auto parent_iter = _tiles.find(key.createParentKey());
            if (parent_iter != _tiles.end())
            {
                auto parent = parent_iter->second._tile;
                if (parent.valid())
                    parent->unloadChildren();
            }
            _tiles.erase(key);
            return true;
        }
        return false;
    };

    _tracker.flush(~0, dispose);
}

vsg::ref_ptr<TerrainTileNode>
TileNodeRegistry::createTile(
    const TileKey& key,
    vsg::ref_ptr<TerrainTileNode> parent,
    shared_ptr<TerrainContext> terrain)
{
    GeometryPool::Settings geomSettings
    {
        terrain->settings.tileSize,
        terrain->settings.skirtRatio,
        terrain->settings.morphTerrain
    };

    // Get a shared geometry from the pool that corresponds to this tile key:
    auto geometry = terrain->geometryPool->getPooledGeometry(
        key,
        geomSettings,
        nullptr);

    // initialize all the per-tile uniforms the shaders will need:
    float range, morphStart, morphEnd;
    terrain->selectionInfo->get(key, range, morphStart, morphEnd);
    float one_over_end_minus_start = 1.0f / (morphEnd - morphStart);
    fvec2 morphConstants = fvec2(morphEnd * one_over_end_minus_start, one_over_end_minus_start);

    // Calculate the visibility range for this tile's children.
    float childrenVisibilityRange = FLT_MAX;
    if (key.levelOfDetail() < (terrain->selectionInfo->getNumLODs() - 1))
    {
        auto[tw, th] = key.profile().numTiles(key.levelOfDetail());
        TileKey testKey = key.createChildKey((key.tileY() <= th / 2) ? 0 : 3);
        childrenVisibilityRange = terrain->selectionInfo->getRange(testKey);
    }

    // Make the new terrain tile
    auto tile = TerrainTileNode::create(
        key,
        parent,
        geometry,
        morphConstants,
        childrenVisibilityRange,
        terrain->worldSRS,
        terrain->stateFactory->defaultTileDescriptors,
        terrain->tiles->_host,
        terrain->runtime);

    // inherit model data from the parent
    if (parent)
        tile->inheritFrom(parent);

    // update the bounding sphere for culling
    tile->recomputeBound();

    // Generate its state group:
    terrain->stateFactory->updateTerrainTileDescriptors(
        tile->renderModel,
        tile->stategroup,
        terrain->runtime);

    return tile;
}

vsg::ref_ptr<TerrainTileNode>
TileNodeRegistry::getTile(const TileKey& key) const
{
    std::scoped_lock lock(_mutex);
    auto iter = _tiles.find(key);
    return
        iter != _tiles.end() ? iter->second._tile :
        vsg::ref_ptr<TerrainTileNode>(nullptr);
}
void
TileNodeRegistry::requestLoadChildren(
    vsg::ref_ptr<TerrainTileNode> parent,
    shared_ptr<TerrainContext> terrain) const
{
    ROCKY_SOFT_ASSERT_AND_RETURN(parent, void());

    // make sure we're not already working on it
    if (!parent->childrenLoader.empty())
        return;

    vsg::observer_ptr<TerrainTileNode> weak_parent(parent);

    // function that will create all 4 children and compile them
    auto create_children = [terrain, weak_parent](Cancelable& p)
    {
        vsg::ref_ptr<vsg::Node> result;
        auto parent = weak_parent.ref_ptr();
        if (parent)
        {
            auto quad = vsg::QuadGroup::create();

            for (unsigned quadrant = 0; quadrant < 4; ++quadrant)
            {
                if (p.canceled())
                    return result;

                TileKey childkey = parent->key.createChildKey(quadrant);

                auto tile = terrain->tiles->createTile(
                    childkey,
                    parent,
                    terrain);

                ROCKY_SOFT_ASSERT_AND_RETURN(tile != nullptr, result);

                quad->children[quadrant] = tile;
            }

            // assign the result once all 4 children are added
            result = quad;
        }

        return result;
    };

    // a callback that will return the loading priority of a tile
    auto priority_func = [weak_parent]() -> float
    {
        auto tile = weak_parent.ref_ptr();
        return tile ? -(sqrt(tile->lastTraversalRange) * tile->key.levelOfDetail()) : 0.0f;
    };

    parent->childrenLoader = terrain->runtime.compileAndAddChild(
        parent,
        create_children,
        {
            "create child " + parent->key.str(),
            priority_func,
            util::job_scheduler::get(terrain->loadSchedulerName),
            nullptr
        });
}

void
TileNodeRegistry::requestLoadData(
    vsg::ref_ptr<TerrainTileNode> tile,
    const IOOptions& in_io,
    shared_ptr<TerrainContext> terrain) const
{
    ROCKY_SOFT_ASSERT_AND_RETURN(tile, void());

    // make sure we're not already working on it
    if (tile->dataLoader.working() || tile->dataLoader.available())
    {
        return;
    }

    auto key = tile->key;

    CreateTileManifest manifest;

#ifdef LOAD_ELEVATION_SEPARATELY
    for (auto& layer : terrain->map->layers().ofType<ImageLayer>())
        manifest.insert(layer);
#endif

    const IOOptions io(in_io);

    auto load = [key, manifest, terrain, io](Cancelable& p) -> TerrainTileModel
    {
        if (p.canceled())
        {
            return { };
        }

        TerrainTileModelFactory factory;

        auto model = factory.createTileModel(
            terrain->map.get(),
            key,
            manifest,
            IOOptions(io, p));

        return model;
    };

    // a callback that will return the loading priority of a tile
    // we must use a WEAK pointer to allow job cancelation to work
    vsg::observer_ptr<TerrainTileNode> tile_weak(tile);
    auto priority_func = [tile_weak]() -> float
    {
        vsg::ref_ptr<TerrainTileNode> tile = tile_weak.ref_ptr();
        return tile ? -(sqrt(tile->lastTraversalRange) * tile->key.levelOfDetail()) : 0.0f;
    };

    tile->dataLoader = util::job::dispatch(
        load, {
            "load data " + key.str(),
            priority_func,
            util::job_scheduler::get(terrain->loadSchedulerName),
            nullptr
        } );
}

void
TileNodeRegistry::requestMergeData(
    vsg::ref_ptr<TerrainTileNode> tile,
    const IOOptions& in_io,
    shared_ptr<TerrainContext> terrain) const
{
    ROCKY_SOFT_ASSERT_AND_RETURN(tile, void());

    // make sure we're not already working on it
    if (tile->dataMerger.working() || tile->dataMerger.available())
    {
        return;
    }

    auto key = tile->key;
    const IOOptions io(in_io);

    auto merge = [key, terrain](Cancelable& p) -> bool
    {
        if (p.canceled())
        {
            return false;
        }

        //util::scoped_chrono timer("merge sync " + key.str());

        auto tile = terrain->tiles->getTile(key);
        if (tile)
        {
            auto model = tile->dataLoader.get();

            auto& renderModel = tile->renderModel;

            bool updated = false;

            if (model.colorLayers.size() > 0)
            {
                auto& layer = model.colorLayers[0];
                if (layer.image.valid())
                {
                    renderModel.color.image = layer.image.image();
                    renderModel.color.matrix = layer.matrix;
                }
                updated = true;
            }

#ifndef LOAD_ELEVATION_SEPARATELY
            if (model.elevation.heightfield.valid())
            {
                renderModel.elevation.image = model.elevation.heightfield.heightfield();
                renderModel.elevation.matrix = model.elevation.matrix;

                // prompt the tile can update its bounds
                tile->setElevation(
                    renderModel.elevation.image,
                    renderModel.elevation.matrix);

                updated = true;
            }

            if (model.normalMap.image.valid())
            {
                renderModel.normal.image = model.normalMap.image.image();
                renderModel.normal.matrix = model.normalMap.matrix;

                updated = true;
            }
#endif

            if (updated)
            {
                terrain->stateFactory->updateTerrainTileDescriptors(
                    renderModel,
                    tile->stategroup,
                    terrain->runtime);
            }
        }

        return true;
    };

    auto merge_op = util::PromiseOperation<bool>::create(merge);

    tile->dataMerger = merge_op->future();

    //terrain->runtime.updates()->add(merge_op);

    vsg::observer_ptr<TerrainTileNode> tile_weak(tile);
    auto priority_func = [tile_weak]() -> float
    {
        vsg::ref_ptr<TerrainTileNode> tile = tile_weak.ref_ptr();
        return tile ? -(sqrt(tile->lastTraversalRange) * tile->key.levelOfDetail()) : 0.0f;
    };

    terrain->runtime.runDuringUpdate(merge_op, priority_func);
}

void
TileNodeRegistry::requestLoadElevation(
    vsg::ref_ptr<TerrainTileNode> tile,
    const IOOptions& in_io,
    shared_ptr<TerrainContext> terrain) const
{
    ROCKY_SOFT_ASSERT_AND_RETURN(tile, void());

    // make sure we're not already working on it
    if (tile->elevationLoader.working() || tile->elevationLoader.available())
    {
        return;
    }

    auto key = tile->key;

    CreateTileManifest manifest;
    for (auto& elev : terrain->map->layers().ofType<ElevationLayer>())
        manifest.insert(elev);

    const IOOptions io(in_io);

    auto load = [key, manifest, terrain, io](Cancelable& p) -> TerrainTileModel
    {
        if (p.canceled())
        {
            return { };
        }

        TerrainTileModelFactory factory;

        auto model = factory.createTileModel(
            terrain->map.get(),
            key,
            manifest,
            IOOptions(io, p));

        return model;
    };

    // a callback that will return the loading priority of a tile
    // we must use a WEAK pointer to allow job cancelation to work
    vsg::observer_ptr<TerrainTileNode> tile_weak(tile);
    auto priority_func = [tile_weak]() -> float
    {
        vsg::ref_ptr<TerrainTileNode> tile = tile_weak.ref_ptr();
        return tile ? -(sqrt(tile->lastTraversalRange) * 0.9 * tile->key.levelOfDetail()) : 0.0f;
    };

    tile->elevationLoader = util::job::dispatch(
        load, {
             "load elevation " + key.str(),
             priority_func,
             util::job_scheduler::get(terrain->loadSchedulerName),
             nullptr
        }
    );
}



void
TileNodeRegistry::requestMergeElevation(
    vsg::ref_ptr<TerrainTileNode> tile,
    const IOOptions& in_io,
    shared_ptr<TerrainContext> terrain) const
{
    ROCKY_SOFT_ASSERT_AND_RETURN(tile, void());

    // make sure we're not already working on it
    if (tile->elevationMerger.working() || tile->elevationMerger.available())
    {
        return;
    }

    auto key = tile->key;
    const IOOptions io(in_io);

    auto merge = [key, terrain](Cancelable& p) -> bool
    {
        if (p.canceled())
        {
            return false;
        }

        //util::scoped_chrono timer("merge sync " + key.str());

        auto tile = terrain->tiles->getTile(key);
        if (tile)
        {
            auto model = tile->elevationLoader.get();

            auto& renderModel = tile->renderModel;

            bool updated = false;

            if (model.elevation.heightfield.valid())
            {
                renderModel.elevation.image = model.elevation.heightfield.heightfield();
                renderModel.elevation.matrix = model.elevation.matrix;

                // prompt the tile can update its bounds
                tile->setElevation(
                    renderModel.elevation.image,
                    renderModel.elevation.matrix);

                updated = true;
            }

            if (model.normalMap.image.valid())
            {
                renderModel.normal.image = model.normalMap.image.image();
                renderModel.normal.matrix = model.normalMap.matrix;
                updated = true;
            }

            if (updated)
            {
                terrain->stateFactory->updateTerrainTileDescriptors(
                    renderModel,
                    tile->stategroup,
                    terrain->runtime);

                Log::info() << "Elevation merged for " << key.str() << std::endl;
            }
        }

        return true;
    };

    auto merge_op = util::PromiseOperation<bool>::create(merge);

    tile->elevationMerger = merge_op->future();

    //terrain->runtime.updates()->add(merge_op);


    vsg::observer_ptr<TerrainTileNode> tile_weak(tile);
    auto priority_func = [tile_weak]() -> float
    {
        vsg::ref_ptr<TerrainTileNode> tile = tile_weak.ref_ptr();
        return tile ? -(sqrt(tile->lastTraversalRange) * 0.9 * tile->key.levelOfDetail()) : 0.0f;
    };
    terrain->runtime.runDuringUpdate(merge_op, priority_func);
}