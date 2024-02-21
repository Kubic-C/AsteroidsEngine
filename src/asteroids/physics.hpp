#pragma once
#include "includes.hpp"

AE_NAMESPACE_BEGIN

inline float crossProduct(const sf::Vector2f& v1, const sf::Vector2f& v2) {
	return (v1.x * v2.y) - (v1.y * v2.x);
}

inline float fastSin(float x) {
	return std::sin(x);
}

inline float fastCos(float x) {
	return std::cos(x);
}

inline sf::Vector2f fastRotate(sf::Vector2f v, float a) {
	sf::Vector2f result;
	const float cos = fastCos(a);
	const float sin = fastSin(a);

	result.x = v.x * cos - v.y * sin;
	result.y = v.x * sin + v.y * cos;
	return result;
}

// rotate using precalcuted sin and cos
inline sf::Vector2f fastRotateWithPrecalc(sf::Vector2f v, float sin, float cos) {
	sf::Vector2f result;

	result.x = v.x * cos - v.y * sin;
	result.y = v.x * sin + v.y * cos;
	return result;
}


struct AABB {
	AABB() {
		min[0] = 0.0f;
		min[1] = 0.0f;
		max[0] = 0.0f;
		max[1] = 0.0f;
	}

	AABB(float halfWidth, float halfHeight, sf::Vector2f pos = sf::Vector2f()) {
		min[0] = -halfWidth + pos.x;
		min[1] = -halfHeight + pos.y;
		max[0] = halfWidth + pos.x;
		max[1] = halfHeight + pos.y;
	}

	std::array<float, 2> min;
	std::array<float, 2> max;

	bool isPointInside(sf::Vector2f v) {
		return min[0] <= v.x && v.x <= max[0] && min[1] <= v.y && v.y <= max[1];
	}
};

// doess aabb1 collide with aabb2
inline bool testCollision(const AABB& aabb1, const AABB& aabb2) {
	// tooken from box2d: https://github.com/erincatto/box2d, which is licensed under Erin Cato using the MIT license

	sf::Vector2f d1, d2;
	d1 = sf::Vector2f(aabb2.min[0], aabb2.min[1]) - sf::Vector2f(aabb1.max[0], aabb1.max[1]);
	d2 = sf::Vector2f(aabb1.min[0], aabb1.min[1]) - sf::Vector2f(aabb2.max[0], aabb2.max[1]);

	if (d1.x > 0.0f || d1.y > 0.0f)
		return false;

	if (d2.x > 0.0f || d2.y > 0.0f)
		return false;

	return true;
}

enum class ShapeEnum : u8 {
	Polygon = 0,
	Circle,
	Invalid
};

template<typename S>
void serialize(S& s, ShapeEnum& e) {
	s.value1b(e);
}

class Shape {
public:
	Shape()
		: rot(0.0f), pos(0.0f, 0.0f) {
		markFullDirty();
	}

	Shape(sf::Vector2f pos, float rot)
		: pos(pos), rot(rot) {
		markFullDirty();
	}

    virtual ~Shape() = default;

    NODISCARD virtual ShapeEnum getType() const = 0;

    NODISCARD virtual float getRadius() const = 0;

	virtual AABB getAABB() = 0;

    NODISCARD float getRot() const { return rot; }
	void setRot(float rot) { markLocalDirty(); this->rot = rot; }

	NODISCARD sf::Vector2f getPos() const { return pos; }
    NODISCARD sf::Vector2f getWeightedPos() const { return pos + getCentroid(); }
    NODISCARD virtual sf::Vector2f getCentroid() const { return { 0.0f, 0.0f }; }
	void setPos(sf::Vector2f pos) { markLocalDirty(); this->pos = pos; }

    NODISCARD bool isNetworkDirty() { return localFlags[NETWORK_DIRTY]; }
	void resetNetworkDirty() { localFlags[NETWORK_DIRTY] = false; }

	void markLocalDirty() { localFlags[LOCAL_DIRTY] = true; }

	template<typename S>
	void serialize(S& s) {
		s.value4b(rot);
		s.object(pos);
	}
protected:
	virtual void Update() {}

	enum shapeFlagsIndexes_t {
		LOCAL_DIRTY = 0,
		NETWORK_DIRTY = 1
	};

	void markFullDirty() {
		localFlags[LOCAL_DIRTY] = true;
		localFlags[NETWORK_DIRTY] = true;
	}

	std::bitset<8> localFlags;
	float rot;
	sf::Vector2f pos;
};

class Circle : public Shape {
public:
	Circle()
		: Shape(), radius(0.0f) {}

	explicit Circle(float radius)
		: Shape(), radius(radius) {}

	Circle(sf::Vector2f pos, float rot, float radius = 0.0f)
		: Shape(pos, rot), radius(radius) {}

