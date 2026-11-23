#include "Camera.h"

#include <cmath>
#include <GL/glew.h>
#include <GL/glfw3.h>
#include <gl/glm/gtc/matrix_transform.hpp>

namespace {
	const glm::vec3 WORLD_UP = glm::vec3(0.0f, 1.0f, 0.0f);
	const float PITCH_LIMIT = 89.0f;  //--- 90도가 되면 front와 up이 평행해져 lookAt이 깨지므로 89로 제한
}

Camera::Camera(const glm::vec3& startPos, float startYaw)
	: position(startPos), yaw(startYaw), pitch(0.0f), fov(75.0f),
	moveSpeed(5.0f), mouseSensitivity(0.1f), isBlocked(nullptr)
{}

//--- 구면 좌표 -> 방향 벡터
//--- yaw는 xz 평면에서의 각도, pitch는 위아래 각도
glm::vec3 Camera::front() const
{
	float y = glm::radians(yaw);
	float p = glm::radians(pitch);
	glm::vec3 dir;
	dir.x = cos(y) * cos(p);
	dir.y = sin(p);
	dir.z = sin(y) * cos(p);
	return glm::normalize(dir);
}

glm::vec3 Camera::flatFront() const
{
	float y = glm::radians(yaw);
	return glm::normalize(glm::vec3(cos(y), 0.0f, sin(y)));
}

glm::vec3 Camera::right() const
{
	return glm::normalize(glm::cross(flatFront(), WORLD_UP));
}

glm::mat4 Camera::viewMatrix() const
{
	return glm::lookAt(position, position + front(), WORLD_UP);
}

glm::mat4 Camera::projectionMatrix(float aspect) const
{
	return glm::perspective(glm::radians(fov), aspect, 0.1f, 200.0f);
}

void Camera::processMouse(float dx, float dy)
{
	yaw += dx * mouseSensitivity;
	pitch -= dy * mouseSensitivity;  //--- 화면 y는 아래로 갈수록 커지므로 빼준다

	if (pitch > PITCH_LIMIT) pitch = PITCH_LIMIT;
	if (pitch < -PITCH_LIMIT) pitch = -PITCH_LIMIT;

	//--- yaw가 계속 커지지 않게 0~360 사이로 유지
	yaw = fmod(yaw, 360.0f);
}

void Camera::processKeyboard(GLFWwindow* window, float dt)
{
	glm::vec3 dir(0.0f);
	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) dir += flatFront();
	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) dir -= flatFront();
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) dir += right();
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) dir -= right();

	if (glm::length(dir) < 0.0001f)
		return;

	//--- 대각선(W+D)으로 갈 때 더 빨라지지 않게 길이를 1로 맞춤
	glm::vec3 delta = glm::normalize(dir) * moveSpeed * dt;

	//--- x, z를 따로 움직여 본다
	//--- 벽에 비스듬히 부딪혔을 때 막힌 축만 멈추고 나머지 축으로는 미끄러지듯 이동할 수 있다
	glm::vec3 next = position;
	next.x += delta.x;
	if (isBlocked == nullptr || !isBlocked(next))
		position.x = next.x;

	next = position;
	next.z += delta.z;
	if (isBlocked == nullptr || !isBlocked(next))
		position.z = next.z;
}