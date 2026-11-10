#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <GL/glew.h>
#include <GL/glfw3.h>
#include <fstream>
#include <string>

#include <gl/glm/glm.hpp>
#include <gl/glm/ext.hpp>
#include <gl/glm/gtc/matrix_transform.hpp>
#include <gl/glm/gtc/type_ptr.hpp>

#define MAX_LINE_LENGTH 234

//--- 사용자 정의 함수
void make_vertexShaders();
void make_fragmentShaders();
void InitBuffer();
int make_shaderProgram();
GLvoid drawScene();

typedef struct {
	float x, y, z;
} Vertex;

typedef struct {
	unsigned int v1, v2, v3;
} Face;

typedef struct {
	Vertex* vertices;
	size_t vertex_count;
	Face* faces;
	size_t face_count;
} Model;

std::string filetobuf(const char* file)
{
	std::ifstream shaderFile(file);
	if (!shaderFile.is_open())
	{
		std::cerr << "ERROR: 파일을 열 수 없음: " << file << std::endl;
		return "";
	}
	//--- 파일 전체를 문자열로 읽기
	std::string source((std::istreambuf_iterator<char>(shaderFile)), std::istreambuf_iterator<char>());
	shaderFile.close();
	return source;
}

void read_newline(char* str) {
	char* pos;
	if ((pos = strchr(str, '\n')) != NULL)
		*pos = '\0';
}

void read_obj_file(const char* filename, Model* model) {
	FILE* file;
	fopen_s(&file, filename, "r");
	if (!file) {
		perror("Error opening file");
		exit(EXIT_FAILURE);
	}

	char line[MAX_LINE_LENGTH];
	model->vertex_count = 0;
	model->face_count = 0;

	while (fgets(line, sizeof(line), file)) {
		read_newline(line);

		if (line[0] == 'v' && line[1] == ' ')
			model->vertex_count++;
		else if (line[0] == 'f' && line[1] == ' ')
			model->face_count++;
	}
	fseek(file, 0, SEEK_SET);
	model->vertices = (Vertex*)malloc(model->vertex_count * sizeof(Vertex));
	model->faces = (Face*)malloc(model->face_count * sizeof(Face));
	size_t vertex_index = 0; size_t face_index = 0;

	while (fgets(line, sizeof(line), file)) {
		read_newline(line);

		if (line[0] == 'v' && line[1] == ' ') {
			int result = sscanf_s(line + 2, "%f %f %f", &model->vertices[vertex_index].x,
				&model->vertices[vertex_index].y,
				&model->vertices[vertex_index].z);
			vertex_index++;
		}
		else if (line[0] == 'f' && line[1] == ' ') {
			unsigned int v1 = 1, v2 = 1, v3 = 1, n1, n2, n3;
			int result = sscanf_s(line + 2, "%u//%u %u//%u %u//%u", &v1, &n1, &v2, &n2, &v3, &n3);
			if (result != 6)   // pyramid.obj처럼 "f 4 3 5" 형식(법선 없음)도 읽도록
				sscanf_s(line + 2, "%u %u %u", &v1, &v2, &v3);
			model->faces[face_index].v1 = v1 - 1;
			model->faces[face_index].v2 = v2 - 1;
			model->faces[face_index].v3 = v3 - 1;
			face_index++;
		}
	}
	fclose(file);
}

// ***************************************************************************************************************************************

//--- 필요한 변수 선언
GLint width, height;
GLuint shaderProgramID;
GLuint vertexShader;
GLuint fragmentShader;

Model cubeModel;
Model pyramidModel;
GLuint cubeVao, cubeVbo;
GLuint pyrVao, pyrVbo;
GLuint circleVao, circleVbo;
const int CIRCLE_SEG = 120;

// 카메라
const glm::vec3 cameraPos = glm::vec3(0.0f, 2.5f, 6.0f);

