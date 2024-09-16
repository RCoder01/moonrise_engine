#define _SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING 1
#define SDL_MAIN_HANDLED
#include <algorithm>
#include <functional>
#include <filesystem>
#include <iostream>
#include <optional>
#include <queue>
#include <stdlib.h>
#include <chrono>
#include <thread>
#include <sstream>

#include "rapidjson/filereadstream.h"
#include "source.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#else
#endif


void open_url(const char* url) {
#if defined(__EMSCRIPTEN__)
	emscripten_run_script(std::string("window.open(\"" + std::string(url) + "\")").c_str());
#elif defined(_WIN32)
	std::string command = "start ";
	command += url;
	std::system(command.c_str());
#elif defined(__APPLE__)
	std::string command = "open ";
	command += url;
	std::system(command.c_str());
#else
	std::string command = "xdg-open ";
	command += url;
	std::system(command.c_str());
#endif
}

std::string translate_path(const char* path) {
	return std::string(base_path) + path;
}

const char* get_window_title(GameConfig* config) {
	return config->window_name.c_str();
}

void mainloop(void* arg) {
	World* world = static_cast<World*>(arg);
	world->run_turn(); // SDL_Quit is only recieved as application is about to shutdown
}

int main(int argc, char** argv) {
	lua_State* lua_state = luaL_newstate();
	luaL_openlibs(lua_state);

	if (!file_exists(translate_path("resources/"))) {
		std::cout << "error: resources/ missing" << std::endl;
		return 0;
	}

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "Could not initialize SDL! Error: " << SDL_GetError() << std::endl;
        exit(1);
    }

	std::shared_ptr<GameConfig> game_config = std::make_shared<GameConfig>();

	World world = { game_config, lua_state };

	//using std::chrono::high_resolution_clock;
	//using std::chrono::duration;
	//using std::chrono::milliseconds;

	//auto t1 = high_resolution_clock::now();
	#ifdef __EMSCRIPTEN__
	emscripten_set_main_loop_arg((em_arg_callback_func)mainloop, &world, -1, 1);
	#else
	while (!world.run_turn()) {}
	#endif
	//auto t2 = high_resolution_clock::now();
	//duration<double, std::milli> ms = t2 - t1;
	//std::cout << '\n' << ms.count() / 1000 << std::endl;

	SDL_Quit();
}


bool file_exists(std::string_view path) {
    return std::filesystem::exists(path);
}


void BitVec::clear() {
	for (auto& v : data) {
		v = 0;
	}
}

void BitVec::fill() {
	for (auto& v : data) {
		v = numeric_max<size_t>();
	}
}

void BitVec::set_len(size_t new_size, size_t new_val) {
	size_t new_len = (new_size / word_size) + 1;
	data.resize(new_len, new_val);
}

bool BitVec::get(size_t index) const {
	size_t word_index = index / word_size;
	if (word_index >= data.size()) {
		return false;
	}
	size_t mask = static_cast<size_t>(1) << (index % word_size);
	return data[word_index] & mask;
}

void BitVec::set(size_t index, bool new_val) {
	size_t word_index = index / word_size;
	if (word_index >= data.size()) {
		return;
	}
	size_t mask = static_cast<size_t>(1) << (index % word_size);
	if (new_val) {
		data[word_index] |= mask;
	} else {
		data[word_index] &= ~mask;
	}
}


GameConfig::GameConfig() {
	if (!file_exists(translate_path("resources/game.config"))) {
		std::cout << "error: resources/game.config missing";
		exit(0);
	}
	rapidjson::Document doc = ReadJsonFile(translate_path("resources/game.config"));

	std::optional<const char*> initial_scene = get_value<const char*>(doc, "initial_scene");
	if (!initial_scene.has_value()) {
		std::cout << "error: initial_scene unspecified";
		exit(0);
	}
	this->initial_scene = initial_scene.value();
	window_name = get_string(doc, "game_title").value_or("");
	font = get_string(doc, "font");
}


std::size_t Ivec2Hasher::operator()(const glm::ivec2& vec) const noexcept {
	uint32_t ux = static_cast<uint32_t>(vec.x);
	uint32_t uy = static_cast<uint32_t>(vec.y);
	uint64_t result = static_cast<uint64_t>(ux);
	result <<= 32;
	result |= static_cast<uint64_t>(uy);
	return std::hash<uint64_t>{}(result);
}


void Actor::insert_sorted(std::vector<ComponentIndex>& v, ComponentIndex e) {
	for (auto it = v.begin(); it != v.end(); it++) {
		if (components[*it].key < components[e].key) {
			continue;
		}
		v.insert(it, e);
		return;
	}
	v.push_back(e);
}

void Actor::remove_sorted(std::vector<ComponentIndex>& v, ComponentIndex e) {
	auto it = std::find(v.begin(), v.end(), e);
	if (it != v.end()) {
		v.erase(it);
	}
}

Component& Actor::add_component(Component new_component) {
	ComponentIndex index = 0;
	if (!free_list.empty()) {
		index = free_list.back();
		free_list.pop_back();
		components[index] = std::move(new_component);
	} else {
		components.push_back(std::move(new_component));
		index = static_cast<ComponentIndex>(components.size() - 1);
	}
	Component& component = components[index];
	keys[component.key] = index;
	insert_sorted(types[component.type], index);
	if (!component.lua_component["OnUpdate"].isNil()) {
		insert_sorted(have_update, index);
	}
	if (!component.lua_component["OnLateUpdate"].isNil()) {
		insert_sorted(have_late_update, index);
	}
	if (!component.lua_component["OnCollisionEnter"].isNil()) {
		insert_sorted(have_on_collision_enter, index);
	}
	if (!component.lua_component["OnCollisionExit"].isNil()) {
		insert_sorted(have_on_collision_exit, index);
	}
	if (!component.lua_component["OnTriggerEnter"].isNil()) {
		insert_sorted(have_on_trigger_enter, index);
	}
	if (!component.lua_component["OnTriggerExit"].isNil()) {
		insert_sorted(have_on_trigger_exit, index);
	}
	needs_destroy += static_cast<uint32_t>(!component.lua_component["OnDestroy"].isNil());
	return component;
}

void Actor::remove_component(std::string key, bool force) {
	ComponentIndex index = keys.at(key);
	auto& component = components[index];
	if (!force && needs_destroy != 0 && !component.lua_component["OnDestroy"].isNil()) {
		insert_sorted(to_destroy, index);
		return;
	}

	keys.erase(key);
	std::vector<ComponentIndex>& type_list = types.at(component.type);
	type_list.erase(std::find(type_list.begin(), type_list.end(), index));
	if (type_list.empty()) {
		types.erase(component.type);
	}
	remove_sorted(have_update, index);
	remove_sorted(have_late_update, index);
	remove_sorted(have_on_collision_enter, index);
	remove_sorted(have_on_collision_exit, index);
	remove_sorted(have_on_trigger_enter, index);
	remove_sorted(have_on_trigger_exit, index);

	component.lua_component = luabridge::LuaRef(component.lua_component.state());
	component.key = "Erased Key";
	component.type = "Erased Type";
	free_list.push_back(index);
}

void Actor::clear() {
	to_destroy.clear();
	if (needs_destroy != 0) {
		for (size_t i = 0; i < components.size(); i++) {
			auto& component = components[i];
			if (!component.lua_component.isNil()) {
				remove_component(component.key);
			}
		}
		call_destroy();
	}
	name = "Uninit";
	components.clear();
	keys.clear();
	types.clear();
	have_update.clear();
	have_late_update.clear();
	have_on_collision_enter.clear();
	have_on_collision_exit.clear();
	have_on_trigger_enter.clear();
	have_on_trigger_exit.clear();

	free_list.clear();
	id = numeric_max<ActorId>();
}

void Actor::call_destroy() {
	for (ComponentIndex i : to_destroy) {
		luabridge::LuaRef component = components[i].lua_component;
		sandbox_call(component["OnDestroy"], name, component);
		remove_component(components[i].key, true);
		needs_destroy -= 1;
	}
	to_destroy.clear();
}

