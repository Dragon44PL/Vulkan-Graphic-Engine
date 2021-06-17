#version 450

layout(location = 0) in vec3 fragColour; // Intorpolated colour from vertex

layout(location = 0) out vec4 outColour; // Final output colour

void main() {
	outColour = vec4(fragColour, 1.0); // Alpha value
}