// 객체 상태
const float ROTATE_SPEED = 60.0f;    // 객체 자전 속도 (초당 도)
const float REVOLVE_SPEED = 80.0f;   // 조명 공전 속도 (초당 도)
const float LIGHT_MIN = 1.8f;        // 조명-객체 최소 거리 (객체와 겹치지 않게)
const float LIGHT_MAX = 5.0f;        // 조명-객체 최대 거리 (카메라 앞쪽까지만)
const float LIGHT_STEP = 0.2f;

int shape = 0;                // n: 0 육면체, 1 사각뿔
bool lightOn = true;          // m: 조명 켜기/끄기
float angleY = 30.0f;         // 객체 자전 각도
int animY = 0;                // y: 객체 자전 on/off
float lightAngle = 0.0f;      // 조명 공전 각도 (0도 = 객체 앞쪽 +z)
int revolveDir = 0;           // r: 0 정지, 1 양의 방향, -1 음의 방향
float lightDist = 3.5f;       // 조명과 객체 중심 사이 거리 = 공전 궤도 반지름
const float lightHeight = 0.0f;

GLuint axisVao, axisVbo[2];
// xyz축 (고정)
GLfloat axisVertices[] = {
	-1.0,0,0,  1.0,0,0,
	0,-1.5,0,  0,2.5,0,
	0,0,-1.0,  0,0,1.0
};
GLfloat axisColors[] = {
	1,0,0, 1,0,0,
	0,1,0, 0,1,0,
	0,0,1, 0,0,1
};

glm::vec3 lightColor = glm::vec3(1.0f, 1.0f, 1.0f);
glm::vec3 cubeColor = glm::vec3(0.0f, 1.0f, 0.0f);
glm::vec3 pyramidColor = glm::vec3(0.0f, 1.0f, 0.0f);

glm::vec3 get_light_pos() {
	float a = glm::radians(lightAngle);
	return glm::vec3(lightDist * sinf(a), lightHeight, lightDist * cosf(a));
}

// 모델을 (위치 + 면 법선) 정점으로 펼쳐서 VAO/VBO에 올림.
// 각 삼각형마다 법선을 외적으로 구하고, 모델 중심에서 바깥쪽을 향하도록 방향을 맞춘다.
// offset으로 객체 중심을 원점에 맞추고 scale로 크기를 정한다.
int make_lit_buffers(Model* model, glm::vec3 offset, float scale, GLuint* vao, GLuint* vbo) {
	size_t n = model->face_count * 3;
	GLfloat* data = (GLfloat*)malloc(n * 6 * sizeof(GLfloat));

	glm::vec3 centroid(0.0f);
	for (size_t i = 0; i < model->vertex_count; i++)
		centroid += glm::vec3(model->vertices[i].x, model->vertices[i].y, model->vertices[i].z);
	centroid /= (float)model->vertex_count;

	for (size_t i = 0; i < model->face_count; i++) {
		unsigned int idx[3] = { model->faces[i].v1, model->faces[i].v2, model->faces[i].v3 };
		glm::vec3 p[3];
		for (int j = 0; j < 3; j++)
			p[j] = glm::vec3(model->vertices[idx[j]].x, model->vertices[idx[j]].y, model->vertices[idx[j]].z);

		glm::vec3 normal = glm::normalize(glm::cross(p[1] - p[0], p[2] - p[0]));
		glm::vec3 faceCenter = (p[0] + p[1] + p[2]) / 3.0f;
		if (glm::dot(normal, faceCenter - centroid) < 0.0f)
			normal = -normal;

		for (int j = 0; j < 3; j++) {
			glm::vec3 pos = (p[j] + offset) * scale;
			size_t k = (i * 3 + j) * 6;
			data[k + 0] = pos.x; data[k + 1] = pos.y; data[k + 2] = pos.z;
			data[k + 3] = normal.x; data[k + 4] = normal.y; data[k + 5] = normal.z;
		}
	}

	glGenVertexArrays(1, vao);
	glBindVertexArray(*vao);
	glGenBuffers(1, vbo);
	glBindBuffer(GL_ARRAY_BUFFER, *vbo);
	glBufferData(GL_ARRAY_BUFFER, n * 6 * sizeof(GLfloat), data, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (void*)0);                      // 위치
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (void*)(3 * sizeof(GLfloat)));  // 법선
	glEnableVertexAttribArray(1);

	free(data);
	return (int)n;
}