static rapidjson::Document ReadJsonFile(const std::string &path)
{
    FILE* file_pointer = nullptr;
	rapidjson::Document out_document;
#ifdef _WIN32
	fopen_s(&file_pointer, path.c_str(), "rb");
#else
	file_pointer = fopen(path.c_str(), "rb");
#endif
	char buffer[65536];
	rapidjson::FileReadStream stream(file_pointer, buffer, sizeof(buffer));
	out_document.ParseStream(stream);
	std::fclose(file_pointer);

	if (out_document.HasParseError()) {
		//rapidjson::ParseErrorCode errorCode = out_document.GetParseError();
		std::cout << "error parsing json at [" << path << "]" << std::endl;
		exit(0);
	}
	return out_document;
}

template<typename T>
std::optional<T> get_value(const rapidjson::Value& val, const char* key) {
	if (val.HasMember(key) && val[key].Is<T>()) {
		return val[key].Get<T>();
	}
	return {};
}

template<typename T>
void swap_remove(std::vector<T>& v, size_t index) {
	if (v.size() == 0) {
		return;
	}
	if (index == v.size() - 1) {
		v.pop_back();
		return;
	}
	std::swap(v[index], v.back());
	v.pop_back();
}

static uint64_t ivec2_to_u64(glm::ivec2 v) {
	uint32_t ux = static_cast<uint32_t>(v.x);
	uint32_t uy = static_cast<uint32_t>(v.y);
	uint64_t result = static_cast<uint64_t>(ux) << 32 | static_cast<uint32_t>(uy);
	return result;
}

template<class ...Params>
void sandbox_call(luabridge::LuaRef function, std::string_view actor_name, Params... params) {
	try {
		function(params...);
	}
	catch (luabridge::LuaException e) {
		std::string error = e.what();
		std::replace(error.begin(), error.end(), '\\', '/');
		std::cout << "\033[31m" << actor_name << " : " << error << "\033[0m" << std::endl;
	}
}


std::optional<float> get_number(const rapidjson::Value& val, const char* key) {
	if (val.HasMember(key) && (val[key].IsNumber())) {
		return val[key].Get<float>();
	}
	return {};
}

std::optional<std::string> get_string(const rapidjson::Value& val, const char* key) {
	if (val.HasMember(key) && val[key].Is<const char*>()) {
		return val[key].Get<const char*>();
	}
	return {};
}

luabridge::LuaRef get_value(lua_State* lua_state, const rapidjson::Value& val, const char* key) {
	if (!val.HasMember(key) || val[key].IsNull()) {
		return luabridge::LuaRef(lua_state);
	}
	return get_value(lua_state, val[key]);
}

luabridge::LuaRef get_value(lua_State* lua_state, const rapidjson::Value& val) {
	if (val.IsArray()) {
		luabridge::LuaRef ref = luabridge::newTable(lua_state);
		for (auto it = val.Begin(); it != val.End(); it++) {
			ref.append(get_value(lua_state, *it));
		}
		return ref;
	}
	if (val.IsBool()) {
		return luabridge::LuaRef(lua_state, val.Get<bool>());
	}
	if (val.IsInt()) {
		return luabridge::LuaRef(lua_state, val.Get<int32_t>());
	}
	if (val.IsNumber()) {
		return luabridge::LuaRef(lua_state, val.Get<float>());
	}
	if (val.IsObject()) {
		luabridge::LuaRef ref = luabridge::newTable(lua_state);
		for (auto it = val.MemberBegin(); it != val.MemberEnd(); it++) {
			ref[it->name.GetString()] = get_value(lua_state, it->value);
		}
		return ref;
	}
	if (val.IsString()) {
		return luabridge::LuaRef(lua_state, val.GetString());
	}
	return luabridge::LuaRef(lua_state);
}

void set_metatable(const luabridge::LuaRef& base, const luabridge::LuaRef& meta) {
	lua_State* lua_state = base.state();
	base.push(lua_state);
	meta.push(lua_state);
	lua_setmetatable(lua_state, -2);
	lua_pop(lua_state, 1);
}

RenderConfig::RenderConfig() {
	if (!file_exists(translate_path("resources/rendering.config"))) {
		return;
	}
	rapidjson::Document config = ReadJsonFile(translate_path("resources/rendering.config"));
	size.x = get_value<int>(config, "x_resolution").value_or(size.x);
	size.y = get_value<int>(config, "y_resolution").value_or(size.y);
	clear_color.r = static_cast<uint8_t>(get_value<int>(config, "clear_color_r").value_or(255));
	clear_color.g = static_cast<uint8_t>(get_value<int>(config, "clear_color_g").value_or(255));
	clear_color.b = static_cast<uint8_t>(get_value<int>(config, "clear_color_b").value_or(255));
	zoom = get_number(config, "zoom_factor").value_or(1.f);
}


luabridge::LuaRef TemplateManager::make_template_component(std::string type) {
	if (type == "Model") {
		return { lua_state, Model(lua_state) };
	}
	if (components.count(type) == 0) {
		load_component(type);
	}
	luabridge::LuaRef component = luabridge::newTable(lua_state);
	set_metatable(component, components.find(type)->second);
	return component;
}

void TemplateManager::load_component(std::string type) {
	if (type == "Rigidbody") {
		components.insert({ type, luabridge::LuaRef(lua_state) });
		return;
	}
	const std::string filetype = translate_path("resources/component_types/") + type + ".lua";
	if (!file_exists(filetype)) {
		std::cout << "error: failed to locate component " << type;
		exit(0);
	}
	if (luaL_dofile(lua_state, filetype.c_str()) != LUA_OK) {
		std::cout << "problem with lua file " << type;
		exit(0);
	}
	luabridge::LuaRef meta = luabridge::getGlobal(lua_state, type.c_str());
	meta["__index"] = meta;
	meta["enabled"] = true;
	components.insert({ type, meta });
}

void TemplateManager::load_template(std::string name) {
	if (templates.find(name) != templates.end()) {
		return;
	}
	const std::string filename = translate_path("resources/actor_templates/") + name + ".template";
	if (!file_exists(filename)) {
		std::cout << "error: template " << name << " is missing";
		exit(0);
	}
	rapidjson::Document doc = ReadJsonFile(filename);

	Actor templ;
	templ.name = get_value<const char*>(doc, "name").value_or("");

	if (!doc.HasMember("components")) {
		templates.insert({ name, templ });
		return;
	}

	const auto& components = doc["components"];
	std::vector<std::string> keys;
	for (auto it = components.MemberBegin(); it != components.MemberEnd(); it++) {
		keys.push_back(it->name.GetString());
	}
	std::sort(keys.begin(), keys.end());

	for (auto& key : keys) {
		const auto& component = components[key.c_str()];
		std::string type = component["type"].GetString();
		luabridge::LuaRef lua_component = make_template_component(type);
		templ.keys.insert({ key, static_cast<ComponentIndex>(templ.components.size()) });
		Component new_component = { lua_component, std::move(key), std::move(type) };
		for (auto it = component.MemberBegin(); it != component.MemberEnd(); it++) {
			if (it->name == "type") {
				continue;
			}
			new_component.lua_component[it->name.GetString()] = get_value(lua_state, it->value);
		}
		new_component.lua_component["__index"] = new_component.lua_component;
		templ.components.push_back(new_component);
	}

	templates.insert({ name, templ });
	return;
}


void Model::on_start(lua_State* lua_state) {
	on_update(lua_state); // Just run update early
}

void Model::on_update(lua_State* lua_state) {
	if (mesh_dirty) {
		if (instance.has_value()) {
			on_destroy(lua_state);
		}
		Renderer* renderer = luabridge::getGlobal(lua_state, "_Renderer").cast<Renderer*>();
		ModelHandle model = renderer->loadModel(translate_path("resources/meshes/") + mesh);
		instance = {renderer->spawnInstance(model, transform)};
		transform_dirty = false;
		mesh_dirty = false;
	} else if (transform_dirty) {
		Renderer* renderer = luabridge::getGlobal(lua_state, "_Renderer").cast<Renderer*>();
		renderer->getModelInstance(instance.value()) = transform;
		transform_dirty = false;
	}
}

