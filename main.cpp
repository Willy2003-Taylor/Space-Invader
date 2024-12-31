#include <cstdio>
#include <cstdint>
#include <GL/glew.h>
#include <GLFW/glfw3.h>

//GLEW must be read before GLFW when identifying the header

#define GL_ERROR_CASE(glerror)\
	case glerror: snprintf(error, sizeof(error), "%s", #glerror)

	//Why snprintf is used here instead of fprintf?

	/*
	Since we want to use a char array to store the error info
	and snprintf can be only used to format text to an array of char
	*/

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

struct Game {
	size_t width, height;
	size_t num_aliens;
	Alien* aliens;
	Player player;
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


int main() {
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

	

	Sprite alien_sprite0;
	alien_sprite0.width = 11;
	alien_sprite0.height = 8;
	alien_sprite0.data = new uint8_t[11 * 8]{
		0,0,1,0,0,0,0,0,1,0,0, // ..@.....@..
		0,0,0,1,0,0,0,1,0,0,0, // ...@...@...
		0,0,1,1,1,1,1,1,1,0,0, // ..@@@@@@@..
		0,1,1,0,1,1,1,0,1,1,0, // .@@.@@@.@@.
		1,1,1,1,1,1,1,1,1,1,1, // @@@@@@@@@@@
		1,0,1,1,1,1,1,1,1,0,1, // @.@@@@@@@.@
		1,0,1,0,0,0,0,0,1,0,1, // @.@.....@.@
		0,0,0,1,1,0,1,1,0,0,0  // ...@@.@@...
	};

	Sprite alien_sprite1;
	alien_sprite1.width = 11;
	alien_sprite1.height = 8;
	alien_sprite1.data = new uint8_t[11 * 8]{
		0,0,1,0,0,0,0,0,1,0,0, // ..@.....@..
		1,0,0,1,0,0,0,1,0,0,1, // @..@...@..@
		1,0,1,1,1,1,1,1,1,0,1, // @.@@@@@@@.@
		1,1,1,0,1,1,1,0,1,1,1, // @@@.@@@.@@@
		1,1,1,1,1,1,1,1,1,1,1, // @@@@@@@@@@@
		0,1,1,1,1,1,1,1,1,1,0, // .@@@@@@@@@.
		0,0,1,0,0,0,0,0,1,0,0, // ..@.....@..
		0,1,0,0,0,0,0,0,0,1,0  // .@.......@.
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

	SpriteAnimation* alien_animation = new SpriteAnimation;

	//"new" operator returns the memory address of "SpriteAnimation"

	alien_animation->loop = true;
	alien_animation->num_frames = 2;
	alien_animation->frame_duration = 10;
	alien_animation->time = 0;

	alien_animation->frames = new Sprite*[2];
	alien_animation->frames[0] = &alien_sprite0;
	alien_animation->frames[1] = &alien_sprite1;

	Game game;
	game.width = buffer.width;
	game.height = buffer.height;
	game.num_aliens = 55;
	game.aliens = new Alien[game.num_aliens];

	game.player.x = 107;
	game.player.y = 32;

	game.player.life = 3;

	for(size_t yi = 0; yi < 5; ++yi)
		for (size_t xi = 0; xi < 11; ++xi) {
			game.aliens[yi * 11 + xi].x = 16 * xi + 20;
			game.aliens[yi * 11 + xi].y = 17 * yi + 128;
		}

	uint32_t clear_color = rgb_to_uint32(0, 128, 0);
	
	int player_move_direction = 1;

	while (!glfwWindowShouldClose(window)) {
		buffer_clear(&buffer, clear_color);

		for (size_t ai = 0; ai < game.num_aliens; ++ai) {
			const Alien& alien = game.aliens[ai];
			size_t current_frame = alien_animation->time / alien_animation->frame_duration;
			const Sprite& sprite = *alien_animation->frames[current_frame];

			buffer_draw_sprite(&buffer, sprite, alien.x, alien.y, rgb_to_uint32(128, 0, 0));
		}

		buffer_draw_sprite(&buffer, player_sprite, game.player.x, game.player.y, rgb_to_uint32(128, 0, 0));

		alien_animation->time++;

		if (alien_animation->time == alien_animation->num_frames * alien_animation->frame_duration) {
			if (alien_animation->loop) alien_animation->time = 0;
			
			else {
				delete alien_animation;
				alien_animation = nullptr;
			}
		}

		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, buffer.width, buffer.height, GL_RGBA,
						GL_UNSIGNED_INT_8_8_8_8, buffer.data
		);

		glDrawArrays(GL_TRIANGLE_STRIP, 0, 3);

		glfwSwapBuffers(window);
		
		 
		// horizontal, if the value is positive, it points to right; otherwise, left

		if (game.player.x + player_sprite.width + player_move_direction >= game.width - 1) {
			game.player.x = game.width - 1 - player_sprite.width - player_move_direction;
			player_move_direction *= -1;
		}

		else if (game.player.x + player_move_direction <= 0) {
			game.player.x = 0;
			player_move_direction *= -1;
		}

		else game.player.x += player_move_direction;

		glfwPollEvents();
		glfwSwapInterval(1);

		//To turn on V-sync to lock the refresh rate so that the game can run slower
	}

	glfwDestroyWindow(window);
	glfwTerminate();

	glDeleteVertexArrays(1, &fullscreen_triangle_vao);

	delete[] alien_sprite0.data;
	delete[] alien_sprite1.data;
	delete[] alien_animation->frames;
	delete[] buffer.data;
	delete[] game.aliens;

	delete alien_animation;

	return 0;
}