int cubeVertexCount, pyrVertexCount;

// 객체 자전, 조명 공전 갱신
void update(float dt) {
	angleY += animY * ROTATE_SPEED * dt;
	lightAngle += revolveDir * REVOLVE_SPEED * dt;
	if (lightAngle > 360.0f) lightAngle -= 360.0f;
	if (lightAngle < -360.0f) lightAngle += 360.0f;
}

void set_matrix(const char* name, const glm::mat4& m) {
	glUniformMatrix4fv(glGetUniformLocation(shaderProgramID, name), 1, GL_FALSE, glm::value_ptr(m));
}

void set_vec3(const char* name, const glm::vec3& v) {
	glUniform3f(glGetUniformLocation(shaderProgramID, name), v.x, v.y, v.z);
}

//--- 그리기 콜백 함수
GLvoid drawScene() {
	glClearColor(0.05f, 0.05f, 0.08f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);

	glUseProgram(shaderProgramID);

	//--- 뷰잉 변환, 투영 변환
	glm::mat4 view = glm::lookAt(cameraPos, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.5f, 0.0f));
	glm::mat4 proj = glm::perspective(glm::radians(75.0f), (float)width / (float)height, 0.1f, 100.0f);
	set_matrix("view", view);
	set_matrix("projection", proj);

	glm::vec3 lightPos = get_light_pos();
	set_vec3("lightPos", lightPos);
	set_vec3("lightColor", lightColor);
	set_vec3("viewPos", cameraPos);
	glUniform1i(glGetUniformLocation(shaderProgramID, "lightOn"), lightOn);

	glm::mat4 axisRx = glm::rotate(glm::mat4(1.0f), glm::radians(30.0f), glm::vec3(1, 0, 0));
	glm::mat4 axisRy = glm::rotate(glm::mat4(1.0f), glm::radians(-45.0f), glm::vec3(0, 1, 0));
	glm::mat4 axisModel = axisRx * axisRy;
	glUniformMatrix4fv(glGetUniformLocation(shaderProgramID, "modelTransform"), 1, GL_FALSE, glm::value_ptr(axisModel));
	glBindVertexArray(axisVao);
	glDrawArrays(GL_LINES, 0, 6);

	//--- 객체: 제자리에서 y축 자전
	glm::mat4 model = glm::rotate(glm::mat4(1.0f), glm::radians(angleY), glm::vec3(0, 1, 0));
	set_matrix("model", model);
	glUniform1i(glGetUniformLocation(shaderProgramID, "unlit"), 0);
	if (shape == 0) {
		set_vec3("objectColor", cubeColor);
		glBindVertexArray(cubeVao);
		glDrawArrays(GL_TRIANGLES, 0, cubeVertexCount);
	}
	else {
		set_vec3("objectColor", pyramidColor);
		glBindVertexArray(pyrVao);
		glDrawArrays(GL_TRIANGLES, 0, pyrVertexCount);
	}

	//--- 조명 표시용 작은 육면체 (조명 계산 없이 조명 색으로 그린다. 꺼져 있으면 회색)
	glUniform1i(glGetUniformLocation(shaderProgramID, "unlit"), 1);
	glm::mat4 lightModel = glm::translate(glm::mat4(1.0f), lightPos) * glm::scale(glm::mat4(1.0f), glm::vec3(0.12f));
	set_matrix("model", lightModel);
	if (lightOn)
		set_vec3("objectColor", lightColor);
	else
		set_vec3("objectColor", glm::vec3(0.3f));
	glBindVertexArray(cubeVao);
	glDrawArrays(GL_TRIANGLES, 0, cubeVertexCount);

	//--- 조명의 공전 궤도 (원): 단위 원을 반지름(lightDist)만큼 키워서 그린다 -> z/Z로 거리가 바뀌면 궤도도 같이 바뀜
	glm::mat4 orbitModel = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, lightHeight, 0.0f))
		* glm::scale(glm::mat4(1.0f), glm::vec3(lightDist, 1.0f, lightDist));
	set_matrix("model", orbitModel);
	set_vec3("objectColor", glm::vec3(0.6f, 0.6f, 0.6f));
	glBindVertexArray(circleVao);
	glDrawArrays(GL_LINE_LOOP, 0, CIRCLE_SEG);
}