void Model::on_destroy(lua_State* lua_state) {
	if (!instance.has_value()) {
		return;
	}
	Renderer* renderer = luabridge::getGlobal(lua_state, "_Renderer").cast<Renderer*>();
	renderer->destroyInstance(instance.value());
	instance = {};
}


TemplateManager::TemplateManager(std::shared_ptr<Renderer> renderer, lua_State* lua_state) : lua_state(lua_state), renderer(renderer) {
	templates.insert({ "", {} });
}

Actor TemplateManager::create_actor(const rapidjson::Value& actor_json) {
	std::string template_name = get_value<const char*>(actor_json, "template").value_or("");
	load_template(template_name);
	std::optional<const char*> name = get_value<const char*>(actor_json, "name");
	const Actor& templ = templates.find(template_name)->second;

	if (!actor_json.HasMember("components")) {
		Actor actor = create_template_actor(std::move(template_name));
		actor.name = name.value_or(templ.name.c_str());
		return actor;
	}

	Actor actor;
	actor.name = name.value_or(templ.name.c_str());

	const auto& components = actor_json["components"];

	std::vector<std::string> keys;
	for (auto it = components.MemberBegin(); it != components.MemberEnd(); it++) {
		keys.push_back(it->name.GetString());
	}
	for (const auto& component : templ.components) {
		if (!components.HasMember(component.key.c_str())) {
			keys.push_back(component.key);
		}
	}
	std::sort(keys.begin(), keys.end());

	for (auto& key : keys) {
		luabridge::LuaRef new_component = luabridge::newTable(lua_state);
		auto it = templ.keys.find(key);
		std::string type;
		if (it != templ.keys.end()) {
			type = templ.components[it->second].type;
			if (type == "Model") {
				new_component = Model(templ.components[it->second].lua_component.cast<Model>());
			} else {
				set_metatable(new_component, templ.components[it->second].lua_component);
			}
		}
		else {
			type = components[key.c_str()]["type"].GetString();
			if (type == "Model") {
				new_component = Model(lua_state);
			} else {
				load_component(type);
				set_metatable(new_component, this->components.find(type)->second);
			}
		}
		if (components.HasMember(key.c_str())) {
			const auto& component = components[key.c_str()];
			for (auto it = component.MemberBegin(); it != component.MemberEnd(); it++) {
				if (std::string_view{ it->name.GetString() } != "type") {
					new_component[it->name.GetString()] = get_value(lua_state, it->value);
				}
			}
		}
		new_component["key"] = key;
		actor.add_component({ new_component, std::move(key), std::move(type) });
	}
	return actor;
}

Actor TemplateManager::create_template_actor(std::string template_name) {
	load_template(template_name);
	const Actor& templ = templates.find(template_name)->second;
	Actor actor;
	actor.name = templ.name.c_str();

	for (ComponentIndex i = 0; i < templ.components.size(); i++) {
		const auto& component = templ.components[i];
		std::string key = component.key;
		std::string type = component.type;
		if (type == "Model") {
			actor.add_component({ { lua_state, Model(component.lua_component.cast<Model>()) }, std::move(key), std::move(type) });
		} else {
			luabridge::LuaRef new_component = luabridge::newTable(lua_state);
			set_metatable(new_component, component.lua_component);
			new_component["key"] = key;
			actor.add_component({ new_component, std::move(key), std::move(type) });
		}
	}
	return actor;
}

luabridge::LuaRef TemplateManager::create_component(std::string type, std::string key) {
	if (type == "Model") {
		Model new_component = { lua_state };
		new_component.key = key;
		return { lua_state, new_component };
	} else {
		luabridge::LuaRef new_component = luabridge::newTable(lua_state);
		load_component(type);
		set_metatable(new_component, components.find(type)->second);
		new_component["key"] = key;
		return new_component;
	}
}

AudioManager::AudioManager() {
	if (Mix_OpenAudio(48000, AUDIO_S16SYS, 1, 2048)) {
		std::cout << "Failed to open audio";
		exit(0);
	}
	Mix_AllocateChannels(50);
}

Mix_Chunk* AudioManager::load_sound(const std::string& file_name) {
	if (audio.count(file_name) != 0) {
		return audio[file_name];
	}
	std::string file_path_wav = translate_path("resources/audio/") + file_name + ".wav";
	std::string file_path_ogg = translate_path("resources/audio/") + file_name + ".ogg";
	Mix_Chunk* chunk = Mix_LoadWAV(file_path_wav.c_str());
	if (chunk == nullptr) {
		chunk = Mix_LoadWAV(file_path_ogg.c_str());
	}
	if (chunk == nullptr) {
		std::cout << "error: failed to play audio clip " << file_name;
		exit(0);
	}
	audio[file_name] = chunk;
	return chunk;
}

int AudioManager::play_sound(Mix_Chunk* audio, int channel, bool loops) const {
	return Mix_PlayChannel(channel, audio, -static_cast<int>(loops));
}

void AudioManager::stop_sound(int channel) const {
	Mix_HaltChannel(channel);
}

void AudioManager::set_volume(int channel, int volume) const {
	Mix_Volume(channel, volume);
}

luabridge::LuaRef AddComponentQueue::push(std::string type, ActorIndex index, ActorId id) {
	std::stringstream key;
	key << 'r' << global_count;
	global_count += 1;
	luabridge::LuaRef component_ref = templates.create_component(type, key.str());
	Component component = {component_ref, key.str(), std::move(type)};
	queue.push_back(Descriptor{ component, index, id });
	return component_ref;
}


const std::string& World::LuaActor::get_name() const {
	return actor.name;
}

ActorId World::LuaActor::get_id() const {
	return actor.id;
}

luabridge::LuaRef World::LuaActor::get_component_by_key(const char* key, lua_State* lua_state) const {
	auto it = actor.keys.find(key);
	if (it == actor.keys.end()) {
		return luabridge::LuaRef(lua_state);
	}
	return actor.components[it->second].lua_component;
}

luabridge::LuaRef World::LuaActor::get_component_by_type(const char* type, lua_State* lua_state) const {
	auto it = actor.types.find(type);
	if (it == actor.types.end()) {
		return luabridge::LuaRef(lua_state);
	}
	return actor.components[it->second[0]].lua_component;
}

luabridge::LuaRef World::LuaActor::get_components_by_type(const char* type, lua_State* lua_state) const {
	auto it = actor.types.find(type);
	luabridge::LuaRef components = luabridge::newTable(lua_state);
	if (it == actor.types.end()) {
		return components;
	}
	const auto& actor_components = it->second;
	for (uint32_t i = 0; i < actor_components.size(); i++) {
		components[i + 1] = actor.components[actor_components[i]].lua_component;
	}
	return components;
}

luabridge::LuaRef World::LuaActor::add_component(const char* type, lua_State* lua_state) {
	return actors.component_queue.push(type, index, actor.id);
}

void World::LuaActor::remove_component(luabridge::LuaRef component_ref) {
	actor.remove_component(component_ref["key"]);
}

void World::LuaActor::call_component_method(luabridge::LuaRef component, std::string_view name) {
	if (component.isNil()) {
		return;
	}
	const luabridge::LuaRef& ref = component[name.data()];
	if (ref.isNil() || !component["enabled"].cast<bool>()) {
		return;
	}
	sandbox_call(ref, actor.name, component);
}


void InputManager::new_frame() {
	just_pressed_keys.reset();
	just_released_keys.reset();
	just_pressed_mouse = 0;
	just_released_mouse = 0;
	scroll_delta = 0;
}

void InputManager::handle_key_event(SDL_KeyboardEvent& e) {
	if (e.type == SDL_KEYDOWN) {
		currently_down_keys[e.keysym.scancode] = true;
		just_pressed_keys[e.keysym.scancode] = true;
		just_released_keys[e.keysym.scancode] = false;
	}
	else if (e.type == SDL_KEYUP) {
		currently_down_keys[e.keysym.scancode] = false;
		just_pressed_keys[e.keysym.scancode] = false;
		just_released_keys[e.keysym.scancode] = true;
	}
}

