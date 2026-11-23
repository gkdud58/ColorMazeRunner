#pragma once

#include <gl/glm/glm.hpp>

struct GLFWwindow;

//--- 1인칭 카메라 (플레이어의 눈)
//--- yaw/pitch 두 각도로 바라보는 방향을 정하고, 위치는 WASD로 바닥과 평행하게 움직인다.
class Camera {
public:
	glm::vec3 position;      //--- 카메라(눈) 위치
	float yaw;               //--- 좌우 회전 각도(도). -90이면 -z 방향을 바라봄
	float pitch;             //--- 상하 회전 각도(도). 위아래로 뒤집히지 않게 ±89로 제한
	float fov;               //--- 시야각(도). 2단계 우클릭 줌에서 이 값을 줄이면 확대됨

	float moveSpeed;         //--- 초당 이동 거리
	float mouseSensitivity;  //--- 마우스 1픽셀당 회전 각도(도)

	//--- 충돌 검사 함수 (2단계에서 연결). 그 위치로 가면 안 되면 true를 돌려준다.
	//--- nullptr이면 충돌 검사 없이 자유롭게 이동
	bool (*isBlocked)(const glm::vec3& pos);

	Camera(const glm::vec3& startPos, float startYaw = -90.0f);

	glm::vec3 front() const;      //--- 바라보는 방향 (총알 발사 방향으로도 사용)
	glm::vec3 flatFront() const;  //--- 바닥과 평행한 앞 방향 (이동용: 위를 봐도 날아가지 않게)
	glm::vec3 right() const;      //--- 오른쪽 방향

	glm::mat4 viewMatrix() const;
	glm::mat4 projectionMatrix(float aspect) const;

	void processMouse(float dx, float dy);              //--- 마우스 이동량으로 시점 회전
	void processKeyboard(GLFWwindow* window, float dt);  //--- WASD 이동
};