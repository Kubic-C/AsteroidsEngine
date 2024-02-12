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
 * Its purpose is to comply with the GameNeworkingSockets interface. Unlike std::vector<>
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


	MessageBuffer(MessageBuffer&& other) noexcept {
		setData(other.size, other.data, true);
		other.setOwner(false);
		other.reset();
	}

	~MessageBuffer() {
		if (hasOwnership && data) {
			delete[] data;
		}
	}

	size_t getSize() const { return size; }
	const u8* getData() const { return data; }
	u8* getData() { return data; }
	bool isOwner() const { return hasOwnership; }

	// will this resource automatically be free'd?
	void setOwner(bool isOwner) {
		hasOwnership = isOwner;
	}

	void setData(size_t size, u8* data, bool hasOwnership = true) {
		this->capacity = size;
		this->size = size;
		this->data = data;
		this->hasOwnership = hasOwnership;
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
			u8* oldData = data;
			data = new u8[newSize];

			memcpy(data, oldData, size);
			capacity = newSize;
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
		if (index > size - 1) {
			log(ERROR_SEVERITY_FATAL, "MessageBuffer: Index out of bounds\n");
		}
		if (!data) {
			log(ERROR_SEVERITY_FATAL, "MessageBuffer: No data allocated\n");
		}

		return data[index];
	}

public: // for std
	u8* begin() { return data; }
	u8* end() { return data + size; }
	const u8* cbegin() const { return data; }
	const u8* cend() const { return data + size; }

