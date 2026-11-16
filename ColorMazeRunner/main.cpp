#include <iostream>
#include <GL/glew.h>
#include <GL/glfw3.h>

#include <gl/glm/glm.hpp>
#include <gl/glm/gtc/matrix_transform.hpp>

#include "Shader.h"

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

// 카메라
const glm::vec3 cameraPos = glm::vec3(0.0f, 6.0f, 14.0f);

// 조명
const glm::vec3 lightPos = glm::vec3(0.0f, 15.0f, 10.0f);
const glm::vec3 lightColor = glm::vec3(1.0f, 1.0f, 1.0f); // 조명 색

//--- 그리기 콜백 함수
GLvoid drawScene() {
	glClearColor(0.05f, 0.05f, 0.08f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);

	floorShader->use();

	//--- 뷰잉 변환, 투영 변환
	glm::mat4 view = glm::lookAt(cameraPos, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	glm::mat4 proj = glm::perspective(glm::radians(75.0f), (float)width / (float)height, 0.1f, 100.0f);
	floorShader->setMat4("view", view);
	floorShader->setMat4("projection", proj);

	floorShader->setVec3("lightPos", lightPos);
	floorShader->setVec3("lightColor", lightColor);
	floorShader->setVec3("viewPos", cameraPos);
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

	//--- 모니터 해상도에 맞춰 전체화면 윈도우 생성
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

	//--- 세이더 프로그램 만들기
	floorShader = new Shader("light_vertex.glsl", "light_fragment.glsl");



	//--- 바닥, 장애물 VBO/VAO 생성하고 설정하기
	init_maze();
	init_obstacle();

	glfwSetKeyCallback(window, key_callback);

	
	
	//**************************렌더링 루프******************************
	while (!glfwWindowShouldClose(window)) {
		drawScene();
		glfwSwapBuffers(window);
		glfwPollEvents();
	}
	
	
	//--- 종료
	delete floorShader;
	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}