void InputManager::handle_mouse_event(SDL_MouseButtonEvent& e) {
	if (e.type == SDL_MOUSEBUTTONDOWN) {
		mouse_state |= SDL_BUTTON(e.button);
		just_pressed_mouse |= SDL_BUTTON(e.button);
	}
	else if (e.type == SDL_MOUSEBUTTONUP) {
		mouse_state &= ~SDL_BUTTON(e.button);
		just_released_mouse |= SDL_BUTTON(e.button);
	}
}

void InputManager::handle_mouse_wheel_event(SDL_MouseWheelEvent& e) {
	scroll_delta = e.preciseY;
}

void InputManager::handle_mouse_motion_event(SDL_MouseMotionEvent& e) {
	mouse_pos.x = static_cast<float>(e.x);
	mouse_pos.y = static_cast<float>(e.y);
}

glm::vec2 InputManager::get_mouse_pos() const {
	return mouse_pos;
}

float InputManager::get_scroll_delta() const {
	return scroll_delta;
}

bool InputManager::key_is_pressed(SDL_Scancode scancode) const {
	return currently_down_keys[scancode];
}

bool InputManager::key_just_pressed(SDL_Scancode scancode) const {
	return just_pressed_keys[scancode];
}

bool InputManager::key_just_released(SDL_Scancode scancode) const {
	return just_released_keys[scancode];
}

bool InputManager::mouse_is_pressed(uint8_t button) const {
	return mouse_state & SDL_BUTTON(button);
}

bool InputManager::mouse_just_pressed(uint8_t button) const {
	return just_pressed_mouse & SDL_BUTTON(button);
}

bool InputManager::mouse_just_released(uint8_t button) const {
	return just_released_mouse & SDL_BUTTON(button);
}


void EventBus::publish(std::string event_type, luabridge::LuaRef message) {
	for (auto& handler : subs[event_type]) {
		handler.function(handler.component, message);
	}
}

void EventBus::schedule_subscribe(std::string event_type, luabridge::LuaRef component, luabridge::LuaRef function) {
	subscribe_queue.push_back({ event_type, { component, function } });
}

void EventBus::schedule_unsubscribe(std::string event_type, luabridge::LuaRef component, luabridge::LuaRef function) {
	unsubscribe_queue.push_back({ event_type, { component, function } });
}

void EventBus::apply_scheduled() {
	for (auto& handler : subscribe_queue) {
		subs[handler.first].push_back(handler.second);
	}
	for (auto& handler : unsubscribe_queue) {
		auto& set = subs[handler.first];
		auto it = std::find(set.begin(), set.end(), handler.second);
		if (it != set.end()) {
			set.erase(it);
		}
	}
	subscribe_queue.clear();
	unsubscribe_queue.clear();
}


void World::ActorCollection::apply_queue() {
	std::vector<AddComponentQueue::Descriptor> to_add;
	component_queue.queue.swap(to_add);
	for (auto& descriptor : to_add) {
		auto& lua_actor = *actors[descriptor.actor].actor;
		if (lua_actor.actor.id != descriptor.id) {
			continue;
		}
		Component& inserted = lua_actor.actor.add_component(std::move(descriptor.component));
		inserted.lua_component["actor"] = &lua_actor;
		lua_actor.call_component_method(inserted.lua_component, "OnStart");
	}
}

luabridge::LuaRef World::ActorCollection::find(const char* name, lua_State* lua_state) {
	auto it = names.find(name);
	if (it == names.end()) {
		return luabridge::LuaRef(lua_state);
	}
	return { lua_state, *actors[it->second[0]].actor };
}

luabridge::LuaRef World::ActorCollection::find_all(const char* name, lua_State* lua_state) {
	auto it = names.find(name);
	luabridge::LuaRef table = luabridge::newTable(lua_state);
	if (it == names.end()) {
		return table;
	}

	const auto& actor_indices = it->second;
	for (uint32_t i = 0; i < actor_indices.size(); i++) {
		ActorId id = actor_indices[i];
		table[i + 1] = *actors[id].actor;
	}
	return table;
}

void World::ActorCollection::call_actor_start(ActorIndex from) {
	ActorIndex next = head;
	if (from != numeric_max<ActorIndex>()) {
		next = from;
	}
	while (next != numeric_max<ActorIndex>()) {
		LuaActor& lua_actor = *actors[next].actor;
		curr_actor = next;
		size_t size = lua_actor.actor.components.size();
		for (size_t i = 0; i < size; i++) {
			if (curr_actor_destroyed) {
				curr_actor_destroyed = false;
				break;
			}
			luabridge::LuaRef component = lua_actor.actor.components[i].lua_component;
			lua_actor.call_component_method(component, "OnStart");
		}
		next = actors[curr_actor].next;
	}
	curr_actor = numeric_max<ActorIndex>();
}

void World::ActorCollection::call_new_actor_start() {
	new_actors.clear();
	std::vector<ActorIndex> actors_to_start;
	new_actor_list.swap(actors_to_start);
	for (ActorIndex index : actors_to_start) {
		if (actors[index].actor.get() == nullptr) {
			break;
		}
		LuaActor& lua_actor = *actors[index].actor;
		size_t size = lua_actor.actor.components.size();
		for (size_t i = 0; i < size; i++) {
			luabridge::LuaRef component = lua_actor.actor.components[i].lua_component;
			lua_actor.call_component_method(component, "OnStart");
		}
	}
}

void World::ActorCollection::call_actor_update() {
	std::vector<ComponentIndex> to_run;
	ActorIndex next = head;
	while (next != numeric_max<ActorIndex>()) {
		if (new_actors.count(next) != 0) {
			next = actors[next].next;
			continue;
		}
		auto& lua_actor = *actors[next].actor;
		curr_actor = next;
		to_run = lua_actor.actor.have_update;
		auto& components = lua_actor.actor.components;
		for (ComponentIndex i : to_run) {
			if (curr_actor_destroyed) {
				curr_actor_destroyed = false;
				break;
			}
			lua_actor.call_component_method(components[i].lua_component, "OnUpdate");
		}
		next = actors[curr_actor].next;
	}
	curr_actor = numeric_max<ActorIndex>();
}

void World::ActorCollection::call_actor_late_update() {
	std::vector<ComponentIndex> to_run;
	ActorIndex next = head;
	while (next != numeric_max<ActorIndex>()) {
		if (new_actors.count(next) != 0) {
			next = actors[next].next;
			continue;
		}
		LuaActor& lua_actor = *actors[next].actor;
		curr_actor = next;
		to_run = lua_actor.actor.have_late_update;
		for (ComponentIndex i : to_run) {
			if (curr_actor_destroyed) {
				curr_actor_destroyed = false;
				break;
			}
			lua_actor.call_component_method(lua_actor.actor.components[i].lua_component, "OnLateUpdate");
		}
		next = actors[curr_actor].next;
	}
	curr_actor = numeric_max<ActorIndex>();
}

void World::ActorCollection::call_actor_destroy() {
	std::vector<ComponentIndex> to_run;
	ActorIndex next = head;
	while (next != numeric_max<ActorIndex>()) {
		if (new_actors.count(next) != 0) {
			next = actors[next].next;
			continue;
		}
		LuaActor& lua_actor = *actors[next].actor;
		curr_actor = next;
		lua_actor.actor.call_destroy();
		next = actors[curr_actor].next;
	}
	curr_actor = numeric_max<ActorIndex>();
	for (ActorIndex index : to_destroy) {
		actors[index].actor->actor.clear();
		freed_list.push_back(index);
	}
	to_destroy.clear();
}

luabridge::LuaRef World::ActorCollection::instantiate(const char* template_name, lua_State* lua_state) {
	ActorIndex index = add_actor(component_queue.templates.create_template_actor(template_name));
	return { lua_state, *actors[index].actor };
}

void World::ActorCollection::dont_destroy_on_load(ActorIndex index) {
	destroy_on_load.set(index, false);
}

