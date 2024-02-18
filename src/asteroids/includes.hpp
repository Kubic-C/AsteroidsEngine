#pragma once

#include <iostream>
#include <queue>
#include <algorithm>
#include <random>
#include <fstream>
#include <functional>
#include <variant>
#include <bitset>
#include <set>

// Networking
#include <steam/isteamnetworkingsockets.h>
#include <steam/isteamnetworkingmessages.h>
#include <steam/isteamnetworkingutils.h>
#include <steam/steamnetworkingsockets.h>

// Serialization
#include <bitsery/bitsery.h>
#include <bitsery/adapter/buffer.h>
#include <bitsery/traits/vector.h>
#include <bitsery/traits/deque.h>
#include <bitsery/traits/list.h>
#include <bitsery/traits/string.h>
#include <bitsery/traits/array.h>
#include <bitsery/ext/inheritance.h>

// User I/O
#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>

// Spatial Partitioning for physics
#include <RTree.h>

// ECS
#include <flecs.h>

// GUI
#include <TGUI/TGUI.hpp>
#include <TGUI/Backend/SFML-Graphics.hpp>

// I am extremely lazy
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef int64_t  i64;

#ifndef NODISCARD 
#define NODISCARD [[nodiscard]]
#endif

#define AE_NAME AsteroidsEngine
#define AE_BUILD 1

// to stop auto-formatting
#define AE_NAMESPACE_NAME ae
#define AE_NAMESPACE_BEGIN namespace AE_NAMESPACE_NAME {
#define AE_NAMESPACE_END }

AE_NAMESPACE_BEGIN

template<typename T>
class IndirectContainer {
public:
	template<typename int_t>
	IndirectContainer(int_t size, T* data)
		: m_data(data), m_size((size_t)size) {}

	IndirectContainer(const IndirectContainer<T>& container) {
		m_data = container.m_data;
		m_size = container.m_size;
	}

	T* data() const {
		return m_data;
	}

	size_t size() const {
		return m_size;
	}

	size_t empty() const {
		return m_size == 0;
	}

	T* begin() { return data(); }
	T* end() { return data() + size(); }
	T* cbegin() const { return data(); }
	T* cend() const { return data() + size(); }

	template<typename int_t>
	T& operator[](int_t i) {
		static_assert(std::is_integral_v<int_t>);
		assert(i < (int_t)m_size);
		return m_data[i];
	}

	template<typename int_t>
	T& operator[](int_t i) const {
		static_assert(std::is_integral_v<int_t>);
		assert(i < (int_t)m_size);
		return m_data[i];
	}

private:
	T* m_data = nullptr;
	size_t m_size = 0;
};

extern flecs::world& getEntityWorld();

namespace impl {
	// Clears the extra fields of an entity id. Clear Field (cf)
	inline u32 cf(u64 id) {
		return id & ECS_ENTITY_MASK;
	}

	// Adds back the extra fields of an entity id. Add Field(af)
	inline flecs::entity af(u32 id) {
		return getEntityWorld().get_alive(id);
	}
}

AE_NAMESPACE_END
