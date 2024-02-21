#pragma once

#include "logging.hpp"
#include "physics.hpp"

namespace bitsery {
	template<typename S>
	void serialize(S& s, u64& o) {
		s.value8b(o);
	}

	template<typename S>
	void serialize(S& s, u32& o) {
		s.value4b(o);
	}

	template<typename S>
	void serialize(S& s, u16& o) {
		s.value2b(o);
	}
	
	template<typename S>
	void serialize(S& s, u8& o) {
		s.value1b(o);
	}

	template<typename S>
	void serialize(S& s, sf::Vector2f& v) {
		s.value4b(v.x);
		s.value4b(v.y);
	}

	template<typename S>
	void serialize(S& s, sf::Color& v) {
		s.value1b(v.r);
		s.value1b(v.g);
		s.value1b(v.b);
		s.value1b(v.a);
	}
}

AE_NAMESPACE_BEGIN

/**
 * MessageBuffer is a simple dynamic array where elements cannot be removed, only added.
 * 
 * Its purpose is to comply with the GameNetworkingSockets interface. Unlike std::vector<>
 * MessageBuffer can let go ownership of the data. This is important
 * because when sending a message that data must be valid for the entirety of it until
 * its free callback is called (SteamNetworkingMessage::m_pfnFreeData). std::vector<>
 * only allows ownership to be transferred through move semantics and not raw pointers and
 * SteamNetworkingMessage uses raw pointers and not STL containers.
 */
struct MessageBuffer {
public:
	MessageBuffer()
		: capacity(0), size(0), data(nullptr), hasOwnership(true) {
		allocateIfNoData(128);
	}

	// data must be a pointer allocated to on the heap
	// Note: MessageBuffer will MOT assume ownership over the data
	MessageBuffer(size_t size, u8* data)
		: capacity(size), size(size), data(data), hasOwnership(false) {
	}


	MessageBuffer(MessageBuffer&& other) noexcept
		: capacity(other.capacity), size(other.size), data(other.data), hasOwnership(true) {
		other.setOwner(false);
		other.reset();
	}

	~MessageBuffer() {
		if (hasOwnership && data) {
			delete[] data;
		}
	}

	NODISCARD size_t getSize() const { return size; }
	NODISCARD const u8* getData() const { return data; }
	u8* getData() { return data; }
	NODISCARD bool isOwner() const { return hasOwnership; }

	// will this resource automatically be free'd?
	void setOwner(bool isOwner) {
		hasOwnership = isOwner;
	}

	void setData(size_t newSize, u8* newData, bool isOwner = true) {
		this->capacity = newSize;
		this->size = newSize;
		this->data = newData;
		this->hasOwnership = isOwner;
	}

	void resize(size_t newSize) {
		if (!hasOwnership)
			log(ERROR_SEVERITY_FATAL, "Attempt to resize a MessageBuffer that isn't an owner\n");

		if (allocateIfNoData(newSize)) {
			size = newSize;
			return;
		}

		// if the requested size is more then the current capacity
		// resize to match it and copy over old data
		if (newSize > capacity) {
			size_t newCapacity = newSize * 2;
			u8* oldData = data;
			data = new u8[newCapacity];

			memcpy(data, oldData, size);
			capacity = newCapacity;
			size = newSize;
			delete[] oldData;
		}
		else {
			size = newSize;
		}
	}

	void addSize(size_t additionalSize) {
		resize(size + additionalSize);
	}

	// will reset all members and will delete data if it has ownership.
	void reset() {
		if (hasOwnership && data) {
			delete[] data;
		}

		setData(0, nullptr, true);
	}

	void clear() {
		if (!hasOwnership)
			log(ERROR_SEVERITY_FATAL, "Attempt to clear a MessageBuffer that isn't an owner\n");
		size = 0;
	}

	u8& operator[](size_t index) {
		assert(data);
		assert(index < size - 1);

		return data[index];
	}

public: // for std
	u8* begin() { return data; }
	u8* end() { return data + size; }

protected:
	// Will allocate if data is nullptr
	//	size: the new capacity
	// note: this->size will be set to zero
	bool allocateIfNoData(size_t newSize) {
		if (data)
			return false;

		data = new u8[newSize];
		capacity = newSize;
		size = 0;
		hasOwnership = true;
		return true;
	}

private:
	size_t capacity; // Optimizations for capacity not yet added
	size_t size;
	u8* data;
	bool hasOwnership;
};

typedef ::bitsery::OutputBufferAdapter<MessageBuffer> OutputAdapter;
typedef ::bitsery::Serializer<OutputAdapter> Serializer;

typedef ::bitsery::InputBufferAdapter<MessageBuffer> InputAdapter;
typedef ::bitsery::Deserializer<InputAdapter> Deserializer;

AE_NAMESPACE_END

namespace bitsery::traits {
	template<>
	struct BufferAdapterTraits<::ae::MessageBuffer> {
		using TIterator = uint8_t*;
		using TConstIterator = const uint8_t*;
		using TValue = uint8_t;

		static void increaseBufferSize(::ae::MessageBuffer& buffer, size_t currOffset, size_t newOffset) {
			buffer.addSize((u32)newOffset - (u32)currOffset);
		}
	};

	template<>
	struct ContainerTraits<::ae::MessageBuffer> {
		using TValue = uint8_t;

		static constexpr bool isResizable = true;
		static constexpr bool isContiguous = true;

		static void resize(::ae::MessageBuffer& buffer, size_t newSize) {
			buffer.resize(newSize);
		}

		// get container size
		static size_t size(const ::ae::MessageBuffer& buf) {
			return buf.getSize();
		}
	};
}

AE_NAMESPACE_BEGIN

inline Serializer startSerialize(MessageBuffer& buffer) {
	Serializer ser(buffer);
	return ser;
}

inline void endSerialize(Serializer& ser, MessageBuffer& buffer) {
	ser.adapter().flush();
	buffer.resize(ser.adapter().writtenBytesCount());
}

inline Deserializer startDeserialize(u32 size, const void* data) {
	Deserializer des(InputAdapter((const u8*)data, (size_t)size));
	return des;
}

inline bool endDeserialize(Deserializer& des) {
	InputAdapter& adapter = des.adapter();
	return adapter.error() == bitsery::ReaderError::NoError && adapter.isCompletedSuccessfully();
}

enum MessageHeader: u8 {
	MESSAGE_HEADER_INVALID = 0,
	MESSAGE_HEADER_DELTA_SNAPSHOT,
	MESSAGE_HEADER_REQUEST_FULL_SNAPSHOT,
	MESSAGE_HEADER_FULL_SNAPSHOT,
	MESSAGE_HEADER_CORE_LAST // it is named core in the case end-users also want to have multiple MessageHeader enums
};

template<typename S>
void serialize(S& s, MessageHeader& header) {
	s.value1b(header);
}

