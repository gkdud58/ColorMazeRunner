// ================================================================
// 실습 23 - 육면체 무대와 걷는 로봇 (GLEW + GLFW + GLM, 셰이더)
//
// o       : 앞면이 위쪽으로 스케일되며 열림/닫힘
// w/a/s/d : 로봇 이동 방향 (뒤/왼/앞/오른쪽 = -z/-x/+z/+x)
// + / -   : 걷는 속도 증가/감소 (팔다리 흔드는 각도도 같이 변함)
// j       : 점프 (장애물 위로 올라가고 내려올 수 있음)
// i       : 초기화
// z/Z     : 카메라 앞/뒤 이동
// x/X     : 카메라 좌/우 이동
// y/Y     : 카메라가 화면 중심 y축 기준으로 공전
// q       : 종료
//
// vertex.glsl, fragment.glsl 파일이 실행 폴더(프로젝트 폴더)에 있어야 한다.
// ================================================================
#include <iostream>
#include <fstream>
#include <string>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <GL/glew.h>
#include <GL/glfw3.h>
#include <gl/glm/glm.hpp>
#include <gl/glm/ext.hpp>
#include <gl/glm/gtc/matrix_transform.hpp>

//--- 사용자 정의 함수
std::string filetobuf(const char* file);
void make_vertexShaders();
void make_fragmentShaders();
GLuint make_shaderProgram();
void InitBuffer();
void reset();
void update(float dt);
GLvoid drawScene();
void charCallback(GLFWwindow* window, unsigned int ch);

//--- 필요한 변수 선언
GLint width = 1000, height = 700;
GLuint shaderProgramID;
GLuint vertexShader;
GLuint fragmentShader;
GLuint VAO, VBO;

// ---------------- 장면 크기 ----------------
const float STAGE_X = 4.0f, STAGE_Z = 4.0f;   // 무대 반폭 (x, z)
const float STAGE_H = 4.0f;                   // 무대 높이 (y: 0 ~ STAGE_H)
const float ROBOT_R = 0.3f;                   // 로봇 충돌 반경
const float GRAVITY = 9.8f;                   // 중력 가속도
const float JUMP_V = 5.0f;                   // 점프 초기 속도

// ---------------- 상태 ----------------
struct Obstacle {
    float x, z;          // 바닥 중심
    float sx, sy, sz;    // 반크기 (sy는 높이의 절반)
    glm::vec3 color;
};
Obstacle obs[3];

float robotX, robotY, robotZ;   // 로봇 발바닥 위치
float dirX, dirZ;               // 이동 방향
float walkSpeed;                // 초당 이동 거리
float walkPhase;                // 팔다리 흔들기 위상
float velY;                     // 점프 속도
bool  onGround;

float frontScale;               // 앞면 y 스케일 (1 닫힘, 0 열림)
bool  frontOpen;                // o 토글

glm::vec3 camPos, camTarget;    // 카메라 위치, 바라보는 점 (화면 중심)

// 무대 6면 색: 앞, 뒤, 왼, 오른, 위, 아래
const glm::vec3 stageColor[6] = {
    { .9f, .9f, .4f },{ .65f, .2f, .22f },{ .2f, .45f, .25f },
    { .15f, .25f, .5f },{ .3f, .3f, .35f },{ .4f, .6f, .4f }
};

