#include "animation_editor.h"
#include "animation/animation.h"
#include "animation/animation_scene.h"
#include "animation/controller.h"
#include "animation/editor/state_machine_editor.h"
#include "animation/events.h"
#include "animation/state_machine.h"
#include "editor/asset_browser.h"
#include "editor/ieditor_command.h"
#include "editor/platform_interface.h"
#include "editor/property_grid.h"
#include "editor/utils.h"
#include "editor/world_editor.h"
#include "engine/blob.h"
#include "engine/crc32.h"
#include "engine/engine.h"
#include "engine/fs/os_file.h"
#include "engine/hash_map.h"
#include "engine/log.h"
#include "engine/path.h"
#include "engine/path_utils.h"
#include "engine/plugin_manager.h"
#include "engine/properties.h"
#include "engine/resource_manager.h"
#include "engine/universe/universe.h"
#include <SDL.h>


static ImVec2 operator+(const ImVec2& lhs, const ImVec2& rhs) { return ImVec2(lhs.x + rhs.x, lhs.y + rhs.y); }


namespace Lumix
{


static const ComponentType ANIMABLE_HASH = Properties::getComponentType("animable");
static const ComponentType CONTROLLER_TYPE = Properties::getComponentType("anim_controller");
static const ResourceType ANIMATION_TYPE("animation");
static const ResourceType CONTROLLER_RESOURCE_TYPE("anim_controller");


namespace AnimEditor
{


struct BeginGroupCommand LUMIX_FINAL : IEditorCommand
{
	BeginGroupCommand() {}
	BeginGroupCommand(WorldEditor&) {}

	bool execute() override { return true; }
	void undo() override { ASSERT(false); }
	void serialize(JsonSerializer& serializer) override {}
	void deserialize(JsonSerializer& serializer) override {}
	bool merge(IEditorCommand& command) override { ASSERT(false); return false; }
	const char* getType() override { return "begin_group"; }
};


struct EndGroupCommand LUMIX_FINAL : IEditorCommand
{
	EndGroupCommand() {}
	EndGroupCommand(WorldEditor&) {}

	bool execute() override { return true; }
	void undo() override { ASSERT(false); }
	void serialize(JsonSerializer& serializer) override {}
	void deserialize(JsonSerializer& serializer) override {}
	bool merge(IEditorCommand& command) override { ASSERT(false); return false; }
	const char* getType() override { return "end_group"; }

	u32 group_type;
};


struct MoveAnimNodeCommand : IEditorCommand
{
	MoveAnimNodeCommand(ControllerResource& controller, Node* node, const ImVec2& pos)
		: m_controller(controller)
		, m_node_uid(node->engine_cmp->uid)
		, m_new_pos(pos)
		, m_old_pos(node->pos)
	{
	}


	bool execute() override
	{
		auto* node = (Node*)m_controller.getByUID(m_node_uid);
		node->pos = m_new_pos;
		return true;
	}


	void undo() override
	{
		auto* node = (Node*)m_controller.getByUID(m_node_uid);
		node->pos = m_old_pos;
	}


	const char* getType() override { return "move_anim_node"; }


	bool merge(IEditorCommand& command) override
	{ 
		auto& cmd = (MoveAnimNodeCommand&)command;
		if (m_node_uid != cmd.m_node_uid || &cmd.m_controller != &m_controller) return false;
		cmd.m_new_pos = m_new_pos;
		return true;
	}


	void serialize(JsonSerializer& serializer) override
	{
		// TODO
		ASSERT(false);
	}


	void deserialize(JsonSerializer& serializer) override
	{
		// TODO
		ASSERT(false);
	}

	ControllerResource& m_controller;
	int m_node_uid;
	ImVec2 m_new_pos;
	ImVec2 m_old_pos;
};


struct CreateAnimNodeCommand : IEditorCommand
{
	CreateAnimNodeCommand(ControllerResource& controller,
		Container* container,
		Anim::Component::Type type,
		const ImVec2& pos)
		: m_controller(controller)
		, m_node_uid(-1)
		, m_container_uid(container->engine_cmp->uid)
		, m_type(type)
		, m_pos(pos)
	{
	}


	bool execute() override
	{
		auto* container = (Container*)m_controller.getByUID(m_container_uid);
		if (m_node_uid < 0) m_node_uid = m_controller.createUID();
		container->createNode(m_type, m_node_uid, m_pos);
		return true;
	}


	void undo() override
	{
		auto* container = (Container*)m_controller.getByUID(m_container_uid);
		container->destroyChild(m_node_uid);
	}


	const char* getType() override { return "create_anim_node"; }


	bool merge(IEditorCommand& command) override { return false; }


	void serialize(JsonSerializer& serializer) override
	{
		// TODO
		ASSERT(false);
	}


	void deserialize(JsonSerializer& serializer) override
	{
		// TODO
		ASSERT(false);
	}

	ControllerResource& m_controller;
	int m_container_uid;
	int m_node_uid;
	ImVec2 m_pos;
	Anim::Component::Type m_type;
};


struct DestroyAnimEdgeCommand : IEditorCommand
{
	DestroyAnimEdgeCommand(ControllerResource& controller, int edge_uid)
		: m_controller(controller)
		, m_edge_uid(edge_uid)
		, m_original_values(controller.getAllocator())
	{
		Edge* edge = (Edge*)m_controller.getByUID(edge_uid);
		ASSERT(!edge->isNode());
		Container* parent = edge->getParent();
		ASSERT(parent);
		m_original_container_uid = parent->engine_cmp->uid;
		m_from_uid = edge->getFrom()->engine_cmp->uid;
		m_to_uid = edge->getTo()->engine_cmp->uid;
	}


	bool execute() override
	{
		m_original_values.clear();
		Edge* edge = (Edge*)m_controller.getByUID(m_edge_uid);
		edge->serialize(m_original_values);
		LUMIX_DELETE(m_controller.getAllocator(), edge);
		return true;
	}


	void undo() override
	{
		
		Container* container = (Container*)m_controller.getByUID(m_original_container_uid);
		container->createEdge(m_from_uid, m_to_uid, m_edge_uid);
		Edge* edge = (Edge*)container->getByUID(m_edge_uid);
		InputBlob input(m_original_values);
		edge->deserialize(input);
	}


	const char* getType() override { return "destroy_anim_edge"; }


	bool merge(IEditorCommand& command) override { return false; }


	void serialize(JsonSerializer& serializer) override
	{
		ASSERT(false);
	}


	void deserialize(JsonSerializer& serializer) override
	{
		ASSERT(false);
	}