namespace impl {
	ISteamNetworkingUtils* getUtils();
	ISteamNetworkingSockets* getSockets();
	extern float getTickRate();

	struct MessageBufferMeta {
		u32 messagesSent = 0;
		u32 messagesFreed = 0;
	};
}

/**
 * An abstract interface defining how connections and incoming messages are dealt with.
 */
class NetworkInterface {
	friend class NetworkManager;
public:
	virtual ~NetworkInterface() = default;

	virtual void update() {};
	virtual bool shouldAcceptConnection(HSteamNetConnection conn) { return true; }
	virtual void onConnectionJoin(HSteamNetConnection conn) {}
	virtual void onConnectionLeave(HSteamNetConnection conn) {}
	virtual void onMessageRecieved(HSteamNetConnection conn, MessageHeader header, Deserializer& deserializer) {}

	virtual bool isOpen() = 0;
	virtual bool hasFailed() = 0;

protected:
	virtual bool open(const SteamNetworkingIPAddr& addr, const SteamNetworkingConfigValue_t& opt) = 0;
	virtual void acceptConnection(HSteamNetConnection conn) = 0;
	
	virtual void closeConnection(HSteamNetConnection conn) {
		impl::getSockets()->CloseConnection(conn, 0, nullptr, false);
	}

	virtual void close() = 0;

	/* This used for snapshotBegin and snapshotEnd in servers 
	   and applying recieved snapshots by the tick in clients */
	virtual void beginTick() {}
	virtual void endTick() {}

public:
	// returns true if the message can't be used
	virtual bool _internalOnMessageRecieved(HSteamNetConnection conn, MessageHeader header, Deserializer& deserializer) {
		return true;
	}

	virtual void _internalOnConnectionJoin(HSteamNetConnection conn) {}

	virtual void _internalUpdate() {}
};

class NetworkManager;
NetworkManager& getNetworkManager();

/**
 * A high level interface for network management.
 * 
 * Its purpose is to simplify network managament, meaning: sending messages, recieving messages, 
 * accepting connections, closing connections, etc. It makes code more reusable between server and client
 * implementations.
 * 
 * The details behind how connections are dealt with, is done through NetworkInterface.
 */
class NetworkManager {
public:
	NetworkManager() {
		pollGroup = impl::getSockets()->CreatePollGroup();
		if(pollGroup == k_ESteamNetConnectionEnd_Invalid)
			log(ERROR_SEVERITY_FATAL, "Unable to create poll group?\n");
	}

	~NetworkManager() {
		if(networkInterface) {
			if(networkInterface->isOpen()) {
				close();
			}

			networkInterface = nullptr;
		}

		impl::getSockets()->DestroyPollGroup(pollGroup);
	}

	void setNetworkInterface(std::shared_ptr<NetworkInterface> newInterface) {
		networkInterface = std::move(newInterface);
	}

	NetworkInterface& getNetworkInterface() const {
		return *networkInterface;
	}

	template<typename T>
	T& getNetworkInterface() const {
		return dynamic_cast<T&>(*networkInterface);
	}


	bool hasNetworkInterface() const {
		return (bool)networkInterface;
	}

	template<typename T> 
	bool hasNetworkInterface() const {
		if(!hasNetworkInterface())
			return false;

		return dynamic_cast<T*>(&*networkInterface) != nullptr;
	}

	/**
	 * Will send a message containing "data" to the connection "who"
	 * 
	 * "data" must have a messageHeader as the first serialized object.
	 * 
	 * If sendAll is true, all connections will be sent the message aside from "who." "who" may be zero if sendAll is true to send to all clients.
	 * If sendReliable is true, it is ensured that the client(s) will recieve the message, but speed may be sacrificed.
	 */
	void sendMessage(HSteamNetConnection who, MessageBuffer&& messageBuffer_, bool sendAll = false, bool sendReliable = false) {
		MessageBuffer messageBuffer = std::move(messageBuffer_);
		assert(messageBuffer.isOwner() && messageBuffer.getData());

		int steamMessageFlags = 0;
		if(sendReliable) {
			steamMessageFlags = k_nSteamNetworkingSend_Reliable | k_nSteamNetworkingSend_AutoRestartBrokenSession;
		} else {
			steamMessageFlags = k_nSteamNetworkingSend_Unreliable;
		}

		stats.writtenBytes += messageBuffer.getSize();

		EResult result = k_EResultOK;
		if(sendAll) {

			// MessageBufferMeta will keep track how many messages
			// need to be sent and how many have been free'd.
			// This allows multiple messages to use the same buffer.
			// (check message.m_pfnFreeData below for implementation)
			//
			// Note: I haven't checked the performance gain of only one MessageBuffer.
			// But I think it is reasonable to assume that:
			// ---
			// 2 calls on the heap (One for the MessageBuffer and one MessageBufferMeta) 
			// - Vs. -
			// AT A MINIMUM 1 heap call but possibly up whatever the max connections is PLUS copying the contents of MessageBuffer everwhere
			// ---
			// that the former is much faster.
			auto meta = new impl::MessageBufferMeta;
			messageBuffer.setOwner(false);

			for (auto& pair : connections) {
				if (pair.first == who)
					continue;

				networkingMessages.push_back(impl::getUtils()->AllocateMessage(0));
				ISteamNetworkingMessage& message = *networkingMessages.back();

				message.m_conn = pair.first;
				message.m_cbSize = (int)messageBuffer.getSize();
				message.m_pData = (void*)messageBuffer.getData();
				message.m_nFlags = steamMessageFlags;
				message.m_nUserData = (int64)meta;

				message.m_pfnFreeData = 
					[](ISteamNetworkingMessage* message){
						auto meta = (impl::MessageBufferMeta*)message->m_nUserData;

						meta->messagesFreed++;
						if(meta->messagesFreed == meta->messagesSent) {
							delete[] (u8*)message->m_pData;
							delete meta;						
						}
					};
				
				meta->messagesSent++;
			}

			if(networkingMessages.empty())
				return;

			std::vector<int64_t> results;
			results.resize(networkingMessages.size());

			impl::getSockets()->SendMessages((int)networkingMessages.size(), networkingMessages.data(), (int64*)results.data());
			networkingMessages.clear();
		
			for(int64_t messageResult : results) {
				if(messageResult < 0) {
					result = (EResult)-messageResult;
					break;
				}
			}
		
		} else {
			if(connections.find(who) == connections.end())
				log(ERROR_SEVERITY_FATAL, "Cannot send a message to an invalid connection: %u\n", who);

			result = impl::getSockets()->SendMessageToConnection(who, messageBuffer.getData(), (u32)messageBuffer.getSize(), steamMessageFlags, nullptr);
		}

		if(result != k_EResultOK) {
			log(ERROR_SEVERITY_WARNING, "Failed to send message: %i\n", result);
		}
	}

