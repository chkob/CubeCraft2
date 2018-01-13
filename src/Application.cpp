#include <iostream>
#include <GL/glew.h>
#include "Chunk.hpp"
#include "Application.hpp"
#include "Resource.hpp"
#include "Renderer.hpp"

#include <glm/gtx/string_cast.hpp>


//glfw callbacks
void Application::focusCallback(GLFWwindow *window, int focused)
{
	auto app = getCallbackInstance(window);
	if(!focused)
		app->control = false;
}
void Application::framebufferSizeCallback(GLFWwindow *window, int width, int height)
{
	auto app = getCallbackInstance(window);
	app->Width = width;
	app->Height = height;
	glViewport(0, 0, width, height);
	glfwSetCursorPos(window, width / 2, height / 2);
	app->Matrices.UpdateMatrices(width, height, WALK_FOVY);
}
void Application::keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
	auto app = getCallbackInstance(window);
	if(action == GLFW_PRESS)
	{
		if(key == GLFW_KEY_ESCAPE)
		{
			glfwSetCursorPos(window, app->Width / 2, app->Height / 2);
			app->control = !app->control;
		}
		else if(key == GLFW_KEY_V)
			app->showFramewire = !app->showFramewire;
		else if(key == GLFW_KEY_F)
			app->GamePlayer.flying = !app->GamePlayer.flying;
		else if(key == GLFW_KEY_U)
			app->showUI = !app->showUI;
	}
}
void Application::scrollCallback(GLFWwindow *window, double xOffset, double yOffset)
{
	auto app = getCallbackInstance(window);
	auto y = (int)yOffset;

	uint8_t &target = app->GamePlayer.UsingBlock;
	target -= y;
	if(target >= BLOCKS_NUM)
		target = 1;
	else if(target <= 0)
		target = BLOCKS_NUM - 1;
}

Application::Application() : world(), GamePlayer(world),
							 showFramewire(false), control(true), showUI(true)
{
	InitWindow();
}

void Application::InitWindow()
{
	glfwInit();

	//glfwWindowHint(GLFW_SAMPLES, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_RESIZABLE, GL_TRUE);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_FALSE);

	Window = glfwCreateWindow(Width, Height, "CubeCraft2", nullptr, nullptr);
    glfwSetWindowUserPointer(Window, reinterpret_cast<void *>(this));

	if(Window == nullptr)
	{
		printf("ERROR WHEN CREATING WINDOW");
		exit(EXIT_FAILURE);
	}

	glfwMakeContextCurrent(Window);

	glewExperimental = GL_TRUE;
	if(glewInit() != GLEW_OK)
	{
		printf("ERROR WHEN INITIALIZE GLEW");
		exit(EXIT_FAILURE);
	}
	glViewport(0, 0, Width, Height);
	Matrices.UpdateMatrices(Width, Height, WALK_FOVY);

	GameUI.Init(Window, false);
	Resource::InitResources();

	glfwSetWindowFocusCallback(Window, focusCallback);
	glfwSetFramebufferSizeCallback(Window, framebufferSizeCallback);
	glfwSetKeyCallback(Window, keyCallback);
	glfwSetScrollCallback(Window, scrollCallback);

	glCullFace(GL_CCW);
}

void Application::Run()
{
	GamePlayer.SetPosition({GamePlayer.GetPosition().x, 150.0f, GamePlayer.GetPosition().z});
	while(!glfwWindowShouldClose(Window))
	{
		//logic process
		LogicProcess();
		//render
		Render();
		if(showUI)
		{
			GameUI.NewFrame();
			RenderUI();
			GameUI.Render();
		}
		glfwSetInputMode(Window, GLFW_CURSOR, control ? GLFW_CURSOR_HIDDEN : GLFW_CURSOR_NORMAL);

		glfwSwapBuffers(Window);
		glfwPollEvents();
	}
}

void Application::Render()
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glClearColor(0.6f, 0.8f, 1.0f, 1.0f);

	glm::mat4 ViewMatrix = GamePlayer.GetViewMatrix();
	glm::mat4 vpMatrix = Matrices.Projection3d * ViewMatrix;

	if(showFramewire)
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	else
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	Renderer::RenderWorld(world, vpMatrix, GamePlayer.GetPosition());
	Renderer::RenderCrosshair(Matrices.Matrix2dCenter);
	Renderer::RenderSelectionBox(vpMatrix, GamePlayer.GetSelection(false));
}

void Application::LogicProcess()
{
	//update frame rate info
	FramerateManager.UpdateFrameRateInfo();

	static int lastTime = 0;
	if(glfwGetTime() > lastTime + 1)
	{
		lastTime = static_cast<int>(glfwGetTime());
		FPS = FramerateManager.GetFps();
	}

	GamePlayer.Control(control, Window, Width, Height, FramerateManager, Matrices.Projection3d);

	world.Update(GamePlayer.GetChunkPosition());
}

void Application::RenderUI()
{
	//copied from IMGUI example

	//information box
	const float DISTANCE = 10.0f;
	int corner = 0;
	ImVec2 window_pos = ImVec2((corner & 1) ? ImGui::GetIO().DisplaySize.x - DISTANCE : DISTANCE, (corner & 2) ? ImGui::GetIO().DisplaySize.y - DISTANCE : DISTANCE);
	ImVec2 window_pos_pivot = ImVec2((corner & 1) ? 1.0f : 0.0f, (corner & 2) ? 1.0f : 0.0f);
	ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.3f)); // Transparent background
	if (ImGui::Begin("INFO", nullptr,
					 ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings))
	{
		ImGui::Text("fps: %f", FPS);
		ImGui::Text("running threads: %u", world.GetRunningThreadNum());
		ImGui::Text("position: %s", glm::to_string(GamePlayer.GetPosition()).c_str());
		ImGui::Text("chunk position: %s", glm::to_string(GamePlayer.GetChunkPosition()).c_str());
		ImGui::Separator();
		ImGui::Text("flying [F]: %s", GamePlayer.flying ? "true" : "false");
		ImGui::Text("frame wire [V]: %s", showFramewire ? "true" : "false");
		ImGui::Separator();
		ImGui::Text("using block: %s", BlockMethods::GetName(GamePlayer.UsingBlock));

		ImGui::End();
	}
	ImGui::PopStyleColor();
}

