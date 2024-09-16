#pragma once
#include <string>
#include <vector>
#include <array>
#include <bitset>
#include <unordered_map>
#include <unordered_set>

#include "glm/glm.hpp"
#include "rapidjson/document.h"
#include "lua.hpp"
#include "LuaBridge/LuaBridge.h"

#include "SDL.h"
// #include "SDL_image.h"
// #include "SDL_ttf.h"
#include "SDL_mixer.h"

#include "SDL_scancode.h"
#include "SDL_mouse.h"

#include "renderer.h"

constexpr float coord_size = 100.f;
constexpr glm::ivec2 coord_tile_size = { 100, 100 };
constexpr glm::vec2 coord_tile_fsize = { 100.f, 100.f };

using AudioHandle = uint32_t;
using ActorId = uint32_t;
using ActorIndex = uint32_t;
using ComponentIndex = uint32_t;

static bool file_exists(std::string_view path);


struct Color {
	uint32_t data;

	static Color make(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
		return { static_cast<uint32_t>(r) | static_cast<uint32_t>(g) << 8 | static_cast<uint32_t>(b) << 16 | static_cast<uint32_t>(a) << 24 };
	};

	uint8_t r() const {
		return static_cast<uint8_t>(data & 0xFF);
	}

	uint8_t g() const {
		return static_cast<uint8_t>((data >> 8) & 0xFF);
	}

	uint8_t b() const {
		return static_cast<uint8_t>((data >> 16) & 0xFF);
	}

	uint8_t a() const {
		return static_cast<uint8_t>((data >> 24) & 0xFF);
	}

	bool operator==(const Color& rhs) const {
		return data == rhs.data;
	}
};

template<typename T>
static constexpr T numeric_max() {
	return std::numeric_limits<T>::max();
}

constexpr Color WHITE = { numeric_max<uint32_t>() };

class BitVec {
	std::vector<size_t> data;
	static constexpr size_t word_size = sizeof(size_t) * 8;
public:
	void clear();
	void fill();
	void set_len(size_t new_size, size_t new_val = 0);

	bool get(size_t index) const;
	void set(size_t index, bool new_val);
};

struct GameConfig {
	std::string initial_scene;
	std::string window_name;
	std::optional<std::string> font;

	GameConfig();
};

struct Ivec2Hasher {
	std::size_t operator()(const glm::ivec2& vec) const noexcept;
};

struct Component {
	luabridge::LuaRef lua_component;
	std::string key;
	std::string type;
};

struct Actor {
	std::string name = "";
	std::vector<Component> components;
	std::unordered_map<std::string, ComponentIndex> keys;
	std::unordered_map<std::string, std::vector<ComponentIndex>> types;
	std::vector<ComponentIndex> have_update;
	std::vector<ComponentIndex> have_late_update;
	std::vector<ComponentIndex> have_on_collision_enter;
	std::vector<ComponentIndex> have_on_collision_exit;
	std::vector<ComponentIndex> have_on_trigger_enter;
	std::vector<ComponentIndex> have_on_trigger_exit;
	std::vector<ComponentIndex> to_destroy;
	std::vector<ComponentIndex> free_list;
	ActorId id = numeric_max<ActorId>();
	uint32_t needs_destroy = 0;

	void insert_sorted(std::vector<ComponentIndex>& v, ComponentIndex e);
	void remove_sorted(std::vector<ComponentIndex>& v, ComponentIndex e);
	Component& add_component(Component new_component);
	void remove_component(std::string key, bool force = false);
	void call_destroy();

	void clear();
};

static rapidjson::Document ReadJsonFile(const std::string& path);

template<typename T>
std::optional<T> get_value(const rapidjson::Value& val, const char* key);

std::optional<float> get_number(const rapidjson::Value& val, const char* key);

std::optional<std::string> get_string(const rapidjson::Value& val, const char* key);

luabridge::LuaRef get_value(lua_State* lua_state, const rapidjson::Value& val, const char* key);

luabridge::LuaRef get_value(lua_State* lua_state, const rapidjson::Value& val);

void set_metatable(const luabridge::LuaRef& base, const luabridge::LuaRef& meta);

template<typename T>
void swap_remove(std::vector<T>& v, size_t index);

static uint64_t ivec2_to_u64(glm::ivec2 v);

template<class... Params>
void sandbox_call(luabridge::LuaRef function, std::string_view actor_name, Params...);

class AudioManager {
	std::unordered_map<std::string, Mix_Chunk*> audio;
public:
	AudioManager();
	Mix_Chunk* load_sound(const std::string& file_name);
	int play_sound(Mix_Chunk* audio, int channel = -1, bool loops = false) const;
	void stop_sound(int channel) const;
	void set_volume(int channel, int volume) const;
};

struct Model {
	std::string key;
	luabridge::LuaRef actor;
	std::string mesh;
	Transform transform;
	std::optional<Renderer::InstanceHandle> instance;
	bool transform_dirty = true;
	bool enabled = true;
	bool mesh_dirty = true;

	Model(lua_State* lua_state) : actor(lua_state) {};

	void on_start(lua_State* lua_state);
	void on_update(lua_State* lua_state);
	void on_destroy(lua_State* lua_state);
};

struct Camera {
	lua_State* lua_state;
};

class TemplateManager {
	lua_State* lua_state;
	std::unordered_map<std::string, Actor> templates;
	std::unordered_map<std::string, luabridge::LuaRef> components;
	std::shared_ptr<Renderer> renderer;

	luabridge::LuaRef make_template_component(std::string type);
	void load_component(std::string type);
	void load_template(std::string name);
public:
	TemplateManager(std::shared_ptr<Renderer> renderer, lua_State* lua_state);

