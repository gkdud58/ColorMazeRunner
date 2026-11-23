#include "gun.h"
#include "Shader.h"
#include "Camera.h"

#include <cmath>
#include <vector>
#include <GL/glew.h>
#include <GL/glfw3.h>
#include <gl/glm/glm.hpp>
#include <gl/glm/gtc/matrix_transform.hpp>

//--- 총의 로컬 좌표계: 카메라 기준과 같게 -z가 앞(총구 방향), +x 오른쪽, +y 위
//--- 길이 단위는 월드와 같다 (플레이어 눈 높이 1.0 기준으로 총 전체 길이 약 0.8)

namespace {
	struct Mesh {
		GLuint vao = 0, vbo = 0;
		int count = 0;
	};

	Mesh cylinderMesh;   //--- 뚜껑 있는 원통 (총열, 축, 고정판)
	Mesh tubeMesh;       //--- 뚜껑 없는 원통 (스코프 몸통: 안이 뚫려 있어야 들여다볼 수 있음)
	Mesh boxMesh;        //--- 상자 (몸통, 손잡이, 마운트, 조준선)
	Mesh frontBellMesh;  //--- 스코프 대물렌즈 쪽 깔때기 (앞이 굵음)
	Mesh rearBellMesh;   //--- 스코프 접안렌즈 쪽 깔때기 (뒤가 굵음)

	const float PI = 3.14159265f;
	const int CYLINDER_SEGMENTS = 24;  //--- 원을 몇 조각으로 나눌지 (많을수록 매끈)

	//--- 위치(3) + 법선(3)을 data에 하나 추가
	void push(std::vector<float>& data, const glm::vec3& p, const glm::vec3& n)
	{
		data.push_back(p.x); data.push_back(p.y); data.push_back(p.z);
		data.push_back(n.x); data.push_back(n.y); data.push_back(n.z);
	}