void make_vertexShaders() {
	std::string vertexSource = filetobuf("light_vertex.glsl");
	const char* source = vertexSource.c_str();
	vertexShader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertexShader, 1, &source, NULL);
	glCompileShader(vertexShader);
	GLint result;
	GLchar errorLog[512];
	glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &result);
	if (!result)
	{
		glGetShaderInfoLog(vertexShader, 512, NULL, errorLog);
		std::cerr << "ERROR: vertex shader 컴파일 실패\n" << errorLog << std::endl;
		return;
	}
}

void make_fragmentShaders() {
	std::string fragmentSource = filetobuf("light_fragment.glsl");
	const char* source = fragmentSource.c_str();
	fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentShader, 1, &source, NULL);
	glCompileShader(fragmentShader);
	GLint result;
	GLchar errorLog[512];
	glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &result);
	if (!result)
	{
		glGetShaderInfoLog(fragmentShader, 512, NULL, errorLog);
		std::cerr << "ERROR: fragment shader 컴파일 실패\n" << errorLog << std::endl;
		return;
	}
}

void InitBuffer() {
	// 육면체: 좌표가 0~1이라 중심을 원점으로 옮기고 1.5배 키움
	cubeVertexCount = make_lit_buffers(&cubeModel, glm::vec3(-0.5f, -0.5f, -0.5f), 1.5f, &cubeVao, &cubeVbo);
	// 사각뿔: 높이 1.5의 절반만큼 내려 중심을 맞춤
	pyrVertexCount = make_lit_buffers(&pyramidModel, glm::vec3(0.0f, -0.6f, 0.0f), 1.0f, &pyrVao, &pyrVbo);

	// 궤도 원 (xz 평면 단위 원). 법선은 사용하지 않지만 속성 형식을 맞춰 0으로 채움
	GLfloat circle[CIRCLE_SEG * 6];
	for (int i = 0; i < CIRCLE_SEG; i++) {
		float a = 2.0f * 3.14159265f * i / CIRCLE_SEG;
		circle[i * 6 + 0] = cosf(a);
		circle[i * 6 + 1] = 0.0f;
		circle[i * 6 + 2] = sinf(a);
		circle[i * 6 + 3] = 0.0f;
		circle[i * 6 + 4] = 1.0f;
		circle[i * 6 + 5] = 0.0f;
	}
	// 축 VAO/VBO 생성
	glGenVertexArrays(1, &axisVao);
	glBindVertexArray(axisVao);
	glGenBuffers(2, axisVbo);
	glBindBuffer(GL_ARRAY_BUFFER, axisVbo[0]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(axisVertices), axisVertices, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(0);

	glBindBuffer(GL_ARRAY_BUFFER, axisVbo[1]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(axisColors), axisColors, GL_STATIC_DRAW);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(1);

	glGenVertexArrays(1, &circleVao);
	glBindVertexArray(circleVao);
	glGenBuffers(1, &circleVbo);
	glBindBuffer(GL_ARRAY_BUFFER, circleVbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(circle), circle, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (void*)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (void*)(3 * sizeof(GLfloat)));
	glEnableVertexAttribArray(1);
}

//--- 세이더 프로그램 생성하기
int make_shaderProgram() {
	shaderProgramID = glCreateProgram();
	glAttachShader(shaderProgramID, vertexShader);
	glAttachShader(shaderProgramID, fragmentShader);
	glLinkProgram(shaderProgramID);

	GLint result;
	glGetProgramiv(shaderProgramID, GL_LINK_STATUS, &result);
	if (!result) {
		GLchar errorLog[512];
		glGetProgramInfoLog(shaderProgramID, 512, NULL, errorLog);
		std::cerr << "ERROR: shader program 링크 실패\n" << errorLog << std::endl;
	}

	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);
	glUseProgram(shaderProgramID);

	return shaderProgramID;
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
	bool shift = (mods & GLFW_MOD_SHIFT) != 0;

	// z/Z는 키를 누르고 있어도 계속 이동
	if (action == GLFW_PRESS || action == GLFW_REPEAT) {
		if (key == GLFW_KEY_Z) {
			if (shift)
				lightDist += LIGHT_STEP;   // Z: 객체에서 멀게
			else
				lightDist -= LIGHT_STEP;   // z: 객체에 가깝게
			if (lightDist < LIGHT_MIN) lightDist = LIGHT_MIN;
			if (lightDist > LIGHT_MAX) lightDist = LIGHT_MAX;
		}
	}

	if (action != GLFW_PRESS)
		return;

	switch (key) {
	// n: 육면체/사각뿔 전환
	case GLFW_KEY_N:
		shape = 1 - shape;
		break;

	// m: 조명 켜기/끄기
	case GLFW_KEY_M:
		lightOn = !lightOn;
		break;

	// y: 객체 y축 자전
	case GLFW_KEY_Y:
		if (animY == 0)
			animY = 1;
		else
			animY = 0;
		break;

	// r: 조명 공전 (양의 방향) / shift+r: 음의 방향. 같은 키를 다시 누르면 정지
	case GLFW_KEY_R:
		if (shift) {
			if (revolveDir == -1)
				revolveDir = 0;
			else
				revolveDir = -1;
		}
		else {
			if (revolveDir == 1)
				revolveDir = 0;
			else
				revolveDir = 1;
		}
		break;

	// q: 프로그램 종료
	case GLFW_KEY_Q:
		glfwSetWindowShouldClose(window, GLFW_TRUE);
		break;
	}
}

