#include "CratesMode.hpp"

#include "MenuMode.hpp"
#include "Load.hpp"
#include "Sound.hpp"
#include "MeshBuffer.hpp"
#include "gl_errors.hpp" //helper for dumpping OpenGL error messages
#include "read_chunk.hpp" //helper for reading a vector of structures from a file
#include "data_path.hpp" //helper to get paths relative to executable
#include "compile_program.hpp" //helper to compile opengl shader programs
#include "draw_text.hpp" //helper to... um.. draw text
#include "vertex_color_program.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <fstream>
#include <map>
#include <cstddef>
#include <random>

//Ref from MeshBuffer
Load< WalkMesh > walk_mesh(LoadTagDefault, []() {
    return new WalkMesh(data_path("walkmesh.blob"));
});

Load< MeshBuffer > crates_meshes(LoadTagDefault, [](){
	return new MeshBuffer(data_path("maze.pnc"));
});

Load< GLuint > crates_meshes_for_vertex_color_program(LoadTagDefault, [](){
	return new GLuint(crates_meshes->make_vao_for_program(vertex_color_program->program));
});


Load< Sound::Sample > sample_roar(LoadTagDefault, [](){
	return new Sound::Sample(data_path("european_dragon_roaring_and_breathe_fire.wav"));
});
Load< Sound::Sample > sample_loop(LoadTagDefault, [](){
	//return new Sound::Sample(data_path("cave_ambience.wav"));  //shorter ambience music
	return new Sound::Sample(data_path("atmosphere_cave_loop.wav"));  //longer ambience music
});
Load< Sound::Sample > sample_scary(LoadTagDefault, [](){
	return new Sound::Sample(data_path("scary.wav"));
});


CratesMode::CratesMode() {
	//----------------
	//set up scene:
	//TODO: this should load the scene from a file!

    //Referenced from MeshBuffer.cpp
	std::ifstream file(data_path("maze.scene"), std::ios::binary);
    //str0 len < char > * [strings chunk]
    //xfh0 len < ... > * [transform hierarchy]
    //msh0 len < uint uint uint > [hierarchy point + mesh name]
    //cam0 len < uint params > [heirarchy point + camera params]
    //lig0 len < uint params > [hierarchy point + light params]
    struct TransformEntry {
        int32_t parent_ref;
        uint32_t obj_name_begin, obj_name_end;
        glm::vec3 position;
        glm::vec4 rotation;
        glm::vec3 scale;
    };

    struct MeshesEntry {
        int32_t mesh_ref;
        uint32_t mesh_name_begin, mesh_name_end;
    };

    std::vector< char > strings;
    std::vector< TransformEntry > transforms;
    std::vector< MeshesEntry > meshes;

    read_chunk(file, "str0", &strings);
    read_chunk(file, "xfh0", &transforms);
    read_chunk(file, "msh0", &meshes);
    //read_chunk(file, "cam0", &camera);  //might need to change variable name
    //read_chunk(file, "lig0", &light);

	auto attach_object = [this](Scene::Transform *transform, std::string const &name) {
		Scene::Object *object = scene.new_object(transform);
		object->program = vertex_color_program->program;
		object->program_mvp_mat4 = vertex_color_program->object_to_clip_mat4;
		object->program_mv_mat4x3 = vertex_color_program->object_to_light_mat4x3;
		object->program_itmv_mat3 = vertex_color_program->normal_to_light_mat3;
		object->vao = *crates_meshes_for_vertex_color_program;
		MeshBuffer::Mesh const &mesh = crates_meshes->lookup(name);
		object->start = mesh.start;
		object->count = mesh.count;
		return object;
	};


	{ //build scene from maze.scene
        for (auto& t : transforms) {
            Scene::Transform *trans = scene.new_transform();
            trans->position = t.position;
            trans->rotation = glm::quat(t.rotation.w, t.rotation.x, t.rotation.y, t.rotation.z);
            trans->scale = t.scale;
            std::string name(&strings[0] + t.obj_name_begin, &strings[0] + t.obj_name_end);
            if (name == "CageFloor") {
                cage_floor = attach_object(trans, "CageFloor");
            } else if (name == "Monster") {
                monster = attach_object(trans, "Monster");
            } else if (name == "Player") {
                camera = scene.new_camera(trans);
                camera->transform->rotation = camera->original_rotation;
            }
        }
	}

	//start the 'loop' sample playing at the large crate:
    //                      (position, volumn, Loop or Once)
	loop = sample_loop->play(camera->transform->position, 0.5f, Sound::Loop);
    //play 'scary' once at the beginning
    sample_scary->play(camera->transform->position, 0.3f);


    walk_point = walk_mesh->start(camera->transform->position);  //do I need make_local_to_world()?
    camera->normal = walk_mesh->world_normal(walk_point);
    camera->transform->position = walk_mesh->world_point(walk_point) + camera->height * camera->normal;
    //camera->elevation can be used to modify camera_up
    //camera_up = walk_mesh->world_normal(walk_point);
}

CratesMode::~CratesMode() {
	if (loop) loop->stop();
}