    NODISCARD virtual ShapeEnum getType() const { return ShapeEnum::Circle; }

    NODISCARD float getRadius() const override { return radius; }

    NODISCARD AABB getAABB() override {
		return AABB(getRadius(), getRadius(), pos);
	}

	void setRadius(float newRadius) { radius = newRadius; markFullDirty(); }

	template<typename S>
	void serialize(S& s) {
		s.ext(*this, bitsery::ext::BaseClass<Shape>{});
		s.value4b(radius);
	}
private:
	float radius;
};

// Convex polygons only.
class Polygon : public Shape {
public:
	typedef IndirectContainer<sf::Vector2f> vertices_t;

	Polygon()
		: Shape(), verticesCount(0), vertices(), radius(0.0f) {}

	Polygon(const std::initializer_list<sf::Vector2f>& localVertices)
		: Shape(), verticesCount((u8)localVertices.size()) {
		memcpy(vertices.data(), localVertices.begin(), verticesCount * sizeof(sf::Vector2f));
		fixVertices(); // radius initialized here
	}

	Polygon(sf::Vector2f pos, float rot)
		: Shape(pos, rot), verticesCount(0), vertices(), radius(0.0f) {
	}

	Polygon(sf::Vector2f pos, float rot, const std::initializer_list<sf::Vector2f>& localVertices)
		: Shape(pos, rot), verticesCount((u8)localVertices.size()) {
		memcpy(vertices.data(), localVertices.begin(), verticesCount * sizeof(sf::Vector2f));
		fixVertices();// radius initialized here
	}

	NODISCARD virtual sf::Vector2f getCentroid() const override { return centroid; }

    NODISCARD virtual ShapeEnum getType() const { return ShapeEnum::Polygon; }

    NODISCARD float getRadius() const override { return radius; }

    NODISCARD AABB getAABB() override {
		AABB aabb;

		aabb.min[0] = std::numeric_limits<float>::max();
		aabb.min[1] = std::numeric_limits<float>::max();
		aabb.max[0] = std::numeric_limits<float>::min();
		aabb.max[1] = std::numeric_limits<float>::min();

		if (localFlags[LOCAL_DIRTY])
			computeWorldVertices();

		for (u8 i = 0; i < verticesCount; i++) {
			sf::Vector2f vertex = cache.vertices[i];

			if (vertex.x < aabb.min[0]) {
				aabb.min[0] = vertex.x;
			}
			if (vertex.x > aabb.max[0]) {
				aabb.max[0] = vertex.x;
			}
			if (vertex.y < aabb.min[1]) {
				aabb.min[1] = vertex.y;
			}
			if (vertex.y > aabb.max[1]) {
				aabb.max[1] = vertex.y;
			}
		}

		return aabb;
	}

	void setVertices(u8 count, sf::Vector2f* localVertices) {
		verticesCount = count;
		memcpy(vertices.data(), localVertices, count * sizeof(sf::Vector2f));
		fixVertices();
		computeWorldVertices();
	}

    NODISCARD u8 getVerticeCount() const {
		return verticesCount;
	}

    NODISCARD vertices_t getWorldVertices() {
		if (localFlags[LOCAL_DIRTY])
			computeWorldVertices();

		return IndirectContainer<sf::Vector2f>(verticesCount, cache.vertices.data());
	}

    NODISCARD vertices_t getWorldNormals() {
		if (localFlags[LOCAL_DIRTY])
			computeWorldVertices();

		return IndirectContainer<sf::Vector2f>(verticesCount, cache.normals.data());
	}

	template<typename S>
	void serialize(S& s) {
		s.ext(*this, bitsery::ext::BaseClass<Shape>{});
		s.value1b(verticesCount);

		assert(verticesCount <= 8 && "Vertices exceed max\n");

		for (int i = 0; i < verticesCount; i++) {
			s.object(vertices[i]);
		}
	}

protected:
	// When a set of vertices is given, we must:
	// 1. Insure Convex- Error
	// 2. Sort CCW
	// 3. Get radius
	// 4. Get normals
	bool fixVertices() {
		assert(3 <= verticesCount && verticesCount <= 8);

		centroid = { 0.0f, 0.0f };
		std::for_each(vertices.begin(), vertices.begin() + verticesCount, [&](const sf::Vector2f& vertex) {
			centroid += vertex;
			});
		centroid /= (float)verticesCount;

		radius = 0.0f;
		std::for_each(vertices.begin(), vertices.begin() + verticesCount, [&](const sf::Vector2f& vertex) {
			float distance = (centroid - vertex).length();

			if (distance > radius) {
				distance = radius;
			}
			});

		sf::Vector2f midpoint2 = { centroid.x, centroid.y + 1.0f };
		sf::Vector2f midsegment = (centroid - midpoint2).normalized();

		// sort vertices to be CCW order
		std::sort(vertices.begin(), vertices.begin() + verticesCount,
			[&](sf::Vector2f v1, sf::Vector2f v2) {
				sf::Angle a1 = midsegment.angleTo(v1 - centroid);
				sf::Angle a2 = midsegment.angleTo(v2 - centroid);

				return a1 < a2;
			});

		// normals
		for (u8 i = 0; i < verticesCount; i++) {
			sf::Vector2f va = vertices[i];
			sf::Vector2f vb = vertices[(i + 1) % verticesCount];

			sf::Vector2f edge = vb - va;
			edge = edge.normalized();

			normals[i] = sf::Vector2f(edge.y, -edge.x);
		}

		return true;
	}