	void update() {
		if(!hasNetworkInterface())
			return;

		impl::getSockets()->RunCallbacks();

		ISteamNetworkingMessage* message = nullptr;
		while(impl::getSockets()->ReceiveMessagesOnPollGroup(pollGroup, &message, 1)) {
			Deserializer des = startDeserialize(message->GetSize(), message->GetData());
			MessageHeader header = MESSAGE_HEADER_INVALID;

			stats.readBytes += (size_t)message->GetSize();

			des.object(header);
			if(networkInterface->_internalOnMessageRecieved(message->m_conn, header, des))
				networkInterface->onMessageRecieved(message->m_conn, header, des);
			
			if(!endDeserialize(des)) {
				log(ERROR_SEVERITY_WARNING, "Deserialization failed: (bitsery::ReaderError)%i\n", (int)des.adapter().error());
				connectionAddWarning(message->m_conn);
			}

			message->Release(); // No need for this message anymore
		}

		networkInterface->_internalUpdate();
		networkInterface->update();
	}

	bool open(const SteamNetworkingIPAddr& addr) {
		if(!networkInterface)
			log(ERROR_SEVERITY_FATAL, "Before using NetworkManager::open(), the network interface must be set\n");

		SteamNetworkingConfigValue_t opt = {};
		opt.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged, (void*)handleConnectionChange);
		
		return networkInterface->open(addr, opt);
	}

	void close() {
		for(auto& pair: connections) {
			networkInterface->closeConnection(pair.first);
		}

		networkInterface->close();
	}

	void beginTick() {
		if(!hasNetworkInterface())
			return;

		networkInterface->beginTick();
	}

	void endTick() {
		if (!hasNetworkInterface())
			return;

		networkInterface->endTick();
	}

	// Adds a warning to a connection
	void connectionAddWarning(HSteamNetConnection conn) {
		connections[conn].warnings++;
		if (connections[conn].warnings > maxWarnings) {
			onConnectionLeave(conn);
			log(ERROR_SEVERITY_WARNING, "Connection <red>exceeded maxWarnings<reset> and was forcibily disconnected\n");
		}
	}

	NODISCARD size_t getWrittenByteCount() const {
		return stats.writtenBytes;
	}

	NODISCARD size_t getReadByteCount() const {
		return stats.readBytes;
	}

	/* resets read byte count and written byte count to zero. */
	void clearStats() {
		stats.readBytes = 0;
		stats.writtenBytes = 0;
	}

protected:
	struct Stats {
		size_t writtenBytes = 0;
		size_t readBytes = 0;
	} stats;

protected:
	static void handleConnectionChange(SteamNetConnectionStatusChangedCallback_t* info) {
		NetworkManager& manager = getNetworkManager();

		switch (info->m_info.m_eState)
		{
		case k_ESteamNetworkingConnectionState_None:
			break;

		case k_ESteamNetworkingConnectionState_ClosedByPeer:
		case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
			manager.onConnectionLeave(info->m_hConn);
			break;

		case k_ESteamNetworkingConnectionState_Connecting:
			manager.onConnectionIncoming(info->m_hConn);
			break;

		case k_ESteamNetworkingConnectionState_Connected:
			manager.onConnectionJoin(info->m_hConn);
			break;

		default:
			break;
		}
	}

	void onConnectionIncoming(HSteamNetConnection conn) {
		if(!networkInterface->shouldAcceptConnection(conn))
			return;

		connections.insert(std::pair(conn, ConnectionData{}));

		networkInterface->acceptConnection(conn);
	}

	void onConnectionJoin(HSteamNetConnection conn) {
		impl::getSockets()->SetConnectionPollGroup(conn, pollGroup);
		
		networkInterface->_internalOnConnectionJoin(conn);
		networkInterface->onConnectionJoin(conn);
	}

	void onConnectionLeave(HSteamNetConnection conn) {
		networkInterface->onConnectionLeave(conn);
		networkInterface->closeConnection(conn);
		connections.erase(conn);
	}

protected:
	static constexpr u32 maxWarnings = 5;

	struct ConnectionData {
		// Connections have "warnings."
		// If the connection were to assume some suspicous behaviour i.e. any of the following:
		//	- Sending malformed messages
		//  - Attempting to impersonate another connection
		//  - etc.
		//  it will recieve a warning.
		// If a connection exceeds the maxWarnings, it will be forcibly disconnected.
		u32 warnings = 0; 
	};

	HSteamNetPollGroup pollGroup;
	std::vector<ISteamNetworkingMessage*> networkingMessages;
	std::unordered_map<HSteamNetConnection, ConnectionData> connections;
	std::shared_ptr<NetworkInterface> networkInterface;
};

/* Network Syncing */

struct NetworkedEntity {};
struct NetworkedComponent {};

enum class ComponentPiority {
	// Connected clients are ensured to recieve the updates of this component
	High,
	// Connected clients are NOT ensured to recieve the updates of this component. 
	// Component update could be lost due to packet loss.
	Low 
};

struct NoPhase {};

struct ShapeComponent : public NetworkedComponent {
	u32 shape = UINT32_MAX;

	NODISCARD bool isValid() const {
		return shape != UINT32_MAX && getPhysicsWorld().doesShapeExist(shape);
	}

	template<typename S>
	void serialize(S& s) {
		s.value4b(shape);
	}
};

extern u64 getCurrentTick();
extern u64 getCurrentStateId();
extern void transitionState(u64 id, bool immediate);

namespace impl {
	enum SnapshotFlags : u8 {
		STATE = 1 << 0,
		PHYSICS_SNAPSHOT = 1 << 1,
		META_DATA_SNAPSHOT = 1 << 2,
		COMPONENT_UPDATE_SNAPSHOT = 1 << 3,
		LOW_PIORITY = 1 << 4 // Does this snapshot contain low piority data?
	};

	template<typename K, typename T>
	using FastMap = boost::container::flat_map<K, T>;
}

template<typename S>
void serialize(S& s, impl::SnapshotFlags& flags) {
	s.value1b(flags);
}

class NetworkStateManager;

NetworkStateManager& getNetworkStateManager();

/**
 * @brief the network state manager is responsible for creating snapshots
 * of the application's current state that can be sent to clients. It is also
 * used for deserilizing these snapshots client side.
 */
class NetworkStateManager {
	using ListSize = u32;
	using CompId = u32;
	using EntityId = u32;
	using PhysicsId = u32;

public:
	NetworkStateManager() {
		auto& world = getEntityWorld();
		
		registerComponent<NetworkedEntity>();
		world.add<NetworkedEntity>();

		flecs::entity addObserver = world.observer()
			.term<NetworkedEntity>()
			.event(flecs::OnRemove).each([this](flecs::entity e) {
				deltaSnapshot.resetEntity(impl::cf<EntityId>(e));
				deltaSnapshot.metaData.removeEntities.insert(impl::cf<EntityId>(e));
			});

		allDeltaSnapshotSystems.push_back(addObserver);

		flecs::entity getAllBodiesSystem = world.system<ShapeComponent>().kind<NoPhase>().each([this](ShapeComponent& shapeId){
			if(!shapeId.isValid())
				return;

			Shape& shape = getPhysicsWorld().getShape(shapeId.shape);
			fullSnapshot.physicsSnapshot.bodiesToUpdate[shape.getType()].push_back(shapeId.shape);
		});

		fullSnapshotSystems.push_back(getAllBodiesSystem);
	}

