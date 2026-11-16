#include "Shader.h"

#include <fstream>
#include <iostream>
#include <gl/glm/gtc/type_ptr.hpp>

std::string filetobuf(const char* file)
{
	std::ifstream shaderFile(file);
	if (!shaderFile.is_open())
	{
		std::cerr << "ERROR: 파일을 열 수 없음: " << file << std::endl;
		return "";
	}
	std::string source((std::istreambuf_iterator<char>(shaderFile)), std::istreambuf_iterator<char>());
	shaderFile.close();
	return source;
}

GLuint compile(GLenum type, const char* path)
{
	std::string src = filetobuf(path);
	const char* source = src.c_str();

	GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, &source, NULL);
	glCompileShader(shader);

	GLint result;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &result);
	if (!result) {
		GLchar errorLog[512];
		glGetShaderInfoLog(shader, 512, NULL, errorLog);
		std::cerr << "ERROR: shader 컴파일 실패 (" << path << ")\n" << errorLog << std::endl;
	}
	return shader;
}


Shader::Shader(const char* vertexPath, const char* fragmentPath)
{
	GLuint vertexShader = compile(GL_VERTEX_SHADER, vertexPath);
	GLuint fragmentShader = compile(GL_FRAGMENT_SHADER, fragmentPath);

	ID = glCreateProgram();
	glAttachShader(ID, vertexShader);
	glAttachShader(ID, fragmentShader);
	glLinkProgram(ID);

	GLint result;
	glGetProgramiv(ID, GL_LINK_STATUS, &result);
	if (!result) {
		GLchar errorLog[512];
		glGetProgramInfoLog(ID, 512, NULL, errorLog);
		std::cerr << "ERROR: shader program 링크 실패\n" << errorLog << std::endl;
	}

	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);
}

void Shader::use() const
{
	glUseProgram(ID);
}

void Shader::setBool(const std::string& name, bool value) const
{
	glUniform1i(glGetUniformLocation(ID, name.c_str()), (int)value);
}

void Shader::setInt(const std::string& name, int value) const
{
	glUniform1i(glGetUniformLocation(ID, name.c_str()), value);
}

void Shader::setVec3(const std::string& name, const glm::vec3& value) const
{
	glUniform3f(glGetUniformLocation(ID, name.c_str()), value.x, value.y, value.z);
}

void Shader::setMat4(const std::string& name, const glm::mat4& value) const
{
	glUniformMatrix4fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, glm::value_ptr(value));
}