ActorIndex World::ActorCollection::add_actor(Actor actor) {
	ActorIndex new_index = raw_add_actor(actor);
	new_actors.insert(new_index);
	new_actor_list.push_back(new_index);
	return new_index;
}

ActorIndex World::ActorCollection::raw_add_actor(Actor actor) {
	constexpr ActorIndex max_index = numeric_max<ActorIndex>();
	ActorIndex new_index;
	actor.id = next_id;
	next_id += 1;
	auto& name = names[actor.name];
	if (freed_list.empty() || freed_list.back() == curr_actor) {
		new_index = static_cast<ActorIndex>(actors.size());
		actors.push_back({ std::make_unique<LuaActor>(LuaActor{std::move(actor), new_index, *this}), max_index, tail });
		destroy_on_load.set_len(actors.size(), 0);
	} else {
		new_index = freed_list.back();
		freed_list.pop_back();
		ActorSlot& slot = actors[new_index];
		slot.actor = std::make_unique<LuaActor>(LuaActor{ std::move(actor), new_index, *this });
		slot.next = max_index;
		slot.prev = tail;
	}
	for (auto& component : actors[new_index].actor->actor.components) {
		component.lua_component["actor"] = actors[new_index].actor.get();
	}
	destroy_on_load.set(new_index, true);
	if (tail != numeric_max<ActorIndex>()) {
		actors[tail].next = new_index;
	}
	if (head == numeric_max<ActorIndex>()) {
		head = new_index;
	}
	tail = new_index;
	name.push_back(new_index);
	return new_index;
}

void World::clear_scene() {
	for (ActorIndex i = 0; i < actors.actors.size(); i++) {
		if (actors.destroy_on_load.get(i)) {
			actor_destroy(*actors.actors[i].actor);
		}
	}
	actors.call_actor_destroy();
	next_scene = {};
}

void World::update_actors() {
	actors.call_new_actor_start();
	actors.apply_queue();
	actors.call_actor_update();
	actors.call_actor_late_update();
	actors.call_actor_destroy();
}

#if defined(__EMSCRIPTEN__)
EM_JS(int, get_canvas_width, (), { return Module.canvas.width; });
EM_JS(int, get_canvas_height, (), { return Module.canvas.height; });
#endif

bool World::process_events() {
	inputs.new_frame();
	bool should_end = false;
	SDL_Event e;
	while (SDL_PollEvent(&e)) {
		switch (e.type) {
		case SDL_QUIT:
			should_end = true;
			break;
		case SDL_KEYDOWN:
		case SDL_KEYUP:
			inputs.handle_key_event(e.key);
			break;
		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
			inputs.handle_mouse_event(e.button);
			break;
		case SDL_MOUSEWHEEL:
			inputs.handle_mouse_wheel_event(e.wheel);
			break;
		case SDL_MOUSEMOTION:
			inputs.handle_mouse_motion_event(e.motion);
			break;
		case SDL_WINDOWEVENT:
			if (e.window.event == SDL_WINDOWEVENT_RESIZED) {
				std::cout << "Window resized to " << e.window.data1 << "x" << e.window.data2 << std::endl;
				renderer->resize();
				exit(0);
			}
			break;
		default:
			break;
		}
	}
	#if defined(__EMSCRIPTEN__)
	uint32_t width = static_cast<uint32_t>(get_canvas_width());
	uint32_t height = static_cast<uint32_t>(get_canvas_height());
	if (width != renderer->screen_size.width || height != renderer->screen_size.height) {
		renderer->screen_size = { width, height };
		renderer->resize();
	}
	#endif
	return should_end;
}

World::World(std::shared_ptr<GameConfig> game_config, std::shared_ptr<Renderer> renderer, lua_State* lua_state)
	: config(game_config),
	renderer(renderer),
	// inputs(std::make_unique<InputManager>()),
	// render_queues(std::make_unique<RenderQueues>()), 
	// audio_manager(std::make_shared<AudioManager>()),
	templates(renderer, lua_state),
	actors(templates),
	lua_state(lua_state) {}

void World::actor_destroy(LuaActor actor) {
	bool needs_destroy = actor.actor.needs_destroy != 0;
	if (needs_destroy) {
		actors.to_destroy.insert(actor.index);
	}

	auto& actors_list = actors.actors;

	actors.destroy_on_load.set(actor.index, false);
	auto& name = actors.names[actor.actor.name];
	name.erase(std::find(name.begin(), name.end(), actor.index));
	if (name.size() == 0) {
		actors.names.erase(actor.actor.name);
	}
	if (!needs_destroy) {
		actors.freed_list.push_back(actor.index);
	}

	auto& slot = actors_list[actor.index];
	if (!needs_destroy) {
		slot.actor->actor.clear();
		slot.actor = nullptr;
	}
	if (slot.next == numeric_max<ActorIndex>()) {
		actors.tail = slot.prev;
	} else {
		actors_list[slot.next].prev = slot.prev;
	}
	if (slot.prev == numeric_max<ActorIndex>()) {
		actors.head = slot.next;
	} else {
		actors_list[slot.prev].next = slot.next;
	}

	if (actors.curr_actor == actor.index) {
		actors.curr_actor_destroyed = true;
	}

	if (actors.curr_actor != numeric_max<ActorIndex>() && actor.index == actors.actors[actors.curr_actor].next) {
		actors.actors[actors.curr_actor].next = slot.next;
	}
}

