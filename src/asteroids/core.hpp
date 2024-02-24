#pragma once

#include "network.hpp"

AE_NAMESPACE_BEGIN

struct TransformComponent : public NetworkedComponent {
public:
    TransformComponent() = default;
    explicit TransformComponent(const sf::Vector2f& newPos)
        : pos(newPos) {}

    NODISCARD sf::Vector2f getUnweightedPos() const { return pos; }
    NODISCARD sf::Vector2f getPos() const { return pos + origin; }
    void setPos(sf::Vector2f newPos) {
        lastPos = pos;
        pos = newPos - origin;
    }

    NODISCARD float getRot() const { return rot; }
    void setRot(float newRot) {
        lastRot = rot;
        rot = newRot;
    }

    NODISCARD sf::Vector2f getOrigin() const { return origin; }
    void setOrigin(sf::Vector2f newOrigin) { origin = newOrigin; }

    template<typename S>
    void serialize(S& s) {
        s.object(pos);
        s.value4b(rot);
        s.object(origin);
    }

    NODISCARD bool isSameAsLast() const {
        return lastPos == pos && lastRot == rot;
    }

private:
    sf::Vector2f pos = { 0.0f, 0.0f };
    float rot = 0.0f;
    sf::Vector2f origin = { 0.0f, 0.0f };

    sf::Vector2f lastPos = { 0.0f, 0.0f };
    float lastRot = 0.0f;        
};

struct IntegratableComponent : public NetworkedComponent {
public:
    NODISCARD sf::Vector2f getLinearVelocity() const { return linearVelocity; }

    void addLinearVelocity(sf::Vector2f vel) {
        setLast();
        linearVelocity += vel;
    }

    NODISCARD float getAngularVelocity() const { return angularVelocity; }

    NODISCARD bool isSameAsLast() const {
        return angularVelocity == lastAngularVelocity && lastLinearVelocity == linearVelocity;
    }

    template<typename S>
    void serialize(S& s) {
        s.object(linearVelocity);
        s.value4b(angularVelocity);
    }

protected:
    void setLast() {
        lastLinearVelocity = linearVelocity;
        lastAngularVelocity = angularVelocity;
    }

private:
    sf::Vector2f lastLinearVelocity = { 0.0f, 0.0f };
    float lastAngularVelocity = 0.0f;
    sf::Vector2f linearVelocity = { 0.0f, 0.0f };
    float angularVelocity = 0.0f;
};

struct TimedDeleteComponent {
    TimedDeleteComponent() 
        : timeLeft(0.0f) {}
    explicit TimedDeleteComponent(float time)
        : timeLeft(time) {}

    NODISCARD float getTime() const { return timeLeft; }
    void setTime(float time) { timeLeft = time;}
    void subtractTime(float amount)  { timeLeft -= amount; }
    NODISCARD bool isTimeDone() const { return timeLeft <= 0.0f; }

private:
    float timeLeft;
};

struct CollisionEvent {
    CollisionEvent() = default;
    CollisionEvent(CollisionManifold manifold, flecs::entity self, flecs::entity other)
        : manifold(manifold), entitySelf(self), entityOther(other) {}

    CollisionManifold manifold;
    flecs::entity entitySelf;
    flecs::entity entityOther;
};


namespace impl {
    inline void integrate(flecs::iter& iter, TransformComponent* transforms, IntegratableComponent* integratables) {
        for(auto i : iter) {
            TransformComponent& transform = transforms[i];
            IntegratableComponent& integratable = integratables[i];

            transform.setPos(transform.getPos() + integratable.getLinearVelocity() * iter.delta_time());
            transform.setRot(transform.getRot() + integratable.getAngularVelocity() * iter.delta_time());

            if (!integratable.isSameAsLast()) {
                iter.entity(i).modified<IntegratableComponent>();
                iter.entity(i).modified<TransformComponent>();
            }
        }    
    }

    inline void shapeSet(flecs::iter& iter, TransformComponent* transforms, ShapeComponent* shapes) {
        PhysicsWorld& world = getPhysicsWorld();

        for (auto i : iter) {
            TransformComponent& transform = transforms[i];
            u32 shapeId = shapes[i].shape;
            if(!shapes[i].isValid())
                continue;

            Shape& shape = world.getShape(shapeId);

            shape.setPos(transform.getUnweightedPos());
            shape.setRot(transform.getRot());

            world.insertShapeIntoTree(shapeId, iter.entity(i));
        }
    }

    inline bool testCollision(Shape& shape1, Shape& shape2, CollisionManifold& manifold) {
        switch (shape1.getType()) {
        case ShapeEnum::Polygon:
            switch (shape2.getType()) {
            case ShapeEnum::Polygon:
                return testCollision(dynamic_cast<Polygon&>(shape1), dynamic_cast<Polygon&>(shape2), manifold);

            case ShapeEnum::Circle:
                return testCollision(dynamic_cast<Polygon&>(shape1), dynamic_cast<Circle&>(shape2), manifold);
            default:
                log(ERROR_SEVERITY_FATAL, "Invalid Shape Type");
            } break;

        case ShapeEnum::Circle:
            switch (shape2.getType()) {
            case ShapeEnum::Polygon:
                return testCollision(dynamic_cast<Polygon&>(shape2), dynamic_cast<Circle&>(shape1), manifold);

            case ShapeEnum::Circle:
                return testCollision(dynamic_cast<Circle&>(shape1), dynamic_cast<Circle&>(shape2), manifold);

            default:
                log(ERROR_SEVERITY_FATAL, "Invalid Shape Type");
            }

        default:
            log(ERROR_SEVERITY_FATAL, "Invalid Shape Type");
            break;
        }

        return false; // silences warnings
    }

