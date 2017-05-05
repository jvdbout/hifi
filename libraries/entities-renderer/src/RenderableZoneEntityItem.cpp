//
//  RenderableZoneEntityItem.cpp
//
//
//  Created by Clement on 4/22/15.
//  Copyright 2015 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "RenderableZoneEntityItem.h"

#include <gpu/Batch.h>

#include <AbstractViewStateInterface.h>
#include <DependencyManager.h>
#include <GeometryCache.h>
#include <PerfStat.h>

#include "EntityTreeRenderer.h"
#include "RenderableEntityItem.h"

#include <LightPayload.h>


// Sphere entities should fit inside a cube entity of the same size, so a sphere that has dimensions 1x1x1
// is a half unit sphere.  However, the geometry cache renders a UNIT sphere, so we need to scale down.
static const float SPHERE_ENTITY_SCALE = 0.5f;

EntityItemPointer RenderableZoneEntityItem::factory(const EntityItemID& entityID, const EntityItemProperties& properties) {
    EntityItemPointer entity{ new RenderableZoneEntityItem(entityID) };
    entity->setProperties(properties);
    return entity;
}

template<typename Lambda>
void RenderableZoneEntityItem::changeProperties(Lambda setNewProperties) {
    QString oldShapeURL = getCompoundShapeURL();
    glm::vec3 oldPosition = getPosition(), oldDimensions = getDimensions();
    glm::quat oldRotation = getRotation();
    
    setNewProperties();
    
    if (oldShapeURL != getCompoundShapeURL()) {
        if (_model) {
            delete _model;
        }
        
        _model = getModel();
        _needsInitialSimulation = true;
        _model->setURL(getCompoundShapeURL());
    }
    if (oldPosition != getPosition() ||
        oldRotation != getRotation() ||
        oldDimensions != getDimensions()) {
        _needsInitialSimulation = true;
    }
}

bool RenderableZoneEntityItem::setProperties(const EntityItemProperties& properties) {
    bool somethingChanged = false;
    changeProperties([&]() {
        somethingChanged = this->ZoneEntityItem::setProperties(properties);
    });
    return somethingChanged;
}

void RenderableZoneEntityItem::somethingChangedNotification() {
    DependencyManager::get<EntityTreeRenderer>()->updateZone(_id);

    // A new way:
    if (_keyLightPropertiesChanged || _backgroundPropertiesChanged) {
        notifyChangedRenderItem();
    }

    // Poopagate back to parent
    ZoneEntityItem::somethingChangedNotification();
}


int RenderableZoneEntityItem::readEntitySubclassDataFromBuffer(const unsigned char* data, int bytesLeftToRead,
                                                                ReadBitstreamToTreeParams& args,
                                                                EntityPropertyFlags& propertyFlags, bool overwriteLocalData,
                                                                bool& somethingChanged) {
    int bytesRead = 0;
    changeProperties([&]() {
        bytesRead = ZoneEntityItem::readEntitySubclassDataFromBuffer(data, bytesLeftToRead,
                                                                     args, propertyFlags, 
                                                                     overwriteLocalData, somethingChanged);
    });
    return bytesRead;
}

Model* RenderableZoneEntityItem::getModel() {
    Model* model = new Model(nullptr);
    model->setIsWireframe(true);
    model->init();
    return model;
}

void RenderableZoneEntityItem::initialSimulation() {
    _model->setScaleToFit(true, getDimensions());
    _model->setSnapModelToRegistrationPoint(true, getRegistrationPoint());
    _model->setRotation(getRotation());
    _model->setTranslation(getPosition());
    _model->simulate(0.0f);
    _needsInitialSimulation = false;
}

void RenderableZoneEntityItem::updateGeometry() {
    if (_model && !_model->isActive() && hasCompoundShapeURL()) {
        // Since we have a delayload, we need to update the geometry if it has been downloaded
        _model->setURL(getCompoundShapeURL());
    }
    if (_model && _model->isActive() && _needsInitialSimulation) {
        initialSimulation();
    }
}