bool CratesMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {
	//ignore any keys that are the result of automatic key repeat:
	if (evt.type == SDL_KEYDOWN && evt.key.repeat) {
		return false;
	}
	//handle tracking the state of WSAD for movement control:
	if (evt.type == SDL_KEYDOWN || evt.type == SDL_KEYUP) {
		if (evt.key.keysym.scancode == SDL_SCANCODE_W) {
			controls.forward = (evt.type == SDL_KEYDOWN);
			return true;
		} else if (evt.key.keysym.scancode == SDL_SCANCODE_S) {
			controls.backward = (evt.type == SDL_KEYDOWN);
			return true;
		} else if (evt.key.keysym.scancode == SDL_SCANCODE_A) {
			controls.left = (evt.type == SDL_KEYDOWN);
			return true;
		} else if (evt.key.keysym.scancode == SDL_SCANCODE_D) {
			controls.right = (evt.type == SDL_KEYDOWN);
			return true;
		}
	}
	//handle tracking the mouse for rotation control:
	if (!mouse_captured) {
		if (evt.type == SDL_KEYDOWN && evt.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
			Mode::set_current(nullptr);
			return true;
		}
		if (evt.type == SDL_MOUSEBUTTONDOWN) {
			SDL_SetRelativeMouseMode(SDL_TRUE);
			mouse_captured = true;
			return true;
		}
	} else if (mouse_captured) {
		if (evt.type == SDL_KEYDOWN && evt.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
			SDL_SetRelativeMouseMode(SDL_FALSE);
			mouse_captured = false;
			return true;
		}
		if (evt.type == SDL_MOUSEMOTION) {
			//Note: float(window_size.y) * camera->fovy is a pixels-to-radians conversion factor
			float yaw = evt.motion.xrel / float(window_size.y) * camera->fovy;
			//float pitch = evt.motion.yrel / float(window_size.y) * camera->fovy;
            //update total rotation of the camera
            camera->azimuth -= yaw;
			//camera->elevation -= pitch;
            //always re-compute the rotation of the camera from its original rotation
            camera->transform->rotation = glm::normalize(
                camera->original_rotation
                * glm::angleAxis(camera->azimuth, glm::vec3(0.0f, 1.0f, 0.0f))
                * glm::angleAxis(camera->elevation, glm::vec3(1.0f, 0.0f, 0.0f))
            );
			return true;
		}
	}
	return false;
}

void CratesMode::update(float elapsed) {
	glm::mat3 directions = glm::mat3_cast(camera->transform->rotation);
	float amt = 5.0f * elapsed;
    //if (controls.right) camera->transform->position += amt * directions[0];
    //if (controls.left) camera->transform->position -= amt * directions[0];
    //if (controls.backward) camera->transform->position += amt * directions[2];
    //if (controls.forward) camera->transform->position -= amt * directions[2];
    if (controls.right)    walk_mesh->walk(walk_point,  amt * directions[0]);
    if (controls.left)     walk_mesh->walk(walk_point, -amt * directions[0]);
    if (controls.backward) walk_mesh->walk(walk_point,  amt * directions[2]);
    if (controls.forward)  walk_mesh->walk(walk_point, -amt * directions[2]);

    //update camera normal and position
    camera->normal = walk_mesh->world_normal(walk_point);
    camera->transform->position = walk_mesh->world_point(walk_point) + camera->height * camera->normal;

	{ //set sound positions:
		glm::mat4 cam_to_world = camera->transform->make_local_to_world();
		Sound::listener.set_position( cam_to_world[3] );
		//camera looks down -z, so right is +x:
		Sound::listener.set_right( glm::normalize(cam_to_world[0]) );

		if (loop) {
			glm::mat4 camera_to_world = camera->transform->make_local_to_world();
			loop->set_position( camera_to_world * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f) );
		}
	}

	roar_countdown -= elapsed;
	if (roar_countdown <= 0.0f) {
		roar_countdown = (rand() / float(RAND_MAX) * 8.0f) + 0.5f;  //Reset the countdown
        glm::mat4x3 monster_to_world = monster->transform->make_local_to_world();
        sample_roar->play( monster_to_world * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f), 0.5f );
	}
}

void CratesMode::draw(glm::uvec2 const &drawable_size) {
	//set up basic OpenGL state:
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	//set up light position + color:
	glUseProgram(vertex_color_program->program);
	glUniform3fv(vertex_color_program->sun_color_vec3, 1, glm::value_ptr(glm::vec3(0.81f, 0.81f, 0.76f)));
	glUniform3fv(vertex_color_program->sun_direction_vec3, 1, glm::value_ptr(glm::normalize(glm::vec3(-0.2f, 0.2f, 1.0f))));
	glUniform3fv(vertex_color_program->sky_color_vec3, 1, glm::value_ptr(glm::vec3(0.4f, 0.4f, 0.45f)));
	glUniform3fv(vertex_color_program->sky_direction_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 1.0f, 0.0f)));
	glUseProgram(0);

	//fix aspect ratio of camera
	camera->aspect = drawable_size.x / float(drawable_size.y);

	scene.draw(camera);

	if (Mode::current.get() == this) {
		glDisable(GL_DEPTH_TEST);
		std::string message;
		if (mouse_captured) {
			message = "ESCAPE TO UNGRAB MOUSE * WASD MOVE";
		} else {
			message = "CLICK TO GRAB MOUSE * ESCAPE QUIT";
		}
		float height = 0.06f;
		float width = text_width(message, height);
		draw_text(message, glm::vec2(-0.5f * width,-0.99f), height, glm::vec4(0.0f, 0.0f, 0.0f, 0.5f));
		draw_text(message, glm::vec2(-0.5f * width,-1.0f), height, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));

		glUseProgram(0);
	}

	GL_ERRORS();
}


void CratesMode::show_pause_menu() {
	std::shared_ptr< MenuMode > menu = std::make_shared< MenuMode >();

	std::shared_ptr< Mode > game = shared_from_this();
	menu->background = game;

	menu->choices.emplace_back("PAUSED");
	menu->choices.emplace_back("RESUME", [game](){
		Mode::set_current(game);
	});
	menu->choices.emplace_back("QUIT", [](){
		Mode::set_current(nullptr);
	});

	menu->selected = 1;

	Mode::set_current(menu);
}