	ControllerResource& m_controller;
	int m_edge_uid;
	int m_from_uid;
	int m_to_uid;
	OutputBlob m_original_values;
	int m_original_container_uid;
};


struct DestroyNodeCommand : IEditorCommand
{
	DestroyNodeCommand(ControllerResource& controller, int node_uid)
		: m_controller(controller)
		, m_node_uid(node_uid)
		, m_original_values(controller.getAllocator())
	{
		Component* cmp = m_controller.getByUID(m_node_uid);
		ASSERT(cmp->isNode());
		Container* parent = cmp->getParent();
		ASSERT(parent);
		m_original_container = parent->engine_cmp->uid;
	}


	bool execute() override
	{
		m_original_values.clear();
		Node* node = (Node*)m_controller.getByUID(m_node_uid);
		node->serialize(m_original_values);
		m_cmp_type = node->engine_cmp->type;
		ASSERT(node->getEdges().empty());
		ASSERT(node->getInEdges().empty());
		LUMIX_DELETE(m_controller.getAllocator(), node);
		return true;
	}


	void undo() override
	{
		auto* container = (Container*)m_controller.getByUID(m_original_container);
		container->createNode(m_cmp_type, m_node_uid, ImVec2(0, 0));
		Component* cmp = m_controller.getByUID(m_node_uid);
		ASSERT(cmp->isNode());
		Node* node = (Node*)cmp;
		InputBlob input(m_original_values);
		cmp->deserialize(input);
	}


	const char* getType() override { return "destroy_anim_node"; }


	bool merge(IEditorCommand& command) override { return false; }


	void serialize(JsonSerializer& serializer) override
	{
		// TODO
		ASSERT(false);
	}


	void deserialize(JsonSerializer& serializer) override
	{
		// TODO
		ASSERT(false);
	}

	ControllerResource& m_controller;
	int m_node_uid;
	OutputBlob m_original_values;
	int m_original_container;
	Anim::Component::Type m_cmp_type;
};



struct CreateAnimEdgeCommand : IEditorCommand
{
	CreateAnimEdgeCommand(ControllerResource& controller, Container* container, Node* from, Node* to)
		: m_controller(controller)
		, m_from_uid(from->engine_cmp->uid)
		, m_to_uid(to->engine_cmp->uid)
		, m_edge_uid(-1)
		, m_container_uid(container->engine_cmp->uid)
	{
	}

	bool execute() override
	{
		auto* container = (Container*)m_controller.getByUID(m_container_uid);
		if (m_edge_uid < 0) m_edge_uid = m_controller.createUID();
		container->createEdge(m_from_uid, m_to_uid, m_edge_uid);
		return true;
	}


	void undo() override
	{
		auto* container = (Container*)m_controller.getByUID(m_container_uid);
		container->destroyChild(m_edge_uid);
	}


	const char* getType() override { return "create_anim_edge"; }


	bool merge(IEditorCommand& command) override { return false; }


	void serialize(JsonSerializer& serializer) override
	{
		// TODO
		ASSERT(false);
	}


	void deserialize(JsonSerializer& serializer) override
	{
		// TODO
		ASSERT(false);
	}

	ControllerResource& m_controller;
	int m_from_uid;
	int m_to_uid;
	int m_container_uid;
	int m_edge_uid;
};


class AnimationEditor : public IAnimationEditor
{
public:
	AnimationEditor(StudioApp& app);
	~AnimationEditor();