//--- 메인 함수
int main(int argc, char** argv)
{
    srand((unsigned)time(nullptr));

    //--- GLFW 초기화
    if (!glfwInit())
        return -1;
    //--- OpenGL 버전 및 프로파일 설정
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    //--- 윈도우 생성
    GLFWwindow* window = glfwCreateWindow(width, height, "Practice 23", nullptr, nullptr);
    if (!window)
    {
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetCharCallback(window, charCallback);   //--- 대/소문자 구분을 위해 문자 콜백

    //--- GLEW 초기화
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK)
    {
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    //--- 셰이더 읽어와서 셰이더 프로그램 만들기
    make_vertexShaders();
    make_fragmentShaders();
    shaderProgramID = make_shaderProgram();
    InitBuffer();

    //--- 은면 제거 + 컬링
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glFrontFace(GL_CCW);

    reset();

    //--- 렌더링 루프
    double prev = glfwGetTime();
    while (!glfwWindowShouldClose(window))
    {
        double now = glfwGetTime();
        float dt = (float)(now - prev);
        prev = now;
        if (dt > .05f) dt = .05f;     //--- 창 드래그 등으로 멈췄을 때 순간이동 방지
        update(dt);

        glfwGetFramebufferSize(window, &width, &height);
        if (height == 0) height = 1;
        glViewport(0, 0, width, height);

        drawScene();
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

// ---------------- 셰이더 (수업 자료와 같은 방식) ----------------
std::string filetobuf(const char* file)
{
    std::ifstream shaderFile(file);
    if (!shaderFile.is_open())
    {
        std::cerr << "ERROR: " << file << " 파일을 열 수 없음\n";
        return "";
    }
    std::string source((std::istreambuf_iterator<char>(shaderFile)), std::istreambuf_iterator<char>());
    shaderFile.close();
    return source;
}

void make_vertexShaders()
{
    std::string vertexSource = filetobuf("vertex.glsl");
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

void make_fragmentShaders()
{
    std::string fragmentSource = filetobuf("fragment.glsl");
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

GLuint make_shaderProgram()
{
    GLint result;
    GLchar errorLog[512];
    GLuint shaderID = glCreateProgram();
    glAttachShader(shaderID, vertexShader);
    glAttachShader(shaderID, fragmentShader);
    glLinkProgram(shaderID);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    glGetProgramiv(shaderID, GL_LINK_STATUS, &result);
    if (!result) {
        glGetProgramInfoLog(shaderID, 512, NULL, errorLog);
        std::cerr << "ERROR: shader program 연결 실패\n" << errorLog << std::endl;
        return 0;
    }
    glUseProgram(shaderID);
    return shaderID;
}

// ---------------- 큐브 버퍼 ----------------
// 중심 원점, 한 변 1인 큐브. 면마다 삼각형 2개 = 정점 6개, 바깥쪽에서 볼 때 CCW
// 면 순서: 0앞(+z) 1뒤(-z) 2왼(-x) 3오른(+x) 4위(+y) 5아래(-y)
void InitBuffer()
{
    const float a = .5f;
    GLfloat v[] = {
        // vPos               vNormal
        -a,-a, a,  0,0,1,   a,-a, a,  0,0,1,   a, a, a,  0,0,1,
        -a,-a, a,  0,0,1,   a, a, a,  0,0,1,  -a, a, a,  0,0,1,     // 앞 (+z)
         a,-a,-a,  0,0,-1, -a,-a,-a,  0,0,-1, -a, a,-a,  0,0,-1,
         a,-a,-a,  0,0,-1, -a, a,-a,  0,0,-1,  a, a,-a,  0,0,-1,    // 뒤 (-z)
        -a,-a,-a, -1,0,0,  -a,-a, a, -1,0,0,  -a, a, a, -1,0,0,
        -a,-a,-a, -1,0,0,  -a, a, a, -1,0,0,  -a, a,-a, -1,0,0,     // 왼 (-x)
         a,-a, a,  1,0,0,   a,-a,-a,  1,0,0,   a, a,-a,  1,0,0,
         a,-a, a,  1,0,0,   a, a,-a,  1,0,0,   a, a, a,  1,0,0,     // 오른 (+x)
        -a, a, a,  0,1,0,   a, a, a,  0,1,0,   a, a,-a,  0,1,0,
        -a, a, a,  0,1,0,   a, a,-a,  0,1,0,  -a, a,-a,  0,1,0,     // 위 (+y)
        -a,-a,-a,  0,-1,0,  a,-a,-a,  0,-1,0,  a,-a, a,  0,-1,0,
        -a,-a,-a,  0,-1,0,  a,-a, a,  0,-1,0, -a,-a, a,  0,-1,0,    // 아래 (-y)
    };
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (void*)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);
}

// 변환 행렬과 색을 셰이더로 보내고 큐브(또는 한 면)를 그린다. face = -1이면 6면 전부
void drawCube(const glm::mat4& model, const glm::vec3& color, int face = -1)
{
    unsigned int modelLocation = glGetUniformLocation(shaderProgramID, "modelTransform");
    unsigned int colorLocation = glGetUniformLocation(shaderProgramID, "objColor");
    glUniformMatrix4fv(modelLocation, 1, GL_FALSE, glm::value_ptr(model));
    glUniform3fv(colorLocation, 1, glm::value_ptr(color));
    glBindVertexArray(VAO);
    if (face < 0) glDrawArrays(GL_TRIANGLES, 0, 36);
    else          glDrawArrays(GL_TRIANGLES, face * 6, 6);
}

// 부모 행렬 기준 (pos) 위치, (size) 크기의 상자
void drawBox(const glm::mat4& parent, glm::vec3 pos, glm::vec3 size, glm::vec3 color)
{
    glm::mat4 m = glm::translate(parent, pos);
    m = glm::scale(m, size);
    drawCube(m, color);
}

// ---------------- 초기화 ----------------
float frand(float a, float b) { return a + (b - a) * (rand() / (float)RAND_MAX); }

void reset()
{
    const glm::vec3 cols[3] = { { .85f, .5f, .15f },{ .55f, .4f, .75f },{ .6f, .6f, .6f } };
    int n = 0;
    while (n < 3) {
        Obstacle o;
        o.sx = frand(.25f, .6f); o.sz = frand(.25f, .6f); o.sy = frand(.15f, .45f);
        o.x = frand(-STAGE_X + 1, STAGE_X - 1);
        o.z = frand(-STAGE_Z + 1, STAGE_Z - 1);
        // 로봇 시작 위치(원점)와 겹치면 다시 뽑기
        if (fabsf(o.x) < o.sx + 1 && fabsf(o.z) < o.sz + 1) continue;
        o.color = cols[n];
        obs[n++] = o;
    }
    robotX = robotY = robotZ = 0;
    dirX = 0; dirZ = 1;            // +z(화면 앞쪽)로 출발
    walkSpeed = 1.5f; walkPhase = 0;
    velY = 0; onGround = true;
    frontScale = 1; frontOpen = false;
    camPos = glm::vec3(0, 5, 12);
    camTarget = glm::vec3(0, 1, 0);
}

// ---------------- 그리기 ----------------
// 앞면 컬링(GL_FRONT): 카메라 쪽 벽은 바깥면이 보이므로 잘려 나가고, 안쪽 면만 남는다
void drawStage()
{
    glCullFace(GL_FRONT);
    glm::mat4 stage = glm::translate(glm::mat4(1.0f), glm::vec3(0, STAGE_H / 2, 0));
    stage = glm::scale(stage, glm::vec3(2 * STAGE_X, STAGE_H, 2 * STAGE_Z));
    for (int f = 1; f < 6; ++f)
        drawCube(stage, stageColor[f], f);

    // 앞면: 윗변 기준 y 스케일 → 위로 말려 올라가며 열림
    if (frontScale > .001f) {
        glm::mat4 front = glm::translate(glm::mat4(1.0f), glm::vec3(0, STAGE_H, 0));  // ③ 윗변을 원래 자리로
        front = glm::scale(front, glm::vec3(1, frontScale, 1));                       // ② y 방향으로 줄이기
        front = glm::translate(front, glm::vec3(0, -STAGE_H, 0));                     // ① 윗변을 원점으로
        drawCube(front * stage, stageColor[0], 0);
    }
    glCullFace(GL_BACK);           // 무대 안 물체는 일반 컬링
}

void drawObstacles()
{
    for (int i = 0; i < 3; ++i) {
        const Obstacle& o = obs[i];
        drawBox(glm::mat4(1.0f), glm::vec3(o.x, o.sy, o.z),
            glm::vec3(2 * o.sx, 2 * o.sy, 2 * o.sz), o.color);
    }
}

// 팔/다리: 관절(어깨·엉덩이)을 회전축으로 → 이동 → 회전 → 아래로 반 길이
void drawLimb(const glm::mat4& body, glm::vec3 joint, float angle, float w, float len, glm::vec3 color)
{
    glm::mat4 m = glm::translate(body, joint);                          // 관절 위치로
    m = glm::rotate(m, glm::radians(angle), glm::vec3(1, 0, 0));        // 관절 기준 앞뒤 흔들기
    m = glm::translate(m, glm::vec3(0, -len / 2, 0));                   // 팔다리 중심을 관절 아래로
    m = glm::scale(m, glm::vec3(w, len, w));
    drawCube(m, color);
}

void drawRobot()
{
    float yaw = atan2f(dirX, dirZ);                       // +z 기준 방향각 (라디안)
    float maxSwing = 15 + walkSpeed * 10;                 // 빠를수록 크게 흔듦
    if (maxSwing > 60) maxSwing = 60;
    float swing = onGround ? maxSwing * sinf(walkPhase) : 0;

    //--- 로봇 전체 변환 (모든 부위의 부모)
    glm::mat4 base = glm::translate(glm::mat4(1.0f), glm::vec3(robotX, robotY, robotZ));
    base = glm::rotate(base, yaw, glm::vec3(0, 1, 0));

    drawBox(base, { 0, 1.2f, 0 }, { .5f, .8f, .3f }, { .2f, .3f, .8f });   // 몸통
    drawBox(base, { 0, 1.85f, 0 }, { .4f, .4f, .4f }, { .95f, .8f, .65f }); // 머리
    drawBox(base, { 0, 1.85f, .24f }, { .08f, .08f, .1f }, { .9f, .1f, .1f });   // 코 (머리 앞면 +z)

    drawLimb(base, { -.33f, 1.55f, 0 }, swing, .15f, .7f, { .9f, .2f, .2f });   // 왼팔 (빨강)
    drawLimb(base, { .33f, 1.55f, 0 }, -swing, .15f, .7f, { .95f, .85f, .2f }); // 오른팔 (노랑)
    drawLimb(base, { -.13f, .8f, 0 }, -swing, .18f, .8f, { .8f, .2f, .8f });   // 왼다리 (보라)
    drawLimb(base, { .13f, .8f, 0 }, swing, .18f, .8f, { .2f, .8f, .8f });   // 오른다리 (청록)
}

GLvoid drawScene()
{
    glClearColor(.12f, .12f, .16f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(shaderProgramID);

    //--- 뷰잉 변환 (카메라)
    glm::mat4 view = glm::lookAt(camPos, camTarget, glm::vec3(0, 1, 0));
    unsigned int viewLocation = glGetUniformLocation(shaderProgramID, "viewTransform");
    glUniformMatrix4fv(viewLocation, 1, GL_FALSE, glm::value_ptr(view));

    //--- 투영 변환 (원근)
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)width / height, 0.1f, 100.0f);
    unsigned int projLocation = glGetUniformLocation(shaderProgramID, "projectionTransform");
    glUniformMatrix4fv(projLocation, 1, GL_FALSE, glm::value_ptr(projection));

    drawStage();
    drawObstacles();
    drawRobot();
}

// ---------------- 충돌 / 이동 ----------------
// (x,z)가 장애물 xz 영역(+로봇 반경) 안에 있는가
bool inside(const Obstacle& o, float x, float z)
{
    return fabsf(x - o.x) < o.sx + ROBOT_R && fabsf(z - o.z) < o.sz + ROBOT_R;
}

// 현재 위치에서 설 수 있는 높이 (바닥 0 또는 장애물 윗면)
float groundAt(float x, float z, float y)
{
    float g = 0;
    for (int i = 0; i < 3; ++i) {
        float top = 2 * obs[i].sy;
        if (inside(obs[i], x, z) && y >= top - .05f && top > g) g = top;
    }
    return g;
}

void update(float dt)
{
    // 앞면 열기/닫기
    if (frontOpen) { frontScale -= dt; if (frontScale < 0) frontScale = 0; }
    else { frontScale += dt; if (frontScale > 1) frontScale = 1; }

    walkPhase += walkSpeed * 4 * dt;          // 빠를수록 빨리 흔듦

    // 다음 위치
    float nx = robotX + dirX * walkSpeed * dt;
    float nz = robotZ + dirZ * walkSpeed * dt;

    bool blocked = nx < -STAGE_X + ROBOT_R || nx > STAGE_X - ROBOT_R ||
        nz < -STAGE_Z + ROBOT_R || nz > STAGE_Z - ROBOT_R;   // 가장자리
    for (int i = 0; i < 3; ++i)               // 장애물: 발이 윗면보다 낮으면 막힘
        if (inside(obs[i], nx, nz) && robotY < 2 * obs[i].sy - .05f) blocked = true;

    if (blocked) { dirX = -dirX; dirZ = -dirZ; }   // 뒤로 돌기
    else { robotX = nx; robotZ = nz; }

    // 점프 / 중력 (장애물에서 걸어 내려갈 때도 떨어짐)
    float g = groundAt(robotX, robotZ, robotY);
    if (!onGround || robotY > g) {
        velY -= GRAVITY * dt;
        robotY += velY * dt;
        if (robotY <= g) { robotY = g; velY = 0; onGround = true; }
        else onGround = false;
    }
}

// ---------------- 입력 ----------------
void charCallback(GLFWwindow* window, unsigned int ch)
{
    const float step = .3f;
    const float rot = glm::radians(5.0f);    // 공전 1회 5도
    switch (ch) {
    case 'o': case 'O': frontOpen = !frontOpen; break;
    case 'w': case 'W': dirX = 0;  dirZ = -1; break;
    case 's': case 'S': dirX = 0;  dirZ = 1;  break;
    case 'a': case 'A': dirX = -1; dirZ = 0;  break;
    case 'd': case 'D': dirX = 1;  dirZ = 0;  break;
    case '+': case '=': walkSpeed += .5f; if (walkSpeed > 5)   walkSpeed = 5;   break;
    case '-': case '_': walkSpeed -= .5f; if (walkSpeed < .5f) walkSpeed = .5f; break;
    case 'j': case 'J': if (onGround) { velY = JUMP_V; onGround = false; } break;
    case 'i': case 'I': reset(); break;

        // 카메라 이동: 위치와 바라보는 점을 함께 평행이동
    case 'z': camPos.z += step; camTarget.z += step; break;
    case 'Z': camPos.z -= step; camTarget.z -= step; break;
    case 'x': camPos.x += step; camTarget.x += step; break;
    case 'X': camPos.x -= step; camTarget.x -= step; break;

        // 카메라 공전: 바라보는 점(화면 중심)을 지나는 y축 기준으로 카메라 위치 회전
    case 'y': case 'Y': {
        float a = (ch == 'y') ? rot : -rot;
        glm::mat4 R = glm::rotate(glm::mat4(1.0f), a, glm::vec3(0, 1, 0));
        camPos = camTarget + glm::vec3(R * glm::vec4(camPos - camTarget, 0.0f));
        break;
    }
    case 'q': case 'Q': glfwSetWindowShouldClose(window, GLFW_TRUE); break;
    }
}