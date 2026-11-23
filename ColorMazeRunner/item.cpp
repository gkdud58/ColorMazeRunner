#include "Shader.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <GL/glew.h>
#include <gl/glm/glm.hpp>
#include <gl/glm/gtc/matrix_transform.hpp>

namespace {
	const int MAX_LINE_LENGTH = 234;

	struct Vertex { float x, y, z; };
	struct Face { unsigned int v1, v2, v3; };


	void read_newline(char* str) {
		char* pos;
		if ((pos = strchr(str, '\n')) != NULL)
			*pos = '\0';
	}

	void read_obj_file(const char* filename, Vertex** outVertices, size_t* outVertexCount, Face** outFaces, size_t* outFaceCount) {
	FILE* file;
	fopen_s(&file, filename, "r");
	if (!file) {
		perror("Error opening file");
		exit(EXIT_FAILURE);
	}

	char line[MAX_LINE_LENGTH];
	size_t vertexCount = 0, faceCount = 0;

	while (fgets(line, sizeof(line), file)) {
		read_newline(line);
		if (line[0] == 'v' && line[1] == ' ')
			vertexCount++;
		else if (line[0] == 'f' && line[1] == ' ')
			faceCount++;
	}
	fseek(file, 0, SEEK_SET);

	Vertex* vertices = (Vertex*)malloc(vertexCount * sizeof(Vertex));
	Face* faces = (Face*)malloc(faceCount * sizeof(Face));
	size_t vertexIndex = 0, faceIndex = 0;

	while (fgets(line, sizeof(line), file)) {
		read_newline(line);

		if (line[0] == 'v' && line[1] == ' ') {
			sscanf_s(line + 2, "%f %f %f", &vertices[vertexIndex].x,
				&vertices[vertexIndex].y, &vertices[vertexIndex].z);
			vertexIndex++;
		}
		else if (line[0] == 'f' && line[1] == ' ') {
			unsigned int v1 = 1, v2 = 1, v3 = 1, n1, n2, n3;
			int result = sscanf_s(line + 2, "%u//%u %u//%u %u//%u", &v1, &n1, &v2, &n2, &v3, &n3);
			if (result != 6)
				sscanf_s(line + 2, "%u %u %u", &v1, &v2, &v3);
			faces[faceIndex].v1 = v1 - 1;
			faces[faceIndex].v2 = v2 - 1;
			faces[faceIndex].v3 = v3 - 1;
			faceIndex++;
		}
	}
	fclose(file);

		*outVertices = vertices;
		*outVertexCount = vertexCount;
		*outFaces = faces;
		*outFaceCount = faceCount;
	}

	GLuint vao, vbo;
	int vertexCount;
}


void init_item() {
	Vertex* vertices; size_t objVertexCount;
	Face* faces; size_t faceCount;
	read_obj_file("cube.obj", &vertices, &objVertexCount, &faces, &faceCount);

	glm::vec3 offset(-0.5f, -0.5f, -0.5f);

	glm::vec3 centroid(0.0f);
	for (size_t i = 0; i < objVertexCount; i++)
		centroid += glm::vec3(vertices[i].x, vertices[i].y, vertices[i].z);
	centroid /= (float)objVertexCount;

	size_t n = faceCount * 3;
	GLfloat* data = (GLfloat*)malloc(n * 6 * sizeof(GLfloat));

	for (size_t i = 0; i < faceCount; i++) {
		unsigned int idx[3] = { faces[i].v1, faces[i].v2, faces[i].v3 };
		glm::vec3 p[3];
		for (int j = 0; j < 3; j++)
			p[j] = glm::vec3(vertices[idx[j]].x, vertices[idx[j]].y, vertices[idx[j]].z);

		glm::vec3 normal = glm::normalize(glm::cross(p[1] - p[0], p[2] - p[0]));
		glm::vec3 faceCenter = (p[0] + p[1] + p[2]) / 3.0f;
		if (glm::dot(normal, faceCenter - centroid) < 0.0f)
			normal = -normal;

		for (int j = 0; j < 3; j++) {
			glm::vec3 pos = p[j] + offset;
			size_t k = (i * 3 + j) * 6;
			data[k + 0] = pos.x; data[k + 1] = pos.y; data[k + 2] = pos.z;
			data[k + 3] = normal.x; data[k + 4] = normal.y; data[k + 5] = normal.z;
		}
	}
	free(vertices);
	free(faces);

	vertexCount = (int)n;

	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, n * 6 * sizeof(GLfloat), data, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (void*)0);                      // 위치
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (void*)(3 * sizeof(GLfloat)));  // 법선
	glEnableVertexAttribArray(1);

	free(data);
}



// ========================= 큐브 그리는 용도 / 큐브추가 여기서 =========================

const glm::vec3 ITEM_COLOR = glm::vec3(0.8f, 1.0f, 0.8f);

void draw_item(Shader* shader) {
	glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.5f, 0.0f))
		* glm::scale(glm::mat4(1.0f), glm::vec3(2.0f));
	shader->setMat4("model", model);
	shader->setVec3("objectColor", ITEM_COLOR);
	glBindVertexArray(vao);
	glDrawArrays(GL_TRIANGLES, 0, vertexCount);
}
