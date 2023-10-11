#version 450

// From vertex input stage
layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 col;

// UBO
layout(binding = 0) uniform MVP 
{
    mat4 Model;
    mat4 View;
    mat4 Projection;
} matrices;

// To fragment shader
layout(location = 0) out vec3 fragColor;

void main() 
{
    gl_Position = matrices.Projection * matrices.View * matrices.Model * vec4( pos, 1.0 );
    fragColor = col;
}