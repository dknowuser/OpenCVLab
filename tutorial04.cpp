// Include standard headers
#include <stdio.h>
#include <stdlib.h>
#include <ctime>
#include <Windows.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>

// Include GLEW
#include <GL/glew.h>

// Include GLFW
#include <GLFW/glfw3.h>
GLFWwindow* window;

// Include GLM
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
using namespace glm;

#include <common/shader.hpp>

#include "CL/cl.hpp"

#define __CL_ENABLE_EXCEPTIONS

namespace TaskEnvironment {
	constexpr unsigned int GraphicsCardIndex = 1;
	constexpr unsigned int N = 12800 * 3;
	constexpr float G = 6.67e-11;
	constexpr unsigned int T = 6400; // Simulation time
	constexpr float dt = 100; //1e-3

	unsigned int actN = 12800 * 3;
	unsigned int metresConstraint = 1000; // Distance constraint in metres
	unsigned int massConstraint = 10;	  // Mass constraint in kg (10) should be less than 1000
	double time = 0;

	struct PointsWithProps {
		float x, y, z;
		float mass;
	};

	float points[N];
	float masses[N / 3];
	float accel[N];
	float speed[N];

	void initBuffers(GLfloat *g_vertex_buffer_data, GLfloat *g_color_buffer_data)
	{
		#pragma omp parallel for
		for (int i = 0; i < actN / 3; i++) {
			points[3 * i + 0] = (float)rand() / (float)RAND_MAX * (float)metresConstraint - (float)rand() / (float)RAND_MAX * (float)metresConstraint;
			points[3 * i + 1] = (float)rand() / (float)RAND_MAX * (float)metresConstraint - (float)rand() / (float)RAND_MAX * (float)metresConstraint;
			points[3 * i + 2] = (float)rand() / (float)RAND_MAX * (float)metresConstraint - (float)rand() / (float)RAND_MAX * (float)metresConstraint;
			masses[i] = (rand()) % massConstraint;

			accel[3 * i + 0] = 0;
			accel[3 * i + 1] = 0;
			accel[3 * i + 2] = 0;

			speed[3 * i + 0] = 0;
			speed[3 * i + 1] = 0;
			speed[3 * i + 2] = 0;

			g_vertex_buffer_data[3 * i + 0] = points[3 * i + 0] / (float)metresConstraint;
			g_vertex_buffer_data[3 * i + 1] = points[3 * i + 1] / (float)metresConstraint;
			g_vertex_buffer_data[3 * i + 2] = points[3 * i + 2] / (float)metresConstraint;

			if (masses[i] <= (massConstraint / 3)) {
				g_color_buffer_data[3 * i + 0] = 0.0f;
				g_color_buffer_data[3 * i + 1] = 0.0f;
				g_color_buffer_data[3 * i + 2] = 1.0f;
			}
			else
			{
				if (masses[i] <= (2 * massConstraint / 3)) {
					g_color_buffer_data[3 * i + 0] = 0.0f;
					g_color_buffer_data[3 * i + 1] = 1.0f;
					g_color_buffer_data[3 * i + 2] = 0.0f;
				}
				else {
					g_color_buffer_data[3 * i + 0] = 1.0f;
					g_color_buffer_data[3 * i + 1] = 0.0f;
					g_color_buffer_data[3 * i + 2] = 0.0f;
				};
			};
		};
	};
};