void RenderableZoneEntityItem::render(RenderArgs* args) {
    Q_ASSERT(getType() == EntityTypes::Zone);
    
    if (_drawZoneBoundaries) {
        switch (getShapeType()) {
            case SHAPE_TYPE_COMPOUND: {
                PerformanceTimer perfTimer("zone->renderCompound");
                updateGeometry();
                if (_model && _model->needsFixupInScene()) {
                    // check to see if when we added our models to the scene they were ready, if they were not ready, then
                    // fix them up in the scene
                    render::ScenePointer scene = AbstractViewStateInterface::instance()->getMain3DScene();
                    render::Transaction transaction;
                    _model->removeFromScene(scene, transaction);
                    render::Item::Status::Getters statusGetters;
                    makeEntityItemStatusGetters(getThisPointer(), statusGetters);
                    _model->addToScene(scene, transaction);
                    
                    scene->enqueueTransaction(transaction);
                    
                    _model->setVisibleInScene(getVisible(), scene);
                }
                break;
            }
            case SHAPE_TYPE_BOX:
            case SHAPE_TYPE_SPHERE: {
                PerformanceTimer perfTimer("zone->renderPrimitive");
                glm::vec4 DEFAULT_COLOR(1.0f, 1.0f, 1.0f, 1.0f);
                
                Q_ASSERT(args->_batch);
                gpu::Batch& batch = *args->_batch;

                bool success;
                auto shapeTransform = getTransformToCenter(success);
                if (!success) {
                    break;
                }
                auto geometryCache = DependencyManager::get<GeometryCache>();
                if (getShapeType() == SHAPE_TYPE_SPHERE) {
                    shapeTransform.postScale(SPHERE_ENTITY_SCALE);
                    batch.setModelTransform(shapeTransform);
                    geometryCache->renderWireSphereInstance(batch, DEFAULT_COLOR);
                } else {
                    batch.setModelTransform(shapeTransform);
                    geometryCache->renderWireCubeInstance(batch, DEFAULT_COLOR);
                }
                break;
            }
            default:
                // Not handled
                break;
        }
    }
    
    if ((!_drawZoneBoundaries || getShapeType() != SHAPE_TYPE_COMPOUND) &&
        _model && !_model->needsFixupInScene()) {
        // If the model is in the scene but doesn't need to be, remove it.
        render::ScenePointer scene = AbstractViewStateInterface::instance()->getMain3DScene();
        render::Transaction transaction;
        _model->removeFromScene(scene, transaction);
        scene->enqueueTransaction(transaction);
    }

    /*
    {
        // Set the keylight
        sceneKeyLight->setColor(ColorUtils::toVec3(this->getKeyLightProperties().getColor()));
        sceneKeyLight->setIntensity(this->getKeyLightProperties().getIntensity());
        sceneKeyLight->setAmbientIntensity(this->getKeyLightProperties().getAmbientIntensity());
        sceneKeyLight->setDirection(this->getKeyLightProperties().getDirection());

        // Set the stage
        bool isSunModelEnabled = this->getStageProperties().getSunModelEnabled();
        sceneStage->setSunModelEnable(isSunModelEnabled);
        if (isSunModelEnabled) {
            sceneStage->setLocation(this->getStageProperties().getLongitude(),
                this->getStageProperties().getLatitude(),
                this->getStageProperties().getAltitude());

            auto sceneTime = sceneStage->getTime();
            sceneTime->setHour(this->getStageProperties().calculateHour());
            sceneTime->setDay(this->getStageProperties().calculateDay());
        }

        // Set the ambient texture
        _ambientTextureURL = this->getKeyLightProperties().getAmbientURL();
        if (_ambientTextureURL.isEmpty()) {
            _pendingAmbientTexture = false;
            _ambientTexture.clear();
        } else {
            _pendingAmbientTexture = true;
        }
    }*/
}

bool RenderableZoneEntityItem::contains(const glm::vec3& point) const {
    if (getShapeType() != SHAPE_TYPE_COMPOUND) {
        return EntityItem::contains(point);
    }
    const_cast<RenderableZoneEntityItem*>(this)->updateGeometry();
    
    if (_model && _model->isActive() && EntityItem::contains(point)) {
        return _model->convexHullContains(point);
    }
    
    return false;
}

class RenderableZoneEntityItemMeta {
public:
    RenderableZoneEntityItemMeta(EntityItemPointer entity) : entity(entity){ }
    typedef render::Payload<RenderableZoneEntityItemMeta> Payload;
    typedef Payload::DataPointer Pointer;
    
    EntityItemPointer entity;
};

namespace render {
    template <> const ItemKey payloadGetKey(const RenderableZoneEntityItemMeta::Pointer& payload) {
        return ItemKey::Builder::opaqueShape();
    }
    