	// lets us know that the user state has changed
	void userStateChanged() {
		deltaSnapshot.state = getCurrentStateId();
	}

	~NetworkStateManager() {
		for(flecs::entity observer : allDeltaSnapshotSystems) {
			observer.destruct();
		}

		for (flecs::entity system : fullSnapshotSystems) {
			system.destruct();
		}
	}

	std::string getNetworkedEntityInfo() {
		std::string info;

		flecs::world& world = getEntityWorld();

		auto q = world.query_builder().term<NetworkedEntity>().build();
	
		std::vector<flecs::entity> entities;
		q.iter([&](flecs::iter& iter) {
			for(auto i : iter) {
				entities.push_back(iter.entity(i));
			}
		});
		q.destruct();


		for(flecs::entity entity : entities) {
			info += formatString("<bold>Entity %u %u<reset>\n", impl::cf<u32>(entity.id()), (u32)ECS_GENERATION(entity.id()));

			entity.each([&](flecs::id comp) {
				info += formatString("\t%s - %u\n", comp.str().c_str(), impl::cf<u32>(comp.raw_id()));
			});
		}

		return info;
	}

	flecs::entity entity() {
		return ae::getEntityWorld().entity().add<NetworkedEntity>();
	}

	flecs::entity enable(flecs::entity e) {
		e.enable();
		deltaSnapshot.needActive(e, MetaDataSnapshot::DO_ENABLE);
		return e;
	}

	flecs::entity disable(flecs::entity e) {
		e.disable();
		deltaSnapshot.needActive(e, MetaDataSnapshot::DO_DISABLE);
		return e;
	}

	template<typename ComponentType>
	void registerComponent(ComponentPiority piority = ComponentPiority::Low) {
		auto& entityWorld = getEntityWorld();

		flecs::entity component = entityWorld.component<ComponentType>();
		CompId id = impl::cf<CompId>(component); 

		ComponentInfo& info = registeredComponents[id];

		info.piority = piority;

		registerComponentInfo<ComponentType>(id, piority, std::is_empty<ComponentType>());

		// Adding and Destroying component type 
		flecs::entity addObserver = entityWorld.observer().term<ComponentType>().event(flecs::OnAdd).each([this, id](flecs::entity entity){
			deltaSnapshot.needAdd(entity, id);
		});

		flecs::entity removeObserver = entityWorld.observer().term<ComponentType>().event(flecs::OnRemove).each([this, id](flecs::entity entity) {
			deltaSnapshot.needRemove(entity, id);
		});

		allDeltaSnapshotSystems.push_back(addObserver);
		allDeltaSnapshotSystems.push_back(removeObserver);
	}

private:
	template<typename TagType>
	void registerComponentInfo(CompId id, ComponentPiority piority, std::true_type isEmpty) {
		ComponentInfo& info = registeredComponents[id];

		info.ser = nullptr;
		info.des = nullptr;

		flecs::entity fullsnapshotTagAdd = getEntityWorld().system().term<TagType>().template kind<NoPhase>().each([this, id](flecs::entity entity) {
			fullSnapshot.tags[impl::cf<EntityId>(entity)].insert(id);
		});

		fullSnapshotSystems.push_back(fullsnapshotTagAdd);
	}

	template<typename ComponentType>
	void registerComponentInfo(CompId id, ComponentPiority piority, std::false_type isEmpty) {
		auto& entityWorld = getEntityWorld();
		ComponentInfo& info = registeredComponents[id];

		info.ser =
			[](Serializer& ser, const void* data) {
				ser.object(*(const ComponentType*)data);
			};

		info.des =
			[](Deserializer& des, void* data) {
				des.object((ComponentType&)*(ComponentType*)data);
			};

		flecs::entity addObserver = entityWorld.observer().term<ComponentType>().event(flecs::OnAdd).each([this, id](flecs::entity entity) {
			deltaSnapshot.needUpdate(entity, id, registeredComponents);
		});
		flecs::entity setObserver = entityWorld.observer().term<ComponentType>().event(flecs::OnSet).each([this, id](flecs::entity entity) {
			deltaSnapshot.needUpdate(entity, id, registeredComponents);
		});

		allDeltaSnapshotSystems.push_back(addObserver);
		allDeltaSnapshotSystems.push_back(setObserver);

		flecs::entity fullsnapshotComponentAdd = entityWorld.system().term<ComponentType>().template kind<NoPhase>().each([this, id](flecs::entity entity) {
			fullSnapshot.components[impl::cf<EntityId>(entity)].insert(id);
		});

		fullSnapshotSystems.push_back(fullsnapshotComponentAdd);
	}

public:
	/*
	 * @brief Create a delta compresesed snapshot that may be sent to clients.
	 * @param reliableBuffer Data that must be sent reliably (meaning no or minimal packet loss)
	 * @param unreliableBuffer Data that can be sent unreliably and will not
	 * heavily impact the client when recieved.
	 */
	void createDeltaSnapshot(MessageBuffer& reliableBuffer, MessageBuffer& unreliableBuffer) {
		/* RELIABLE MESSAGE */
		deltaSnapshot.checkForDirtyShapes();

		deltaSnapshot.flags = 0;
		if(deltaSnapshot.state != 0)
			deltaSnapshot.flags |= impl::STATE;
		if(deltaSnapshot.metaData.canSerialize())
			deltaSnapshot.flags |= impl::META_DATA_SNAPSHOT;
		if(deltaSnapshot.physicsSnapshot.canSerialize())
			deltaSnapshot.flags |= impl::PHYSICS_SNAPSHOT;
		if(deltaSnapshot.componentData[(int)ComponentPiority::High].canSerialize())
			deltaSnapshot.flags |= impl::COMPONENT_UPDATE_SNAPSHOT;

		Serializer ser = startSerialize(reliableBuffer);
		// HEADER
		ser.object(MESSAGE_HEADER_DELTA_SNAPSHOT);
		ser.object(deltaSnapshot.flags);
		// State
		if(deltaSnapshot.flags & impl::STATE) {
			ser.object(getCurrentStateId());
			deltaSnapshot.state = 0;
		}
		// Meta Data
		if(deltaSnapshot.flags & impl::META_DATA_SNAPSHOT) {
			MetaDataSnapshot& metaData = deltaSnapshot.metaData;
			serializeSet(ser, metaData.removeEntities);
			sortByArchetypes(metaData.toAdd);
			serializeArchetypes(ser, cache.archetypeMap, nullptr);
			sortByArchetypes(metaData.toRemove);
			serializeArchetypes(ser, cache.archetypeMap, nullptr);
			serializeMap(ser, metaData.toUpdateActive);
		}
		// Physics Data
		if(deltaSnapshot.flags & impl::PHYSICS_SNAPSHOT) {
			auto& physicsWorld = getPhysicsWorld();
			PhysicsSnapshot& physicsData = deltaSnapshot.physicsSnapshot;
			serializePhysicsMap(ser, physicsData.bodiesToUpdate, [&](Serializer& ser, ShapeEnum shapeEnum, PhysicsId id) {
				switch (shapeEnum) {
				case ShapeEnum::Circle:
					ser.object(physicsWorld.getCircle((u32)id));
					break;
				case ShapeEnum::Polygon:
					ser.object(physicsWorld.getPolygon((u32)id));
					break;

				default:
					assert(!"Invalid shape enum");
					break;
				}
			});
		}
		// High Piortiy Component Updates
		if(deltaSnapshot.flags & impl::COMPONENT_UPDATE_SNAPSHOT) {
			sortByArchetypes(deltaSnapshot.componentData[(int)ComponentPiority::High].toUpdate);
			serializeArchetypes(ser, cache.archetypeMap, [&](Serializer& ser, EntityId entityId, CompId compId){
				flecs::entity entity = impl::af(entityId);

				assert(entity.is_alive());

				registeredComponents[compId].ser(ser, entity.get(compId));
			});
		}
		endSerialize(ser, reliableBuffer);

		/* UNRELIABLE MESSAGE */
		deltaSnapshot.flags = impl::LOW_PIORITY;
		if(deltaSnapshot.componentData[(int)ComponentPiority::Low].canSerialize())
			deltaSnapshot.flags |= impl::COMPONENT_UPDATE_SNAPSHOT;

		ser = startSerialize(unreliableBuffer);
		// Header
		ser.object(MESSAGE_HEADER_DELTA_SNAPSHOT);
		ser.object(deltaSnapshot.flags);
		// Low Piortiy Component Updates
		if(deltaSnapshot.flags & impl::COMPONENT_UPDATE_SNAPSHOT) {
			sortByArchetypes(deltaSnapshot.componentData[(int)ComponentPiority::Low].toUpdate);
			serializeArchetypes(ser, cache.archetypeMap, [&](Serializer& ser, EntityId entityId, CompId compId) {
				flecs::entity entity = impl::af(entityId);

				registeredComponents[compId].ser(ser, entity.get(compId));
			});
		}
		endSerialize(ser, unreliableBuffer);

		// cleanup ...
		deltaSnapshot.resetAll();
	}