static SDL_Scancode map_key(const char* key) {
	std::string_view key_str = key;
	if (key_str == "up") {
		return SDL_Scancode::SDL_SCANCODE_UP;
	} else if (key_str == "down") {
		return SDL_Scancode::SDL_SCANCODE_DOWN;
	} else if (key_str == "right") {
		return SDL_Scancode::SDL_SCANCODE_RIGHT;
	} else if (key_str == "left") {
		return SDL_Scancode::SDL_SCANCODE_LEFT;
	} else if (key_str == "escape") {
		return SDL_Scancode::SDL_SCANCODE_ESCAPE;
	} else if (key_str == "lshift") {
		return SDL_Scancode::SDL_SCANCODE_LSHIFT;
	} else if (key_str == "rshift") {
		return SDL_Scancode::SDL_SCANCODE_RSHIFT;
	} else if (key_str == "lctrl") {
		return SDL_Scancode::SDL_SCANCODE_LCTRL;
	} else if (key_str == "rctrl") {
		return SDL_Scancode::SDL_SCANCODE_RCTRL;
	} else if (key_str == "lalt") {
		return SDL_Scancode::SDL_SCANCODE_LALT;
	} else if (key_str == "ralt") {
		return SDL_Scancode::SDL_SCANCODE_RALT;
	} else if (key_str == "tab") {
		return SDL_Scancode::SDL_SCANCODE_TAB;
	} else if (key_str == "return" || key_str == "enter") {
		return SDL_Scancode::SDL_SCANCODE_RETURN;
	} else if (key_str == "backspace") {
		return SDL_Scancode::SDL_SCANCODE_BACKSPACE;
	} else if (key_str == "delete") {
		return SDL_Scancode::SDL_SCANCODE_DELETE;
	} else if (key_str == "insert") {
		return SDL_Scancode::SDL_SCANCODE_INSERT;
	} else if (key_str == "space") {
		return SDL_Scancode::SDL_SCANCODE_SPACE;
	} else if (key_str == "a") {
		return SDL_Scancode::SDL_SCANCODE_A;
	} else if (key_str == "b") {
		return SDL_Scancode::SDL_SCANCODE_B;
	} else if (key_str == "c") {
		return SDL_Scancode::SDL_SCANCODE_C;
	} else if (key_str == "d") {
		return SDL_Scancode::SDL_SCANCODE_D;
	} else if (key_str == "e") {
		return SDL_Scancode::SDL_SCANCODE_E;
	} else if (key_str == "f") {
		return SDL_Scancode::SDL_SCANCODE_F;
	} else if (key_str == "g") {
		return SDL_Scancode::SDL_SCANCODE_G;
	} else if (key_str == "h") {
		return SDL_Scancode::SDL_SCANCODE_H;
	} else if (key_str == "i") {
		return SDL_Scancode::SDL_SCANCODE_I;
	} else if (key_str == "j") {
		return SDL_Scancode::SDL_SCANCODE_J;
	} else if (key_str == "k") {
		return SDL_Scancode::SDL_SCANCODE_K;
	} else if (key_str == "l") {
		return SDL_Scancode::SDL_SCANCODE_L;
	} else if (key_str == "m") {
		return SDL_Scancode::SDL_SCANCODE_M;
	} else if (key_str == "n") {
		return SDL_Scancode::SDL_SCANCODE_N;
	} else if (key_str == "o") {
		return SDL_Scancode::SDL_SCANCODE_O;
	} else if (key_str == "p") {
		return SDL_Scancode::SDL_SCANCODE_P;
	} else if (key_str == "q") {
		return SDL_Scancode::SDL_SCANCODE_Q;
	} else if (key_str == "r") {
		return SDL_Scancode::SDL_SCANCODE_R;
	} else if (key_str == "s") {
		return SDL_Scancode::SDL_SCANCODE_S;
	} else if (key_str == "t") {
		return SDL_Scancode::SDL_SCANCODE_T;
	} else if (key_str == "u") {
		return SDL_Scancode::SDL_SCANCODE_U;
	} else if (key_str == "v") {
		return SDL_Scancode::SDL_SCANCODE_V;
	} else if (key_str == "w") {
		return SDL_Scancode::SDL_SCANCODE_W;
	} else if (key_str == "x") {
		return SDL_Scancode::SDL_SCANCODE_X;
	} else if (key_str == "y") {
		return SDL_Scancode::SDL_SCANCODE_Y;
	} else if (key_str == "z") {
		return SDL_Scancode::SDL_SCANCODE_Z;
	} else if (key_str == "0") {
		return SDL_Scancode::SDL_SCANCODE_0;
	} else if (key_str == "1") {
		return SDL_Scancode::SDL_SCANCODE_1;
	} else if (key_str == "2") {
		return SDL_Scancode::SDL_SCANCODE_2;
	} else if (key_str == "3") {
		return SDL_Scancode::SDL_SCANCODE_3;
	} else if (key_str == "4") {
		return SDL_Scancode::SDL_SCANCODE_4;
	} else if (key_str == "5") {
		return SDL_Scancode::SDL_SCANCODE_5;
	} else if (key_str == "6") {
		return SDL_Scancode::SDL_SCANCODE_6;
	} else if (key_str == "7") {
		return SDL_Scancode::SDL_SCANCODE_7;
	} else if (key_str == "8") {
		return SDL_Scancode::SDL_SCANCODE_8;
	} else if (key_str == "9") {
		return SDL_Scancode::SDL_SCANCODE_9;
	} else if (key_str == "/") {
		return SDL_Scancode::SDL_SCANCODE_SLASH;
	} else if (key_str == ";") {
		return SDL_Scancode::SDL_SCANCODE_SEMICOLON;
	} else if (key_str == "=") {
		return SDL_Scancode::SDL_SCANCODE_EQUALS;
	} else if (key_str == "-") {
		return SDL_Scancode::SDL_SCANCODE_MINUS;
	} else if (key_str == ".") {
		return SDL_Scancode::SDL_SCANCODE_PERIOD;
	} else if (key_str == ",") {
		return SDL_Scancode::SDL_SCANCODE_COMMA;
	} else if (key_str == "[") {
		return SDL_Scancode::SDL_SCANCODE_LEFTBRACKET;
	} else if (key_str == "]") {
		return SDL_Scancode::SDL_SCANCODE_RIGHTBRACKET;
	} else if (key_str == "\\") {
		return SDL_Scancode::SDL_SCANCODE_BACKSLASH;
	} else if (key_str == "'") {
		return SDL_Scancode::SDL_SCANCODE_APOSTROPHE;
	} else {
		return SDL_Scancode::SDL_NUM_SCANCODES;
	}
}

bool World::map_key_func(bool(InputManager::*func)(SDL_Scancode) const, const char* key) {
	SDL_Scancode scancode = map_key(key);
	if (scancode == SDL_Scancode::SDL_NUM_SCANCODES) {
		return false;
	} else {
		return (inputs.*func)(scancode);
	}
}

