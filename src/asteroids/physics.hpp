#pragma once
#include "includes.hpp"

AE_NAMESPACE_BEGIN

static constexpr float pi = 3.141592653589793238462643383279502884197;

/** Tiles physics
 *
 */

template<typename T>
struct Vec2 {
	union {
		T x, w;
	};

	union {
		T y, h;
	};
};

template<typename T>
T convert2DTo1D(const Vec2<T>& vec2D, T widthPerRow) {
	return (vec2D.y * widthPerRow) + vec2D.x;
}

using Vec2f = Vec2<float>;
using Vec2u = Vec2<u32>;

struct PhysicsMaterial {
	float restitution;
	float density;
	float staticFriction;
	float dynamicFriction;

	float mass;
	float rotInertia;
};

struct Tile {
	// Dimensions predefined
};

struct Rectangle {
	Vec2f dim;
};

struct Circle {
	float radius;
};

template<typename ShapeType> 
struct PhysicsShape {
	ShapeType shape;
	PhysicsMaterial material;
};

template<typename ShapeType> 
float getArea(const ShapeType& shape) { return 0.0f; } 

template<>
inline float getArea<Vec2f>(const Vec2f& vec) {
	return vec.x * vec.y;
}

template<>
inline float getArea<Rectangle>(const Rectangle& rect) {
	return getArea<Vec2f>(rect.dim);
}

template<>
inline float getArea<Circle>(const Circle& circle) {
	return circle.radius * circle.radius * pi;
}

struct TileRigidBody {
	Tile& createTile(u32 x, u32 y) {
		u32 coord = convert2DTo1D({ x, y }, rowWidth);

		assert(y < columnHeight);
		assert(!shapes.contains(coord));

		shapes.insert({ coord, PhysicsShape<Tile>() });
		PhysicsMaterial& tile = shapes[coord].material;

		float area = getArea<Vec2f>({tileWidth, tileHeight});
		float mass = area * tile.density;

		return shapes[coord].shape;
	}
	
	void destroyTile(u32 x, u32 y) {
		u32 coord = convert2DTo1D({ x, y }, rowWidth);

		assert(shapes.contains(coord));

		shapes.erase(coord);
	}

public: // tile stuff
	float tileWidth = 1.0f;
	float tileHeight = 1.0f;
	u32 rowWidth = 100;
	u32 columnHeight = 100;
	impl::FastMap<u32, PhysicsShape<Tile>> shapes;

public: // Transform et al.
	Vec2f pos;
	float rot;
	Vec2f centroid;
	Vec2f com; // center of mass
	
	Vec2f linearVelocity;
	float mass;
	float angularVelocity;
	float rotInertia;
};

// Think:
//	- How do we create *cohesive* interface between all the body types?
//		a) How do these body types collide with eachother?
//		b) How fast are we able to make this?
// 
//

// RigidBody<Circle> circles;
// RigidBody<Rectangle> rectangles;
// RigidBody<Tile> tiles;
//
// linkBodies(circles, rectangles, tiles)
// 
//

class PhysicsWorld {
public:


private:

};

AE_NAMESPACE_END