	Actor create_actor(const rapidjson::Value& actor);
	Actor create_template_actor(std::string template_name);
	luabridge::LuaRef create_component(std::string type, std::string key);
};

struct AddComponentQueue {
	struct Descriptor {
		Component component;
		ActorIndex actor;
		ActorId id;
	};

	uint64_t global_count;
	std::vector<Descriptor> queue;
	TemplateManager& templates;

	luabridge::LuaRef push(std::string type, ActorIndex index, ActorId id);
};

class InputManager {
	std::bitset<512> currently_down_keys;
	std::bitset<512> just_pressed_keys;
	std::bitset<512> just_released_keys;
	uint8_t mouse_state = 0;
	uint8_t just_pressed_mouse = 0;
	uint8_t just_released_mouse = 0;
	float scroll_delta = 0;
	glm::vec2 mouse_pos = { 0.f, 0.f };
public:
	void new_frame();
	void handle_key_event(SDL_KeyboardEvent& e);
	void handle_mouse_event(SDL_MouseButtonEvent& e);
	void handle_mouse_wheel_event(SDL_MouseWheelEvent& e);
	void handle_mouse_motion_event(SDL_MouseMotionEvent& e);

	bool key_is_pressed(SDL_Scancode scancode) const;
	bool key_just_pressed(SDL_Scancode scancode) const;
	bool key_just_released(SDL_Scancode scancode) const;

	bool mouse_is_pressed(uint8_t button) const;
	bool mouse_just_pressed(uint8_t button) const;
	bool mouse_just_released(uint8_t button) const;
	float get_scroll_delta() const;
	glm::vec2 get_mouse_pos() const;
};

class EventBus {
	struct Handler {
		luabridge::LuaRef component;
		luabridge::LuaRef function;

		bool operator==(const Handler& other) const {
			return component.operator==(other.component) && function.operator==(other.function);
		}
	};

	std::unordered_map<std::string, std::vector<Handler>> subs;
	std::vector<std::pair<std::string, Handler>> subscribe_queue;
	std::vector<std::pair<std::string, Handler>> unsubscribe_queue;
public:
	void publish(std::string event_type, luabridge::LuaRef message);
	void schedule_subscribe(std::string event_type, luabridge::LuaRef component, luabridge::LuaRef function);
	void schedule_unsubscribe(std::string event_type, luabridge::LuaRef component, luabridge::LuaRef function);
	void apply_scheduled();
};

class World {
	enum class GameState {
		Intro,
		Gameloop,
		Outro,
	};

	struct LuaActor;

	struct ActorCollection {
		struct ActorSlot {
			std::unique_ptr<LuaActor> actor;
			ActorIndex next;
			ActorIndex prev;
		};

		std::vector<ActorSlot> actors;
		ActorIndex head = numeric_max<ActorIndex>();
		ActorIndex tail = numeric_max<ActorIndex>();
		BitVec destroy_on_load;
		std::unordered_map<std::string, std::vector<ActorIndex>> names;
		AddComponentQueue component_queue;
		std::unordered_set<ActorIndex> new_actors;
		std::vector<ActorIndex> new_actor_list;
		std::vector<ActorIndex> freed_list;
		std::unordered_set<ActorIndex> to_destroy;
		ActorId next_id = 0;

		ActorIndex curr_actor = numeric_max<ActorIndex>();
		bool curr_actor_destroyed = false;

		void apply_queue();
		ActorIndex add_actor(Actor actor);
		ActorIndex raw_add_actor(Actor actor);

		luabridge::LuaRef find(const char* name, lua_State* lua_state);
		luabridge::LuaRef find_all(const char* name, lua_State* lua_state);

		void call_actor_start(ActorIndex from = numeric_max<ActorIndex>());
		void call_new_actor_start();
		void call_actor_update();
		void call_actor_late_update();
		void call_actor_destroy();

		luabridge::LuaRef instantiate(const char* template_name, lua_State* lua_state);
		void dont_destroy_on_load(ActorIndex index);

		ActorCollection(TemplateManager& templates) : component_queue(AddComponentQueue{ 0, {}, templates }) {};
	};

	struct LuaActor {
		Actor actor;
		ActorIndex index;
		ActorCollection& actors;

		const std::string& get_name() const;
		ActorId get_id() const;
		luabridge::LuaRef get_component_by_key(const char* key, lua_State* lua_state) const;
		luabridge::LuaRef get_component_by_type(const char* type, lua_State* lua_state) const;
		luabridge::LuaRef get_components_by_type(const char* type, lua_State* lua_state) const;

		luabridge::LuaRef add_component(const char* type, lua_State* lua_state);
		void remove_component(luabridge::LuaRef component_ref);

		void call_component_method(luabridge::LuaRef component, std::string_view name);
	};

	std::shared_ptr<GameConfig> config;
	std::shared_ptr<Renderer> renderer;
	std::unique_ptr<uint64_t> frame_number;

	glm::vec2 camera_pos = { 0.f, 0.f };
	std::string current_scene;
	std::optional<std::string> next_scene;

	AudioManager audio_manager;
	TemplateManager templates;
	InputManager inputs;

	ActorCollection actors;

	lua_State* lua_state;

	EventBus events;

	void clear_scene();
	void update_actors();

	// returns true if the game should end
	bool process_events();
	bool map_key_func(bool(InputManager::* func)(SDL_Scancode) const, const char* key);
	void actor_destroy(LuaActor actor);

	World(std::shared_ptr<GameConfig> game_config, std::shared_ptr<Renderer> renderer, lua_State* lua_state);
public:
	World(std::shared_ptr<GameConfig> game_config, lua_State* lua_state);
	void load_scene(std::string scene_name);

	// returns true if the game should end
	bool run_turn();
};
