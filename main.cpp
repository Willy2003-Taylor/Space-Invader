#include <cstdio>
#include <cstdint>
#include <GL/glew.h>
#include <GLFW/glfw3.h>

//GLEW must be read before GLFW when identifying the header

int move_direction = 0;

bool game_running = false;
bool fire_pressed = 0;

#define GL_ERROR_CASE(glerror)\
	case glerror: snprintf(error, sizeof(error), "%s", #glerror)

	//Why snprintf is used here instead of fprintf?

	/*
	Since we want to use a char array to store the error info
	and snprintf can be only used to format text to an array of char
	*/

#define GAME_MAX_BULLETS 128

inline void gl_debug(const char* file, int line) {
	GLenum err;
	while ((err = glGetError()) != GL_NO_ERROR) {
		char error[128];

		switch (err) {
			GL_ERROR_CASE(GL_INVALID_ENUM); break;
			GL_ERROR_CASE(GL_INVALID_VALUE); break;
			GL_ERROR_CASE(GL_INVALID_OPERATION); break;
			GL_ERROR_CASE(GL_INVALID_FRAMEBUFFER_OPERATION); break;
			GL_ERROR_CASE(GL_OUT_OF_MEMORY); break;
		default: snprintf(error, sizeof(error), "%s", "UNKNOWN_ERROR"); break;
		}

		fprintf(stderr, "%s - %s: %d\n", error, file, line);
	}
}

#undef GL_ERROR_CASE

void error_callback(int error, const char* description) {
	fprintf(stderr, "Error: %s\n", description);
}
	//Give an error callback and store an error description to stderr

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
	/*"mods" indicates if any key modifiers are pressed.*/
	switch (key) {
	case GLFW_KEY_ESCAPE:
		if (action == GLFW_PRESS) game_running = false;
		break;

	case GLFW_KEY_LEFT:
		if (action == GLFW_PRESS) move_direction -= 1;
		else if (action == GLFW_RELEASE) move_direction += 1;
		break;

	case GLFW_KEY_RIGHT:
		if (action == GLFW_PRESS) move_direction += 1;
		else if (action == GLFW_RELEASE) move_direction -= 1;
		break;

	case GLFW_KEY_SPACE:
		if (action == GLFW_PRESS) fire_pressed = true;
		break;

		//Bind the firing to "Space" by adding one more switch case to the key callback
	default:
		break;
	}
}



struct Buffer {
	size_t width, height;
	uint32_t* data;
};
//Create a buffer to be passed to GPU as a texture and drawn to computer screen

struct Sprite {
	size_t width, height;
	uint8_t* data;
};

struct Alien {
	size_t x, y;
	uint8_t type;
};

struct Player {
	size_t x, y;
	size_t life;
};

struct Bullet {
	size_t x, y;
	int direction;
};

struct Game {
	size_t width, height;
	size_t num_aliens;
	size_t num_bullets;
	Alien* aliens;
	Player player;
	Bullet bullets[GAME_MAX_BULLETS];
};

struct SpriteAnimation {
	bool loop;
	/*"Loop" determines whether the animation should
	repeat continuously or stop after playing once*/

	size_t num_frames;
	size_t frame_duration;
	size_t time;

	Sprite** frames;

	/*Why do we use double pointers here?
	
	Later we will create a SpriteAnimation object using 'new' operator,
	which returns the memory of the created object 
	
	To dynamically allocate the frame, we need to obtain its memory */
};

enum Alien_Type : uint8_t {
	Alien_Dead = 0,
	Alien_Type_A = 1,
	Alien_Type_B = 2,
	Alien_Type_C = 3
};

uint32_t rgb_to_uint32(uint8_t red, uint8_t green, uint8_t blue) {
	return (red << 24) | (green << 16) | (blue << 8) | 255;
}

void buffer_clear(Buffer* buffer, uint32_t color) {
	for (size_t i = 0; i < buffer->width * buffer->height; ++i)
		buffer->data[i] = color;
}

void buffer_draw_sprite(Buffer* buffer, const Sprite& sprite, size_t x, size_t y, uint32_t color) {
	for (size_t xi = 0; xi < sprite.width; ++xi)
		for (size_t yi = 0; yi < sprite.height; ++yi) {
			size_t sx = x + xi;
			size_t sy = y - yi + sprite.height - 1;

			if (sprite.data[xi + yi * sprite.width] && sx < buffer->width && sy < buffer->height)
				buffer->data[sy * buffer->width + sx] = color;
		}
}