	void update(float time_delta) override;
	const char* getName() const override { return "animation_editor"; }
	void setContainer(Container* container) override { m_container = container; }
	bool isEditorOpen() override { return m_editor_open; }
	void toggleEditorOpen() override { m_editor_open = !m_editor_open; }
	bool isInputsOpen() override { return m_inputs_open; }
	void toggleInputsOpen() override { m_inputs_open = !m_inputs_open; }
	void onWindowGUI() override;
	StudioApp& getApp() override { return m_app; }
	int getEventTypesCount() const override;
	EventType& createEventType(const char* type) override;
	EventType& getEventTypeByIdx(int idx) override  { return m_event_types[idx]; }
	EventType& getEventType(u32 type) override;
	void executeCommand(IEditorCommand& command);
	void createEdge(ControllerResource& ctrl, Container* container, Node* from, Node* to) override;
	void moveNode(ControllerResource& ctrl, Node* node, const ImVec2& pos) override;
	void createNode(ControllerResource& ctrl,
		Container* container,
		Anim::Node::Type type,
		const ImVec2& pos) override;
	void destroyNode(ControllerResource& ctrl, Node* node) override;
	void destroyEdge(ControllerResource& ctrl, Edge* edge) override;
	bool hasFocus() override { return m_is_focused; }
	ControllerResource& getController() { return *m_resource; }

private:
	void checkShortcuts();
	void beginCommandGroup(u32 type);
	void endCommandGroup();
	void newController();
	void save();
	void saveAs();
	void drawGraph();
	void load();
	void loadFromEntity();
	void loadFromFile();
	void editorGUI();
	void inputsGUI();
	void constantsGUI();
	void masksGUI();
	void animationSlotsGUI();
	void menuGUI();
	void onSetInputGUI(u8* data, Component& component);
	void undo();
	void redo();
	void clearUndoStack();

private:
	StudioApp& m_app;
	bool m_editor_open;
	bool m_inputs_open;
	ImVec2 m_offset;
	ControllerResource* m_resource;
	Container* m_container;
	StaticString<MAX_PATH_LENGTH> m_path;
	Array<EventType> m_event_types;
	Array<IEditorCommand*> m_undo_stack;
	int m_undo_index = -1;
	bool m_is_playing = false;
	bool m_is_focused = false;
	u32 m_current_group_type;
};


AnimationEditor::AnimationEditor(StudioApp& app)
	: m_app(app)
	, m_editor_open(false)
	, m_inputs_open(false)
	, m_offset(0, 0)
	, m_event_types(app.getWorldEditor().getAllocator())
	, m_undo_stack(app.getWorldEditor().getAllocator())
{
	m_path = "";
	IAllocator& allocator = app.getWorldEditor().getAllocator();

	auto* action = LUMIX_NEW(allocator, Action)("Animation Editor", "animation_editor");
	action->func.bind<AnimationEditor, &AnimationEditor::toggleEditorOpen>(this);
	action->is_selected.bind<AnimationEditor, &AnimationEditor::isEditorOpen>(this);
	app.addWindowAction(action);

	action = LUMIX_NEW(allocator, Action)("Animation Inputs", "animation_inputs");
	action->func.bind<AnimationEditor, &AnimationEditor::toggleInputsOpen>(this);
	action->is_selected.bind<AnimationEditor, &AnimationEditor::isInputsOpen>(this);
	app.addWindowAction(action);

	Engine& engine = m_app.getWorldEditor().getEngine();
	auto* manager = engine.getResourceManager().get(CONTROLLER_RESOURCE_TYPE);
	m_resource = LUMIX_NEW(allocator, ControllerResource)(*this, *manager, allocator);
	m_container = (Container*)m_resource->getRoot();

	EventType& event_type = createEventType("set_input");
	event_type.size = sizeof(Anim::SetInputEvent);
	event_type.label = "Set Input";
	event_type.editor.bind<AnimationEditor, &AnimationEditor::onSetInputGUI>(this);

	Action* undo_action = LUMIX_NEW(allocator, Action)("Undo", "animeditor_undo", SDL_SCANCODE_LCTRL, 'Z', -1);
	undo_action->is_global = true;
	undo_action->plugin = this;
	undo_action->func.bind<AnimationEditor, &AnimationEditor::undo>(this);
	app.addAction(undo_action);

	Action* redo_action = LUMIX_NEW(allocator, Action)("Redo", "animeditor_redo", SDL_SCANCODE_LCTRL, KMOD_SHIFT, 'Z');
	redo_action->is_global = true;
	redo_action->plugin = this;
	redo_action->func.bind<AnimationEditor, &AnimationEditor::redo>(this);
	app.addAction(redo_action);
}


AnimationEditor::~AnimationEditor()
{
	IAllocator& allocator = m_app.getWorldEditor().getAllocator();
	LUMIX_DELETE(allocator, m_resource);
	for (auto& cmd : m_undo_stack) {
		LUMIX_DELETE(allocator, cmd);
	}
}


void AnimationEditor::moveNode(ControllerResource& ctrl, Node* node, const ImVec2& pos)
{
	IAllocator& allocator = m_app.getWorldEditor().getAllocator();
	auto* cmd = LUMIX_NEW(allocator, MoveAnimNodeCommand)(ctrl, node, pos);
	executeCommand(*cmd);
}


void AnimationEditor::createNode(ControllerResource& ctrl,
	Container* container,
	Anim::Node::Type type,
	const ImVec2& pos)
{
	IAllocator& allocator = m_app.getWorldEditor().getAllocator();
	auto* cmd = LUMIX_NEW(allocator, CreateAnimNodeCommand)(ctrl, container, type, pos);
	executeCommand(*cmd);
}


void AnimationEditor::destroyEdge(ControllerResource& ctrl, Edge* edge)
{
	IAllocator& allocator = m_app.getWorldEditor().getAllocator();
	auto* cmd = LUMIX_NEW(allocator, DestroyAnimEdgeCommand)(ctrl, edge->engine_cmp->uid);
	executeCommand(*cmd);
}


void AnimationEditor::destroyNode(ControllerResource& ctrl, Node* node)
{
	beginCommandGroup(crc32("destroy_node_group"));
	
	while (!node->getEdges().empty())
	{
		destroyEdge(ctrl, node->getEdges().back());
	}

	while (!node->getInEdges().empty())
	{
		destroyEdge(ctrl, node->getInEdges().back());
	}

	IAllocator& allocator = m_app.getWorldEditor().getAllocator();
	auto* cmd = LUMIX_NEW(allocator, DestroyNodeCommand)(ctrl, node->engine_cmp->uid);
	executeCommand(*cmd);
	endCommandGroup();
}


void AnimationEditor::createEdge(ControllerResource& ctrl, Container* container, Node* from, Node* to)
{
	IAllocator& allocator = m_app.getWorldEditor().getAllocator();
	auto* cmd = LUMIX_NEW(allocator, CreateAnimEdgeCommand)(ctrl, container, from, to);
	executeCommand(*cmd);
}


void AnimationEditor::beginCommandGroup(u32 type)
{
	IAllocator& allocator = m_app.getWorldEditor().getAllocator();

	if (m_undo_index < m_undo_stack.size() - 1)
	{
		for (int i = m_undo_stack.size() - 1; i > m_undo_index; --i)
		{
			LUMIX_DELETE(allocator, m_undo_stack[i]);
		}
		m_undo_stack.resize(m_undo_index + 1);
	}

	if (m_undo_index >= 0)
	{
		static const u32 end_group_hash = crc32("end_group");
		if (crc32(m_undo_stack[m_undo_index]->getType()) == end_group_hash)
		{
			if (static_cast<EndGroupCommand*>(m_undo_stack[m_undo_index])->group_type == type)
			{
				LUMIX_DELETE(allocator, m_undo_stack[m_undo_index]);
				--m_undo_index;
				m_undo_stack.pop();
				return;
			}
		}
	}

	m_current_group_type = type;
	auto* cmd = LUMIX_NEW(allocator, BeginGroupCommand);
	m_undo_stack.push(cmd);
	++m_undo_index;
}


void AnimationEditor::endCommandGroup()
{
	IAllocator& allocator = m_app.getWorldEditor().getAllocator();

	if (m_undo_index < m_undo_stack.size() - 1)
	{
		for (int i = m_undo_stack.size() - 1; i > m_undo_index; --i)
		{
			LUMIX_DELETE(allocator, m_undo_stack[i]);
		}
		m_undo_stack.resize(m_undo_index + 1);
	}

	auto* cmd = LUMIX_NEW(allocator, EndGroupCommand);
	cmd->group_type = m_current_group_type;
	m_undo_stack.push(cmd);
	++m_undo_index;
}


void AnimationEditor::executeCommand(IEditorCommand& command)
{
	// TODO clean memory in destructor
	IAllocator& allocator = m_app.getWorldEditor().getAllocator();
	while (m_undo_stack.size() > m_undo_index + 1)
	{
		LUMIX_DELETE(allocator, m_undo_stack.back());
		m_undo_stack.pop();
	}

	if (!m_undo_stack.empty())
	{
		auto* back = m_undo_stack.back();
		if (back->getType() == command.getType())
		{
			if (command.merge(*back))
			{
				back->execute();
				LUMIX_DELETE(allocator, &command);
				return;
			}
		}
	}

	m_undo_stack.push(&command);
	++m_undo_index;
	command.execute();
}


AnimationEditor::EventType& AnimationEditor::getEventType(u32 type)
{
	for (auto& i : m_event_types)
	{
		if (i.type == type) return i;
	}
	return m_event_types[0];
}


void AnimationEditor::onSetInputGUI(u8* data, Component& component)
{
	auto event = (Anim::SetInputEvent*)data;
	auto& input_decl = component.getController().getEngineResource()->m_input_decl;
	auto getter = [](void* data, int idx, const char** out) -> bool {
		const auto& input_decl = *(Anim::InputDecl*)data;
		int i = input_decl.inputFromLinearIdx(idx);
		*out = input_decl.inputs[i].name;
		return true;
	};
	int idx = input_decl.inputToLinearIdx(event->input_idx);
	ImGui::Combo("Input", &idx, getter, &input_decl, input_decl.inputs_count);
	event->input_idx = input_decl.inputFromLinearIdx(idx);

	if (event->input_idx >= 0 && event->input_idx < lengthOf(input_decl.inputs))
	{
		switch (input_decl.inputs[event->input_idx].type)
		{
			case Anim::InputDecl::BOOL: ImGui::Checkbox("Value", &event->b_value); break;
			case Anim::InputDecl::INT: ImGui::InputInt("Value", &event->i_value); break;
			case Anim::InputDecl::FLOAT: ImGui::InputFloat("Value", &event->f_value); break;
			default: ASSERT(false); break;
		}
	}
}


void AnimationEditor::onWindowGUI()
{
	editorGUI();
	inputsGUI();
}


void AnimationEditor::saveAs()
{
	if (!PlatformInterface::getSaveFilename(m_path.data, lengthOf(m_path.data), "Animation controllers\0*.act\0", "")) return;
	save();
}


void AnimationEditor::save()
{
	if (m_path[0] == 0 &&
		!PlatformInterface::getSaveFilename(m_path.data, lengthOf(m_path.data), "Animation controllers\0*.act\0", ""))
		return;
	IAllocator& allocator = m_app.getWorldEditor().getAllocator();
	OutputBlob blob(allocator);
	m_resource->serialize(blob);
	FS::OsFile file;
	file.open(m_path, FS::Mode::CREATE_AND_WRITE, allocator);
	file.write(blob.getData(), blob.getPos());
	file.close();
}


void AnimationEditor::drawGraph()
{
	ImGui::BeginChild("canvas", ImVec2(0, 0), true);
	if (ImGui::IsWindowHovered() && !ImGui::IsAnyItemActive() && ImGui::IsMouseDragging(2, 0.0f))
	{
		m_offset = m_offset + ImGui::GetIO().MouseDelta;
	}

	auto* scene = (AnimationScene*)m_app.getWorldEditor().getUniverse()->getScene(ANIMABLE_HASH);
	auto& entities = m_app.getWorldEditor().getSelectedEntities();
	Anim::ComponentInstance* runtime = nullptr;
	if (!entities.empty())
	{
		ComponentHandle ctrl = scene->getComponent(entities[0], CONTROLLER_TYPE);
		if (ctrl.isValid())
		{
			runtime = scene->getControllerRoot(ctrl);
		}
	}

	ImDrawList* draw = ImGui::GetWindowDrawList();
	auto canvas_screen_pos = ImGui::GetCursorScreenPos() + m_offset;
	m_container->drawInside(draw, canvas_screen_pos);
	if(runtime) m_resource->getRoot()->debugInside(draw, canvas_screen_pos, runtime, m_container);

	ImGui::EndChild();
}


void AnimationEditor::loadFromEntity()
{
	auto& entities = m_app.getWorldEditor().getSelectedEntities();
	if (entities.empty()) return;
	auto* scene = (AnimationScene*)m_app.getWorldEditor().getUniverse()->getScene(ANIMABLE_HASH);
	ComponentHandle ctrl = scene->getComponent(entities[0], CONTROLLER_TYPE);
	if (!ctrl.isValid()) return;
	m_path = scene->getControllerSource(ctrl).c_str();
	clearUndoStack();
	load();
}


void AnimationEditor::load()
{
	IAllocator& allocator = m_app.getWorldEditor().getAllocator();
	FS::OsFile file;
	file.open(m_path, FS::Mode::OPEN_AND_READ, allocator);
	Array<u8> data(allocator);
	data.resize((int)file.size());
	file.read(&data[0], data.size());
	InputBlob blob(&data[0], data.size());
	if (m_resource->deserialize(blob, m_app.getWorldEditor().getEngine(), allocator))
	{
		m_container = (Container*)m_resource->getRoot();
	}
	else
	{
		LUMIX_DELETE(allocator, m_resource);
		Engine& engine = m_app.getWorldEditor().getEngine();
		auto* manager = engine.getResourceManager().get(CONTROLLER_RESOURCE_TYPE);
		m_resource = LUMIX_NEW(allocator, ControllerResource)(*this, *manager, allocator);
		m_container = (Container*)m_resource->getRoot();
	}
	file.close();
}


void AnimationEditor::loadFromFile()
{
	if (!PlatformInterface::getOpenFilename(m_path.data, lengthOf(m_path.data), "Animation controllers\0*.act\0", "")) return;
	clearUndoStack();
	load();
}


void AnimationEditor::newController()
{
	IAllocator& allocator = m_app.getWorldEditor().getAllocator();
	LUMIX_DELETE(allocator, m_resource);
	Engine& engine = m_app.getWorldEditor().getEngine();
	auto* manager = engine.getResourceManager().get(CONTROLLER_RESOURCE_TYPE);
	m_resource = LUMIX_NEW(allocator, ControllerResource)(*this, *manager, allocator);
	m_container = (Container*)m_resource->getRoot();
	m_path = "";
	clearUndoStack();
}


int AnimationEditor::getEventTypesCount() const
{
	return m_event_types.size();
}


AnimationEditor::EventType& AnimationEditor::createEventType(const char* type)
{
	EventType& event_type = m_event_types.emplace();
	event_type.type = crc32(type);
	return event_type;
}


void AnimationEditor::menuGUI()
{
	if (ImGui::BeginMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("New")) newController();
			if (ImGui::MenuItem("Save")) save();
			if (ImGui::MenuItem("Save As")) saveAs();
			if (ImGui::MenuItem("Open")) loadFromFile();
			if (ImGui::MenuItem("Open from selected entity")) loadFromEntity();
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Edit"))
		{
			if (ImGui::MenuItem("Undo")) undo();
			if (ImGui::MenuItem("Redo")) redo();
			ImGui::EndMenu();
		}

		ImGui::SameLine();
		ImGui::Checkbox("Play", &m_is_playing);
		ImGui::SameLine();
		if (ImGui::MenuItem("Go up", nullptr, false, m_container->getParent() != nullptr))
		{
			m_container = m_container->getParent();
		}

		ImGui::EndMenuBar();
	}
}


void AnimationEditor::redo()
{
	if (m_undo_index == m_undo_stack.size() - 1) return;

	static const u32 end_group_hash = crc32("end_group");
	static const u32 begin_group_hash = crc32("begin_group");

	++m_undo_index;
	if (crc32(m_undo_stack[m_undo_index]->getType()) == begin_group_hash)
	{
		++m_undo_index;
		while (crc32(m_undo_stack[m_undo_index]->getType()) != end_group_hash)
		{
			m_undo_stack[m_undo_index]->execute();
			++m_undo_index;
		}
	}
	else
	{
		m_undo_stack[m_undo_index]->execute();
	}
}


void AnimationEditor::undo()
{
	if (m_undo_index >= m_undo_stack.size() || m_undo_index < 0) return;

	static const u32 end_group_hash = crc32("end_group");
	static const u32 begin_group_hash = crc32("begin_group");

	if (crc32(m_undo_stack[m_undo_index]->getType()) == end_group_hash)
	{
		--m_undo_index;
		while (crc32(m_undo_stack[m_undo_index]->getType()) != begin_group_hash)
		{
			m_undo_stack[m_undo_index]->undo();
			--m_undo_index;
		}
		--m_undo_index;
	}
	else
	{
		m_undo_stack[m_undo_index]->undo();
		--m_undo_index;
	}
}


void AnimationEditor::clearUndoStack()
{
	IAllocator& allocator = m_app.getWorldEditor().getAllocator();
	for (auto& cmd : m_undo_stack) {
		LUMIX_DELETE(allocator, cmd);
	}
	m_undo_stack.clear();
	m_undo_index = -1;
}


void AnimationEditor::checkShortcuts()
{

}


void AnimationEditor::update(float time_delta)
{
	checkShortcuts();
	if (!m_is_playing) return;
	auto& entities = m_app.getWorldEditor().getSelectedEntities();
	if (entities.empty()) return;
	auto* scene = (AnimationScene*)m_app.getWorldEditor().getUniverse()->getScene(ANIMABLE_HASH);
	ComponentHandle ctrl = scene->getComponent(entities[0], CONTROLLER_TYPE);
	if (!ctrl.isValid()) return;
	scene->updateController(ctrl, time_delta);
}


void AnimationEditor::editorGUI()
{
	if (ImGui::BeginDock("Animation Editor", &m_editor_open, ImGuiWindowFlags_MenuBar))
	{
		m_is_focused = ImGui::IsFocusedHierarchy();
		menuGUI();
		ImGui::Columns(2);
		drawGraph();
		ImGui::NextColumn();
		ImGui::Text("Properties");
		if(m_container->getSelectedComponent()) m_container->getSelectedComponent()->onGUI();
		ImGui::Columns();
	}
	else
	{
		m_is_focused = false;
	}
	ImGui::EndDock();
}


void AnimationEditor::inputsGUI()
{
	if (ImGui::BeginDock("Animation inputs", &m_inputs_open))
	{
		if (ImGui::CollapsingHeader("Inputs"))
		{
			const auto& selected_entities = m_app.getWorldEditor().getSelectedEntities();
			auto* scene = (AnimationScene*)m_app.getWorldEditor().getUniverse()->getScene(ANIMABLE_HASH);
			ComponentHandle cmp = selected_entities.empty() ? INVALID_COMPONENT : scene->getComponent(selected_entities[0], CONTROLLER_TYPE);
			u8* input_data = cmp.isValid() ? scene->getControllerInput(cmp) : nullptr;
			Anim::InputDecl& input_decl = m_resource->getEngineResource()->m_input_decl;

			for (auto& input : input_decl.inputs)
			{
				if (input.type == Anim::InputDecl::EMPTY) continue;
				ImGui::PushID(&input);
				ImGui::PushItemWidth(100);
				ImGui::InputText("##name", input.name, lengthOf(input.name));
				ImGui::SameLine();
				if (ImGui::Combo("##type", (int*)&input.type, "float\0int\0bool\0"))
				{
					input_decl.recalculateOffsets();
				}
				if (input_data)
				{
					ImGui::SameLine();
					switch (input.type)
					{
						case Anim::InputDecl::FLOAT: ImGui::DragFloat("##value", (float*)(input_data + input.offset)); break;
						case Anim::InputDecl::BOOL: ImGui::Checkbox("##value", (bool*)(input_data + input.offset)); break;
						case Anim::InputDecl::INT: ImGui::InputInt("##value", (int*)(input_data + input.offset)); break;
						default: ASSERT(false); break;
					}
				}
				ImGui::SameLine();
				if (ImGui::Button("x"))
				{
					input.type = Anim::InputDecl::EMPTY;
					--input_decl.inputs_count;
				}

				ImGui::PopItemWidth();
				ImGui::PopID();
			}

			if (input_decl.inputs_count < lengthOf(input_decl.inputs) && ImGui::Button("Add"))
			{
				for (auto& input : input_decl.inputs)
				{
					if (input.type == Anim::InputDecl::EMPTY)
					{
						input.name[0] = 0;
						input.type = Anim::InputDecl::BOOL;
						input.offset = input_decl.getSize();
						++input_decl.inputs_count;
						break;
					}
				}
			}
		}

		masksGUI();
		constantsGUI();
		animationSlotsGUI();
	}
	ImGui::EndDock();
}


template <typename T> auto getMembers();


template <typename T, typename... Members>
struct ClassDesc
{
	const char* name;
	Tuple<Members...> members;
};


template <typename Getter, typename Setter, typename... Attrs>
struct RWProperty
{
	using C = typename ClassOf<Getter>::Type;

