#version 450

// From vertex input stage
layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 col;

// UBO
layout(binding = 0) uniform ViewProjection 
{
    mat4 View;
    mat4 Projection;
} view_proj;

//  push constant
layout(push_constant) uniform Model
{
    mat4 Model;
} model;

// To fragment shader
layout(location = 0) out vec3 fragColor;

void main() 
{
    gl_Position = view_proj.Projection * view_proj.View * model.Model * vec4( pos, 1.0 );
    fragColor = col;
}