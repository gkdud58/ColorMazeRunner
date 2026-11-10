#version 330 core
layout (location = 0) in vec3 vPos;
layout (location = 1) in vec3 vNormal;

out vec3 FragPos;   //--- 월드 공간의 위치 (프래그먼트 셰이더에서 조명 계산)
out vec3 Normal;    //--- 월드 공간의 법선

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
	gl_Position = projection * view * model * vec4(vPos, 1.0);
	FragPos = vec3(model * vec4(vPos, 1.0));
	//--- 회전/크기 변환이 있어도 법선이 표면에 수직을 유지하도록 법선 행렬 사용
	Normal = mat3(transpose(inverse(model))) * vNormal;
}