	auto getValue(C& obj) const { return (obj.*getter)(); }
	template <typename T>
	void setValue(C& obj, const T& value) const { (obj.*setter)(value); }

	const char* name;
	Setter setter;
	Getter getter;
	Tuple<Attrs...> attributes;
};


template <typename Getter, typename... Attrs>
struct ROProperty
{
	using C = typename ClassOf<Getter>::Type;

	decltype(auto) getValue(C& obj) const { return (obj.*getter)(); }
	template <typename T>
	void setValue(C& obj, const T& value) const { ASSERT(false); }

	const char* name;
	Getter getter;
	Tuple<Attrs...> attributes;
};


template <typename Member, typename... Attrs>
struct DataProperty
{
	using C = typename ClassOf<Member>::Type;

	auto& getValue(C& obj) const { return obj.*member; }
	template <typename T>
	void setValue(C& obj, const T& value) const { obj.*member = value; }

	const char* name;
	Member member;
	Tuple<Attrs...> attributes;
};


template <typename T, typename... Members>
auto klass(const char* name, Members... members)
{
	ClassDesc<T, Members...> class_desc;
	class_desc.name = name;
	class_desc.members = makeTuple(members...);
	return class_desc;
}


template <typename R, typename C, typename... Attrs>
auto property(const char* name, R(C::*getter)(), Attrs... attrs)
{
	ROProperty<R(C::*)(), Attrs...> prop;
	prop.name = name;
	prop.getter = getter;
	prop.attributes = makeTuple(attrs...);
	return prop;
}


template <typename T, typename C, typename... Attrs>
auto property(const char* name, T (C::*member), Attrs... attrs)
{
	DataProperty<decltype(member), Attrs...> prop;
	prop.name = name;
	prop.member = member;
	prop.attributes = makeTuple(attrs...);
	return prop;
}


template <typename R, typename C, typename T, typename... Attrs>
auto property(const char* name, R& (C::*getter)(), void (C::*setter)(T), Attrs... attrs)
{
	RWProperty<R&(C::*)(), void (C::*)(T), Attrs...> prop;
	prop.name = name;
	prop.getter = getter;
	prop.setter = setter;
	prop.attributes = makeTuple(attrs...);
	return prop;
}



template <typename Adder, typename Remover>
struct ArrayAttribute
{
	Adder adder;
	Remover remover;
};

template <typename Adder, typename Remover>
auto array_attribute(Adder adder, Remover remover)
{
	ArrayAttribute<Adder, Remover> attr;
	attr.adder = adder;
	attr.remover = remover;
	return attr;
}


template <>
auto getMembers<ControllerResource>()
{
	return klass<ControllerResource>("controller",
		property("Masks", &ControllerResource::getMasks,
			array_attribute(&ControllerResource::addMask, &ControllerResource::removeMask)
		)
	);
}


template <>
auto getMembers<ControllerResource::Mask>()
{
	return klass<ControllerResource>("Mask",
		property("Name", &ControllerResource::Mask::getName, &ControllerResource::Mask::setName),
		property("Bones", &ControllerResource::Mask::bones,
			array_attribute(&ControllerResource::Mask::addBone, &ControllerResource::Mask::removeBone)
		)
	);
}

template <>
auto getMembers<ControllerResource::Mask::Bone>()
{
	return klass<ControllerResource::Mask>("Bone",
		property("Name", &ControllerResource::Mask::Bone::getName, &ControllerResource::Mask::Bone::setName)
	);
}



template <typename T, typename PP>
struct SetPropertyCommand : IEditorCommand
{
	SetPropertyCommand(ControllerResource& _controller, PP _pp, T _value) 
		: controller(_controller)
		, pp(_pp) 
		, value(_value)
		, old_value(_value)
	{}


