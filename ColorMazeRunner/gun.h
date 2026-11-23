#pragma once

class Shader;
class Camera;
struct GLFWwindow;

//--- 1인칭 머신건 (개틀링: 원통 총열 6개를 동그랗게 붙여서 돌림) + 스코프
//--- 좌클릭 누르고 있으면 총열 회전, 우클릭 누르고 있으면 스코프 조준(줌)

void init_gun();                                            //--- 원통/상자 메시 만들기 (init_maze 다음에 호출)
void update_gun(GLFWwindow* window, Camera* camera, float dt);  //--- 마우스 입력 -> 총열 회전, 조준 상태 갱신
void draw_gun(Shader* shader, const Camera* camera);        //--- 화면에 총 그리기 (drawScene 맨 끝에서 호출)