	/**
	 * @brief Updates the games current state with a delta snapshot
	 */
	void updateWithDeltaSnapshot(Deserializer& des) {
		auto& entityWorld = getEntityWorld();
		auto& physicsWorld = getPhysicsWorld();

		entityWorld.enable_range_check(false);

		u8 flags;
		des.object(flags);

		if(flags & impl::STATE) {
			u64 stateId;
			des.object(stateId);
			transitionState(stateId, true);
		}
		if(flags & impl::META_DATA_SNAPSHOT) {
			// Entities to kill
			deserializeSet<EntityId>(des, [&](EntityId id){
				flecs::entity toDestroy = getEntityWorld().ensure(id);

				toDestroy.destruct();
			});

			// Components to add
			deserializeArchetypes(des, [](Deserializer& des, flecs::entity entity, CompId compId){
				entity.add(compId);
			});
			
			// Components to remove
			deserializeArchetypes(des, [](Deserializer& des, flecs::entity entity, CompId compId) {
				entity.remove(compId);
			});

			// Disable or enable entities
			deserializeMap<EntityId, u8>(des, [](EntityId id, u8 activeFlags){
				flecs::entity entity = impl::af(id);

				assert(entity.id() != 0);
				assert(activeFlags);

				if(activeFlags & MetaDataSnapshot::ActiveFlags::DO_ENABLE) {
					entity.enable();
				} else {
					entity.disable();
				}
			});
			
		}
		if (flags & impl::PHYSICS_SNAPSHOT) {
			deserializePhysicsMap(des, [&](Deserializer& des, ShapeEnum shapeEnum, PhysicsId shortId){
				u32 id = (u32)shortId;

				switch (shapeEnum) {
				case ShapeEnum::Circle:
					if(physicsWorld.doesShapeExist(id))
						des.object(physicsWorld.getCircle(id));
					else
						des.object(physicsWorld.getCircle(physicsWorld.insertShape<Circle>(id)));
					break;
				case ShapeEnum::Polygon:
					if (physicsWorld.doesShapeExist(id))
						des.object(physicsWorld.getPolygon(id));
					else
						des.object(physicsWorld.getPolygon(physicsWorld.insertShape<Polygon>(id)));
					break;

				default:
					assert(!"Invalid shape enum");
					break;
				}

				physicsWorld.getShape(id).markLocalDirty();
			});

		}
		if (flags & impl::COMPONENT_UPDATE_SNAPSHOT) {
			deserializeArchetypes(des, [&](Deserializer& des, flecs::entity entity, CompId compId) {
				registeredComponents[compId].des(des, entity.get_mut((u64)compId));
			});
		}

		entityWorld.enable_range_check(true);
	}

public:
	/**
	 * @brief Creates a full snapshot of the world
	 */
	void createFullSnapshot(MessageBuffer& buffer) {
		auto& world = getEntityWorld();
		auto& physicsWorld = getPhysicsWorld();

		for(auto system : fullSnapshotSystems) {
			world.system(system).run(0.0f);
		}

		Serializer ser = startSerialize(buffer);
		ser.object(MESSAGE_HEADER_FULL_SNAPSHOT);
		ser.object(getCurrentStateId());
		sortByArchetypes(fullSnapshot.tags);
		serializeArchetypes(ser, cache.archetypeMap, nullptr);
		sortByArchetypes(fullSnapshot.components);
		serializeArchetypes(ser, cache.archetypeMap, [&](Serializer& ser, EntityId entityId, CompId compId) {
			flecs::entity entity = impl::af(entityId);

			registeredComponents[compId].ser(ser, entity.get(compId));
		});
		serializePhysicsMap(ser, fullSnapshot.physicsSnapshot.bodiesToUpdate, [&](Serializer& ser, ShapeEnum shapeEnum, PhysicsId id) {
			switch (shapeEnum) {
			case ShapeEnum::Circle:
				ser.object(physicsWorld.getCircle((u32)id));
				break;
			case ShapeEnum::Polygon:
				ser.object(physicsWorld.getPolygon((u32)id));
				break;

			default:
				assert(!"Invalid shape enum");
				break;
			}
		});
		endSerialize(ser, buffer);

		fullSnapshot.resetAll();
	}