	bool execute() override 
	{
		old_value = pp.getValueFromRoot(controller);
		pp.setValueFromRoot(controller, value);
		return true;
	}


	void undo() override 
	{
		pp.setValueFromRoot(controller, old_value);
	}


	void serialize(JsonSerializer& serializer) override { ASSERT(false); }
	void deserialize(JsonSerializer& serializer) override { ASSERT(false); }
	const char* getType() override { return "set_anim_editor_property"; }
	bool merge(IEditorCommand& command) override { return false; }

	T value;
	T old_value;
	PP pp;
	ControllerResource& controller;
};


struct Serializer
{
	static void serialize(OutputBlob& blob, string& obj)
	{
		blob.write(obj);
	}
	
	static void serialize(OutputBlob& blob, Array<string>& array)
	{
		blob.write(array.size());
		for (string& item : array)
		{
			blob.write(item);
		}
	}

	template <typename T>
	static void serialize(OutputBlob& blob, Array<T>& obj)
	{
		/*auto l = [&blob, &obj](const auto& member) {
			decltype(auto) x = member.getValue(obj);
			serialize(blob, x);
		};
		apply(l, getMembers<T>().members);*/
		ASSERT(false);
	}

	template <typename T>
	static void serialize(OutputBlob& blob, T& obj)
	{
		auto l = [&blob, &obj](const auto& member) {
			decltype(auto) x = member.getValue(obj);
			serialize(blob, x);
		};
		apply(l, getMembers<T>().members);
	}
};




struct Deserializer
{
	template <typename Root, typename PP, typename T>
	static void deserialize(InputBlob& blob, Root& root, PP& pp, T& obj)
	{
		auto l = [&root, &blob, &obj, &pp](const auto& member) {
			auto child_pp = makePP(pp, member);
			auto& value = child_pp.getValue(obj);
			deserialize(blob, root, child_pp, value);
		};
		apply(l, getMembers<T>().members);
	}