	void computeWorldVertices() {
		localFlags[LOCAL_DIRTY] = false;

		for (u8 i = 0; i < verticesCount; i++) {
			cache.vertices[i] = fastRotate(vertices[i], rot) + pos;
		}
		for (u8 i = 0; i < verticesCount; i++) {
			cache.normals[i] = fastRotate(normals[i], rot);
		}
	}

	struct {
		std::array<sf::Vector2f, 8> vertices;
		std::array<sf::Vector2f, 8> normals;
	} cache;
private:
	float radius;

	sf::Vector2f centroid;
	u8 verticesCount;
	std::array<sf::Vector2f, 8> vertices;
	std::array<sf::Vector2f, 8> normals;
};


struct CollisionManifold {
	CollisionManifold() {
		depth = std::numeric_limits<float>::max();
		normal = { 0.0f, 0.0f };
	}

	// the depth of the penetration occuring on the axis described by normal
	float depth;
	// the minimum translation vector that can be applied on either 
	// shape to reverse the collision
	sf::Vector2f normal;
};

struct Projection {
	float min, max;
};

inline Projection project(const Polygon::vertices_t& vertices, sf::Vector2f normal) {
	Projection projection;
	projection.min = normal.dot(vertices[0]);
	projection.max = projection.min;

	for (u32 i = 1; i < vertices.size(); i++) {
		float projected = normal.dot(vertices[i]);

		if (projected < projection.min) {
			projection.min = projected;
		}
		else if (projected > projection.max) {
			projection.max = projected;
		}
	}

	return projection;
}

inline float pointSegmentDistance(sf::Vector2f p, sf::Vector2f v1, sf::Vector2f v2, sf::Vector2f& cp) {
	// credit goes to https://www.youtube.com/watch?v=egmZJU-1zPU&ab_channel=Two-BitCoding
	// for this function, incredible channel and resource

	sf::Vector2f p_to_v1 = p - v1;
	sf::Vector2f v1_to_v2 = v2 - v1;
	float proj = p_to_v1.dot(v1_to_v2);
	float length = v1_to_v2.lengthSq();

	float d = proj / length;

	if (d <= 0.0f) {
		cp = v1;
	}
	else if (d >= 1.0f) {
		cp = v2;
	}
	else {
		cp = v1 + v1_to_v2 * d;
	}

	return (p - cp).length();
}

/**
 * @brief Performs a SAT test on only one set of normals. Returns true if the
 * a collision is detected
 *
 * @param vertices1 A set of vertices that will be tested against vertices2
 * @param vertices2 A set of vertices that will be tested against vertices1
 * @param normals1 A set of normals that were calculated from vertices 1
 * @param depth An out parameter of the depth of the penetration
 * @param normal An out parameter of the normal of the minimum translation vector
 * @return True if a collision was found, false otherwise
 */
inline bool satHalfTest(
	const Polygon::vertices_t& vertices1,
	const Polygon::vertices_t& vertices2,
	const Polygon::vertices_t& normals1,
	float& depth, sf::Vector2f& normal) {
	for (u32 i = 0; i < normals1.size(); i++) {
		Projection proj1 = project(vertices1, normals1[i]);
		Projection proj2 = project(vertices2, normals1[i]);

		if (!(proj1.max >= proj2.min && proj2.max >= proj1.min)) {
			// they are not collding
			return false;
		}
		else {
			float new_depth = std::max(0.0f, std::min(proj1.max, proj2.max) - std::max(proj1.min, proj2.min));

			if (new_depth <= depth) {
				normal = normals1[i];
				depth = new_depth;
			}
		}
	}

	return true;
}

/**
 * @brief Takes in 2 polygons whose vertices and normals will be compared
 * to eachother to determine if a collision has occurred. Resulting information
 * found will be put into manifold
 *
 * @param shape1 a shape that will be compared to shape2 for a collision
 * @param shape2 a shape that will be compared to shape1 for a collision
 * @param manifold an output parameter. The members normal and depth will be changed, whether
 * or not a collision was found.
 * @return true if a collision was to be found, false otherwise
 */