	/**
	 * @brief Updates the games current state with a full snapshot.
	 * This will delete all networked entities and then reconstruct
	 * the world according to the message.
	 */
	void updateWithFullSnapshot(Deserializer& des) {
		flecs::world& entityWorld = getEntityWorld();
		PhysicsWorld& physicsWorld = getPhysicsWorld();

		entityWorld.enable_range_check(false);

		entityWorld.delete_with<NetworkedEntity>();

		u64 stateId;
		des.object(stateId);
		transitionState(stateId, true);

		deserializeArchetypes(des, [](Deserializer& des, flecs::entity entity, CompId compId) {
			entity.add(compId);
		});
		deserializeArchetypes(des, [&](Deserializer& des, flecs::entity entity, CompId compId) {
			registeredComponents[compId].des(des, entity.get_mut((u64)compId));
		});
		deserializePhysicsMap(des, [&](Deserializer& des, ShapeEnum shapeEnum, PhysicsId shortId) {
			u32 id = (u32)shortId;

			switch (shapeEnum) {
			case ShapeEnum::Circle:
				if (physicsWorld.doesShapeExist(id))
					des.object(physicsWorld.getCircle(id));
				else
					des.object(physicsWorld.getCircle(physicsWorld.insertShape<Circle>(id)));
				break;
			case ShapeEnum::Polygon:
				if (physicsWorld.doesShapeExist(id))
					des.object(physicsWorld.getPolygon(id));
				else
					des.object(physicsWorld.getPolygon(physicsWorld.insertShape<Polygon>(id)));
				break;

			default:
				assert(!"Invalid shape enum");
				break;
			}

			physicsWorld.getShape(id).markLocalDirty();
		});
	
		entityWorld.enable_range_check(true);
	}

private: /* Cache things */
	template<typename T>
	using Set = std::set<T>;

	template<typename K, typename T>
	using Map = boost::container::flat_map<K, T>;

	struct Cache {
		// used when reversing maps. Helps sort entities by
		// components to update, allowing for smaller message size.
		Map<std::set<CompId>, std::vector<EntityId>> archetypeMap;
	} cache;
private: // Serialization and deserialization helper functions.
	Map<std::set<CompId>, std::vector<EntityId>>& sortByArchetypes(const Map<EntityId, Set<CompId>>& entityMap) {
		cache.archetypeMap.clear();

		for(auto& pair : entityMap) {
			cache.archetypeMap[pair.second].push_back(pair.first);
		}

		return cache.archetypeMap;
	}

	void serializeArchetypes(Serializer& ser, Map<Set<CompId>, std::vector<EntityId>>& archetypes, const std::function<void(Serializer&, EntityId, CompId)>& serCompFunc) {
		ser.object(static_cast<ListSize>(archetypes.size()));
		for(auto& archetype : archetypes) {
			serializeSet(ser, archetype.first); // Component Types
			serializeEntityComponents(ser, archetype.second, archetype.first, serCompFunc); // Entity Types
		}
	}

	void serializeEntityComponents(Serializer& ser, const std::vector<EntityId>& entities, const Set<CompId>& components, const std::function<void(Serializer&, EntityId, CompId)>& serCompFunc) {
		ser.object(static_cast<ListSize>(entities.size()));
		for(auto entity : entities) {
			ser.object(entity);

			if(serCompFunc)
				for(auto comp : components) {
					serCompFunc(ser, entity, comp);
				}
		}
	}

	void deserializeArchetypes(Deserializer& des, const std::function<void(Deserializer& des, flecs::entity, CompId)>& callback) {
		ListSize archetypeCount;
		des.object(archetypeCount);

		std::vector<CompId> comps;
		for (ListSize archetypeI = 0; archetypeI < archetypeCount; archetypeI++) {
			deserializeVector<CompId>(des, comps); // although serialized as a set, sets and vectors follow the same encoding
			deserializeEntityComponents(des, comps, callback);
		}
	}

	void deserializeEntityComponents(Deserializer& des, const std::vector<CompId>& comps, const std::function<void(Deserializer& des, flecs::entity, CompId)>& callback) {
		ListSize entityCount;
		des.object(entityCount);

		for (ListSize entityI = 0; entityI < entityCount; entityI++) {
			EntityId rawId;
			des.object(rawId);
			flecs::entity entity = getEntityWorld().ensure(rawId);

			assert(entity.id() != 0);

			for (CompId compId : comps) {
				callback(des, entity, compId);
			}
		}
	}

	void serializePhysicsMap(Serializer& ser, const Map<ShapeEnum, std::vector<PhysicsId>>& physicsMap, const std::function<void(Serializer&, ShapeEnum, PhysicsId)>& serFunc) {
		assert(serFunc);
		
		ser.object(static_cast<ListSize>(physicsMap.size()));
		for(auto& pair : physicsMap) {
			ser.object(pair.first);

			ser.object(static_cast<ListSize>(pair.second.size()));
			for(PhysicsId id : pair.second) {
				ser.object(id);

				serFunc(ser, pair.first, id);
			}
		}
	}

	void deserializePhysicsMap(Deserializer& des, const std::function<void(Deserializer& des, ShapeEnum, PhysicsId id)>& callback) {
		ListSize enumCount;
		des.object(enumCount);

		for (ListSize enumI = 0; enumI < enumCount; enumI++) {
			ShapeEnum shapeEnum;
			des.object(shapeEnum);

			ListSize idCount;
			des.object(idCount);
			for (ListSize idI = 0; idI < idCount; idI++) {
				PhysicsId id;
				des.object(id);

				callback(des, shapeEnum, id);
			}
		}
	}

	template<typename MapType>
	void serializeMap(Serializer& ser, const MapType& map) {
		ser.object(static_cast<ListSize>(map.size()));
		for (auto& pair : map) {
			ser.object(pair.first);
			ser.object(pair.second);
		}
	}

	template<typename F, typename S>
	void deserializeMap(Deserializer& des, const std::function<void(F, S)>& callback) {
		ListSize size;
		des.object(size);
		for (ListSize i = 0; i < size; i++) {
			F first;
			S second;
			des.object(first);
			des.object(second);
			callback(first, second);
		}
	}

	template<typename T>
	void serializeSet(Serializer& ser, const std::set<T>& list) {
		ser.object(static_cast<ListSize>(list.size()));
		for (auto& value : list) {
			ser.object(value);
		}
	}

	template<typename T>
	void deserializeSet(Deserializer& des, const std::function<void(T)>& callback) {
		ListSize size;
		des.object(size);
		for (ListSize i = 0; i < size; i++) {
			T object;
			des.object(object);
			callback(object);
		}
	}

	template<typename T>
	void serializeVector(Serializer& ser, const std::vector<T>& list) {
		ser.object(static_cast<ListSize>(list.size()));
		for (auto& value : list) {
			ser.object(value);
		}
	}