	template <typename Root, typename PP>
	static void deserialize(InputBlob& blob, Root& root, PP& pp, string& obj)
	{
		blob.read(obj);
	}


	template <typename Root, typename PP, typename T>
	static void deserialize(InputBlob& blob, Root& root, PP& pp, Array<T>& array)
	{
		int count = blob.read<int>();
		auto& owner = ((PP::Base&)pp).getValueFromRoot(root);
		AddVisitor<decltype(owner)> visitor(owner, -1);
		for (int i = 0; i < count; ++i)
		{
			apply(visitor, pp.head.attributes);
			deserialize(blob, root, makePP(pp, i), array[i]);
		}
	}

};


template <typename PP>
struct RemoveArrayItemCommand : IEditorCommand
{
	RemoveArrayItemCommand(ControllerResource& _controller, PP _pp, int _index)
		: controller(_controller)
		, pp(_pp)
		, index(_index)
		, blob(_controller.getAllocator())
	{}


	bool execute() override
	{
		auto& owner = ((PP::Base&)pp).getValueFromRoot(controller);
		auto& array = pp.getValueFromRoot(controller);
		::Lumix::AnimEditor::Serializer::serialize(blob, array[index]);
		RemoveVisitor<decltype(owner)> visitor(owner, index);
		apply(visitor, pp.head.attributes);
		return true;
	}


	void undo() override
	{
		auto& owner = ((PP::Base&)pp).getValueFromRoot(controller);
		auto& array = pp.getValueFromRoot(controller);
		AddVisitor<decltype(owner)> visitor(owner, index);
		apply(visitor, pp.head.attributes);
		auto item_pp = makePP(pp, index);
		::Lumix::AnimEditor::Deserializer::deserialize(InputBlob(blob), controller, item_pp, item_pp.getValueFromRoot(controller));
	}


	void serialize(JsonSerializer& serializer) override { ASSERT(false); }
	void deserialize(JsonSerializer& serializer) override { ASSERT(false); }
	const char* getType() override { return "remove_array_item_anim_editor_property"; }
	bool merge(IEditorCommand& command) override { return false; }

	OutputBlob blob;
	PP pp;
	int index;
	ControllerResource& controller;
};


template <typename Owner>
struct AddVisitor
{
	AddVisitor(Owner& _owner, int _index) : owner(_owner), index(_index) {}