    inline struct {
        std::vector<SpatialIndexElement> results;
    } physicsEcsCache;

    inline void shapeCollide(flecs::iter& iter, ShapeComponent* shapes) {
        PhysicsWorld& world = getPhysicsWorld();
        SpatialIndexTree& tree = world.getTree();

        for (auto i : iter) {
            u32 shapeId = shapes[i].shape;
            if (!shapes[i].isValid())
                continue;

            Shape& shape = world.getShape(shapeId);
            AABB aabb = shape.getAABB();

            physicsEcsCache.results.clear();
            tree.query(spatial::intersects<2>(aabb.min.data(), aabb.max.data()), std::back_inserter(physicsEcsCache.results));

            for (SpatialIndexElement& element : physicsEcsCache.results) {
                if (element.shapeId == shapeId)
                    continue;

                Shape& foundShape = world.getShape(element.shapeId);

                CollisionManifold manifold;
                if (testCollision(shape, foundShape, manifold)) {
                    iter.world()
                        .event<CollisionEvent>()
                        .id<ShapeComponent>()
                        .entity(iter.entity(i))
                        .ctx(CollisionEvent(manifold, iter.entity(i), iter.world().get_alive(element.entityId)))
                        .emit();
                }
            }
        }
    }

    inline void transformSet(flecs::iter& iter, TransformComponent* transforms, ShapeComponent* shapes) {
        PhysicsWorld& world = getPhysicsWorld();

        for (auto i : iter) {
            TransformComponent& transform = transforms[i];
            u32 shapeId = shapes[i].shape;
            if (!shapes[i].isValid())
                continue;

            Shape& shape = world.getShape(shapeId);

            transform.setOrigin(shape.getCentroid());
            transform.setPos(shape.getWeightedPos());
            transform.setRot(shape.getRot());

            if(!transform.isSameAsLast()) {
                iter.entity(i).modified<TransformComponent>();
            }
        }
    }

    inline void treeClear(flecs::iter& iter) {
        getPhysicsWorld().clearTree();
    }

    inline void onShapeDestroy(flecs::iter& iter, ShapeComponent* shapes) {
        PhysicsWorld& world = getPhysicsWorld();

        for (auto i : iter) {
            if (shapes[i].isValid()) {
                world.eraseShape(shapes[i].shape);
                shapes[i].shape = PhysicsWorld::invalidId;
            }
        }
    }

    inline void isTimedDeleteDone(flecs::iter& iter, TimedDeleteComponent* timers) {
        for(auto i : iter) {
            TimedDeleteComponent& timer = timers[i];

            timer.subtractTime(iter.delta_time());
            if(timer.isTimeDone()) {
                iter.entity(i).destruct();
            }
        }
    }
}

/*
 * The core module defines and declares all of the important necessary components and systems
 * that allow the engine to work
 */
struct CoreModule {
    inline static flecs::entity treeClear;
    inline static flecs::entity prePhysics;
    inline static flecs::entity mainPhysics;
    inline static flecs::entity postPhysics;

    explicit CoreModule(flecs::world& world) {
        treeClear = world.entity()
            .add(flecs::Phase)
            .depends_on(flecs::OnUpdate);

        prePhysics = world.entity()
            .add(flecs::Phase)
            .depends_on(treeClear);

        mainPhysics = world.entity()
            .add(flecs::Phase)
            .depends_on(prePhysics);

        postPhysics = world.entity()
            .add(flecs::Phase)
            .depends_on(mainPhysics);

        world.system().kind(treeClear).iter(impl::treeClear);
        world.system<TransformComponent, ShapeComponent>().kind(prePhysics).iter(impl::shapeSet);
        world.system<ShapeComponent>().kind(mainPhysics).iter(impl::shapeCollide);
        world.system<TransformComponent, ShapeComponent>().kind(postPhysics).iter(impl::transformSet);
        world.system<TransformComponent, IntegratableComponent>().iter(impl::integrate);

        world.system<TimedDeleteComponent>().iter(impl::isTimedDeleteDone);
    }

    static void registerCore() {
        getEntityWorld().enable_range_check(false);

        NetworkStateManager& manager = getNetworkStateManager();
        manager.registerComponent<TransformComponent>();
        manager.registerComponent<ShapeComponent>(ComponentPiority::High);
        manager.registerComponent<IntegratableComponent>();

        getEntityWorld().observer<ShapeComponent>().event(flecs::OnRemove).iter(impl::onShapeDestroy);

        getEntityWorld().enable_range_check(true);
    }
};

AE_NAMESPACE_END