void validate_shader(GLuint shader, const char* file = 0) {

	/*About parameter file: If a file name is provided as a string, 
	it will used in error messages; otherwise, the default value(0) will be used*/

	static const int buffer_size = 512;
	char buffer[buffer_size];
	GLsizei length = 0;

	/*Type GLsizei is used by OpenGL for sizes / lengths
	'length' will store the actual length of the error log string retrieved 
	from OpenGL*/

	glGetShaderInfoLog(shader, buffer_size, &length, buffer);

	if (length > 0)
		printf("Shader %d(%s) compile error: %s\n",
			shader, (file ? file : ""), buffer);
}

bool validate_program(GLuint program) {
	static const int buffer_size = 512;
	char buffer[buffer_size];
	GLsizei length = 0;

	glGetShaderInfoLog(program, buffer_size, &length, buffer);

	if (length > 0) {
		printf("Program %d link error: %s\n", program, buffer);
		return false;
	}
	
	return true;
}

bool sprite_overlap_check(const Sprite& sp_a, size_t x_a, size_t y_a, const Sprite& sp_b, size_t x_b, size_t y_b) {

	if (x_a < x_b + sp_b.width && x_a + sp_a.width > x_b && y_a < y_b + sp_b.height && y_a + sp_b.height > y_b)
		return true;

	return false;
}