	template<typename T>
	void deserializeVector(Deserializer& des, const std::function<void(T)>& callback) {
		ListSize size;
		des.object(size);
		for (ListSize i = 0; i < size; i++) {
			T object;
			des.object(object);
			callback(object);
		}
	}

	template<typename T>
	void deserializeVector(Deserializer& des, std::vector<T>& vector) {
		ListSize size;
		des.object(size);
		vector.resize(size);
		for (ListSize i = 0; i < size; i++) {
			des.object(vector[i]);
		}
	}

private:
	struct ComponentInfo {
		ComponentPiority piority;
		std::function<void(Serializer& ser, const void* CompData)> ser;
		std::function<void(Deserializer& ser, void* CompData)> des;
	};

	Map<CompId, ComponentInfo> registeredComponents;

	struct MetaDataSnapshot {
		enum ActiveFlags : u8 {
			NOT_SET = 0,
			DO_ENABLE = 1 << 1,
			DO_DISABLE = 1 << 2
		};
		
		bool canSerialize() const {
			return !removeEntities.empty() ||
				   !toRemove.empty() ||
				   !toAdd.empty() ||
				   !toUpdateActive.empty();
		}

		Set<EntityId> removeEntities;
		Map<EntityId, u32> currentGens;
		Map<EntityId, Set<CompId>> toRemove;
		Map<EntityId, Set<CompId>> toAdd;
		Map<EntityId, u8> toUpdateActive;
	};

	struct ComponentSnapshot {
		bool canSerialize() const {
			return !toUpdate.empty();
		}

		Map<EntityId, Set<CompId>> toUpdate;
	};

	struct PhysicsSnapshot {
		bool canSerialize() const {
			return !bodiesToUpdate.empty();
		}

		Map<ShapeEnum, std::vector<PhysicsId>> bodiesToUpdate;
	};

	/*
	 * Assuming the client's state is the same as the previous
	 * server state (last tick), what is the minimal amount of
	 * data that needs to be sent to get from Point A to Point B?
	 *
	 * Point A: the previous tick.
	 * Point B: this tick.
	 */
	struct DeltaCompressedSnapshot {
		DeltaCompressedSnapshot() {
			shapeCompQuery = getEntityWorld().query<ShapeComponent>();
		}

		~DeltaCompressedSnapshot() {
			shapeCompQuery.destruct();
		}

		void checkForDirtyShapes() {
			PhysicsWorld& physicsWorld = getPhysicsWorld();

			shapeCompQuery.iter([&](flecs::iter& iter, ShapeComponent* shapesArray) {
				for (auto i : iter) {
					Shape& shape = physicsWorld.getShape(shapesArray[i].shape);

					if (shape.isNetworkDirty()) {
						physicsSnapshot.bodiesToUpdate[shape.getType()].push_back(shapesArray[i].shape);
						shape.resetNetworkDirty();
					}
				}
			});
		}

		void needUpdate(flecs::entity entity, CompId id, Map<CompId, ComponentInfo>& infos) {
			tryIncreaseGen(entity);
			componentData[(int)infos[id].piority].toUpdate[impl::cf<EntityId>(entity)].insert(id);
		}

		void needAdd(flecs::entity entity, CompId id) {
			tryIncreaseGen(entity);
			metaData.toAdd[impl::cf<EntityId>(entity)].insert(id);
		}

		void needRemove(flecs::entity entity, CompId id) {
			tryIncreaseGen(entity);
			metaData.toRemove[impl::cf<EntityId>(entity)].insert(id);
		}

		void needActive(flecs::entity entity, MetaDataSnapshot::ActiveFlags flags) {
			tryIncreaseGen(entity);
			metaData.toUpdateActive[impl::cf<EntityId>(entity)] = flags;
		}

		void tryIncreaseGen(flecs::entity entity) {
			assert(entity.is_alive());

			u32 idOnly = impl::cf<EntityId>(entity);
			u32 newGen = ECS_GENERATION(entity.id());

			if (metaData.currentGens.find(idOnly) == metaData.currentGens.end()) {
				metaData.currentGens[idOnly] = newGen;
				return;
			}

			if (metaData.currentGens[idOnly] != newGen) {
				resetEntity(idOnly);
				metaData.removeEntities.insert(idOnly);
				metaData.currentGens[idOnly] = newGen;
			}
		}

		void resetEntity(EntityId id) {
			componentData[(int)ComponentPiority::High].toUpdate.erase(id);
			componentData[(int)ComponentPiority::Low].toUpdate.erase(id);
			metaData.toRemove.erase(id);
			metaData.toAdd.erase(id);
			metaData.toUpdateActive.erase(id);
		}

		void resetAll() {
			metaData.removeEntities.clear();
			metaData.currentGens.clear();
			componentData[(int)ComponentPiority::High].toUpdate.clear();
			componentData[(int)ComponentPiority::Low].toUpdate.clear();
			metaData.toRemove.clear();
			metaData.toAdd.clear();
			metaData.toUpdateActive.clear();
			physicsSnapshot.bodiesToUpdate[ShapeEnum::Circle].clear();
			physicsSnapshot.bodiesToUpdate[ShapeEnum::Polygon].clear();
		}

		/* What data is inside this snapshot? */
		u8 flags;

		/* What state was active between server ticks? */
		u64 state;

		/* What entites or components were created, destroyed, enabled, or disabled between server ticks? .*/
		MetaDataSnapshot metaData;

		/* What shapes were created between server ticks? */
		PhysicsSnapshot physicsSnapshot;

		/* What was the data of the networked entities between server ticks? */
		std::array<ComponentSnapshot, 2> componentData; // use the enum ComponentPiority

		// Used to check if shape's are dirty.
		flecs::query<ShapeComponent> shapeCompQuery;
	} deltaSnapshot;

	/*
	 * A serialized version of all networked entities and their components.
	 * Everything is serialized.
	 */
	struct FullSnapshot {
		void resetAll() {
			tags.clear();
			components.clear();
			physicsSnapshot.bodiesToUpdate[ShapeEnum::Circle].clear();
			physicsSnapshot.bodiesToUpdate[ShapeEnum::Polygon].clear();
		}

		Map<EntityId, Set<CompId>> tags;
		Map<EntityId, Set<CompId>> components;
		PhysicsSnapshot physicsSnapshot;
	} fullSnapshot;

	std::vector<flecs::entity> allDeltaSnapshotSystems;
	std::vector<flecs::entity> fullSnapshotSystems;
};

/* Default network interfaces */

/**
 * @brief the client interface is one of the two main interfaces
 * library-users will use inherit from. The client interface
 * is used to connect to another application that uses the 
 * 'ServerInterface.' The client interface will handle
 * all entity and physics syncing automatically.
 */
class ClientInterface : public NetworkInterface {
public:
	virtual ~ClientInterface() = default;

