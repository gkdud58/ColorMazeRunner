#version 330 core
in vec3 FragPos;
in vec3 Normal;
out vec4 FragColor;

uniform vec3 lightPos;     //--- 조명 위치
uniform vec3 lightColor;   //--- 조명 색
uniform vec3 objectColor;  //--- 객체 색
uniform vec3 viewPos;      //--- 카메라 위치
uniform bool lightOn;      //--- m: 조명 켜기/끄기
uniform bool unlit;        //--- true: 조명 계산 없이 objectColor 그대로 (조명 표시 도형, 궤도)

void main()
{
	if (unlit) {
		FragColor = vec4(objectColor, 1.0);
		return;
	}

	vec3 normalVector = normalize(Normal);

	//--- 주변 조명 (조명이 꺼져도 완전히 까맣지는 않게 아주 약하게 남긴다)
	float ambientStrength = 0.08;
	vec3 ambient = ambientStrength * vec3(1.0);
	if (lightOn) {
		ambientStrength = 0.3;
		ambient = ambientStrength * lightColor;
	}

	vec3 diffuse = vec3(0.0);
	vec3 specular = vec3(0.0);
	if (lightOn) {
		//--- 산란 반사 조명
		vec3 lightDir = normalize(lightPos - FragPos);
		float diffuseLight = max(dot(normalVector, lightDir), 0.0);
		diffuse = diffuseLight * lightColor;

		//--- 거울 반사 조명
		float shininess = 64.0;
		vec3 viewDir = normalize(viewPos - FragPos);
		vec3 reflectDir = reflect(-lightDir, normalVector);
		float specularLight = max(dot(viewDir, reflectDir), 0.0);
		specularLight = pow(specularLight, shininess);
		specular = 0.8 * specularLight * lightColor;
	}

	vec3 result = (ambient + diffuse + specular) * objectColor;
	FragColor = vec4(result, 1.0);
}
