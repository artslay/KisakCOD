#include "gl_backend.h"

#ifdef __SWITCH__
#include <switch.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/gl.h>
#endif

#ifdef __SWITCH__
namespace
{
struct KisakGLShader
{
    GLuint object = 0;
    GLenum stage = 0;
};

static const char *kFallbackVertexShader = R"(#version 430 core
layout(location=0) in vec4 aPosition;
layout(location=4) in vec2 aTexCoord;
layout(location=12) in vec4 aColor;
out vec2 vTexCoord;
out vec4 vColor;
void main() { gl_Position = aPosition; vTexCoord = aTexCoord; vColor = aColor; }
)";

static const char *kFallbackPixelShader = R"(#version 430 core
in vec2 vTexCoord;
in vec4 vColor;
out vec4 FragColor;
uniform sampler2D uTexture0;
void main() { FragColor = vColor; }
)";

static GLuint CompileGLShader(GLenum stage, const char *source, std::string &error)
{
    GLuint shader = glCreateShader(stage);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok)
    {
        char log[2048] = {};
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        error = log;
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

static GLuint LinkGLProgram(GLuint vs, GLuint ps, std::string &error)
{
    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, ps);
    glLinkProgram(program);
    GLint ok = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok)
    {
        char log[2048] = {};
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        error = log;
        glDeleteProgram(program);
        return 0;
    }
    return program;
}

