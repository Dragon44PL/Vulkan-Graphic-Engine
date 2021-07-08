#version 450

layout(location = 0) in vec3 pos;								// position in the world 
layout(location = 1) in vec3 col;

layout(set = 0, binding = 0) uniform UboViewProjection {		// single descriptor
	mat4 projection;
	mat4 view;
} uboViewProjection;

layout(set = 0, binding = 1) uniform UboModel {					// single descriptor
	mat4 model;
} uboModel;

layout(location = 0) out vec3 fragCol;

void main() {
	gl_Position = uboViewProjection.projection * uboViewProjection.view * uboModel.model * vec4(pos, 1.0);
	fragCol = col;
}