inline bool testCollision(Polygon& poly1, Polygon& poly2, CollisionManifold& manifold) {
	auto vertices1 = poly1.getWorldVertices();
	auto normals1 = poly1.getWorldNormals();
	auto vertices2 = poly2.getWorldVertices();
	auto normals2 = poly2.getWorldNormals();

	if (!satHalfTest(vertices1, vertices2, normals1, manifold.depth, manifold.normal))
		return false;

	if (!satHalfTest(vertices2, vertices1, normals2, manifold.depth, manifold.normal))
		return false;

	return true;
}

inline bool testCollision(Circle& circle1, Circle& circle2, CollisionManifold& manifold) {
	float total_radius = circle1.getRadius() + circle2.getRadius();
	sf::Vector2f  dir = circle2.getPos() - circle1.getPos();
	float length = dir.length();

	if (total_radius > length) {
		if(dir.x == 0.0f && dir.y == 0.0f)
			manifold.normal = {0.0f, 1.0f};
		else
			manifold.normal = dir.normalized();
		manifold.depth = total_radius - length;
		return true;
	}

	return false;
}

inline bool testCollision(Polygon& Polygon, Circle& Circle, CollisionManifold& manifold) {
	auto vertices = Polygon.getWorldVertices();
	auto normals = Polygon.getWorldNormals();

	for (u32 i = 0; i < vertices.size(); i++) {
		const sf::Vector2f& v1 = vertices[i];
		const sf::Vector2f& v2 = vertices[(i + 1) % vertices.size()];
		sf::Vector2f cp = {};

		float dist = pointSegmentDistance(Circle.getPos(), v1, v2, cp);
		if (dist < manifold.depth) {
			manifold.depth = dist;
			manifold.normal = normals[i];
		}
	}

	if (manifold.depth < Circle.getRadius()) {
		manifold.depth = Circle.getRadius() - manifold.depth;
		return true;
	}

	return false;
}

struct SpatialIndexElement : AABB {
	u32 shapeId;
	u32 entityId;

	bool operator==(const SpatialIndexElement& other) {
		return shapeId == other.shapeId;
	}
};

struct Indexable {
	const float* min(const SpatialIndexElement& value) const { return value.min.data(); }
	const float* max(const SpatialIndexElement& value) const { return value.max.data(); }
};

typedef spatial::RTree<float, SpatialIndexElement, 2, 4, 1, Indexable> SpatialIndexTree;

class PhysicsWorld;
extern PhysicsWorld& getPhysicsWorld();

class PhysicsWorld {
public:
	static constexpr u32 invalidId = std::numeric_limits<u32>::max();

	bool doesShapeExist(u32 id) {
		return shapes.find(id) != shapes.end();
	}

	Circle& getCircle(u32 circleId) {
		assert(doesShapeExist(circleId));
		assert(shapes[circleId].index() == circleIndex);
		return std::get<Circle>(shapes[circleId]);
	}

	Polygon& getPolygon(u32 polygonId) {
		assert(doesShapeExist(polygonId));
		assert(shapes[polygonId].index() == polygonIndex);
		return std::get<Polygon>(shapes[polygonId]);
	}

	Shape& getShape(u32 shapeId) {
		Shape* base;
		std::visit([&](auto&& data) {
			base = &data;
			}, shapes[shapeId]);

		return *base;
	}

	template<typename Shape, typename ... params>
	u32 createShape(params&& ... args) {
		u32 newId = ++idCounter;
		shapes[newId].template emplace<Shape>(std::forward<params>(args)...);
		return newId;
	}

	template<typename Shape, typename ... params>
	u32 insertShape(u32 id, params&& ... args) {
		assert(!doesShapeExist(id));

		shapes[id].template emplace<Shape>(std::forward<params>(args)...);

		return id;
	}

	void eraseShape(u32 id) {
		assert(doesShapeExist(id));

		SpatialIndexElement element;
		element.shapeId = id;
		AABB aabb = getShape(id).getAABB();
		element.min = aabb.min;
		element.max = aabb.max;

		rtree.remove(element);
		shapes.erase(id);
	}

	/* Inserts a shape into the spatial tree allowing for collision detection */
	void insertShapeIntoTree(u32 id, flecs::entity flecsId) {
		Shape& shape = getShape(id);

		SpatialIndexElement element;
		element.entityId = impl::cf<u32>(flecsId);
		element.shapeId = id;
		AABB aabb = shape.getAABB();
		element.min = aabb.min;
		element.max = aabb.max;

		rtree.insert(element);
	}

	SpatialIndexTree& getTree() { return rtree; }

	void clearTree() {
		rtree.clear();
	}

private:
	enum {
		circleIndex = 0,
		polygonIndex = 1
	};

private:
	SpatialIndexTree rtree;
	boost::container::flat_map<u32, std::variant<Circle, Polygon>> shapes;
	u32 idCounter = 0;
};

AE_NAMESPACE_END