// ***************************************************************************************************************************************
//--- 메인 함수
int main(int argc, char** argv)
{
	width = 1200;
	height = 1200;

	//--- GLFW 초기화
	if (!glfwInit())
		return -1;

	//--- OpenGL 버전 및 프로파일 설정
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	//--- 윈도우 생성
	GLFWwindow* window = glfwCreateWindow(width, height, "Example26", nullptr, nullptr);

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

	//--- 세이더 읽어와서 세이더 프로그램 만들기
	make_vertexShaders();
	make_fragmentShaders();
	shaderProgramID = make_shaderProgram();

	// 육면체, 사각뿔 오브젝트 읽기
	read_obj_file("cube.obj", &cubeModel);
	read_obj_file("pyramid.obj", &pyramidModel);

	//--- VBO, VAO 생성하고 설정하기
	InitBuffer();

	glfwSetKeyCallback(window, key_callback);

	std::cout << "n: 육면체/사각뿔  m: 조명 on/off  y: 객체 자전\n"
		<< "r/R: 조명 공전 (양/음 방향)  z/Z: 조명 가깝게/멀게  q: 종료" << std::endl;

	//**************************렌더링 루프******************************
	double lastTime = glfwGetTime();
	while (!glfwWindowShouldClose(window)) {
		double nowTime = glfwGetTime();
		update((float)(nowTime - lastTime));
		lastTime = nowTime;

		drawScene();
		glfwSwapBuffers(window);
		glfwPollEvents();
	}
	//--- 종료
	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}