	template <typename Adder, typename Remover>
	void operator()(const ArrayAttribute<Adder, Remover>& attr) const
	{
		(owner.*attr.adder)(index);
	}

	template <typename T>
	void operator()(const T& x) const {}

	int index;
	Owner& owner;
};


template <typename Owner>
struct RemoveVisitor
{
	RemoveVisitor(Owner& _owner, int _index) 
		: owner(_owner)
		, index(_index)
	{}

	template <typename Adder, typename Remover>
	void operator()(const ArrayAttribute<Adder, Remover>& attr) const
	{
		(owner.*attr.remover)(index);
	}

	template <typename T>
	void operator()(const T& x) const {}

	int index;
	Owner& owner;
};


template <typename PP>
struct AddArrayItemCommand : IEditorCommand
{
	AddArrayItemCommand(ControllerResource& _controller, PP _pp)
		: controller(_controller)
		, pp(_pp)
	{}


	bool execute() override
	{
		auto& owner = ((PP::Base&)pp).getValueFromRoot(controller);
		auto& array = pp.getValueFromRoot(controller);
		AddVisitor<decltype(owner)> visitor(owner, -1);
		apply(visitor, pp.head.attributes);
		return true;
	}


	void undo() override
	{
		auto& owner = ((PP::Base&)pp).getValueFromRoot(controller);
		auto& array = pp.getValueFromRoot(controller);
		RemoveVisitor<decltype(owner)> visitor(owner, array.size() - 1);
		apply(visitor, pp.head.attributes);
	}


	void serialize(JsonSerializer& serializer) override { ASSERT(false); }
	void deserialize(JsonSerializer& serializer) override { ASSERT(false); }
	const char* getType() override { return "add_array_item_anim_editor_property"; }
	bool merge(IEditorCommand& command) override { return false; }

	PP pp;
	ControllerResource& controller;
};


struct PropertyPathBegin 
{
	template <typename T>
	decltype(auto) getValueFromRoot(T& root) const { return root; }
};

template <typename Prev, typename Member>
struct PropertyPath : Prev
{
	using Base = Prev;

	PropertyPath(Prev& prev, Member _head) : Prev(prev), head(_head), name(_head.name) {}

	template <typename T>
	decltype(auto) getValue(T& obj) const { return head.getValue(obj); }

	template <typename T>
	decltype(auto) getValueFromRoot(T& root) const
	{
		return head.getValue(Prev::getValueFromRoot(root));
	}

	template <typename T, typename O>
	void setValueFromRoot(T& root, O value)
	{
		decltype(auto) x = Prev::getValueFromRoot(root);
		head.setValue(x, value);
	}

	Member head;
	const char* name;
};


template <typename Prev>
struct PropertyPathArray : Prev
{
	using Base = Prev;

	PropertyPathArray(Prev& prev, int _index) 
		: Prev(prev), index(_index) {}

	template <typename T>
	auto& getValue(T& obj) const { return obj[index]; }

	template <typename T>
	auto& getValueFromRoot(T& root) const
	{
		return Prev::getValueFromRoot(root)[index];
	}


	template <typename T, typename O>
	void setValueFromRoot(T& root, const O& value)
	{
		Prev::getValueFromRoot(root)[index] = value;
	}

	int index;
};


template <typename Prev, typename Member>
auto makePP(Prev& prev, Member head)
{
	return PropertyPath<Prev, RemoveReference<Member>::Type>(prev, head);
}


template <typename Prev>
auto makePP(Prev& prev, int index)
{
	return PropertyPathArray<Prev>(prev, index);
}


struct UIBuilder
{
	UIBuilder(AnimationEditor& editor) : m_editor(editor) {}


	template <typename T>
	void build(T& obj)
	{
		apply([this, &obj](const auto& member) {
			auto pp = makePP(PropertyPathBegin{}, member);
			auto& v = member.getValue(obj);
			ui(obj, pp, v);
		}, getMembers<T>().members);
	}


	template <typename O, typename M, typename T>
	void ui(O& owner, const M& member, T& obj)
	{
		apply([&member, this, &obj](const auto& m) {
			auto& v = m.getValue(obj);
			auto pp = makePP(member, m);
			ui(obj, pp, v);
		}, getMembers<T>().members);
	}


	template <typename O, typename M>
	void ui(O& owner, const M& member, string& obj)
	{
		char tmp[32];
		copyString(tmp, obj.c_str());
		if (ImGui::InputText(member.name, tmp, sizeof(tmp)))
		{
			IAllocator& allocator = m_editor.getApp().getWorldEditor().getAllocator();
			string tmp_str(tmp, allocator);
			using PP = RemoveReference<M>::Type;
			auto* command = LUMIX_NEW(allocator, SetPropertyCommand<string, PP>)(m_editor.getController(), member, tmp_str);
			m_editor.executeCommand(*command);
		}
	}


	template <typename O, typename M, typename T>
	void ui(O& owner, const M& member, Array<T>& array)
	{
		IAllocator& allocator = m_editor.getApp().getWorldEditor().getAllocator();
		bool expanded = ImGui::TreeNodeEx(member.name, ImGuiTreeNodeFlags_AllowOverlapMode);
		ImGui::SameLine();
		if (ImGui::SmallButton(StaticString<32>("Add")))
		{
			using PP = RemoveReference<M>::Type;
			auto* command = LUMIX_NEW(allocator, AddArrayItemCommand<PP>)(m_editor.getController(), member);
			m_editor.executeCommand(*command);
		}
		if (!expanded) return;

		int i = 0;
		int subproperties_count = TupleSize<decltype(getMembers<T>().members)>::result;
		if (subproperties_count > 1)
		{
			for (T& item : array)
			{
				StaticString<32> label("", i + 1);
				bool expanded = ImGui::TreeNodeEx(label, ImGuiTreeNodeFlags_AllowOverlapMode);
				ImGui::SameLine();
				if (ImGui::SmallButton("Remove"))
				{
					auto* command = LUMIX_NEW(allocator, RemoveArrayItemCommand<M>)(m_editor.getController(), member, i);
					m_editor.executeCommand(*command);
					if (expanded) ImGui::TreePop();
					break;
				}

				if (expanded)
				{
					ui(array, makePP(member, i), item);
					ImGui::TreePop();
				}
				++i;
			}
		}
		else
		{
			for (T& item : array)
			{
				ImGui::PushID(&item);
				ui(array, makePP(member, i), item);
				ImGui::SameLine();
				if (ImGui::SmallButton("Remove"))
				{
					auto* command = LUMIX_NEW(allocator, RemoveArrayItemCommand<M>)(m_editor.getController(), member, i);
					m_editor.executeCommand(*command);
					break;
				}
				ImGui::PopID();
				++i;
			}
		}
		ImGui::TreePop();
	}

