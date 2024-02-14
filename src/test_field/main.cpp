#include <asteroids/asteroids.hpp>

using namespace ae;

struct MessageText {
	std::string str = "";

	template<typename S>
	void serialize(S& s) {
		s.text1b(str, 100);
	}
};

enum CustomMessageHeader : u8 {
	MESSAGE_HEADER_TEXT = MESSAGE_HEADER_CORE_LAST
};

template<typename S>
void serialize(S& s, CustomMessageHeader& header) {
	s.value1b(header);
}

class CustomClientInterface : public ClientInterface {
public:
	void onConnectionJoin(HSteamNetConnection conn) override {
		ae::log("Client- Connection joined!\n");
	}

	void onMessageRecieved(HSteamNetConnection conn, MessageHeader header, Deserializer& des) override {
		switch (header) {
		case MESSAGE_HEADER_TEXT: {
			MessageText text;
			des.object(text);
			ae::log("recieved text: <green>%s<reset>\n", text.str.c_str());
		} break;
		}
	}

	void onConnectionLeave(HSteamNetConnection conn) override {
		ae::log("Client- Connection left!\n");
	}
};

class CustomServerInterface : public ServerInterface {
	void onConnectionJoin(HSteamNetConnection conn) override {
		NetworkManager& networkManager = getNetworkManager();

		ae::log("Server- Connection joined!\n");
		MessageText text;

		MessageBuffer buffer;
		Serializer ser = startSerialize(buffer);
		ser.object(MESSAGE_HEADER_TEXT);
		text.str = "Hello " + std::to_string(conn) + ".";
		ser.object(text);
		endSerialize(ser, buffer);
		networkManager.sendMessage(conn, std::move(buffer), false, true);

		ser = startSerialize(buffer);
		ser.object(MESSAGE_HEADER_TEXT);
		text.str = "A new client just joined, " + std::to_string(conn) + "\n";
		ser.object(text);
		endSerialize(ser, buffer);
		networkManager.sendMessage(conn, std::move(buffer), true, true);

		// boom boom boom, i want you in my room
		fullSyncUpdate(conn);
	}

	void onConnectionLeave(HSteamNetConnection conn) override {
		ae::log("Server- Connection left!\n");
	}
};

struct TestComponent : NetworkedComponent {
	i8 someTestData = 0;

	template<typename S>
	void serialize(S& s) {
		s.value1b(someTestData);
	}
};

class InitStateModule {
public:
	InitStateModule(flecs::world& world) {}
};

class ViewState;

class InitState : public State {
public:
	void onEntry() override {
		createMainGui();
	}

	void onLeave() override {
		getGui().removeAllWidgets();
		openFailedText = nullptr;
	}

	void onTick(float deltaTime) override {
		if(getNetworkManager().hasNetworkInterface()) {
			auto& networkInterface = getNetworkManager().getNetworkInterface();

			if(networkInterface.hasFailed() && openFailedText)
				openFailedText->setVisible(true);

			if(networkInterface.isOpen())
				transitionState<ViewState>();
		}
	}

protected:
	static void onServerButtonClick() {
		NetworkManager& networkManager = getNetworkManager();

		std::shared_ptr<ServerInterface> interface = std::make_shared<CustomServerInterface>();
		networkManager.setNetworkInterface(interface);

		SteamNetworkingIPAddr addr;
		addr.Clear();
		addr.SetIPv4(0, 9999);
		networkManager.open(addr);

		flecs::entity test = getEntityWorldNetworkManager().entity();
		test.add<TestComponent>().add<NetworkedEntity>().set([](TransformComponent& transform, ShapeComponent& comp) {
			PhysicsWorld& world = getPhysicsWorld();

			comp.shape = world.createShape<Circle>(10.0f);
			transform.setPos(sf::Vector2f(50.0f, 50.0f));
		});
	}