World::World(std::shared_ptr<GameConfig> game_config, lua_State* lua_state) : World(game_config, std::make_shared<Renderer>(Renderer(game_config)), lua_state) {
	frame_number = std::make_unique<uint64_t>(0);
	uint64_t* frame_count_ptr = frame_number.get();
	luabridge::getGlobalNamespace(lua_state)
		.beginNamespace("Debug")
			.addFunction("Log", static_cast<void(*)(std::string message)>([](std::string message) {std::cout << message << '\n'; }))
			.addFunction("LogError", static_cast<void(*)(std::string message)>([](std::string message) {std::cerr << message << '\n'; }))
		.endNamespace()
		.beginNamespace("Application")
			.addFunction("Quit", static_cast<void(*)()>([]() {exit(0); }))
			.addFunction("Sleep", static_cast<void(*)(uint32_t)>([](uint32_t ms) {std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }))
			.addFunction("GetFrame", std::function<uint64_t()>([frame_count_ptr]() {return *frame_count_ptr; }))
			.addFunction("OpenURL", static_cast<void(*)(std::string message)>([](std::string message) {open_url(message.c_str()); }))
			.addFunction("GetTime", std::function<float()>([]() {return static_cast<double>(SDL_GetTicks64()) / 1000.; }))
		.endNamespace()
		.beginClass<LuaActor>("Actor")
			.addProperty("_index", &LuaActor::index)
			.addFunction("GetName", &LuaActor::get_name)
			.addFunction("GetID", &LuaActor::get_id)
			.addFunction("GetComponentByKey", &LuaActor::get_component_by_key)
			.addFunction("GetComponent", &LuaActor::get_component_by_type)
			.addFunction("GetComponents", &LuaActor::get_components_by_type)
			.addFunction("AddComponent", &LuaActor::add_component)
			.addFunction("RemoveComponent", &LuaActor::remove_component)
		.endClass()
		.beginClass<glm::vec2>("vec2")
			.addConstructor<void(*)(float, float)>()
			.addProperty("x", &glm::vec2::x)
			.addProperty("y", &glm::vec2::y)
			.addStaticFunction("add", std::function<glm::vec2(const glm::vec2*, const glm::vec2*)>([](const glm::vec2* a, const glm::vec2* b) {return *a + *b; }))
			.addStaticFunction("sub", std::function<glm::vec2(const glm::vec2*, const glm::vec2*)>([](const glm::vec2* a, const glm::vec2* b) {return *a - *b; }))
			.addStaticFunction("mul", std::function<glm::vec2(const glm::vec2*, float)>([](const glm::vec2* a, float b) {return *a * b; }))
			.addStaticFunction("div", std::function<glm::vec2(const glm::vec2*, float)>([](const glm::vec2* a, float b) {return *a / b; }))
			.addStaticFunction("magnitude", std::function<float(const glm::vec2*)>([](const glm::vec2* a) {return glm::length(*a); }))
			.addStaticFunction("normalize", std::function<glm::vec2(const glm::vec2*)>([](const glm::vec2* a) {
				if (glm::length(*a) == 0) {
					return glm::vec2{ 0, 0 };
				}
				return glm::normalize(*a);
			}))
			.addStaticFunction("dot", std::function<float(const glm::vec2*, const glm::vec2*)>([](const glm::vec2* a, const glm::vec2* b) {return glm::dot(*a, *b); }))
		.endClass()
		.beginClass<glm::vec3>("vec3")
			.addConstructor<void(*)(float, float, float)>()
			.addProperty("x", &glm::vec3::x)
			.addProperty("y", &glm::vec3::y)
			.addProperty("z", &glm::vec3::z)
			.addProperty("yaw", &glm::vec3::x)
			.addProperty("pitch", &glm::vec3::y)
			.addProperty("roll", &glm::vec3::z)
			.addStaticFunction("add", std::function<glm::vec3(const glm::vec3*, const glm::vec3*)>([](const glm::vec3* a, const glm::vec3* b) {return *a + *b; }))
			.addStaticFunction("sub", std::function<glm::vec3(const glm::vec3*, const glm::vec3*)>([](const glm::vec3* a, const glm::vec3* b) {return *a - *b; }))
			.addStaticFunction("mul", std::function<glm::vec3(const glm::vec3*, float)>([](const glm::vec3* a, float b) {return *a * b; }))
			.addStaticFunction("div", std::function<glm::vec3(const glm::vec3*, float)>([](const glm::vec3* a, float b) {return *a / b; }))
			.addStaticFunction("magnitude", std::function<float(const glm::vec3*)>([](const glm::vec3* a) {return glm::length(*a); }))
			.addStaticFunction("normalize", std::function<glm::vec3(const glm::vec3*)>([](const glm::vec3* a) {
				if (glm::length(*a) == 0) {
					return glm::vec3{ 0, 0, 0 };
				}
				return glm::normalize(*a);
			}))
			.addStaticFunction("dot", std::function<float(const glm::vec3*, const glm::vec3*)>([](const glm::vec3* a, const glm::vec3* b) {return glm::dot(*a, *b); }))
			.addStaticFunction("cross", std::function<glm::vec3(const glm::vec3*, const glm::vec3*)>([](const glm::vec3* a, const glm::vec3* b) {return glm::cross(*a, *b); }))
		.endClass()
		.beginClass<Transform>("Transform")
			.addConstructor<void(*)(glm::vec3, glm::vec3, glm::vec3)>()
			.addStaticFunction("identity", std::function<Transform()>([]() {return Transform{}; }))
			.addProperty("translation", &Transform::translation)
			.addProperty("translation_x", std::function<float(const Transform*)>([](const Transform* transform) {return transform->translation.x; }), std::function<void(Transform*, float)>([](Transform* transform, float x) {transform->translation.x = x; }))
			.addProperty("translation_y", std::function<float(const Transform*)>([](const Transform* transform) {return transform->translation.y; }), std::function<void(Transform*, float)>([](Transform* transform, float y) {transform->translation.y = y; }))
			.addProperty("translation_z", std::function<float(const Transform*)>([](const Transform* transform) {return transform->translation.z; }), std::function<void(Transform*, float)>([](Transform* transform, float z) {transform->translation.z = z; }))
			.addProperty("rotation", &Transform::rotation)
			.addProperty("rotation_yaw", std::function<float(const Transform*)>([](const Transform* transform) {return transform->rotation.x; }), std::function<void(Transform*, float)>([](Transform* transform, float yaw) {transform->rotation.x = yaw; }))
			.addProperty("rotation_pitch", std::function<float(const Transform*)>([](const Transform* transform) {return transform->rotation.y; }), std::function<void(Transform*, float)>([](Transform* transform, float pitch) {transform->rotation.y = pitch; }))
			.addProperty("rotation_roll", std::function<float(const Transform*)>([](const Transform* transform) {return transform->rotation.z; }), std::function<void(Transform*, float)>([](Transform* transform, float roll) {transform->rotation.z = roll; }))
			.addProperty("scale", &Transform::scale)
			.addProperty("scale_x", std::function<float(const Transform*)>([](const Transform* transform) {return transform->scale.x; }), std::function<void(Transform*, float)>([](Transform* transform, float x) {transform->scale.x = x; }))
			.addProperty("scale_y", std::function<float(const Transform*)>([](const Transform* transform) {return transform->scale.y; }), std::function<void(Transform*, float)>([](Transform* transform, float y) {transform->scale.y = y; }))
			.addProperty("scale_z", std::function<float(const Transform*)>([](const Transform* transform) {return transform->scale.z; }), std::function<void(Transform*, float)>([](Transform* transform, float z) {transform->scale.z = z; }))
			.addFunction("tostring", std::function<std::string(const Transform*)>([](const Transform* transform) {
				std::stringstream ss;
				ss << "Transform("
					<< "x: " << transform->translation.x
					<< ", y: " << transform->translation.y
					<< ", z: " << transform->translation.z
					<< ", yaw: " << transform->rotation.x
					<< ", pitch: " << transform->rotation.y
					<< ", roll: " << transform->rotation.z
					<< ", scale_x: " << transform->scale.x
					<< ", scale_y: " << transform->scale.y
					<< ", scale_z: " << transform->scale.z << ")";
				return ss.str();
			}))
			.addFunction("add", std::function<Transform(const Transform*, const Transform*)>([](const Transform* a, const Transform* b) {
				return Transform{
					a->translation + b->translation,
					a->rotation + b->rotation,
					a->scale + b->scale
				};
			}))
			.addFunction("mul", std::function<Transform(const Transform*, float)>([](const Transform* a, float b) {
				return Transform{
					a->translation * b,
					a->rotation * b,
					a->scale * b
				};
			}))
		.endClass()
		.beginClass<Model>("Model")
			.addProperty("key", &Model::key)
			.addProperty("actor", &Model::actor)
			.addProperty("enabled", &Model::enabled)
			.addProperty("type", std::function<const char* (const Model*)>([](const Model*) {return "Model"; }), std::function<void(Model*, const char*)>([](Model*, const char*) {}))
			.addProperty("__index", std::function<luabridge::LuaRef(const Model*)>([](const Model* model) {return luabridge::LuaRef{ model->actor.state()}; }), std::function<void(Model*, luabridge::LuaRef)>([](const Model*, luabridge::LuaRef) {}))
			.addProperty("mesh", std::function<const char*(const Model*)>([](const Model* model) {return model->mesh.c_str(); }), std::function<void(Model*, const char*)>([](Model* model, const char* mesh) {model->mesh = mesh; model->mesh_dirty = true; }))
			.addProperty("transform", std::function<Transform(const Model*)>([](const Model* model) {return model->transform; }), std::function<void(Model*, Transform)>([](Model* model, Transform transform) {model->transform = transform; model->transform_dirty = true; }))
			.addProperty("translation", std::function<glm::vec3(const Model*)>([](const Model* model) {return model->transform.translation; }), std::function<void(Model*, glm::vec3)>([](Model* model, glm::vec3 translation) {model->transform.translation = translation; model->transform_dirty = true; }))
			.addProperty("translation_x", std::function<float(const Model*)>([](const Model* model) {return model->transform.translation.x; }), std::function<void(Model*, float)>([](Model* model, float x) {model->transform.translation.x = x; model->transform_dirty = true; }))
			.addProperty("translation_y", std::function<float(const Model*)>([](const Model* model) {return model->transform.translation.y; }), std::function<void(Model*, float)>([](Model* model, float y) {model->transform.translation.y = y; model->transform_dirty = true; }))
			.addProperty("translation_z", std::function<float(const Model*)>([](const Model* model) {return model->transform.translation.z; }), std::function<void(Model*, float)>([](Model* model, float z) {model->transform.translation.z = z; model->transform_dirty = true; }))
			.addProperty("rotation", std::function<glm::vec3(const Model*)>([](const Model* model) {return model->transform.rotation; }), std::function<void(Model*, glm::vec3)>([](Model* model, glm::vec3 rotation) {model->transform.rotation = rotation; model->transform_dirty = true; }))
			.addProperty("rotation_yaw", std::function<float(const Model*)>([](const Model* model) {return model->transform.rotation.x; }), std::function<void(Model*, float)>([](Model* model, float yaw) {model->transform.rotation.x = yaw; model->transform_dirty = true; }))
			.addProperty("rotation_pitch", std::function<float(const Model*)>([](const Model* model) {return model->transform.rotation.y; }), std::function<void(Model*, float)>([](Model* model, float pitch) {model->transform.rotation.y = pitch; model->transform_dirty = true; }))
			.addProperty("rotation_roll", std::function<float(const Model*)>([](const Model* model) {return model->transform.rotation.z; }), std::function<void(Model*, float)>([](Model* model, float roll) {model->transform.rotation.z = roll; model->transform_dirty = true; }))
			.addProperty("scale", std::function<glm::vec3(const Model*)>([](const Model* model) {return model->transform.scale; }), std::function<void(Model*, glm::vec3)>([](Model* model, glm::vec3 scale) {model->transform.scale = scale; model->transform_dirty = true; }))
			.addProperty("scale_x", std::function<float(const Model*)>([](const Model* model) {return model->transform.scale.x; }), std::function<void(Model*, float)>([](Model* model, float x) {model->transform.scale.x = x; model->transform_dirty = true; }))
			.addProperty("scale_y", std::function<float(const Model*)>([](const Model* model) {return model->transform.scale.y; }), std::function<void(Model*, float)>([](Model* model, float y) {model->transform.scale.y = y; model->transform_dirty = true; }))
			.addProperty("scale_z", std::function<float(const Model*)>([](const Model* model) {return model->transform.scale.z; }), std::function<void(Model*, float)>([](Model* model, float z) {model->transform.scale.z = z; model->transform_dirty = true; }))
			.addFunction("OnStart", &Model::on_start)
			.addFunction("OnUpdate", &Model::on_update)
			.addFunction("OnDestroy", &Model::on_destroy)
		.endClass()
		.beginClass<Camera>("_CameraType")
			.addProperty("transform", std::function<Transform(const Camera*)>([](const Camera* camera) {return luabridge::getGlobal(camera->lua_state, "_Renderer").cast<const Renderer*>()->getCameraTransform(); }), std::function<void(Camera*, Transform)>([](Camera* camera, Transform transform) {luabridge::getGlobal(camera->lua_state, "_Renderer").cast<Renderer*>()->getCameraTransform() = transform; }))
		.endClass()
		.beginClass<ActorCollection>("_ActorCollection").endClass()
		.beginClass<InputManager>("_InputManager").endClass()
		.beginClass<Renderer>("_RendererType").endClass()
		.beginClass<AudioManager>("_AudioManager").endClass()
		.beginNamespace("Actor")
			.addFunction("Find", std::function<luabridge::LuaRef(const char*)>([&, lua_state](const char* name) {return actors.find(name, lua_state); }))
			.addFunction("FindAll", std::function<luabridge::LuaRef(const char*)>([&, lua_state](const char* name) {return actors.find_all(name, lua_state); }))
			.addFunction("Instantiate", std::function<luabridge::LuaRef(const char*)>([&, lua_state](const char* name) {return actors.instantiate(name, lua_state); }))
			.addFunction("Destroy", std::function<void(LuaActor)>([&](LuaActor actor) {actor_destroy(actor); }))
		.endNamespace()
		.beginNamespace("Input")
			.addFunction("GetKey", std::function<bool(const char*)>([&](const char* key) {return map_key_func(&InputManager::key_is_pressed, key); }))
			.addFunction("GetKeyDown", std::function<bool(const char*)>([&](const char* key) {return map_key_func(&InputManager::key_just_pressed, key); }))
			.addFunction("GetKeyUp", std::function<bool(const char*)>([&](const char* key) {return map_key_func(&InputManager::key_just_released, key); }))
			.addFunction("GetMousePosition", std::function<glm::vec2(lua_State*)>([&](lua_State*) {return inputs.get_mouse_pos(); }))
			.addFunction("GetMouseButton", std::function<bool(uint8_t)>([&](uint8_t button) {return inputs.mouse_is_pressed(button); }))
			.addFunction("GetMouseButtonDown", std::function<bool(uint8_t)>([&](uint8_t button) {return inputs.mouse_just_pressed(button); }))
			.addFunction("GetMouseButtonUp", std::function<bool(uint8_t)>([&](uint8_t button) {return inputs.mouse_just_released(button); }))
			.addFunction("GetMouseScrollDelta", std::function<float(lua_State*)>([&](lua_State*) {return inputs.get_scroll_delta(); }))
		.endNamespace()
		.beginNamespace("Audio")
			.addFunction("Play", std::function<void(int, std::string, bool)>([&](int channel, std::string clip_name, bool does_loop) {audio_manager.play_sound(audio_manager.load_sound(clip_name), channel, does_loop); }))
			.addFunction("Halt", std::function<void(int)>([&](int channel) {return audio_manager.stop_sound(channel); }))
			.addFunction("SetVolume", std::function<void(int, float)>([&](int channel, float volume) {return audio_manager.set_volume(channel, static_cast<int>(volume)); }))
		.endNamespace()
		.beginNamespace("Scene")
			.addFunction("Load", std::function<void(std::string)>([&](std::string scene_name) { next_scene = { scene_name }; }))
			.addFunction("GetCurrent", std::function<std::string()>([&]() {return current_scene; }))
			.addFunction("DontDestroy", std::function<void(luabridge::LuaRef)>([&](luabridge::LuaRef lua_actor) {actors.dont_destroy_on_load(lua_actor.cast<LuaActor>().index); }))
		.endNamespace()
		.beginNamespace("Event")
			.addFunction("Publish", std::function<void(std::string, luabridge::LuaRef)>([&](std::string event_type, luabridge::LuaRef message) {events.publish(event_type, message); }))
			.addFunction("Subscribe", std::function<void(std::string, luabridge::LuaRef, luabridge::LuaRef)>([&](std::string event_type, luabridge::LuaRef component, luabridge::LuaRef function) {events.schedule_subscribe(event_type, component, function); }))
			.addFunction("Unsubscribe", std::function<void(std::string, luabridge::LuaRef, luabridge::LuaRef)>([&](std::string event_type, luabridge::LuaRef component, luabridge::LuaRef function) {events.schedule_unsubscribe(event_type, component, function); }))
		.endNamespace()
		.beginNamespace("Math")
			.addFunction("Random", std::function<float(float, float)>([](float min, float max) {return min + static_cast<float>(rand()) / (static_cast<float>(static_cast<float>(RAND_MAX) / (max - min))); }))
			.addFunction("Rotate", std::function<glm::vec3(glm::vec3, glm::vec3)>([](glm::vec3 vec, glm::vec3 rot) {
				glm::mat4 rotation = glm::yawPitchRoll(-rot.x, rot.z, rot.y);
				glm::vec3 out = rotation * glm::vec4(vec, 1.0f);
				return out;
			}))
			.addFunction("RotationCompose", std::function<glm::vec3(glm::vec3, glm::vec3)>([](glm::vec3 a, glm::vec3 b) {
				glm::quat q1 = glm::quat(a);
				glm::quat q2 = glm::quat(b);
				glm::quat qComp = q2 * q1;
				glm::vec3 composed = glm::eulerAngles(qComp);
				return composed;
			}))
		.endNamespace();

	luabridge::setGlobal(lua_state, renderer.get(), "_Renderer");
	luabridge::setGlobal(lua_state, new Camera(lua_state), "Camera");
	load_scene(config->initial_scene);
}

void World::load_scene(std::string scene_name) {
	clear_scene();
	current_scene = scene_name;

	std::string scene_path = translate_path("resources/scenes/") + scene_name + ".scene";
	if (!file_exists(scene_path)) {
		std::cout << "error: scene " << scene_name << " is missing";
		exit(0);
	}

	rapidjson::Document doc = ReadJsonFile(scene_path);
	const auto& actor_list = doc["actors"];
	ActorIndex first_index = numeric_max<ActorIndex>();
	for (uint32_t i = 0; i < actor_list.Size(); i++) {
		const auto& actor_data = actor_list[i];
		ActorIndex new_index = actors.add_actor(templates.create_actor(actor_data));
		if (first_index == numeric_max<ActorIndex>()) {
			first_index = new_index;
		}
	}
}

bool World::run_turn() {
	std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();
	if (next_scene.has_value()) {
		load_scene(next_scene.value());
	}
	bool ending = process_events();
	update_actors();
	events.apply_scheduled();
	renderer->renderPresentFrame();
	*frame_number += 1;
	std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double> elapsed = end - start;
	double frame_time = elapsed.count();
	if (*frame_number % 60 == 0) {
		std::cout << "Last frame time: " << frame_time << std::endl;
	}
	return ending;
}