	AnimationEditor& m_editor;
};


void AnimationEditor::masksGUI()
{
	UIBuilder ui_builder(*this);
	ui_builder.build(*m_resource);
}


void AnimationEditor::constantsGUI()
{
	if (!ImGui::CollapsingHeader("Constants")) return;

	Anim::InputDecl& input_decl = m_resource->getEngineResource()->m_input_decl;
	ImGui::PushID("consts");
	for (auto& constant : input_decl.constants)
	{
		if (constant.type == Anim::InputDecl::EMPTY) continue;
		ImGui::PushID(&constant);
		ImGui::PushItemWidth(100);
		ImGui::InputText("", constant.name, lengthOf(constant.name));
		ImGui::SameLine();
		if (ImGui::Combo("##cmb", (int*)&constant.type, "float\0int\0bool\0"))
		{
			input_decl.recalculateOffsets();
		}
		ImGui::SameLine();
		switch (constant.type)
		{
			case Anim::InputDecl::FLOAT: ImGui::DragFloat("##val", &constant.f_value); break;
			case Anim::InputDecl::BOOL: ImGui::Checkbox("##val", &constant.b_value); break;
			case Anim::InputDecl::INT: ImGui::InputInt("##val", &constant.i_value); break;
			default: ASSERT(false); break;
		}
		ImGui::SameLine();
		if (ImGui::Button("x"))
		{
			constant.type = Anim::InputDecl::EMPTY;
			--input_decl.constants_count;
		}
		ImGui::PopItemWidth();
		ImGui::PopID();
	}
	ImGui::PopID();

	if (input_decl.constants_count < lengthOf(input_decl.constants) && ImGui::Button("Add##add_const"))
	{
		for (auto& constant : input_decl.constants)
		{
			if (constant.type == Anim::InputDecl::EMPTY)
			{
				constant.name[0] = 0;
				constant.type = Anim::InputDecl::BOOL;
				constant.b_value = true;
				++input_decl.constants_count;
				break;
			}
		}
	}
}


void AnimationEditor::animationSlotsGUI()
{
	if (!ImGui::CollapsingHeader("Animation slots")) return;
	ImGui::PushID("anim_slots");
	auto& engine_anim_set = m_resource->getEngineResource()->m_animation_set;
	auto& slots = m_resource->getAnimationSlots();
	auto& sets = m_resource->getEngineResource()->m_sets_names;
	ImGui::PushItemWidth(-1);
	ImGui::Columns(sets.size() + 1);
	ImGui::NextColumn();
	ImGui::PushID("header");
	for (int j = 0; j < sets.size(); ++j)
	{
		ImGui::PushID(j);
		ImGui::PushItemWidth(-1);
		ImGui::InputText("", sets[j].data, lengthOf(sets[j].data));
		ImGui::PopItemWidth();
		ImGui::PopID();
		ImGui::NextColumn();
	}
	ImGui::PopID();
	ImGui::Separator();
	for (int i = 0; i < slots.size(); ++i)
	{
		const string& slot = slots[i];
		ImGui::PushID(i);
		char slot_cstr[64];
		copyString(slot_cstr, slot.c_str());

		ImGui::PushItemWidth(-20);
		if (ImGui::InputText("##name", slot_cstr, lengthOf(slot_cstr), ImGuiInputTextFlags_EnterReturnsTrue))
		{
			bool exists = slots.find([&slot_cstr](const string& val) { return val == slot_cstr; }) >= 0;

			if (exists)
			{
				g_log_error.log("Animation") << "Slot " << slot_cstr << " already exists.";
			}
			else
			{
				u32 old_hash = crc32(slot.c_str());
				u32 new_hash = crc32(slot_cstr);

				for (auto& entry : engine_anim_set)
				{
					if (entry.hash == old_hash) entry.hash = new_hash;
				}
				slots[i] = slot_cstr;
			}
		}
		ImGui::PopItemWidth();
		ImGui::SameLine();
		u32 slot_hash = crc32(slot.c_str());
		if (ImGui::Button("x"))
		{
			slots.erase(i);
			engine_anim_set.eraseItems([slot_hash](Anim::ControllerResource::AnimSetEntry& val) { return val.hash == slot_hash; });
			--i;
		}
		ImGui::NextColumn();
		for (int j = 0; j < sets.size(); ++j)
		{
			Anim::ControllerResource::AnimSetEntry* entry = nullptr;
			for (auto& e : engine_anim_set)
			{
				if (e.set == j && e.hash == slot_hash) 
				{
					entry = &e;
					break;
				}
			}

			ImGui::PushItemWidth(ImGui::GetColumnWidth());
			char tmp[MAX_PATH_LENGTH];
			copyString(tmp, entry && entry->animation ? entry->animation->getPath().c_str() : "");
			ImGui::PushID(j);
			if (m_app.getAssetBrowser().resourceInput("", "##res", tmp, lengthOf(tmp), ANIMATION_TYPE))
			{
				if (entry && entry->animation) entry->animation->getResourceManager().unload(*entry->animation);
				auto* manager = m_app.getWorldEditor().getEngine().getResourceManager().get(ANIMATION_TYPE);
				if (entry)
				{
					entry->animation = (Animation*)manager->load(Path(tmp));
				}
				else
				{
					engine_anim_set.push({j, slot_hash, (Animation*)manager->load(Path(tmp))});
				}
			}
			ImGui::PopID();
			ImGui::PopItemWidth();


			ImGui::NextColumn();
		}
		ImGui::PopID();
	}
	ImGui::Columns();

	if (ImGui::Button("Add slot (row)"))
	{
		bool exists = slots.find([](const string& val) { return val == ""; }) >= 0;

		if (exists)
		{
			g_log_error.log("Animation") << "Slot with empty name already exists. Please rename it and then you can create a new slot.";
		}
		else
		{
			IAllocator& allocator = m_app.getWorldEditor().getAllocator();
			slots.emplace("", allocator);
		}
	}
	if (ImGui::Button("Add set (column)"))
	{
		IAllocator& allocator = m_app.getWorldEditor().getAllocator();
		m_resource->getEngineResource()->m_sets_names.emplace("new set");
	}
	ImGui::PopItemWidth();
	ImGui::PopID();


}


IAnimationEditor* IAnimationEditor::create(IAllocator& allocator, StudioApp& app)
{
	return LUMIX_NEW(allocator, AnimationEditor)(app);
}


} // namespace AnimEditor


} // namespace Lumix