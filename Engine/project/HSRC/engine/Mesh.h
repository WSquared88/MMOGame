#pragma once

#include "Common.h"
#include "Transform.h"
#include "ResourceManager.h"
#include "Material.h"

#define DEFAULT_MESH Mesh::getMesh("Models/default.obj")

struct vertex
{
	DirectX::XMFLOAT3 position;
	DirectX::XMFLOAT3 normal;
	DirectX::XMFLOAT2 uv;
};

struct boundingVectors
{
	vec3 halfSize;
	vec3 center;
	vec3 sphere;
	float radius;
};

class Mesh : Resource
{
public:
	static Mesh* getMesh(string modelPath);
	~Mesh();
	void draw(mat4 &camera, mat4 &perspective, mat4 &model, Material* material);
	boundingVectors bounds;
private:
	Mesh(string modelPath);
	ID3D11Buffer *vertexBuffer, *indexBuffer;
	size_t elementArraySize;
};