int main(){
	const size_t buffer_width = 224;
	const size_t buffer_height = 256;


	glfwSetErrorCallback(error_callback);
	GLFWwindow* window;
	
	if (!glfwInit()) return -1;

	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3); //3
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3); //0.3 Combine -> 3.3
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

	window = glfwCreateWindow(640, 480, "Space Invaders", NULL, NULL);

	if (!window) {
		glfwTerminate();
		return -1;
	}

 	glfwSetKeyCallback(window, key_callback);

	glfwMakeContextCurrent(window);

	//Create a window


	GLenum err = glewInit();

	if (err != GLEW_OK) {
		fprintf(stderr, "Failed to initialize GLEW.\n");
		glfwTerminate();
		return -1;
	}

	int glVersion[2] = { -1, 1 };
	
	// The initial value is given manually

	glGetIntegerv(GL_MAJOR_VERSION, &glVersion[0]);
	glGetIntegerv(GL_MINOR_VERSION, &glVersion[1]);

	printf("Using OPENGL: %d.%d\n", glVersion[0], glVersion[1]);
	printf("Renderer used: %s\n", glGetString(GL_RENDERER));
	printf("Shading Language: %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));

	glClearColor(1.0, 0.0, 0.0, 1.0);

	
	
	Buffer buffer;
	

	buffer.width = buffer_width;
	buffer.height = buffer_height;
	buffer.data = new uint32_t[buffer.width * buffer.height];
		
	buffer_clear(&buffer, 0);

	//Buffer texture
	GLuint buffer_texture;
	glGenTextures(1, &buffer_texture);

	glBindTexture(GL_TEXTURE_2D, buffer_texture);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, buffer.width, buffer.height, 0,
		GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, buffer.data);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);



	const char* vertex_shader =
		"\n"
		"#version 330\n"
		"\n"
		"noperspective out vec2 TexCoord;\n"
		"\n"
		"void main(void){\n"
		"\n"
		"    TexCoord.x = (gl_VertexID == 2)? 2.0: 0.0;\n"
		"    TexCoord.y = (gl_VertexID == 1)? 2.0: 0.0;\n"
		"    \n"
		"    gl_Position = vec4(2.0 * TexCoord - 1.0, 0.0, 1.0);\n"
		"}\n";

	const char* fragment_shader = 
		"\n"
		"#version 330\n"
		"\n"
		"uniform sampler2D buffer;\n"
		"noperspective in vec2 TexCoord;\n"
		"\n"
		"out vec3 outColor;\n"
		"\n"
		"void main(void){\n"
		"    outColor = texture(buffer, TexCoord).rgb;\n"
		"}\n";

	GLuint fullscreen_triangle_vao;
	glGenVertexArrays(1, &fullscreen_triangle_vao);
	glBindVertexArray(fullscreen_triangle_vao);


	GLuint shader_id = glCreateProgram();

	//Create a vertex shader
	{
		GLuint shader_vertex = glCreateShader(GL_VERTEX_SHADER);

		glShaderSource(shader_vertex, 1, &vertex_shader, 0);
		glCompileShader(shader_vertex);
		validate_shader(shader_vertex, vertex_shader);
		glAttachShader(shader_id, shader_vertex);

		glDeleteShader(shader_vertex);

	}

	//Create a fragment shader
	{
		GLuint shader_fragment = glCreateShader(GL_FRAGMENT_SHADER);

		glShaderSource(shader_fragment, 1, &fragment_shader, 0);
		glCompileShader(shader_fragment);
		validate_shader(shader_fragment, fragment_shader);
		glAttachShader(shader_id, shader_fragment);

		glDeleteShader(shader_fragment);
	}

	glLinkProgram(shader_id);

	if (!validate_program(shader_id)) {

		fprintf(stderr, "Error while validating the shader.\n");
		glfwTerminate();
		glDeleteVertexArrays(1, &fullscreen_triangle_vao);
		delete[] buffer.data;

		/*delete[] is specifically for arrays that were allocated using new[]
		
		If you use delete instead of delete[] on a dynamically allocated array, only the first element of the array will be destroyed*/
		
		return -1;
	}

	glUseProgram(shader_id);

	GLint location = glGetUniformLocation(shader_id, "buffer");
	glUniform1i(location, 0);

	glDisable(GL_DEPTH_TEST);
	glBindVertexArray(fullscreen_triangle_vao);

	Sprite alien_sprites[6];

	alien_sprites[0].width = 8;
	alien_sprites[0].height = 8;
	alien_sprites[0].data = new uint8_t[64]
	{
		0,0,0,1,1,0,0,0, // ...@@...
		0,0,1,1,1,1,0,0, // ..@@@@..
		0,1,1,1,1,1,1,0, // .@@@@@@.
		1,1,0,1,1,0,1,1, // @@.@@.@@
		1,1,1,1,1,1,1,1, // @@@@@@@@
		0,1,0,1,1,0,1,0, // .@.@@.@.
		1,0,0,0,0,0,0,1, // @......@
		0,1,0,0,0,0,1,0  // .@....@.
	};

	alien_sprites[1].width = 8;
	alien_sprites[1].height = 8;
	alien_sprites[1].data = new uint8_t[64]
	{
		0,0,0,1,1,0,0,0, // ...@@...
		0,0,1,1,1,1,0,0, // ..@@@@..
		0,1,1,1,1,1,1,0, // .@@@@@@.
		1,1,0,1,1,0,1,1, // @@.@@.@@
		1,1,1,1,1,1,1,1, // @@@@@@@@
		0,0,1,0,0,1,0,0, // ..@..@..
		0,1,0,1,1,0,1,0, // .@.@@.@.
		1,0,1,0,0,1,0,1  // @.@..@.@
	};

	alien_sprites[2].width = 11;
	alien_sprites[2].height = 8;
	alien_sprites[2].data = new uint8_t[88]
	{
		0,0,1,0,0,0,0,0,1,0,0, // ..@.....@..
		0,0,0,1,0,0,0,1,0,0,0, // ...@...@...
		0,0,1,1,1,1,1,1,1,0,0, // ..@@@@@@@..
		0,1,1,0,1,1,1,0,1,1,0, // .@@.@@@.@@.
		1,1,1,1,1,1,1,1,1,1,1, // @@@@@@@@@@@
		1,0,1,1,1,1,1,1,1,0,1, // @.@@@@@@@.@
		1,0,1,0,0,0,0,0,1,0,1, // @.@.....@.@
		0,0,0,1,1,0,1,1,0,0,0  // ...@@.@@...
	};

	alien_sprites[3].width = 11;
	alien_sprites[3].height = 8;
	alien_sprites[3].data = new uint8_t[88]
	{
		0,0,1,0,0,0,0,0,1,0,0, // ..@.....@..
		1,0,0,1,0,0,0,1,0,0,1, // @..@...@..@
		1,0,1,1,1,1,1,1,1,0,1, // @.@@@@@@@.@
		1,1,1,0,1,1,1,0,1,1,1, // @@@.@@@.@@@
		1,1,1,1,1,1,1,1,1,1,1, // @@@@@@@@@@@
		0,1,1,1,1,1,1,1,1,1,0, // .@@@@@@@@@.
		0,0,1,0,0,0,0,0,1,0,0, // ..@.....@..
		0,1,0,0,0,0,0,0,0,1,0  // .@.......@.
	};

	alien_sprites[4].width = 12;
	alien_sprites[4].height = 8;
	alien_sprites[4].data = new uint8_t[96]
	{
		0,0,0,0,1,1,1,1,0,0,0,0, // ....@@@@....
		0,1,1,1,1,1,1,1,1,1,1,0, // .@@@@@@@@@@.
		1,1,1,1,1,1,1,1,1,1,1,1, // @@@@@@@@@@@@
		1,1,1,0,0,1,1,0,0,1,1,1, // @@@..@@..@@@
		1,1,1,1,1,1,1,1,1,1,1,1, // @@@@@@@@@@@@
		0,0,0,1,1,0,0,1,1,0,0,0, // ...@@..@@...
		0,0,1,1,0,1,1,0,1,1,0,0, // ..@@.@@.@@..
		1,1,0,0,0,0,0,0,0,0,1,1  // @@........@@
	};


	alien_sprites[5].width = 12;
	alien_sprites[5].height = 8;
	alien_sprites[5].data = new uint8_t[96]
	{
		0,0,0,0,1,1,1,1,0,0,0,0, // ....@@@@....
		0,1,1,1,1,1,1,1,1,1,1,0, // .@@@@@@@@@@.
		1,1,1,1,1,1,1,1,1,1,1,1, // @@@@@@@@@@@@
		1,1,1,0,0,1,1,0,0,1,1,1, // @@@..@@..@@@
		1,1,1,1,1,1,1,1,1,1,1,1, // @@@@@@@@@@@@
		0,0,1,1,1,0,0,1,1,1,0,0, // ..@@@..@@@..
		0,1,1,0,0,1,1,0,0,1,1,0, // .@@..@@..@@.
		0,0,1,1,0,0,0,0,1,1,0,0  // ..@@....@@..
	};

	Sprite alien_death_sprite;
	alien_death_sprite.width = 13;
	alien_death_sprite.height = 7;
	alien_death_sprite.data = new uint8_t[91]
	{
		0,1,0,0,1,0,0,0,1,0,0,1,0, // .@..@...@..@.
		0,0,1,0,0,1,0,1,0,0,1,0,0, // ..@..@.@..@..
		0,0,0,1,0,0,0,0,0,1,0,0,0, // ...@.....@...
		1,1,0,0,0,0,0,0,0,0,0,1,1, // @@.........@@
		0,0,0,1,0,0,0,0,0,1,0,0,0, // ...@.....@...
		0,0,1,0,0,1,0,1,0,0,1,0,0, // ..@..@.@..@..
		0,1,0,0,1,0,0,0,1,0,0,1,0  // .@..@...@..@.
	};

	Sprite player_sprite;
	player_sprite.width = 11;
	player_sprite.height = 7;
	player_sprite.data = new uint8_t[11 * 7]{
		0,0,0,0,0,1,0,0,0,0,0, // .....@.....
		0,0,0,0,1,1,1,0,0,0,0, // ....@@@....
		0,0,0,0,1,1,1,0,0,0,0, // ....@@@....
		0,1,1,1,1,1,1,1,1,1,0, // .@@@@@@@@@.
		1,1,1,1,1,1,1,1,1,1,1, // @@@@@@@@@@@
		1,1,1,1,1,1,1,1,1,1,1, // @@@@@@@@@@@
		1,1,1,1,1,1,1,1,1,1,1, // @@@@@@@@@@@
	};

	Sprite bullet_sprite;
	bullet_sprite.width = 1;
	bullet_sprite.height = 3;
	bullet_sprite.data = new uint8_t[1 * 3]{
		1, // @
		1, // @
		1  // @
	};

	SpriteAnimation alien_animation[3];

	for (size_t i = 0; i < 3; ++i) {
		alien_animation[i].loop = true;
		alien_animation[i].num_frames = 2;
		alien_animation[i].frame_duration = 10;
		alien_animation[i].time = 0;

		alien_animation[i].frames = new Sprite * [2];
		alien_animation[i].frames[0] = &alien_sprites[2 * i];
		alien_animation[i].frames[1] = &alien_sprites[2 * i + 1];
	}

	

	Game game;
	game.width = buffer.width;
	game.height = buffer.height;
	game.num_bullets = 0;
	game.num_aliens = 55;
	game.aliens = new Alien[game.num_aliens];

	game.player.x = 107;
	game.player.y = 32;

	game.player.life = 3;

	for(size_t yi = 0; yi < 5; ++yi)
		for (size_t xi = 0; xi < 11; ++xi) {
			Alien& alien = game.aliens[yi * 11 + xi];
			alien.type = (5 - yi) / 2 + 1;

			const Sprite& sprite = alien_sprites[2 * (alien.type - 1)];

			alien.x = 16 * xi + 20 + (alien_death_sprite.width - sprite.width) / 2;
			alien.y = 17 * yi + 128;
		}

	uint8_t* death_counters = new uint8_t[game.num_aliens];

	for (size_t i = 0; i < game.num_aliens; ++i)
		death_counters[i] = 10;

	uint32_t clear_color = rgb_to_uint32(0, 128, 0);
	
	int player_move_direction = 1;
	game_running = true;

	while (!glfwWindowShouldClose(window) && game_running) {
		buffer_clear(&buffer, clear_color);

		for (size_t ai = 0; ai < game.num_aliens; ++ai) {
			if (!death_counters[ai]) continue;

			const Alien& alien = game.aliens[ai];

			if (alien.type == Alien_Dead)
				buffer_draw_sprite(&buffer, alien_death_sprite, alien.x, alien.y, rgb_to_uint32(128, 0, 0));

			else {
				const SpriteAnimation& animation = alien_animation[alien.type - 1];
				size_t current_frame = alien_animation->time / alien_animation->frame_duration;
				const Sprite sprite = *alien_animation->frames[current_frame];

				buffer_draw_sprite(&buffer, sprite, alien.x, alien.y, rgb_to_uint32(128, 0, 0));
			}
		}

		for (size_t bi = 0; bi < game.num_bullets; ++bi) {
			const Bullet& bullet = game.bullets[bi];
			const Sprite& sprite = bullet_sprite;
			buffer_draw_sprite(&buffer, sprite, bullet.x, bullet.y, rgb_to_uint32(128, 0, 0));
		}

		buffer_draw_sprite(&buffer, player_sprite, game.player.x, game.player.y, rgb_to_uint32(128, 0, 0));

		for (size_t i = 0; i < 3; ++i) {
			alien_animation[i].time++;

			if (alien_animation[i].time == alien_animation[i].frame_duration * alien_animation[i].num_frames)
				alien_animation[i].time = 0;
		}

		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, buffer.width, buffer.height, GL_RGBA,
			GL_UNSIGNED_INT_8_8_8_8, buffer.data
		);

		glDrawArrays(GL_TRIANGLE_STRIP, 0, 3);

		glfwSwapBuffers(window);

		//Simulate aliens

		for (size_t ai = 0; ai < game.num_aliens; ++ai) {
			const Alien& alien = game.aliens[ai];

			if (alien.type == Alien_Dead && death_counters[ai])
				death_counters[ai]--;
		}

		//Simulate bullets

		for (size_t bi = 0; bi < game.num_bullets; ) {
			game.bullets[bi].y += game.bullets[bi].direction;

			if (game.bullets[bi].y >= game.height || game.bullets[bi].y < bullet_sprite.height) {
				game.bullets[bi] = game.bullets[game.num_bullets - 1];
				game.num_bullets--;
				continue;
			}
				//Simulate the hit

			for (size_t ai = 0; ai < game.num_aliens; ++ai) {
				const Alien& alien = game.aliens[ai];
				if (alien.type == Alien_Dead) continue;

				const SpriteAnimation& animation = alien_animation[alien.type - 1];
				size_t current_frame = animation.time / animation.frame_duration;
				const Sprite& alien_sprite = *alien_animation->frames[current_frame];

				bool overlap = sprite_overlap_check(bullet_sprite, game.bullets[bi].x,
					game.bullets[bi].y, alien_sprite, alien.x, alien.y);

				if (overlap) {
					game.aliens[ai].type = Alien_Dead;

					game.aliens[ai].x -= (alien_death_sprite.width - alien_sprites->width) / 2;
					game.bullets[bi] = game.bullets[game.num_bullets - 1];
					game.num_bullets--;
					continue;
				}
			}

			bi++;
		}

			player_move_direction = 2 * move_direction;
			// horizontal, if the value is positive, it points to right; otherwise, left

			if (player_move_direction != 0) {
				if (game.player.x + player_sprite.width + player_move_direction >= game.width - 1)
					game.player.x = game.width - 1 - player_sprite.width - player_move_direction;

				else if (game.player.x + player_move_direction <= 0)
					game.player.x = 0;

				else game.player.x += player_move_direction;
			}

			if (fire_pressed && game.num_bullets < GAME_MAX_BULLETS) {
				game.bullets[game.num_bullets].x = game.player.x + player_sprite.width / 2;
				game.bullets[game.num_bullets].y = game.player.y + player_sprite.height;
				game.bullets[game.num_bullets].direction = 1;
				game.num_bullets++;
			}

			fire_pressed = false;

			glfwPollEvents();
			glfwSwapInterval(1);

			//To turn on V-sync to lock the refresh rate so that the game can run slower
	}
	glfwDestroyWindow(window);
	glfwTerminate();

	glDeleteVertexArrays(1, &fullscreen_triangle_vao);

	for (size_t i = 0; i < 6; ++i)
		delete[] alien_sprites[i].data;

	delete[] alien_death_sprite.data;

	for (size_t i = 0; i < 3; ++i)
		delete[] alien_animation[i].frames;

	delete[] buffer.data;
	delete[] game.aliens;

	delete[] death_counters;

	return 0;
}