int main( void )
{
	SYSTEMTIME beginTime, endTime;

	// Get all available platforms
	std::vector<cl::Platform> platforms;
	cl::Platform::get(&platforms);

	// Get all available devices on chosen platform
	std::vector<cl::Device> devices;
	platforms[TaskEnvironment::GraphicsCardIndex].getDevices(CL_DEVICE_TYPE_GPU, &devices);

	for (unsigned int i = 0; i < devices.size(); i++) {
		std::cout << devices[i].getInfo<CL_DEVICE_NAME>() << std::endl;
	};
	std::cout << std::endl;

	//For the selected device create a context
	std::vector<cl::Device> contextDevices;
	contextDevices.push_back(devices[0]);
	cl::Context context(contextDevices);

	//For the selected device create a context and command queue
	cl::CommandQueue queue(context, devices[0]);

	// Initialise GLFW
	if( !glfwInit() )
	{
		fprintf( stderr, "Failed to initialize GLFW\n" );
		getchar();
		return -1;
	}

	glfwWindowHint(GLFW_SAMPLES, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // To make MacOS happy; should not be needed
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	// Open a window and create its OpenGL context
	window = glfwCreateWindow( 1024, 768, "GPU Computations", NULL, NULL);
	if( window == NULL ){
		fprintf( stderr, "Failed to open GLFW window. If you have an Intel GPU, they are not 3.3 compatible. Try the 2.1 version of the tutorials.\n" );
		getchar();
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);

	// Initialize GLEW
	glewExperimental = true; // Needed for core profile
	if (glewInit() != GLEW_OK) {
		fprintf(stderr, "Failed to initialize GLEW\n");
		getchar();
		glfwTerminate();
		return -1;
	}

	// Ensure we can capture the escape key being pressed below
	glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);

	// Black background
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

	GLuint VertexArrayID;
	glGenVertexArrays(1, &VertexArrayID);
	glBindVertexArray(VertexArrayID);

	// Create and compile our GLSL program from the shaders
	GLuint programID = LoadShaders( "TransformVertexShader.vertexshader", "ColorFragmentShader.fragmentshader" );

	// Get a handle for our "MVP" uniform
	GLuint MatrixID = glGetUniformLocation(programID, "MVP");

	// Projection matrix : 45° Field of View, 4:3 ratio, display range : 0.1 unit <-> 100 units
	glm::mat4 Projection = glm::perspective(glm::radians(45.0f), 4.0f / 3.0f, 0.1f, 1000.0f);
	// Camera matrix
	glm::mat4 View       = glm::lookAt(
								glm::vec3(4,3,-300), // Camera is at (4,3,-3), in World Space
								glm::vec3(0,0,0), // and looks at the origin
								glm::vec3(0,1,0)  // Head is up (set to 0,-1,0 to look upside-down)
						   );
	// Model matrix : an identity matrix (model will be at the origin)
	glm::mat4 Model      = glm::mat4(1.0f);
	// Our ModelViewProjection : multiplication of our 3 matrices
	glm::mat4 MVP        = Projection * View * Model; // Remember, matrix multiplication is the other way around

	// Our vertices. Tree consecutive floats give a 3D vertex; Three consecutive vertices give a triangle.
	// A cube has 6 faces with 2 triangles each, so this makes 6*2=12 triangles, and 12*3 vertices
	static GLfloat g_vertex_buffer_data[TaskEnvironment::N];
	static GLfloat g_color_buffer_data[TaskEnvironment::N];
	TaskEnvironment::initBuffers(g_vertex_buffer_data, g_color_buffer_data);

	GLuint vertexbuffer;
	glGenBuffers(1, &vertexbuffer);
	glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(g_vertex_buffer_data), g_vertex_buffer_data, GL_DYNAMIC_DRAW);

	GLuint colorbuffer;
	glGenBuffers(1, &colorbuffer);
	glBindBuffer(GL_ARRAY_BUFFER, colorbuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(g_color_buffer_data), g_color_buffer_data, GL_DYNAMIC_DRAW);

	//Create memory buffers
	cl::Buffer clGPUpoints = cl::Buffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
		TaskEnvironment::actN * sizeof(float), TaskEnvironment::points);
	cl::Buffer clGPUmasses = cl::Buffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
		TaskEnvironment::actN / 3 * sizeof(float), TaskEnvironment::masses);
	cl::Buffer clGPUaccel = cl::Buffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
		TaskEnvironment::actN * sizeof(float), TaskEnvironment::accel);
	cl::Buffer clGPUspeed = cl::Buffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
		TaskEnvironment::actN * sizeof(float), TaskEnvironment::speed);
	cl::Buffer clGPUvertex = cl::Buffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
		TaskEnvironment::actN * sizeof(GLfloat), g_vertex_buffer_data);

	//Load OpenCL source code
	std::ifstream sourceFile("OpenCLFile1.cl");
	std::string sourceCode(std::istreambuf_iterator<char>(sourceFile), (std::istreambuf_iterator<char>()));

	//Build OpenCL program and make the kernel
	cl::Program::Sources source(1, std::make_pair(sourceCode.c_str(), sourceCode.length()));
	cl::Program program = cl::Program(context, source);
	program.build(contextDevices, "-I ./");
	cl::Kernel kernel(program, "TestKernel");

	//Set arguments to kernel
	int iArg = 0;
	kernel.setArg(iArg++, clGPUpoints);
	kernel.setArg(iArg++, clGPUmasses);
	kernel.setArg(iArg++, clGPUaccel);
	kernel.setArg(iArg++, clGPUspeed);
	kernel.setArg(iArg++, clGPUvertex);
	kernel.setArg(iArg++, TaskEnvironment::actN / 3);
	kernel.setArg(iArg++, TaskEnvironment::dt);
	kernel.setArg(iArg++, TaskEnvironment::G);
	kernel.setArg(iArg++, TaskEnvironment::metresConstraint);

	GetSystemTime(&beginTime);
	do{

		// Clear the screen
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// For GPU calculations
		queue.enqueueNDRangeKernel(kernel, cl::NDRange(), cl::NDRange(TaskEnvironment::actN), cl::NDRange(256));
		queue.enqueueReadBuffer(clGPUpoints, CL_TRUE, 0, TaskEnvironment::actN * sizeof(float), TaskEnvironment::points);
		queue.enqueueReadBuffer(clGPUmasses, CL_TRUE, 0, TaskEnvironment::actN / 3 * sizeof(float), TaskEnvironment::masses);
		queue.enqueueReadBuffer(clGPUaccel, CL_TRUE, 0, TaskEnvironment::actN * sizeof(float), TaskEnvironment::accel);
		queue.enqueueReadBuffer(clGPUspeed, CL_TRUE, 0, TaskEnvironment::actN * sizeof(float), TaskEnvironment::speed);
		queue.enqueueReadBuffer(clGPUvertex, CL_TRUE, 0, TaskEnvironment::actN * sizeof(GLfloat), g_vertex_buffer_data);

		// Use our shader
		glUseProgram(programID);

		// Send our transformation to the currently bound shader, 
		// in the "MVP" uniform
		glUniformMatrix4fv(MatrixID, 1, GL_FALSE, &MVP[0][0]);

		// 1rst attribute buffer : vertices
		glEnableVertexAttribArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
		glBufferData(GL_ARRAY_BUFFER, sizeof(g_vertex_buffer_data), g_vertex_buffer_data, GL_DYNAMIC_DRAW);
		glVertexAttribPointer(
			0,                  // attribute. No particular reason for 0, but must match the layout in the shader.
			3,                  // size
			GL_FLOAT,           // type
			GL_FALSE,           // normalized?
			0,                  // stride
			(void*)0            // array buffer offset
		);

		// 2nd attribute buffer : colors
		glEnableVertexAttribArray(1);
		glBindBuffer(GL_ARRAY_BUFFER, colorbuffer);
		glBufferData(GL_ARRAY_BUFFER, sizeof(g_color_buffer_data), g_color_buffer_data, GL_DYNAMIC_DRAW);
		glVertexAttribPointer(
			1,                                // attribute. No particular reason for 1, but must match the layout in the shader.
			3,                                // size
			GL_FLOAT,                         // type
			GL_FALSE,                         // normalized?
			0,                                // stride
			(void*)0                          // array buffer offset
		);	

		// Draw the array !
		glDrawArrays(GL_POINTS, 0, TaskEnvironment::actN); // 12*3 indices starting at 0 -> 12 triangles

		glDisableVertexAttribArray(0);
		glDisableVertexAttribArray(1);

		// Swap buffers
		glfwSwapBuffers(window);
		glfwPollEvents();

		TaskEnvironment::time += TaskEnvironment::dt;
	} // Check if the ESC key was pressed or the window was closed
	while( glfwGetKey(window, GLFW_KEY_ESCAPE ) != GLFW_PRESS &&
		   glfwWindowShouldClose(window) == 0 && (TaskEnvironment::time < TaskEnvironment::T));
	GetSystemTime(&endTime);

	char str[256];
	sprintf(str, "%lu ms", endTime.wMilliseconds - beginTime.wMilliseconds
		+ (endTime.wSecond - beginTime.wSecond) * 1000
		+ (endTime.wMinute - beginTime.wMinute) * 60000
		+ (endTime.wHour - beginTime.wHour) * 3600000);

	std::cout << str;

	// Cleanup VBO and shader
	glDeleteBuffers(1, &vertexbuffer);
	glDeleteBuffers(1, &colorbuffer);
	glDeleteProgram(programID);
	glDeleteVertexArrays(1, &VertexArrayID);

	// Close OpenGL window and terminate GLFW
	glfwTerminate();

	return 0;
}

