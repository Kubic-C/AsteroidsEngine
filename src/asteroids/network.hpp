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

	void copyData(size_t size, const u8* src) {
		hasOwnership = true;

		resize(size);
		memcpy(data, src, size);
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

typedef ::bitsery::OutputBufferAdapter<MessageBuffer> OutputAdapter;
typedef ::bitsery::Serializer<OutputAdapter> Serializer;

typedef ::bitsery::InputBufferAdapter<MessageBuffer> InputAdapter;
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
	MESSAGE_HEADER_SNAPSHOT_COMPILATION_RELIABLE,
	MESSAGE_HEADER_SNAPSHOT_COMPILATION_UNRELIABLE,
	MESSAGE_HEADER_REQUEST_SNAPSHOT_FULL,
	MESSAGE_HEADER_SNAPSHOT_FULL,
	MESSAGE_HEADER_CORE_LAST // it is named core in the case end-users also want to have multiple MessageHeader enums
};

template<typename S>
void serialize(S& s, MessageHeader& header) {
	s.value1b(header);
}

struct Message {
	HSteamNetConnection who = 0;
	MessageBuffer data;
	bool sendAll = false;
	bool sendReliable = false;
};

namespace impl {
	ISteamNetworkingUtils* getUtils();
	ISteamNetworkingSockets* getSockets();
	extern float getTickRate();

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