    template <> const Item::Bound payloadGetBound(const RenderableZoneEntityItemMeta::Pointer& payload) {
        if (payload && payload->entity) {
            bool success;
            auto result = payload->entity->getAABox(success);
            if (!success) {
                return render::Item::Bound();
            }
            return result;
        }
        return render::Item::Bound();
    }
    template <> void payloadRender(const RenderableZoneEntityItemMeta::Pointer& payload, RenderArgs* args) {
        if (args) {
            if (payload && payload->entity) {
                payload->entity->render(args);
            }
        }
    }
}

bool RenderableZoneEntityItem::addToScene(EntityItemPointer self, const render::ScenePointer& scene,
                                           render::Transaction& transaction) {
    _myMetaItem = scene->allocateID();
    
    auto renderData = std::make_shared<RenderableZoneEntityItemMeta>(self);
    auto renderPayload = std::make_shared<RenderableZoneEntityItemMeta::Payload>(renderData);

    render::Item::Status::Getters statusGetters;
    makeEntityItemStatusGetters(getThisPointer(), statusGetters);
    renderPayload->addStatusGetters(statusGetters);

    transaction.resetItem(_myMetaItem, renderPayload);


    _myKeyLightItem = scene->allocateID();

    auto keyLightPayload = std::make_shared<KeyLightPayload>();
    updateKeyLightItemFromEntity((*keyLightPayload));

    auto keyLightItem = std::make_shared<KeyLightPayload::Payload>(keyLightPayload);

    transaction.resetItem(_myKeyLightItem, keyLightItem);


    return true;
}

void RenderableZoneEntityItem::removeFromScene(EntityItemPointer self, const render::ScenePointer& scene,
                                                render::Transaction& transaction) {
    transaction.removeItem(_myKeyLightItem);
    render::Item::clearID(_myKeyLightItem);
    transaction.removeItem(_myMetaItem);
    render::Item::clearID(_myMetaItem);
    if (_model) {
        _model->removeFromScene(scene, transaction);
    }
}



void RenderableZoneEntityItem::notifyBoundChanged() {
    if (!render::Item::isValidID(_myMetaItem)) {
        return;
    }
    render::Transaction transaction;
    render::ScenePointer scene = AbstractViewStateInterface::instance()->getMain3DScene();
    if (scene) {
        transaction.updateItem<RenderableZoneEntityItemMeta>(_myMetaItem, [](RenderableZoneEntityItemMeta& data) {});
        transaction.updateItem<KeyLightPayload>(_myKeyLightItem, [](KeyLightPayload& data) {});

        scene->enqueueTransaction(transaction);
    } else {
        qCWarning(entitiesrenderer) << "RenderableZoneEntityItem::notifyBoundChanged(), Unexpected null scene, possibly during application shutdown";
    }
}

void RenderableZoneEntityItem::updateKeyLightItemFromEntity(KeyLightPayload& keylightPayload) {
    auto entity = this;

    keylightPayload.setVisible(entity->getVisible());

    auto light = keylightPayload.editLight();
    light->setPosition(entity->getPosition());
    light->setOrientation(entity->getRotation());

    bool success;
    keylightPayload.editBound() = entity->getAABox(success);
    if (!success) {
        keylightPayload.editBound() = render::Item::Bound();
    }

    // Set the keylight
    light->setColor(ColorUtils::toVec3(this->getKeyLightProperties().getColor()));
    light->setIntensity(this->getKeyLightProperties().getIntensity());
    light->setAmbientIntensity(this->getKeyLightProperties().getAmbientIntensity());
    light->setDirection(this->getKeyLightProperties().getDirection());


 //   light->setColor(toGlm(entity->getXColor()));

  //  float intensity = entity->getIntensity();//* entity->getFadingRatio();
  //  light->setIntensity(intensity);


    light->setType(model::Light::SUN);

}

void RenderableZoneEntityItem::sceneUpdateRenderItemFromEntity(render::Transaction& transaction) {
    if (!render::Item::isValidID(_myKeyLightItem)) {
        return;
    }

    transaction.updateItem<KeyLightPayload>(_myKeyLightItem, [&](KeyLightPayload& data) {
        updateKeyLightItemFromEntity(data);
    });
}

void RenderableZoneEntityItem::notifyChangedRenderItem() {
    if (!render::Item::isValidID(_myKeyLightItem)) {
        return;
    }

    render::Transaction transaction;
    render::ScenePointer scene = AbstractViewStateInterface::instance()->getMain3DScene();
    sceneUpdateRenderItemFromEntity(transaction);
    scene->enqueueTransaction(transaction);
}