	/* When an entity is by the client, it will start at this range.
	   I.E. the default client range for created entites. */
	static constexpr u64 defaultLocalEntityRange = 1000000;
	static constexpr size_t defaultMaxDsyncBeforeFullSnapshot = 30;

	ClientInterface() {
		flecs::world& world = getEntityWorld();

		world.set_entity_range(defaultLocalEntityRange, UINT64_MAX);
		world.enable_range_check(true);
	}

	/**
	 * @brief Is the client currently connected to anything?
	 * 
	 * @return true if the client is connected, false otherwise
	 */
	bool isOpen() final {
		return connected;
	}

	/**
	 * @brief Has the previous tried connection failed to connect?
	 * 
	 * @note NetworkManager::open() must be called before calling this function for
	 * its results to be relevant.
	 * 
	 * @returns true if the client failed to connect, and false otherwise
	 */
	bool hasFailed() final {
		return failed;
	}

protected:
	bool open(const SteamNetworkingIPAddr& addr, const SteamNetworkingConfigValue_t& opt) override {
		if (conn != k_HSteamNetConnection_Invalid)
			return false;

		conn = impl::getSockets()->ConnectByIPAddress(addr, 1, &opt);
		if (conn == k_HSteamNetConnection_Invalid) {
			log(ERROR_SEVERITY_WARNING, "Unable to open client socket");
			failed = true;
			return false;
		}

		// this will insure that conn gets added to the connections list within the networkManager
		// so if some function wants to send a message right after open even when we aren't technically connected
		// they can.
		getNetworkManager().update();

		connected = false;
		failed = false;

		return true;
	}

	void acceptConnection(HSteamNetConnection newConn) override {}

	void closeConnection(HSteamNetConnection oldConn) override {
		if(!connected) {
			failed = true;
		}

		connected = false;
		impl::getSockets()->CloseConnection(conn, 0, nullptr, false);
		this->conn = k_HSteamNetConnection_Invalid;
	}

	void close() override {
		conn = k_HSteamNetConnection_Invalid;
	}

	void _internalOnConnectionJoin(HSteamNetConnection newConn) override {
		connected = true;
	}

	bool _internalOnMessageRecieved(HSteamNetConnection newConn, MessageHeader header, Deserializer& des) override {
		switch (header) {
		case MESSAGE_HEADER_DELTA_SNAPSHOT:
			getNetworkStateManager().updateWithDeltaSnapshot(des);
			break;
		case MESSAGE_HEADER_FULL_SNAPSHOT:
			getNetworkStateManager().updateWithFullSnapshot(des);
			break;
		default:
			return true;
		}

		return false;
	}

protected:
	bool failed = false;
	bool connected = false;
	HSteamNetConnection conn = k_HSteamNetConnection_Invalid;
};

/**
 * @brief The server interface is one of the two primary interfaces
 * library-users may inherit from. When the server interface is used
 * snapshots will be automatically enabled and connecting clients will
 * be automatically sync'd. 
 * 
 * @note look at the NetworkInterface documentation for what methods
 * you may override.
 */
class ServerInterface : public NetworkInterface {
public:
	ServerInterface() {
		networkUpdate.setRate(20.0f);
		networkUpdate.setFunction([&](float) { snapshotUpdate(); });
	}

	virtual ~ServerInterface() = default;

	/**
	 * @brief Sends the snapshot compilation created within the
	 * NetworkSnapshotManger and sends it to all clients.
	 */
	void snapshotUpdate() {
		NetworkStateManager& stateManager = getNetworkStateManager();
		NetworkManager& networkManager = getNetworkManager();
		MessageBuffer reliableSnapshot;
		MessageBuffer unreliableSnapshot;
	
		stateManager.createDeltaSnapshot(reliableSnapshot, unreliableSnapshot);
	
		networkManager.sendMessage(0, std::move(reliableSnapshot), true, true);
		networkManager.sendMessage(0, std::move(unreliableSnapshot), true, false);
	}

	/**
	 * @brief Sends a fullSyncUpdate to "who." This means all currently created
	 * components, entities, physics objects will be serialized and then sent
	 * to that connection. This is quite a performance heavy function so use 
	 * only if requested of the client or when a client joins
	 * 
	 * @param who the client/connection to send the update to
	 */
	void fullSyncUpdate(HSteamNetConnection who) {
		MessageBuffer fullsnapshot;

		getNetworkStateManager().createFullSnapshot(fullsnapshot);

		if(who)
			getNetworkManager().sendMessage(who, std::move(fullsnapshot), false, true);
		else
			getNetworkManager().sendMessage(0, std::move(fullsnapshot), true, true);
	}

	/**
	 * @brief affects the call rate of snapshotUpdate. This will dictate
	 * how many times a second a server update is sent out. Should NEVER
	 * be higher then tick rate.
	 * 
	 * @param ups the new Updates Per Second of the server
	 */
	void setNetworkUPS(float ups) {
		debugWarning(ups > impl::getTickRate(), "UPS(%.0f) should not be higher the TPS(%.0f)\n", ups, impl::getTickRate());

		networkUpdate.setRate(ups);
	}

	/**
	 * @brief for servers, this will always return true.
	 * 
	 * @return always true
	 */
	bool isOpen() final {
		return true;
	}

	/**
	 * @brief has the listen socket failed to open, or in laymen's terms:
	 * has the server failed to start?
	 * 
	 * @return true if the server failed to start, false otherwise.
	 */
	bool hasFailed() final {
		return listen == k_HSteamListenSocket_Invalid;
	}
protected:
	bool open(const SteamNetworkingIPAddr& addr, const SteamNetworkingConfigValue_t& opt) override {
		if(listen != k_HSteamListenSocket_Invalid)
			return false;

		listen = impl::getSockets()->CreateListenSocketIP(addr, 1, &opt);
		if (listen == k_HSteamListenSocket_Invalid) {
			log(ERROR_SEVERITY_WARNING, "Unable to open listen socket");
			return false;
		}

		return true;
	}

	void acceptConnection(HSteamNetConnection conn) override {
		impl::getSockets()->AcceptConnection(conn);
	}

	void closeConnection(HSteamNetConnection conn) override {
		impl::getSockets()->CloseConnection(conn, 0, nullptr, false);
	}

	void close() override {
		impl::getSockets()->CloseListenSocket(listen);
	}

	bool _internalOnMessageRecieved(HSteamNetConnection conn, MessageHeader header, Deserializer& des) override {
		switch(header) {
		case MESSAGE_HEADER_REQUEST_FULL_SNAPSHOT:
			fullSyncUpdate(conn);
			break;

		default:
			return true;
		}

		return false;
	}

	void _internalUpdate() override {
		networkUpdate.update();
	}

protected:
	HSteamListenSocket listen = k_HSteamListenSocket_Invalid;

private:
	Ticker<void(float)> networkUpdate;
};

AE_NAMESPACE_END
