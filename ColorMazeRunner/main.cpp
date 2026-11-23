#include <iostream>
#include <GL/glew.h>
#include <GL/glfw3.h>

#include <gl/glm/glm.hpp>
#include <gl/glm/gtc/matrix_transform.hpp>

#include "Shader.h"
#include "Camera.h"

//--- 사용자 정의 함수
void init_maze();
void draw_maze(Shader* shader);
void init_obstacle();
void draw_obstacle(Shader* shader);
GLvoid drawScene();

// ***************************************************************************************************************************************

//--- 필요한 변수 선언
GLint width, height;
Shader* floorShader = nullptr;

// 카메라 (1인칭 플레이어 시점)
const float PLAYER_EYE_HEIGHT = 1.0f;  // 바닥(y=0)에서 눈까지 높이
Camera* camera = nullptr;

// 마우스: 이전 프레임 커서 위치 (이동량 계산용)
double lastMouseX = 0.0, lastMouseY = 0.0;
bool firstMouse = true;  // 첫 입력 때 시점이 확 튀지 않게 하기 위한 플래그

// 조명
const glm::vec3 lightPos = glm::vec3(0.0f, 15.0f, 10.0f);
const glm::vec3 lightColor = glm::vec3(1.0f, 1.0f, 1.0f); // 조명 색

//--- 그리기 콜백 함수
GLvoid drawScene() {
	glClearColor(0.5f, 0.7f, 1.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	floorShader->use();

	//--- 뷰잉 변환, 투영 변환 (카메라에서 받아옴)
	glm::mat4 view = camera->viewMatrix();
	glm::mat4 proj = camera->projectionMatrix((float)width / (float)height);
	floorShader->setMat4("view", view);
	floorShader->setMat4("projection", proj);

	floorShader->setVec3("lightPos", lightPos);
	floorShader->setVec3("lightColor", lightColor);
	floorShader->setVec3("viewPos", camera->position);
	floorShader->setBool("lightOn", true);
	floorShader->setInt("unlit", 0);

	draw_maze(floorShader);
	draw_obstacle(floorShader);
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
	if (action != GLFW_PRESS)
		return;

	// ESC: 프로그램 종료
	if (key == GLFW_KEY_ESCAPE)
		glfwSetWindowShouldClose(window, GLFW_TRUE);
}

void cursor_callback(GLFWwindow* window, double xpos, double ypos) {
	if (firstMouse) {
		lastMouseX = xpos;
		lastMouseY = ypos;
		firstMouse = false;
	}

	float dx = (float)(xpos - lastMouseX);
	float dy = (float)(ypos - lastMouseY);
	lastMouseX = xpos;
	lastMouseY = ypos;

	camera->processMouse(dx, dy);
}

// ***************************************************************************************************************************************
//--- 메인 함수
int main(int argc, char** argv)
{
	//--- GLFW 초기화
	if (!glfwInit())
		return -1;

	//--- OpenGL 버전 및 프로파일 설정
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	//--- 윈도우 생성
	GLFWmonitor* monitor = glfwGetPrimaryMonitor();
	const GLFWvidmode* mode = glfwGetVideoMode(monitor);
	width = mode->width;
	height = mode->height;

	GLFWwindow* window = glfwCreateWindow(width, height, "Color Maze", monitor, nullptr);
	if (!window)
	{
		glfwTerminate();
		return -1;
	}

	glfwMakeContextCurrent(window);

	//--- GLEW 초기화
	glewExperimental = GL_TRUE;
	if (glewInit() != GLEW_OK)
	{
		glfwDestroyWindow(window);
		glfwTerminate();
		return -1;
	}

	//--- 실제 그려지는 픽셀 크기로 뷰포트 설정 (고DPI 모니터 대비)
	glfwGetFramebufferSize(window, &width, &height);
	glViewport(0, 0, width, height);

	glEnable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);

	//--- 세이더 읽어와서 세이더 프로그램 만들기
	floorShader = new Shader("light_vertex.glsl", "light_fragment.glsl");



	//--- VBO, VAO 생성하고 설정하기
	init_maze();
	init_obstacle();

	//--- 카메라: 원점의 장애물에서 z+10 떨어진 곳에서 -z 방향(장애물 쪽)을 바라보며 시작
	camera = new Camera(glm::vec3(0.0f, PLAYER_EYE_HEIGHT, 10.0f), -90.0f);

	glfwSetKeyCallback(window, key_callback);
	glfwSetCursorPosCallback(window, cursor_callback);

	//--- 커서를 숨기고 창 안에 가둔다 (FPS 방식: 마우스를 계속 움직여도 화면 끝에 막히지 않음)
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	double lastTime = glfwGetTime();

	//**************************렌더링 루프******************************
	while (!glfwWindowShouldClose(window)) {
		//--- 프레임 사이 시간(dt): 컴퓨터 속도와 상관없이 이동 속도를 일정하게 만든다
		double now = glfwGetTime();
		float dt = (float)(now - lastTime);
		lastTime = now;
		if (dt > 0.1f) dt = 0.1f;  // 창 이동 등으로 멈췄다 돌아왔을 때 순간이동 방지

		camera->processKeyboard(window, dt);

		drawScene();
		glfwSwapBuffers(window);
		glfwPollEvents();
	}


	//--- 종료
	delete camera;
	delete floorShader;
	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}