	/* This used for snapshotBegin and snapshotEnd in servers 
	   and applying recieved snapshots by the tick in clients */
	virtual void beginTick() {}
	virtual void endTick() {}

public:
	// returns true if the message can't be used
	virtual bool _internalOnMessageRecieved(HSteamNetConnection conn, MessageHeader header, Deserializer& deserializer, u32 size, const void* data) {
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

	void sendMessage(Message&& moveMessage) {
		Message message = std::move(moveMessage);

		sendMessage(message.who, std::move(message.data), message.sendAll, message.sendReliable);
	}

	void update() {
		if(!hasNetworkInterface())
			return;

		impl::getSockets()->RunCallbacks();

		ISteamNetworkingMessage* message = nullptr;
		while(impl::getSockets()->ReceiveMessagesOnPollGroup(pollGroup, &message, 1)) {
			Deserializer des = startDeserialize(message->GetSize(), message->GetData());
			MessageHeader header = MESSAGE_HEADER_INVALID;

			des.object(header);
			if(networkInterface->_internalOnMessageRecieved(message->m_conn, header, des, message->GetSize(), message->GetData()))
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
		for (const auto& i : vec) {
			seed ^= i + 0x9e3779b9 + (seed << 6) + (seed >> 2);
		}
		return seed;
	}
};

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

extern u64 getCurrentTick();

class EntityWorldNetworkManager {
	bool isIdValid(u32 id) {
		return impl::af(id) != 0;
	}

public:
	EntityWorldNetworkManager() {
		flecs::world& world = getEntityWorld();

		registerComponent<NetworkedEntity>();
		world.component<NetworkedComponent>();
		world.add<ae::NetworkedEntity>(); // entity world should be Networked by default.

		allEntityManagement.push_back(
			world.observer()
			.term<NetworkedEntity>()
			.event(flecs::OnRemove).each([this](flecs::entity e) {
				entitiesDestroyed.insert(impl::cf(e));
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

	void serializeHighPiorityComponentUpdates(Serializer& ser) {
		serializeComponentSnapshot(highPiorityComponentsUpdates, ser);
	}

	void serializeLowPiorityComponentUpdates(Serializer& ser) {
		serializeComponentSnapshot(lowPiorityComponentsUpdates, ser);
	}

	bool isHighPiorityComponentSnapshotReady() {
		return highPiorityComponentsUpdates.size();
	}

	bool isLowPiorityComponentSnapshotReady() {
		return lowPiorityComponentsUpdates.size();
	}

	bool isEntityComponentMetaSnapshotReady() {
		return (bool)componentsCreated.size() ||
			   (bool)componentsDestroyed.size() ||
			   (bool)entitiesDestroyed.size() ||
			   (bool)entitiesEnabled.size() ||
			   (bool)entitiesDisabled.size();
	}

	void serializeEntityComponentMetaSnapshot(Serializer& ser, bool noDestroyed = false) {
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
		//  - Created Components
		//	- Destroyed Components
		//  - Destroyed entities
		//  - Enabled entities
		//  - Disabled entities

		// Created Components
		cache.archetypeComponent.clear();
		for (auto& pair : componentsCreated) {
			if (!isIdValid(pair.first)) {
				continue;
			}

			cache.extractIdsFromSet(pair.second);

			cache.archetypeComponent[cache.idList].push_back(pair.first);
		}

		ser.object((u32)cache.archetypeComponent.size());
		for (std::pair<const std::vector<u32>, std::vector<u32>>& pair : cache.archetypeComponent) {
			// Serialize the order of components
			const std::vector<u32>& componentTypes = pair.first;
			ser.object((u32)componentTypes.size()); // entity component type count
			for (u32 i = 0; i < componentTypes.size(); i++) { // entity component types
				ser.object(componentTypes[i]);
			}

			// Then serialize all entity-components
			ser.object((u32)pair.second.size());
			for (u32 entityId : pair.second) {
				ser.object(entityId);
			}
		}

		// Destroyed Components and Destroyed entites have to be skipped in full snapshots
		if(noDestroyed) {
			ser.object((u32)0);
			ser.object((u32)0);
			goto disabledOrEnabled;
		}
		
		cache.archetypeComponent.clear();
		for (auto& pair : componentsDestroyed) {
			if (!isIdValid(pair.first)) {
				continue;
			}
			
			cache.extractIdsFromSet(pair.second);

			cache.archetypeComponent[cache.idList].push_back(pair.first);
		}

		ser.object((u32)cache.archetypeComponent.size());
		for (std::pair<const std::vector<u32>, std::vector<u32>>& pair : cache.archetypeComponent) {
			// Serialize the order of components
			const std::vector<u32>& componentTypes = pair.first;
			ser.object((u32)componentTypes.size()); // entity component type count
			for (u32 i = 0; i < componentTypes.size(); i++) { // entity component types
				ser.object(componentTypes[i]);
			}

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

	disabledOrEnabled:

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

		// very important to make sure we don't forget to clear these!
		componentsCreated.clear();
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
			u32 idListSize;
			des.object(idListSize);
			cache.idList.resize(idListSize);
			for (u32 compI = 0; compI < idListSize; compI++) { // entity component types
				des.object(cache.idList[compI]);
			}

			// Now deserializing all the entity-component data
			u32 entityCount;
			des.object(entityCount);
			for(u32 entityI = 0; entityI < entityCount; entityI++) {
				u32 serId;
				des.object(serId);
				flecs::entity e = world.ensure(serId); 

				for(u32 compId : cache.idList) {
					deserializeRecords[compId](des, e.get_mut(impl::af(compId)));
				}
			} 
		}
	}

	u64 lastTick;

	void deserializeEntityComponentMetaSnapshot(Deserializer& des) {
		flecs::world& world = getEntityWorld();

		if (world.is_deferred())
			log(ERROR_SEVERITY_FATAL, "World must not be in deferred mode when calling deserializeEntityComponentMetaSnapshot");

		// Created Components

		u32 archetypeCount = 0;
		des.object(archetypeCount);
		for (u32 i = 0; i < archetypeCount; i++) {
			// Deserialize the order of components
			cache.idList.clear();
			u32 idListSize;
			des.object(idListSize);
			cache.idList.resize(idListSize);
			for (u32 compI = 0; compI < idListSize; compI++) { // entity component types
				des.object(cache.idList[compI]);
			}

			// Then deserialize all entity-components
			u32 entityCount;
			des.object(entityCount);
			for (u32 entityI = 0; entityI < entityCount; entityI++) {
				u32 serId;
				des.object(serId);
				flecs::entity e = world.ensure(serId);

				for (u32 compId : cache.idList) {
					e.add(impl::af(compId));
				}
			}
		}

		// Destroyed Components

		des.object(archetypeCount);
		for (u32 i = 0; i < archetypeCount; i++) {
			// Deserialize the order of components
			cache.idList.clear();
			u32 idListSize;
			des.object(idListSize);
			cache.idList.resize(idListSize);
			for (u32 compI = 0; compI < idListSize; compI++) { // entity component types
				des.object(cache.idList[compI]);
			}

			// Then deserialize all entity-components
			u32 entityCount;
			des.object(entityCount);
			for (u32 entityI = 0; entityI < entityCount; entityI++) {
				u32 serId;
				des.object(serId);
				flecs::entity e = world.ensure(serId);

				for(u32 compId : cache.idList) {
					e.remove(impl::af(compId));
				}
			}
		}

		// Destroyed entities

		u32 entityCount;
		des.object(entityCount);
		for (u32 i = 0; i < entityCount; i++) {
			u32 serId;
			des.object(serId);

			if(!isIdValid(serId)) {
				log(ERROR_SEVERITY_WARNING, "Possible dsync: recieved destroyEntities contained entity that ws not alive: %u\n", serId);
				continue;
			}
	
			impl::af(serId).destruct();
		}

		// Enabled entities

		des.object(entityCount);
		for (u32 i = 0; i < entityCount; i++) {
			u32 serId;
			des.object(serId);

			if (!isIdValid(serId)) {
				log(ERROR_SEVERITY_WARNING, "Possible dsync: recieved enabledEntities contained entity that ws not alive: %u\n", serId);
				continue;
			}

			impl::af(serId).enable();
		}

		// Disabled entities

		des.object(entityCount);
		for (u32 i = 0; i < entityCount; i++) {
			u32 serId;
			des.object(serId);

			if (!isIdValid(serId)) {
				log(ERROR_SEVERITY_WARNING, "Possible dsync: recieved disabledEntites contained entity that ws not alive: %u\n", serId);
				continue;
			}

			impl::af(serId).disable();
		}
	}

	// this will force a component update, meaning all networked components of all networked entities
	// will be put a single component update. When serializeComponentSnapshot() is called expect lag.
	void yeildAllComponenents() {
		highPiorityComponentsUpdates.clear();
		for(flecs::system sys : forceComponentChangeSystem) {
			sys.run();
		}
	}

	template<typename T>
	flecs::entity registerComponent(ComponentPiority piority = ComponentPiority::Low) {
		flecs::world& world = getEntityWorld();

		flecs::entity componentId = world.component<T>();
		u32 rawId = impl::cf(componentId);

		componentId.is_a<NetworkedComponent>();

		allEntityManagement.push_back(
			world.observer().term<T>
			.event(flecs::OnAdd).each([rawId, this](flecs::entity e) {
				componentsCreated[impl::cf(e)].insert(rawId);
			}));

		allEntityManagement.push_back(
			world.observer().term<T>
			.event(flecs::OnRemove).each([rawId, this](flecs::entity e){
				componentsDestroyed[impl::cf(e)].insert(rawId);
			}));

		registerIfNonEmptyComponent<T>(rawId, piority, std::is_empty<T>());

		return componentId;
	}

	void enable(flecs::entity entity) {
		entity.enable();
		entitiesEnabled.push_back(impl::cf(entity));
	}

	void disable(flecs::entity entity) {
		entity.disable();
		entitiesDisabled.push_back(impl::cf(entity));
	}

	flecs::entity entity() {
		return getEntityWorld().entity().add<NetworkedEntity>();
	}

	void disableAllSerialization() {
		for(flecs::entity e : allEntityManagement) {
			e.disable();
		}
	}

protected:
	template<typename T>
	void registerIfNonEmptyComponent(u32 rawId, ComponentPiority piority, std::true_type) {
		flecs::world& world = getEntityWorld();
		
		forceComponentChangeSystem.push_back(
			 world.system().term<T>()
			.template kind<NoPhase>()
			.each([rawId, this](flecs::entity e) {
				componentsCreated[impl::cf(e)].insert(rawId);
			}));
	}

	template<typename T>
	void registerIfNonEmptyComponent(u32 rawId, ComponentPiority piority, std::false_type) {
		flecs::world& world = getEntityWorld();

		serializeRecords[rawId] =
			[](Serializer& ser, void* data) {
			ser.object(*(T*)data);
			};

		deserializeRecords[rawId] =
			[](Deserializer& des, void* data) {
			des.object(*(T*)data);
			};

		if (piority == ComponentPiority::Low) {
			allEntityManagement.push_back(
				world.observer<T>()
				.event(flecs::OnSet).each([rawId, this](flecs::entity e, T& comp) {
					lowPiorityComponentsUpdates[impl::cf(e)].insert(rawId);
					}));

			allEntityManagement.push_back(
				world.observer<T>()
				.event(flecs::OnAdd).each([rawId, this](flecs::entity e, T& comp) {
					lowPiorityComponentsUpdates[impl::cf(e)].insert(rawId);
					}));
		}
		else {
			allEntityManagement.push_back(
				world.observer<T>()
				.event(flecs::OnSet).each([rawId, this](flecs::entity e, T& comp) {
					highPiorityComponentsUpdates[impl::cf(e)].insert(rawId);
					}));

			allEntityManagement.push_back(
				world.observer<T>()
				.event(flecs::OnAdd).each([rawId, this](flecs::entity e, T& comp) {
					highPiorityComponentsUpdates[impl::cf(e)].insert(rawId);
					}));
		}

		forceComponentChangeSystem.push_back(
			world.system<T>()
			.template kind<NoPhase>()
			.each([rawId, this](flecs::entity e, T& comp) {
				highPiorityComponentsUpdates[impl::cf(e)].insert(rawId);
			}));
	}

	void serializeComponentSnapshot(std::unordered_map<u32, std::unordered_set<u32>>& componentsChanged, Serializer& ser) {
		flecs::world& world = getEntityWorld();

		if (world.is_deferred())
			log(ERROR_SEVERITY_FATAL, "World must not be in deferred mode when calling serializeComponentSnapshot");

		// Note:
		// Component updates are in a seperate function because they make the size of messages incredibily large
		// so to avoid lag we seperate EntityComponentMeta updates and ComponentChanged updates.

		// Must serialize: 
		//	- Components changed

		// Instead of associating entities with their changed components
		// we are associating a set of changed components with a list of entities.
		cache.archetypeComponent.clear();
		for (auto& pair : componentsChanged) {
			if (impl::af(pair.first) == 0)
				continue;

			cache.extractIdsFromSet(pair.second);

			cache.archetypeComponent[cache.idList].push_back(pair.first);
		}

		ser.object((u32)cache.archetypeComponent.size());
		for (std::pair<const std::vector<u32>, std::vector<u32>>& pair : cache.archetypeComponent) {
			// Serialize the order of components
			const std::vector<u32>& componentTypes = pair.first;
			ser.object((u32)componentTypes.size()); // entity component type count
			for (u32 i = 0; i < componentTypes.size(); i++) { // entity component types
				ser.object(componentTypes[i]);
			}

			// Then serialize all entity-components
			ser.object((u32)pair.second.size());
			for (u32 entityId : pair.second) {
				ser.object(entityId);

				for (u32 compId : pair.first) {
					flecs::entity entity = impl::af(entityId);
					flecs::entity component = impl::af(compId);

					serializeRecords[compId](ser, entity.get_mut(component));
				}
			}
		}

		componentsChanged.clear();
	}

	// Putting vectors and maps in cache will mean less calls to malloc and free
	// as the capacity of both containers goes up
	struct Cache {
		template<typename Set>
		void extractIdsFromSet(Set& set) {
			idList.clear();
			for (auto it = set.begin(); it != set.end(); ) {
				idList.push_back(std::move(set.extract(it++).value()));
			}
		}

		std::vector<u32> idList;
		std::map<std::vector<u32>, std::vector<u32>> archetypeComponent;
	} cache;

	std::vector<flecs::entity> allEntityManagement;
	std::vector<flecs::system> forceComponentChangeSystem;

	std::unordered_map<u32, std::function<void(Serializer& ser, void* data)>> serializeRecords;
	std::unordered_map<u32, std::function<void(Deserializer& ser, void* data)>> deserializeRecords;

	std::unordered_map<u32, std::unordered_set<u32>> highPiorityComponentsUpdates;
	std::unordered_map<u32, std::unordered_set<u32>> lowPiorityComponentsUpdates;

	std::unordered_map<u32, std::set<u32>> componentsCreated;
	std::unordered_map<u32, std::set<u32>> componentsDestroyed;
	std::unordered_set<u32> entitiesDestroyed;
	std::vector<u32> entitiesEnabled;
	std::vector<u32> entitiesDisabled;
};

/* Physics Syncing */

// Must be defined here so it may be used with PhysicsWorldNetworkManager
struct ShapeComponent : public NetworkedComponent {
	u32 shape = UINT32_MAX;

	bool isValid() {
		return shape != UINT32_MAX && getPhysicsWorld().doesShapeExist(shape);
	}

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

		query.iter([&](flecs::iter& iter, ShapeComponent* shapesArray) {
			for (auto i : iter) {
				Shape& shape = physicsWorld.getShape(shapesArray[i].shape);

				if (shape.isNetworkDirty()) {
					shapeUpdates[shape.getType()].push_back(shapesArray[i].shape);
					shape.resetNetworkDirty();
				}
			}
			});

		return shapeUpdates.size() > 0;
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

extern u64 getCurrentStateId();
extern void transitionState(u64 id, bool immediate);
extern bool hasStateChanged();

namespace impl {
	enum SnapshotBitIndexes : u8 {
		STATE_SERIALIZED = 1 << 0,
		PHYSICS_SERIALIZED = 1 << 1,
		META_SERIALIZED = 1 << 2,
		HIGH_PIORITY_SERIALIZED = 1 << 3,
		LOW_PIORITY_SERIALIZED = 1 << 4
	};
};

/**
 * The snapshot manager is responsible for putting together
 * of what happens in between frames.
 */
class NetworkSnapshotManager {
public:
	NetworkSnapshotManager() 
		: reliableSnapshotser(startSerialize(reliableSnapshots)), unreliableSnapshotser(startSerialize(unreliableSnapshots)) {}

	MessageBuffer& getReliableSnapshotCompilation() {
		return reliableSnapshots;
	}

	MessageBuffer& getUnreliableSnapshotCompilation() {
		return unreliableSnapshots;
	}

	void serializeFullSnapshot(MessageBuffer& buffer) {
		EntityWorldNetworkManager& world = getEntityWorldNetworkManager();
		PhysicsWorldNetworkManager& physics = getPhysicsWorldNetworkManager();

		world.yeildAllComponenents();
		physics.yieldAll();

		Serializer ser = startSerialize(buffer);
		ser.object(MESSAGE_HEADER_SNAPSHOT_FULL);
		ser.object(getCurrentStateId());
		physics.serializePhysicsWorldSnapshot(ser);
		world.serializeEntityComponentMetaSnapshot(ser, true);
		world.serializeHighPiorityComponentUpdates(ser);
	}

	void beginSnapshotCompilation() {
		u64 tickCount = getCurrentTick();

		clearSnapshotCompilation();

		reliableSnapshotser = startSerialize(reliableSnapshots);
		reliableSnapshotser.object(MESSAGE_HEADER_SNAPSHOT_COMPILATION_RELIABLE);
		reliableSnapshotser.object(tickCount);

		unreliableSnapshotser = startSerialize(unreliableSnapshots);
		unreliableSnapshotser.object(MESSAGE_HEADER_SNAPSHOT_COMPILATION_UNRELIABLE);
		unreliableSnapshotser.object(tickCount);
		compilationStarted = true;
	}

	void endSnapshotCompilation() {
		compilationStarted = false;
		endSerialize(unreliableSnapshotser, unreliableSnapshots);
		endSerialize(reliableSnapshotser, reliableSnapshots);
	}

	void clearSnapshotCompilation() {
		compilationStarted = false;
		reliableSnapshots.reset();
		unreliableSnapshots.reset();
	}

	bool hasSnapshotCompilationStarted() {
		return compilationStarted;
	}

	void beginSnapshot() {}
	
	void endSnapshot() {
		serializeReliableSnapshot();
		serializeUnreliableSnapshot();
	}

public: // static methods
	static void deserializeSnapshotCompilationHeader(Deserializer& des, u64& startingTickNum) {
		des.object(startingTickNum);
	}

	/* deserializes the header data of a snapshot into the output parameters */
	static void deserializeSnapshotHeader(Deserializer& des, u8& flags, u32& size) {
		des.object(flags);
		des.object(size);
	}

	static void deserializeSnapshotBuffer(Deserializer& des, u32 snapshotSize, MessageBuffer& buffer) {
		size_t oldSize = buffer.getSize();

		buffer.addSize(snapshotSize);
		des.adapter().readBuffer<1, u8>(buffer.getData() + oldSize, (size_t)snapshotSize);
	}

	static void updateAllWithSnapshotBuffer(u8 flags, const MessageBuffer& buffer) {
		if (flags == 0) {
			return;
		}
		
		flecs::world& world = getEntityWorld();
		EntityWorldNetworkManager& worldManager = getEntityWorldNetworkManager();
		PhysicsWorldNetworkManager& physicsManager = getPhysicsWorldNetworkManager();
		Deserializer des = startDeserialize(buffer.getSize(), buffer.getData());

		world.enable_range_check(false);

		if (flags & impl::STATE_SERIALIZED) {
			u64 stateId;
			des.object(stateId);
			transitionState(stateId, true);
		}
		if (flags & impl::PHYSICS_SERIALIZED)
			physicsManager.deserializePhysicsWorldSnapshot(des);
		if (flags & impl::META_SERIALIZED)
			worldManager.deserializeEntityComponentMetaSnapshot(des);
		if (flags & impl::HIGH_PIORITY_SERIALIZED)
			worldManager.deserializeComponentSnapshot(des);
		if(flags & impl::LOW_PIORITY_SERIALIZED)
			worldManager.deserializeComponentSnapshot(des);

		world.enable_range_check(true);

		if (!endDeserialize(des)) {
			ae::log(ae::ERROR_SEVERITY_WARNING, "Failed to deserialize unreliable snapshot: %i\n", (int)des.adapter().error());
		}
	}

	static void deserializeFullSnapshot(Deserializer& des) {
		EntityWorldNetworkManager& worldManager = getEntityWorldNetworkManager();
		PhysicsWorldNetworkManager& physicsManager = getPhysicsWorldNetworkManager();

		// must clear all current state! (Only the networked entities of course)
		flecs::world& world = getEntityWorld();
		world.remove<NetworkedEntity>();
		world.delete_with<NetworkedEntity>();
		world.add<NetworkedEntity>();

		u64 stateId;
		des.object(stateId);
		transitionState(stateId, true);
		getEntityWorld().enable_range_check(false);
		physicsManager.deserializePhysicsWorldSnapshot(des);
		worldManager.deserializeEntityComponentMetaSnapshot(des);
		worldManager.deserializeComponentSnapshot(des);
		getEntityWorld().enable_range_check(true);
	}

private:
	void serializeReliableSnapshot() {
		EntityWorldNetworkManager& worldManager = getEntityWorldNetworkManager();
		PhysicsWorldNetworkManager& physicsManager = getPhysicsWorldNetworkManager();

		u8 snapshotFlags = 0;
		if (hasStateChanged())
			snapshotFlags |= impl::STATE_SERIALIZED;
		if (physicsManager.isPhysicsWorldSnapshotReady())
			snapshotFlags |= impl::PHYSICS_SERIALIZED;
		if (worldManager.isEntityComponentMetaSnapshotReady())
			snapshotFlags |= impl::META_SERIALIZED;
		if (worldManager.isHighPiorityComponentSnapshotReady())
			snapshotFlags |= impl::HIGH_PIORITY_SERIALIZED;

		reliableSnapshotser.object(snapshotFlags);

		u32 size = 0;
		OutputAdapter& adapter = reliableSnapshotser.adapter();
		u32 prevWritePos = adapter.currentWritePos();
		reliableSnapshotser.object<u32>(0);

		if (snapshotFlags & impl::STATE_SERIALIZED)
			reliableSnapshotser.object(getCurrentStateId());
		if (snapshotFlags & impl::PHYSICS_SERIALIZED)
			physicsManager.serializePhysicsWorldSnapshot(reliableSnapshotser);
		if (snapshotFlags & impl::META_SERIALIZED)
			worldManager.serializeEntityComponentMetaSnapshot(reliableSnapshotser);
		if (snapshotFlags & impl::HIGH_PIORITY_SERIALIZED)
			worldManager.serializeHighPiorityComponentUpdates(reliableSnapshotser);

		u32 postWritePos = adapter.currentWritePos();
		adapter.currentWritePos(prevWritePos);
		size = postWritePos - prevWritePos - sizeof(u32);
		reliableSnapshotser.object(size);
		adapter.currentWritePos(postWritePos);
	}

	void serializeUnreliableSnapshot() {
		EntityWorldNetworkManager& worldManager = getEntityWorldNetworkManager();
		PhysicsWorldNetworkManager& physicsManager = getPhysicsWorldNetworkManager();

		u8 snapshotFlags = 0;
		if (worldManager.isLowPiorityComponentSnapshotReady())
			snapshotFlags |= impl::LOW_PIORITY_SERIALIZED;

		unreliableSnapshotser.object(snapshotFlags);

		u32 size = 0;
		OutputAdapter& adapter = unreliableSnapshotser.adapter();
		u32 prevWritePos = adapter.currentWritePos();
		unreliableSnapshotser.object<u32>(0);

		if (snapshotFlags != 0) {
			worldManager.serializeLowPiorityComponentUpdates(unreliableSnapshotser);
		}

		u32 postWritePos = adapter.currentWritePos();
		adapter.currentWritePos(prevWritePos);
		size = postWritePos - prevWritePos - sizeof(u32); // Don't include the size of SIZE
		unreliableSnapshotser.object(size);
		adapter.currentWritePos(postWritePos);
	}
private:
	bool compilationStarted = false;

	// order of members matter here fyi
	MessageBuffer reliableSnapshots;
	MessageBuffer unreliableSnapshots;
	Serializer reliableSnapshotser;
	Serializer unreliableSnapshotser;
};

extern void enableSnapshots();
extern void disableSnapshots();
extern NetworkSnapshotManager& getNetworkSnapshotManager();
extern bool isSnapshotsEnabled();

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
	/* When an entity is by the client, it will start at this range.
	   I.E. the default client range for created entites. */
	static constexpr u64 defaultLocalEntityRange = 1000000;
	static constexpr size_t defaultMaxDsyncBeforeFullSnapshot = 30;

	ClientInterface() {
		flecs::world& world = getEntityWorld();

		world.set_entity_range(defaultLocalEntityRange, UINT64_MAX);
		world.enable_range_check(true);

		getEntityWorldNetworkManager().disableAllSerialization();
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

	/**
	 * @brief returns the currently processed server tick
	 */
	u32 getCurrentServerTick() const {
		return currentServerTick;
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
		// so if some function wants to send a message right after open even when we aren't technically connected
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

	bool _internalOnMessageRecieved(HSteamNetConnection conn, MessageHeader header, Deserializer& des, u32 size, const void* data) override {
		flecs::world& world = getEntityWorld();

		switch (header) {
		case MESSAGE_HEADER_SNAPSHOT_COMPILATION_RELIABLE: {
			deserializeReliableSnapshotCompilation(des, data);
		} break;

		case MESSAGE_HEADER_SNAPSHOT_COMPILATION_UNRELIABLE: {
			deserializeUnreliableSnapshotCompilation(des, data);
		} break;

		case MESSAGE_HEADER_SNAPSHOT_FULL: {
			sentFullSnapshotRequest = false;
			while (!snapshotsQueue.empty())
				snapshotsQueue.pop();
			snapshots.clear();

			NetworkSnapshotManager::deserializeFullSnapshot(des);
		} break;

		snapshotArrivedLate:
		default:
			return true;
		}

		return false;
	}

	void beginTick() override {
		if(snapshots.size() > defaultMaxDsyncBeforeFullSnapshot && !sentFullSnapshotRequest) {
			log(ERROR_SEVERITY_WARNING, "Possible dysnc, requesting full snapshot\n");
			// We add a warning to the connection because if the D-sync is too large
			// frequently, we should just give up on the connection
			getNetworkManager().connectionAddWarning(conn);

			MessageBuffer buffer;
			Serializer ser = startSerialize(buffer);
			ser.object(MESSAGE_HEADER_REQUEST_SNAPSHOT_FULL);
			endSerialize(ser, buffer);
	
			getNetworkManager().sendMessage(0, std::move(buffer), true, true);

			sentFullSnapshotRequest = true;
		}

		if(!snapshotsQueue.empty()) {
			currentServerTick = snapshotsQueue.front();

			Snapshot& snapshot = snapshots[currentServerTick];
			NetworkSnapshotManager::updateAllWithSnapshotBuffer(snapshot.flags, snapshot.buffer);

			snapshotsQueue.pop();
			snapshots.erase(currentServerTick);
		}
	}

	void endTick() override {
	
	}

private:
	void deserializeReliableSnapshotCompilation(Deserializer& des, const void* data) {
		u64 tickNum;
		NetworkSnapshotManager::deserializeSnapshotCompilationHeader(des, tickNum);

		for (; !des.adapter().isCompletedSuccessfully(); tickNum++) {
			u8 snapshotFlags;
			u32 snapshotSize = 0;
			NetworkSnapshotManager::deserializeSnapshotHeader(des, snapshotFlags, snapshotSize);

			snapshotsQueue.push(tickNum);
			NetworkSnapshotManager::deserializeSnapshotBuffer(des, snapshotSize, snapshots[tickNum].buffer);
			snapshots[tickNum].flags |= snapshotFlags;
		}
	}

	void deserializeUnreliableSnapshotCompilation(Deserializer& des, const void* data) {
		u64 tickNum;
		NetworkSnapshotManager::deserializeSnapshotCompilationHeader(des, tickNum);

		for(; !des.adapter().isCompletedSuccessfully(); tickNum++) {
			u8 snapshotFlags;
			u32 snapshotSize = 0;
			NetworkSnapshotManager::deserializeSnapshotHeader(des, snapshotFlags, snapshotSize);

			// do not add the snapshot if the it is less then the current server tick.
			// This means that the unreliable snapshot arrived too late and the 
			// tick was already executed!
			if(tickNum < currentServerTick || snapshots.find(tickNum) == snapshots.end()) {
				des.adapter().currentReadPos(des.adapter().currentReadPos() + snapshotSize);
				continue;
			}

			NetworkSnapshotManager::deserializeSnapshotBuffer(des, snapshotSize, snapshots[tickNum].buffer);
			snapshots[tickNum].flags |= snapshotFlags;
		}
	}

protected:
	struct Snapshot {
		u8 flags = 0;
		MessageBuffer buffer;
	};

	std::map<u32, Snapshot> snapshots;
	std::queue<u32> snapshotsQueue;
	u32 currentServerTick = 0;

	bool sentFullSnapshotRequest = false;
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
		enableSnapshots();
	}

	/**
	 * @brief Sends the snapshot compilation created within the
	 * NetworkSnapshotManger and sends it to all clients.
	 */
	void snapshotUpdate() {
		assert(isSnapshotsEnabled());
		NetworkSnapshotManager& snapshotManager = getNetworkSnapshotManager();
		NetworkManager& networkManager = getNetworkManager();

		if(!snapshotManager.hasSnapshotCompilationStarted()) {
			snapshotManager.beginSnapshotCompilation();
			return;
		}

		snapshotManager.endSnapshotCompilation();

		MessageBuffer& reliableSnapshots = snapshotManager.getReliableSnapshotCompilation();
		MessageBuffer& unreliableSnapshots = snapshotManager.getUnreliableSnapshotCompilation();

		networkManager.sendMessage(0, std::move(reliableSnapshots), true, true);
		networkManager.sendMessage(0, std::move(unreliableSnapshots), true, false);

		snapshotManager.beginSnapshotCompilation();
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
		NetworkSnapshotManager& networkSnapshotManager = getNetworkSnapshotManager();

		MessageBuffer buffer;
		networkSnapshotManager.serializeFullSnapshot(buffer);

		getNetworkManager().sendMessage(who, std::move(buffer), false, true);
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
	void beginTick() override {
		getNetworkSnapshotManager().beginSnapshot();
	}

	void endTick() override {
		getNetworkSnapshotManager().endSnapshot();
	}

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

	bool _internalOnMessageRecieved(HSteamNetConnection conn, MessageHeader header, Deserializer& des, u32 size, const void* data) override {
		switch(header) {
		case MESSAGE_HEADER_REQUEST_SNAPSHOT_FULL:
			fullSyncUpdate(conn);
			break;

		default:
			return true;
		}

		return false;
	}

	void _internalUpdate() override {
		if(hasStateChanged())
			needStateUpdate = true;

		networkUpdate.update();
	}

protected:
	HSteamListenSocket listen = k_HSteamListenSocket_Invalid;

private:
	bool needStateUpdate = false;
	Ticker<void(float)> networkUpdate;
};

AE_NAMESPACE_END