	Mesh upload(const std::vector<float>& data)
	{
		Mesh m;
		m.count = (int)(data.size() / 6);
		glGenVertexArrays(1, &m.vao);
		glBindVertexArray(m.vao);
		glGenBuffers(1, &m.vbo);
		glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
		glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), data.data(), GL_STATIC_DRAW);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);                    // 위치
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));  // 법선
		glEnableVertexAttribArray(1);
		return m;
	}

	//--- 반지름 1, 길이 1, z축 방향으로 누운 원통 (z = -0.5 ~ 0.5)
	//--- scale(r, r, 길이)로 원하는 크기를 만든다
	Mesh makeCylinder(bool caps)
	{
		std::vector<float> d;
		for (int i = 0; i < CYLINDER_SEGMENTS; i++) {
			float a0 = 2.0f * PI * i / CYLINDER_SEGMENTS;
			float a1 = 2.0f * PI * (i + 1) / CYLINDER_SEGMENTS;
			glm::vec3 n0(cos(a0), sin(a0), 0.0f);
			glm::vec3 n1(cos(a1), sin(a1), 0.0f);

			//--- 옆면 (사각형 하나 = 삼각형 두 개). 법선을 꼭짓점마다 바깥 방향으로 줘서 매끈하게 보이게
			glm::vec3 b0 = n0 + glm::vec3(0, 0, 0.5f), b1 = n1 + glm::vec3(0, 0, 0.5f);
			glm::vec3 f0 = n0 - glm::vec3(0, 0, 0.5f), f1 = n1 - glm::vec3(0, 0, 0.5f);
			push(d, b0, n0); push(d, b1, n1); push(d, f1, n1);
			push(d, b0, n0); push(d, f1, n1); push(d, f0, n0);

			if (caps) {
				//--- 앞뚜껑(-z), 뒷뚜껑(+z)
				push(d, glm::vec3(0, 0, -0.5f), glm::vec3(0, 0, -1)); push(d, f1, glm::vec3(0, 0, -1)); push(d, f0, glm::vec3(0, 0, -1));
				push(d, glm::vec3(0, 0, 0.5f), glm::vec3(0, 0, 1));   push(d, b0, glm::vec3(0, 0, 1));  push(d, b1, glm::vec3(0, 0, 1));
			}
		}
		return upload(d);
	}

	//--- 깔때기(원뿔대): 앞(z=-0.5) 반지름 1, 뒤(z=+0.5) 반지름 backRatio, 뚜껑 없음
	//--- 굵기가 다른 스코프 부품을 틈 없이 이어 준다
	Mesh makeFrustum(float backRatio)
	{
		std::vector<float> d;
		float slope = (1.0f - backRatio);  //--- 옆면이 기울어진 만큼 법선도 앞뒤로 기울인다
		for (int i = 0; i < CYLINDER_SEGMENTS; i++) {
			float a0 = 2.0f * PI * i / CYLINDER_SEGMENTS;
			float a1 = 2.0f * PI * (i + 1) / CYLINDER_SEGMENTS;
			glm::vec3 c0(cos(a0), sin(a0), 0.0f), c1(cos(a1), sin(a1), 0.0f);
			glm::vec3 n0 = glm::normalize(c0 + glm::vec3(0, 0, slope));
			glm::vec3 n1 = glm::normalize(c1 + glm::vec3(0, 0, slope));
			glm::vec3 f0 = c0 - glm::vec3(0, 0, 0.5f), f1 = c1 - glm::vec3(0, 0, 0.5f);
			glm::vec3 b0 = c0 * backRatio + glm::vec3(0, 0, 0.5f), b1 = c1 * backRatio + glm::vec3(0, 0, 0.5f);
			push(d, b0, n0); push(d, b1, n1); push(d, f1, n1);
			push(d, b0, n0); push(d, f1, n1); push(d, f0, n0);
		}
		return upload(d);
	}

	//--- 한 변 1, 가운데가 원점인 상자
	Mesh makeBox()
	{
		std::vector<float> d;
		const glm::vec3 normals[6] = { {1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1} };
		for (const glm::vec3& n : normals) {
			//--- 면의 두 접선 방향
			glm::vec3 u = (fabs(n.y) > 0.5f) ? glm::vec3(1, 0, 0) : glm::vec3(0, 1, 0);
			glm::vec3 v = glm::cross(n, u);
			glm::vec3 c = n * 0.5f;
			glm::vec3 p0 = c - u * 0.5f - v * 0.5f, p1 = c + u * 0.5f - v * 0.5f;
			glm::vec3 p2 = c + u * 0.5f + v * 0.5f, p3 = c - u * 0.5f + v * 0.5f;
			push(d, p0, n); push(d, p1, n); push(d, p2, n);
			push(d, p0, n); push(d, p2, n); push(d, p3, n);
		}
		return upload(d);
	}

	//====================== 총 모양 / 크기 조절은 여기서 ======================

	const glm::vec3 BODY_COLOR = glm::vec3(0.25f, 0.27f, 0.30f);   //--- 몸통
	const glm::vec3 BARREL_COLOR = glm::vec3(0.12f, 0.12f, 0.13f); //--- 총열
	const glm::vec3 CLAMP_COLOR = glm::vec3(0.55f, 0.45f, 0.20f);  //--- 총열 고정판 (황동색)
	const glm::vec3 SCOPE_COLOR = glm::vec3(0.08f, 0.08f, 0.09f);  //--- 스코프
	const glm::vec3 RETICLE_COLOR = glm::vec3(0.9f, 0.1f, 0.1f);   //--- 조준선 (빨강)

	//--- 몸통 (가운데가 원점)
	const glm::vec3 BODY_SIZE = glm::vec3(0.09f, 0.12f, 0.26f);   //--- z = -0.13 ~ 0.13
	const float BODY_TOP = BODY_SIZE.y * 0.5f;

	//--- 개틀링 총열
	const int BARREL_COUNT = 6;          //--- 총열 개수
	const float BARREL_RADIUS = 0.014f;  //--- 총열 하나의 반지름
	const float BARREL_RING = 0.038f;    //--- 중심축에서 각 총열까지 거리 (동그랗게 배치되는 원의 반지름)
	const float BARREL_LENGTH = 0.55f;
	const float BARREL_START_Z = -BODY_SIZE.z * 0.5f; //--- 몸통 앞면에서 시작
	const float BARREL_CENTER_Z = BARREL_START_Z - BARREL_LENGTH * 0.5f;

	//--- 스코프 (몸통 위에 마운트 2개로 붙임)
	const float SCOPE_Y = 0.112f;          //--- 스코프 중심축 높이
	const float SCOPE_RADIUS = 0.02f;
	const float SCOPE_LENGTH = 0.14f;      //--- 가운데 몸통: z = -0.07 ~ 0.07
	const float FRONT_BELL_RADIUS = 0.028f, FRONT_BELL_LENGTH = 0.05f;  //--- 대물렌즈 쪽
	const float REAR_BELL_RADIUS = 0.025f, REAR_BELL_LENGTH = 0.03f;    //--- 접안렌즈 쪽
	const float SCOPE_REAR_Z = SCOPE_LENGTH * 0.5f + REAR_BELL_LENGTH;  //--- 눈 대는 쪽 끝

	//--- 총이 화면에서 놓이는 위치 (카메라 기준)
	const glm::vec3 HIP_OFFSET = glm::vec3(0.24f, -0.22f, -0.55f);  //--- 평소: 오른쪽 아래
	//--- 조준: 스코프 중심축이 눈과 일직선, 접안렌즈가 눈 앞 0.11
	const glm::vec3 ADS_OFFSET = glm::vec3(0.0f, -SCOPE_Y, -0.11f - SCOPE_REAR_Z);

	const float HIP_FOV = 75.0f;
	const float ADS_FOV = 25.0f;            //--- 조준 시 시야각 (작을수록 많이 확대)
	const float HIP_SENSITIVITY = 0.1f;
	const float ADS_SENSITIVITY = 0.035f;   //--- 확대했을 때 마우스가 너무 휙휙 돌지 않게
	const float AIM_SPEED = 8.0f;           //--- 조준 전환 속도 (클수록 빠름)

	const float MAX_SPIN_SPEED = 1440.0f;   //--- 총열 최대 회전 속도 (도/초)
	const float SPIN_ACCEL = 2400.0f;       //--- 회전이 빨라지는 정도
	const float SPIN_DECEL = 900.0f;        //--- 손 떼면 천천히 멈춤

	//====================== 상태 ======================
	float spinAngle = 0.0f;  //--- 현재 총열 회전 각도
	float spinSpeed = 0.0f;
	float aim = 0.0f;        //--- 0: 평소, 1: 완전히 조준

	void drawPart(Shader* shader, const Mesh& mesh, const glm::mat4& model, const glm::vec3& color)
	{
		shader->setMat4("model", model);
		shader->setVec3("objectColor", color);
		glBindVertexArray(mesh.vao);
		glDrawArrays(GL_TRIANGLES, 0, mesh.count);
	}

	glm::mat4 TS(const glm::vec3& t, const glm::vec3& s)
	{
		return glm::scale(glm::translate(glm::mat4(1.0f), t), s);
	}
}