	static void onClientButtonClick() {
		NetworkManager& networkManager = getNetworkManager();
		InitState& self = getCurrentState<InitState>();

		std::shared_ptr<ClientInterface> interface = std::make_shared<CustomClientInterface>();
		networkManager.setNetworkInterface(interface);
		
		self.createClientMenu();
	}

	static void onConnectClick(tgui::EditBox::Ptr editBox) {
		NetworkManager& networkManager = getNetworkManager();
		InitState& self = getCurrentState<InitState>();

		SteamNetworkingIPAddr addr;
		addr.Clear();
		addr.ParseString(((std::string)editBox->getText()).c_str());
		addr.m_port = 9999;
		if(!networkManager.open(addr)) {
			ae::log(ERROR_SEVERITY_WARNING, "Failed to open client\n");
		}

		self.openFailedText->setVisible(false);
	}

	void createMainGui() {
		tgui::BackendGui& gui = getGui();
		
		gui.removeAllWidgets();
		
		auto serverButton = tgui::Button::create();
		serverButton->setText("Server");
		serverButton->setPosition("25%", "50%");
		serverButton->setSize("175", "120");
		serverButton->setOrigin(0.5f, 0.5f);
		serverButton->setTextSize(38);
		serverButton->onClick(onServerButtonClick);
		gui.add(serverButton);

		auto clientButton = tgui::Button::create();
		clientButton->setText("Client");
		clientButton->setPosition("75%", "50%");
		clientButton->setSize("175", "120");
		clientButton->setOrigin(0.5f, 0.5f);
		clientButton->setTextSize(38);
		clientButton->onClick(onClientButtonClick);
		gui.add(clientButton);
	}

	void createClientMenu() {
		tgui::BackendGui& gui = getGui();

		gui.removeAllWidgets();

		auto ipAddress = tgui::EditBox::create();
		ipAddress->setSize("50%", "10%");
		ipAddress->setPosition("50%", "50%");
		ipAddress->setOrigin(0.5f, 0.5f);
		ipAddress->setDefaultText("Enter IP Adress");
		gui.add(ipAddress);

		auto connectButton = tgui::Button::create();
		connectButton->setSize("50%", "10%");
		connectButton->setPosition("50%", "60%");
		connectButton->setOrigin(0.5f, 0.5f);
		connectButton->setText("Connect");
		connectButton->onPress(onConnectClick, ipAddress);
		gui.add(connectButton);

		openFailedText = tgui::Label::create();
		openFailedText->setSize("50%", "10%");
		openFailedText->setPosition("50%", "40%");
		openFailedText->setOrigin(0.5f, 0.5f);
		openFailedText->setText("Failed to connect!");
		openFailedText->setVisible(false);
		openFailedText->getRenderer()->setTextColor(tgui::Color::White);
		gui.add(openFailedText);
	}

private:
	tgui::Label::Ptr openFailedText = nullptr;
};

class ViewState : public State {
public:
	
};

int main(int argc, char* argv[]) {
	getWindow().setTitle("Test field");

	registerState<InitState, InitStateModule>();
	registerState<ViewState>();
	transitionState<InitState>();

	EntityWorldNetworkManager& worldNetworkManager = getEntityWorldNetworkManager();
	worldNetworkManager.registerComponent<TestComponent>();
	
	auto q = getEntityWorld().query<ShapeComponent>();

	setUpdateCallback([&](){
		PhysicsWorld& physicsWorld = getPhysicsWorld();
		flecs::world& entityWorld = getEntityWorld();
		
		q.each([&](ShapeComponent& comp){
			if(physicsWorld.getShape(comp.shape).getType() == ShapeEnum::Circle) {
				Circle& circle = physicsWorld.getCircle(comp.shape);
				sf::CircleShape drawCircle(circle.getRadius());
				drawCircle.setPosition(circle.getPos());
				getWindow().draw(drawCircle);
			}
		});	
	});

	mainLoop();

	return 0;
}