protected:
	// Will allocate if data is nullptr
	//	size: the new capacity
	// note: this->size will be set to zero
	bool allocateIfNoData(size_t size) {
		if (data)
			return false;

		data = new u8[size];
		capacity = size;
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

typedef MessageBuffer OutputBuffer;
typedef ::bitsery::OutputBufferAdapter<OutputBuffer> OutputAdapter;
typedef ::bitsery::Serializer<OutputAdapter> Serializer;

typedef MessageBuffer InputBuffer;
typedef ::bitsery::InputBufferAdapter<InputBuffer> InputAdapter;
typedef ::bitsery::Deserializer<InputAdapter> Deserializer;

AE_NAMESPACE_END

namespace bitsery {
	namespace traits {
		//template<>
		//struct BufferAdapterTraits<InputBuffer> {
		//	using TIterator = const uint8_t*;
		//	using TConstIterator = const uint8_t*;
		//	using TValue = uint8_t;
		//};

		//template<>
		//struct ContainerTraits<InputBuffer> {
		//	using TValue = uint8_t;

		//	static constexpr bool isResizable = false;
		//	static constexpr bool isContiguous = true;

		//	static void resize(InputBuffer&, size_t) {
		//		engineLog(ERROR_SEVERITY_FATAL, "(bitsery) Attempted to resize on an indirect container\n");
		//	}
		//	// get container size
		//	static size_t size(const InputBuffer& buf) {
		//		return buf.size();
		//	}
		//};

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
}

AE_NAMESPACE_BEGIN

inline Serializer startSerialize(OutputBuffer& buffer) {
	Serializer ser(buffer);
	return ser;
}

inline void endSerialize(Serializer& ser, OutputBuffer& buffer) {
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
	MESSAGE_HEADER_INVALID = UINT8_MAX,
	MESSAGE_HEADER_COMPONENT_SNAPSHOT = 0,
	MESSAGE_HEADER_ENTITY_COMPONENT_META_SNAPSHOT,
	MESSAGE_HEADER_PHYSICS_SNAPSHOT,
	MESSAGE_HEADER_FULL_SNAPSHOT, // A combined snapshot of component and physics snapshot
	MESSAGE_HEADER_CORE_LAST // it is named core in the case end-users also want to have multiple MessageHeader enums
};

template<typename S>
void serialize(S& s, MessageHeader& header) {
	s.value1b(header);
}

struct Message {
	HSteamNetConnection who;
	MessageBuffer data;
	bool sendAll = false;
	bool sendReliable = false;
};

namespace impl {
	ISteamNetworkingUtils* getUtils();
	ISteamNetworkingSockets* getSockets();

	struct MessageBufferMeta {
		u32 messagesSent = 0;
		u32 messagesFreed = 0;
	};
}

class EntityWorldNetworkManager;
class PhysicsWorldNetworkManager;

EntityWorldNetworkManager& getEntityWorldNetworkManager();
PhysicsWorldNetworkManager& getPhysicsWorldNetworkManager();

/**
 * An abstract interface defining how connections and incoming messages are dealt with.
 */
class NetworkInterface {
	friend class NetworkManager;
public:
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
		networkInterface = newInterface;
	}

	NetworkInterface& getNetworkInterface() const {
		return *networkInterface;
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
			steamMessageFlags = k_nSteamNetworkingSend_Reliable;
		} else {
			steamMessageFlags = k_nSteamNetworkingSend_Unreliable;
		}

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
			impl::MessageBufferMeta* meta = new impl::MessageBufferMeta;
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
						impl::MessageBufferMeta* meta = (impl::MessageBufferMeta*)message->m_nUserData;

						meta->messagesFreed++;
						if(meta->messagesFreed == meta->messagesSent) {
							delete[] message->m_pData;
							delete meta;						
						}
					};
				
				meta->messagesSent++;
			}

			if(networkingMessages.empty())
				return;

			impl::getSockets()->SendMessages((int)networkingMessages.size(), networkingMessages.data(), nullptr);
			networkingMessages.clear();
		} else {
			if(connections.find(who) == connections.end())
				log(ERROR_SEVERITY_FATAL, "Cannot send a message to an invalid connection: %u\n", who);

			impl::getSockets()->SendMessageToConnection(who, messageBuffer.getData(), (u32)messageBuffer.getSize(), steamMessageFlags, nullptr);
		}
	}

	void sendMessage(Message&& moveMessage) {
		Message message = std::move(moveMessage);

		sendMessage(message.who, std::move(message.data), message.sendAll, message.sendReliable);
	}

	void update() {
		if(!networkInterface)
			return;

		impl::getSockets()->RunCallbacks();

		ISteamNetworkingMessage* message = nullptr;
		while(impl::getSockets()->ReceiveMessagesOnPollGroup(pollGroup, &message, 1)) {
			Deserializer des = startDeserialize(message->GetSize(), message->GetData());
			MessageHeader header = MESSAGE_HEADER_INVALID;

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

	// Adds a warning to a connection
	void connectionAddWarning(HSteamNetConnection conn) {
		connections[conn].warnings++;
		if (connections[conn].warnings > maxWarnings) {
			onConnectionLeave(conn);
			log(ERROR_SEVERITY_WARNING, "Connection <red>exceeded maxWarnings<reset> and was forcibily disconnected\n");
		}
	}

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

extern flecs::world& getEntityWorld();

struct u32Hasher {
	std::size_t operator()(std::vector<u32> const& vec) const {
		std::size_t seed = vec.size();
		for (auto& i : vec) {
			seed ^= i + 0x9e3779b9 + (seed << 6) + (seed >> 2);
		}
		return seed;
	}
};

struct NetworkedEntity {};
struct NetworkedComponent {};
struct NoPhase {};

class EntityWorldNetworkManager {
	// Clears the extra fields of an entity id. Clear Field (cf)
	u32 cf(u64 id) {
		return id & ECS_ENTITY_MASK;
	}

	// Adds back the extra fields of an entity id. Add Field(af)
	flecs::entity af(u32 id) {
		return getEntityWorld().get_alive(id);
	} 

public:
	EntityWorldNetworkManager() {
		flecs::world& world = getEntityWorld();

		world.component<NetworkedEntity>();
		world.component<NetworkedComponent>();

		allEntityManagement.push_back(
			world.observer()
			.term<NetworkedEntity>()
			.event(flecs::OnRemove).each([this](flecs::entity e) {
				entitiesDestroyed.insert(cf(e));
			}));
	}

	~EntityWorldNetworkManager() {
		for(flecs::entity e : allEntityManagement) {
			e.destruct();
		}

		for (flecs::entity e : forceComponentChangeSystem) {
			e.destruct();
		}
	}

	bool componentSnapshotReady() {
		return (bool)componentsChanged.size();
	}

	bool entityComponentMetaSnapshotReady() {
		return (bool)componentsDestroyed.size() ||
			   (bool)entitiesDestroyed.size() ||
			   (bool)entitiesEnabled.size() ||
			   (bool)entitiesDisabled.size();
	}

	void serializeComponentSnapshot(Serializer& ser) {
		flecs::world& world = getEntityWorld();

		if(world.is_deferred())
			log(ERROR_SEVERITY_FATAL, "World must not be in deferred mode when calling serializeComponentSnapshot");

		for(u32 rawId : entitiesDestroyed) {
			flecs::entity e = af(rawId);

			if (componentsChanged.find(e) != componentsChanged.end()) {
				componentsChanged.erase(cf(e));
			}

			if (componentsDestroyed.find(e) != componentsDestroyed.end()) {
				componentsDestroyed.erase(cf(e));
			}
		}

		// Note:
		// Component updates are in a seperate function because they make the size of messages incredibily large
		// so to avoid lag we seperate EntityComponentMeta updates and ComponentChanged updates.

		// Must serialize: 
		//	- Components changed

		// Instead of associating entities with their changed components
		// we are associating a set of changed components with a list of entities.
		cache.archetypeComponent.clear();
		for(auto& pair : componentsChanged) {
			if(entitiesDestroyed.find(pair.first) != entitiesDestroyed.end())
				continue;

			cache.archetypeComponent[pair.second].push_back(pair.first);
		}

		ser.object((u32)cache.archetypeComponent.size());
		for(std::pair<const std::vector<u32>, std::vector<u32>>& pair : cache.archetypeComponent) {
			// Serialize the order of components
			ser.container(pair.first, UINT32_MAX);

			// Then serialize all entity-components
			ser.object((u32)pair.second.size());
			for(u32 entityId : pair.second) {
				ser.object(entityId);
				
				for(u32 compId : pair.first) {
					serializeRecords[compId](ser, af(entityId).get_mut(af(compId)));
				}
			}
		}

		componentsChanged.clear();
	}

	void serializeEntityComponentMetaSnapshot(Serializer& ser) {
		flecs::world& world = getEntityWorld();

		if (world.is_deferred())
			log(ERROR_SEVERITY_FATAL, "World must not be in deferred mode when calling serializeEntityComponentMetaSnapshot");

		// Note:
		// Instead of serializing std::vector<> (which we CAN do) we instead serialize the size
		// and then all of its elements. I hear you say: Well why would we do that, you now have to make extra
		// call to the serializer?! Well the reason is to avoid allocating on the heap on the deserialization side.
		// It is also makes more sense on the deserialization side because it needs to handle
		// one component/entity at a time instead of all at once.
		// 
		// Is it boiler plate? 
		//	Yes it is. 
		// Do I care? 
		//	No. 
		// Will I write a 4 line helper function that will solve the boiler plate? 
		//	No I dont think I will.

		// Must serialize: 
		//	- Destroyed Components
		//  - Destroyed entities
		//  - Enabled entities
		//  - Disabled entities

		// Destroyed Components

		cache.archetypeComponent.clear();
		for (auto& pair : componentsDestroyed) {
			cache.archetypeComponent[pair.second].push_back(pair.first);
		}

		ser.object((u32)cache.archetypeComponent.size());
		for (std::pair<const std::vector<u32>, std::vector<u32>>& pair : cache.archetypeComponent) {
			// Serialize the order of components
			ser.container(pair.first, UINT32_MAX);

			// Then serialize all entity-components
			ser.object((u32)pair.second.size());
			for (u32 entityId : pair.second) {
				ser.object(entityId);
			}
		}

		// Destroyed entities

		ser.object((u32)entitiesDestroyed.size());
		for(u32 id : entitiesDestroyed) {
			ser.object(id);
		}

		// Enabled entities

		ser.object((u32)entitiesEnabled.size());
		for (u32 id : entitiesEnabled) {
			ser.object(id);
		}

		// Disabled entities

		ser.object((u32)entitiesDisabled.size());
		for (u32 id : entitiesDisabled) {
			ser.object(id);
		}

		componentsDestroyed.clear();
		entitiesDestroyed.clear();
		entitiesEnabled.clear();
		entitiesDisabled.clear();
	}

	void deserializeComponentSnapshot(Deserializer& des) {
		flecs::world& world = getEntityWorld();

		if (world.is_deferred())
			log(ERROR_SEVERITY_FATAL, "World must not be in deferred mode when calling deserializeComponentSnapshot");

		u32 archetypeCount;
		des.object(archetypeCount);
		for(u32 i = 0; i < archetypeCount; i++) {
			// Getting id list of components
			cache.idList.clear();
			des.container(cache.idList, UINT32_MAX);

			// Now deserializing all the entity-component data
			u32 entityCount;
			des.object(entityCount);
			for(u32 entityI = 0; entityI < entityCount; entityI++) {
				u32 serId;
				des.object(serId);
				flecs::entity e = world.ensure(serId); 

				for(u32 compId : cache.idList) {
					deserializeRecords[compId](des, e.get_mut(af(compId)));
				}
			} 
		}
	}

	void deserializeEntityComponentMetaSnapshot(Deserializer& des) {
		flecs::world& world = getEntityWorld();

		if (world.is_deferred())
			log(ERROR_SEVERITY_FATAL, "World must not be in deferred mode when calling deserializeEntityComponentMetaSnapshot");

		u32 archetypeCount = 0;
		des.object(archetypeCount);
		for (u32 i = 0; i < archetypeCount; i++) {
			// Serialize the order of components
			cache.idList.clear();
			des.container(cache.idList, UINT32_MAX);

			// Then serialize all entity-components
			u32 entityCount;
			des.object(entityCount);
			for (u32 entityI = 0; entityI < entityCount; entityI++) {
				u32 serId;
				des.object(serId);
				flecs::entity e = world.ensure(serId);

				for(u32 compId : cache.idList) {
					e.remove(af(compId));
				}
			}
		}

		// Destroyed entities

		u32 entityCount;
		des.object(entityCount);
		for (u32 i = 0; i < entityCount; i++) {
			u32 serId;
			des.object(serId);

			if(!world.is_alive(serId)) {
				log(ERROR_SEVERITY_WARNING, "Possible dsync: recieved EntityComponentSnapshot contained entity that ws not alive: %u", serId);
				continue;
			}
	
			af(serId).destruct();
		}

		// Enabled entities

		des.object(entityCount);
		for (u32 i = 0; i < entityCount; i++) {
			u32 serId;
			des.object(serId);

			if (!world.is_alive(serId)) {
				log(ERROR_SEVERITY_WARNING, "Possible dsync: recieved EntityComponentSnapshot contained entity that ws not alive: %u", serId);
				continue;
			}

			af(serId).enable();
		}

		// Disabled entities

		des.object(entityCount);
		for (u32 i = 0; i < entityCount; i++) {
			u32 serId;
			des.object(serId);

			if (!world.is_alive(serId)) {
				log(ERROR_SEVERITY_WARNING, "Possible dsync: recieved EntityComponentSnapshot contained entity that ws not alive: %u", serId);
				continue;
			}

			af(serId).disable();
		}
	}

	// this will force a component update, meaning all networked components of all networked entities
	// will be put a single component update. When serializeComponentSnapshot() is called expect lag.
	void yeildAllComponenents() {
		componentsChanged.clear();
		for(flecs::system sys : forceComponentChangeSystem) {
			sys.run();
		}
	}

	template<typename T>
	flecs::entity registerComponent() {
		flecs::world& world = getEntityWorld();

		flecs::entity componentId = world.component<T>();
		u32 rawId = cf(componentId);

		componentId.is_a<NetworkedComponent>();

		serializeRecords[rawId] =
			[](Serializer& ser, void* data) {
				ser.object(*(T*)data);
			};

		deserializeRecords[rawId] =
			[](Deserializer& des, void* data) {
				des.object(*(T*)data);
			};

		allEntityManagement.push_back(
			world.observer<T>()
			.event(flecs::OnSet).each([rawId, this](flecs::entity e, T& comp) {
				componentsChanged[cf(e)].push_back(rawId);
			}));

		forceComponentChangeSystem.push_back(
		world.system<T>()
			.kind<NoPhase>()
			.each([rawId, this](flecs::entity e, T& comp) {
				componentsChanged[cf(e)].push_back(rawId);
			}));

		allEntityManagement.push_back(
			world.observer<T>()
			.with<NetworkedEntity>()
			.event(flecs::OnRemove).each([rawId, this](flecs::entity e, T& comp){
				componentsDestroyed[cf(e)].push_back(rawId);
			}));

		return componentId;
	}
	
	void enable(flecs::entity entity) {
		entity.enable();
		entitiesEnabled.push_back(cf(entity));
	}

	void disable(flecs::entity entity) {
		entity.disable();
		entitiesDisabled.push_back(cf(entity));
	}

	flecs::entity entity() {
		return getEntityWorld().entity().add<NetworkedEntity>();
	}

protected:
	void clearAll() {
		componentsChanged.clear();
		componentsDestroyed.clear();
		entitiesDestroyed.clear();
		entitiesEnabled.clear();
		entitiesDisabled.clear();
	}

protected:
	std::vector<flecs::entity> allEntityManagement;
	std::vector<flecs::system> forceComponentChangeSystem;

	std::unordered_map<u32, std::function<void(Serializer& ser, void* data)>> serializeRecords;
	std::unordered_map<u32, std::function<void(Deserializer& ser, void* data)>> deserializeRecords;

	// Putting vectors and maps in cache will mean less calls to malloc and free
	// as the capacity of both containers goes up
	struct Cache {
		std::vector<u32> idList;
		std::unordered_map<std::vector<u32>, std::vector<u32>, u32Hasher> archetypeComponent;
	} cache;


	std::unordered_map<u32, std::vector<u32>> componentsChanged;
	std::unordered_map<u32, std::vector<u32>> componentsDestroyed;
	std::unordered_set<u32> entitiesDestroyed;
	std::vector<u32> entitiesEnabled;
	std::vector<u32> entitiesDisabled;
};

/* Physics Syncing */

// Must be defined here so it may be used with PhysicsWorldNetworkManager
struct ShapeComponent : public NetworkedComponent {
	u32 shape = UINT32_MAX;

	template<typename S>
	void serialize(S& s) {
		s.value4b(shape);
	}
};

class PhysicsWorldNetworkManager {
public:
	PhysicsWorldNetworkManager() {
		query = getEntityWorld().query<ShapeComponent>();
	}

	bool isPhysicsWorldSnapshotReady() {
		PhysicsWorld& physicsWorld = getPhysicsWorld();
		bool needUpdate = false;

		query.iter([&](flecs::iter& iter, ShapeComponent* shapesArray) {
			for (auto i : iter) {
				Shape& shape = physicsWorld.getShape(shapesArray[i].shape);

				if (shape.isNetworkDirty()) {
					shapeUpdates[shape.getType()].push_back(shapesArray[i].shape);
					shape.resetNetworkDirty();
					needUpdate = true;
				}
			}
			});

		return needUpdate;
	}

	void yieldAll() {
		PhysicsWorld& physicsWorld = getPhysicsWorld();

		query.iter([&](flecs::iter& iter, ShapeComponent* shapesArray) {
			for (auto i : iter) {
				Shape& shape = physicsWorld.getShape(shapesArray[i].shape);

				shapeUpdates[shape.getType()].push_back(shapesArray[i].shape);
				shape.resetNetworkDirty();
			}
			});
	}

	void serializePhysicsWorldSnapshot(Serializer& ser) {
		PhysicsWorld& physicsWorld = getPhysicsWorld();
		
		ser.object((u32)shapeUpdates[ShapeEnum::Polygon].size());
		for (u32 shapeId : shapeUpdates[ShapeEnum::Polygon]) {
			ser.object(shapeId);
			ser.object(physicsWorld.getPolygon(shapeId));
		}

		ser.object((u32)shapeUpdates[ShapeEnum::Circle].size());
		for (u32 shapeId : shapeUpdates[ShapeEnum::Circle]) {
			ser.object(shapeId);
			ser.object(physicsWorld.getCircle(shapeId));
		}

		for (auto& pair : shapeUpdates) {
			pair.second.clear();
		}
	}

	void deserializePhysicsWorldSnapshot(Deserializer& des) {
		PhysicsWorld& physicsWorld = getPhysicsWorld();

		u32 listSize;
		des.object(listSize);
		for (u32 i = 0; i < listSize; i++) {
			u32 shapeId;
			des.object(shapeId);

			Polygon* polygon;

			if (physicsWorld.doesShapeExist(shapeId)) {
				polygon = &physicsWorld.getPolygon(shapeId);
			}
			else {
				polygon = &physicsWorld.getPolygon(physicsWorld.insertShape<Polygon>(shapeId));
			}

			des.object(*polygon);
			polygon->markLocalDirty();
		}

		des.object(listSize);
		for (u32 i = 0; i < listSize; i++) {
			u32 shapeId;
			des.object(shapeId);

			Circle* circle;

			if (physicsWorld.doesShapeExist(shapeId)) {
				circle = &physicsWorld.getCircle(shapeId);
			}
			else {
				circle = &physicsWorld.getCircle(physicsWorld.insertShape<Circle>(shapeId));
			}

			des.object(*circle);
			circle->markLocalDirty();
		}
	}

private:
	std::unordered_map<ShapeEnum, std::vector<u32>> shapeUpdates;
	flecs::query<ShapeComponent> query;
};

/* Default network interfaces */

/**
 * The client interface's purpose is to connect and send/recieve messages to a server.
 */
class ClientInterface : public NetworkInterface {
public:
	static constexpr u64 defaultLocalEntityRange = 1000000;

	ClientInterface() {
		flecs::world& world = getEntityWorld();

		world.set_entity_range(defaultLocalEntityRange, 0);
		world.enable_range_check(true);
	}

	bool isOpen() final {
		return connected;
	}

	bool hasFailed() final {
		return failed;
	}

protected:
	bool open(const SteamNetworkingIPAddr& addr, const SteamNetworkingConfigValue_t& opt) {
		if (conn != k_HSteamNetConnection_Invalid)
			return false;

		conn = impl::getSockets()->ConnectByIPAddress(addr, 1, &opt);
		if (conn == k_HSteamNetConnection_Invalid) {
			log(ERROR_SEVERITY_WARNING, "Unable to open client socket");
			failed = true;
			return false;
		}

		// this will insure that conn gets added to the connections list within the networkManager
		// so if some function wants to send a message right open even we aren't technically connected
		// they can.
		getNetworkManager().update();

		connected = false;
		failed = false;

		return true;
	}

	void acceptConnection(HSteamNetConnection conn) override {}

	void closeConnection(HSteamNetConnection conn) override {
		if(!connected) {
			failed = true;
		}

		connected = false;
		impl::getSockets()->CloseConnection(conn, 0, nullptr, false);
		this->conn = k_HSteamNetConnection_Invalid;
	}

	void close() {
		conn = k_HSteamNetConnection_Invalid;
	}

	void _internalOnConnectionJoin(HSteamNetConnection conn) override {
		connected = true;
	}

	bool _internalOnMessageRecieved(HSteamNetConnection conn, MessageHeader header, Deserializer& des) override {
		flecs::world& world = getEntityWorld();
		EntityWorldNetworkManager& entity = getEntityWorldNetworkManager();
		PhysicsWorldNetworkManager& physics = getPhysicsWorldNetworkManager();

		world.enable_range_check(false);

		switch (header) {
		case MESSAGE_HEADER_COMPONENT_SNAPSHOT:
			entity.deserializeComponentSnapshot(des);
			break;

		case MESSAGE_HEADER_ENTITY_COMPONENT_META_SNAPSHOT:
			entity.deserializeEntityComponentMetaSnapshot(des);
			break;

		case MESSAGE_HEADER_PHYSICS_SNAPSHOT:
			physics.deserializePhysicsWorldSnapshot(des);
			break;

		case MESSAGE_HEADER_FULL_SNAPSHOT:
			physics.deserializePhysicsWorldSnapshot(des);
			entity.deserializeComponentSnapshot(des);
			break;
	
		default:
			world.enable_range_check(true);
			return true;
		}

		world.enable_range_check(true);
		return false;
	}

protected:
	bool failed = false;
	bool connected = false;
	HSteamNetConnection conn = k_HSteamNetConnection_Invalid;
};

/**
 * The server interface creates a server that clients can connect to.
 * 
 * This particular interface will automatically sync clients with the
 * current flecs::world. 
 */
class ServerInterface : public NetworkInterface {
public:
	ServerInterface() {
		networkUpdate.setRate(40.0f);
		networkUpdate.setFunction([&](float) { syncUpdate(); });
	}

	void syncUpdate() {
		EntityWorldNetworkManager& worldNetworkManager = getEntityWorldNetworkManager();
		PhysicsWorldNetworkManager& physicsWorldManager = getPhysicsWorldNetworkManager();
		NetworkManager& networkManager = getNetworkManager();

		// Physics world snapshot.
		// Note: the physics world snapshot simply sends the vertices and radius of shapes, so it 
		// must be reliably sent
		if (physicsWorldManager.isPhysicsWorldSnapshotReady()) {
			OutputBuffer message;
			Serializer ser = startSerialize(message);
			ser.object(MESSAGE_HEADER_PHYSICS_SNAPSHOT);
			physicsWorldManager.serializePhysicsWorldSnapshot(ser);
			endSerialize(ser, message);
			networkManager.sendMessage(0, std::move(message), true, true);
		}

		// Send the component update snapshot.
		// Note: Its ok if this is unreliable.
		if (worldNetworkManager.componentSnapshotReady()) {
			OutputBuffer message;
			Serializer ser = startSerialize(message);
			ser.object(MESSAGE_HEADER_COMPONENT_SNAPSHOT);
			worldNetworkManager.serializeComponentSnapshot(ser);
			endSerialize(ser, message);
			networkManager.sendMessage(0, std::move(message), true);
		}

		// Send entity component meta data snapshot. 
		// Note: Must be reliably sent.
		if (worldNetworkManager.entityComponentMetaSnapshotReady()) {
			OutputBuffer message;
			Serializer ser = startSerialize(message);
			ser.object(MESSAGE_HEADER_ENTITY_COMPONENT_META_SNAPSHOT);
			worldNetworkManager.serializeEntityComponentMetaSnapshot(ser);
			endSerialize(ser, message);
			networkManager.sendMessage(0, std::move(message), true, true);
		}
	}

	// Refer to NetworkManager::sendMessage() for param info
	// sends a full snapshot to the selected connection or all.
	void fullSyncUpdate(HSteamNetConnection who, bool sendAll = false) {
		EntityWorldNetworkManager& worldNetworkManager = getEntityWorldNetworkManager();
		PhysicsWorldNetworkManager& physicsWorldManager = getPhysicsWorldNetworkManager();
		NetworkManager& networkManager = getNetworkManager();
		OutputBuffer message;
		
		physicsWorldManager.yieldAll();
		worldNetworkManager.yeildAllComponenents();

		{
			Serializer ser = startSerialize(message);
			ser.object(MESSAGE_HEADER_FULL_SNAPSHOT);
			physicsWorldManager.serializePhysicsWorldSnapshot(ser);
			worldNetworkManager.serializeComponentSnapshot(ser);
			endSerialize(ser, message);
			networkManager.sendMessage(who, std::move(message), sendAll, true);
		}

	}

	// sets the amount of network updates (flecs::world entity syncing and such)
	// that are sent per second. High UPS WILL impact performance
	void setNetworkUPS(float ups) {
		networkUpdate.setRate(ups);
	}

	bool isOpen() final {
		return true;
	}

	bool hasFailed() final {
		return false;
	}
protected:
	bool open(const SteamNetworkingIPAddr& addr, const SteamNetworkingConfigValue_t& opt) {
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

	void close() {
		impl::getSockets()->CloseListenSocket(listen);
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