void init_gun()
{
	cylinderMesh = makeCylinder(true);
	tubeMesh = makeCylinder(false);
	boxMesh = makeBox();
	frontBellMesh = makeFrustum(SCOPE_RADIUS / FRONT_BELL_RADIUS);  //--- 앞 굵고 뒤는 스코프 몸통 굵기
	rearBellMesh = makeFrustum(REAR_BELL_RADIUS / SCOPE_RADIUS);    //--- 앞은 스코프 몸통 굵기, 뒤가 굵음
}

void update_gun(GLFWwindow* window, Camera* camera, float dt)
{
	bool firing = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
	bool aiming = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
	//--- 총열 회전: 누르면 점점 빨라지고, 떼면 천천히 멈춘다
	if (firing) spinSpeed = fmin(spinSpeed + SPIN_ACCEL * dt, MAX_SPIN_SPEED);
	else        spinSpeed = fmax(spinSpeed - SPIN_DECEL * dt, 0.0f);
	spinAngle = fmod(spinAngle + spinSpeed * dt, 360.0f);

	//--- 조준 값을 0 <-> 1 사이로 부드럽게 이동
	float target = aiming ? 1.0f : 0.0f;
	aim += (target - aim) * fmin(AIM_SPEED * dt, 1.0f);

	camera->fov = glm::mix(HIP_FOV, ADS_FOV, aim);
	camera->mouseSensitivity = glm::mix(HIP_SENSITIVITY, ADS_SENSITIVITY, aim);
}

void draw_gun(Shader* shader, const Camera* camera)
{
	//--- 총이 벽에 파묻혀 보이지 않게 깊이 버퍼를 비우고 맨 위에 그린다
	glClear(GL_DEPTH_BUFFER_BIT);

	//--- 카메라에 붙어 다니게: (뷰 행렬의 역행렬) = 카메라의 월드 변환
	glm::vec3 offset = glm::mix(HIP_OFFSET, ADS_OFFSET, aim);
	glm::mat4 gun = glm::inverse(camera->viewMatrix()) * glm::translate(glm::mat4(1.0f), offset);

	//--- 몸통
	drawPart(shader, boxMesh, gun * TS(glm::vec3(0.0f, 0.0f, 0.0f), BODY_SIZE), BODY_COLOR);

	//--- 손잡이 (아래로, 살짝 뒤로 기울임)
	glm::mat4 grip = gun * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -0.10f, 0.08f))
		* glm::rotate(glm::mat4(1.0f), glm::radians(-15.0f), glm::vec3(1, 0, 0));
	drawPart(shader, boxMesh, grip * glm::scale(glm::mat4(1.0f), glm::vec3(0.04f, 0.12f, 0.05f)), BODY_COLOR);

	//--- 모터 하우징 (몸통과 총열 사이의 굵은 원통)
	drawPart(shader, cylinderMesh, gun * TS(glm::vec3(0.0f, 0.0f, BARREL_START_Z - 0.02f), glm::vec3(0.064f, 0.064f, 0.05f)), BODY_COLOR);

	//--- 회전하는 총열 묶음: 이 행렬 하나만 z축으로 돌리면 총열, 축, 고정판이 같이 돈다
	glm::mat4 spin = gun * glm::rotate(glm::mat4(1.0f), glm::radians(spinAngle), glm::vec3(0, 0, 1));

	//--- 중심축
	drawPart(shader, cylinderMesh, spin * TS(glm::vec3(0.0f, 0.0f, BARREL_CENTER_Z), glm::vec3(0.012f, 0.012f, BARREL_LENGTH)), BARREL_COLOR);

	//--- 총열 6개를 원 위에 같은 간격으로 배치
	for (int i = 0; i < BARREL_COUNT; i++) {
		float a = 2.0f * PI * i / BARREL_COUNT;
		glm::vec3 pos(cos(a) * BARREL_RING, sin(a) * BARREL_RING, BARREL_CENTER_Z);
		drawPart(shader, cylinderMesh, spin * TS(pos, glm::vec3(BARREL_RADIUS, BARREL_RADIUS, BARREL_LENGTH)), BARREL_COLOR);
	}

	//--- 총열을 묶어주는 고정판 2개 (중간, 총구 쪽)
	float clampR = BARREL_RING + BARREL_RADIUS + 0.006f;
	drawPart(shader, cylinderMesh, spin * TS(glm::vec3(0.0f, 0.0f, BARREL_START_Z - 0.12f), glm::vec3(clampR, clampR, 0.02f)), CLAMP_COLOR);
	drawPart(shader, cylinderMesh, spin * TS(glm::vec3(0.0f, 0.0f, BARREL_START_Z - BARREL_LENGTH + 0.04f), glm::vec3(clampR, clampR, 0.025f)), CLAMP_COLOR);

	//--- 스코프 마운트 2개 (몸통 윗면 y=0.06 ~ 스코프 아래)
	//--- 스코프 안으로 뚫고 들어가면 조준할 때 안쪽에 보이므로 스코프 바닥 바로 아래까지만
	float mountH = SCOPE_Y - SCOPE_RADIUS - BODY_TOP - 0.0001f;
	float mountY = BODY_TOP + mountH * 0.5f;
	drawPart(shader, boxMesh, gun * TS(glm::vec3(0.0f, mountY, -0.045f), glm::vec3(0.012f, mountH, 0.02f)), BODY_COLOR);
	drawPart(shader, boxMesh, gun * TS(glm::vec3(0.0f, mountY, 0.045f), glm::vec3(0.012f, mountH, 0.02f)), BODY_COLOR);

	//--- 스코프 몸통, 대물렌즈 쪽(앞, 더 굵게), 접안렌즈 쪽(뒤) - 모두 뚜껑 없는 원통이라 안이 뚫려 있음
	drawPart(shader, tubeMesh, gun * TS(glm::vec3(0.0f, SCOPE_Y, 0.0f), glm::vec3(SCOPE_RADIUS, SCOPE_RADIUS, SCOPE_LENGTH)), SCOPE_COLOR);
	drawPart(shader, frontBellMesh, gun * TS(glm::vec3(0.0f, SCOPE_Y, -SCOPE_LENGTH * 0.5f - FRONT_BELL_LENGTH * 0.5f),
		glm::vec3(FRONT_BELL_RADIUS, FRONT_BELL_RADIUS, FRONT_BELL_LENGTH)), SCOPE_COLOR);
	drawPart(shader, rearBellMesh, gun * TS(glm::vec3(0.0f, SCOPE_Y, SCOPE_LENGTH * 0.5f + REAR_BELL_LENGTH * 0.5f),
		glm::vec3(SCOPE_RADIUS, SCOPE_RADIUS, REAR_BELL_LENGTH)), SCOPE_COLOR);

	//--- 조준선(십자): 스코프 앞쪽 안에 얇은 막대 두 개. 조명 영향 없이 항상 선명한 색
	shader->setInt("unlit", 1);
	float reticleZ = -SCOPE_LENGTH * 0.5f + 0.005f;
	float reticleLen = SCOPE_RADIUS * 2.0f;
	float reticleThick = 0.0012f;
	drawPart(shader, boxMesh, gun * TS(glm::vec3(0.0f, SCOPE_Y, reticleZ), glm::vec3(reticleLen, reticleThick, 0.001f)), RETICLE_COLOR);
	drawPart(shader, boxMesh, gun * TS(glm::vec3(0.0f, SCOPE_Y, reticleZ), glm::vec3(reticleThick, reticleLen, 0.001f)), RETICLE_COLOR);
	shader->setInt("unlit", 0);
}
