#include <iostream>
#include <SDL.h>
#include <GL/glew.h>
#include <GL/glu.h>
#include <SDL_opengl.h>
#include <glm/glm.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtx/io.hpp>

#define DEFAULT_WIDTH 1400
#define DEFAULT_HEIGHT 1000
#define TITLE "Fractal"
#define POSITION "pos"
#define MATRIX "transformMat"
#define SCALE_FACTOR 1.05

const GLchar* vertexCode = R"glsl(
    #version 450 core

    precise in dvec2 pos;
    precise uniform dmat4 transformMat;
    precise out dvec2 point;

    void main() {
        point = pos.xy;
        precise dvec4 p = transformMat * dvec4(pos.xy, 0, 0);
        gl_Position = vec4(float(p.x), float(p.y), float(p.z), float(p.w));
    })glsl";

const GLchar* fragmentCode = R"glsl(
    #version 450 core

    #define LIMIT 200

    out vec4 outColor;
    precise flat in dvec2 point;

    void main() {
        precise dvec2 c = point;
        precise dvec2 zn = dvec2(0.0, 0.0);
        int iter = 0;
        
        while (iter <= LIMIT) {

            precise double temp = zn.x;
            zn.x = (zn.x - zn.y) * (zn.x + zn.y) + c.x;
            zn.y = 2.0*temp*zn.y + c.y;

            if (zn.x > 2.0 || zn.y > 2.0) {
                float clr = float(iter) / LIMIT;
                outColor = vec4(clr/3.0, clr/2.0, clr, 1);
                break;
            } else if (iter == LIMIT) {
                outColor = vec4(0, 0, 0, 1);
            }
            ++iter;
        }
    })glsl";

    // x is real part and y is imaginary part

const GLdouble vertices[] = {
    -2.0, -2.0,
    -2.0, 2.0,
    2.0,  -2.0,
    2.0,  2.0
};

GLuint createBuffer(GLenum type, const void * data, size_t size) {
    GLuint buffer;
    glGenBuffers(1, &buffer);
    glBindBuffer(type, buffer);
    glBufferData(type, size, data, GL_STATIC_DRAW);
    return buffer;
}

GLint compileShader(const bool isVertex) {
    GLenum type;
    const GLchar* source;
    if (isVertex) {
        type = GL_VERTEX_SHADER;
        source = vertexCode;
    } else {
        type = GL_FRAGMENT_SHADER;
        source = fragmentCode;
    }
    const GLint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    GLint isShaderCompiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &isShaderCompiled);
    if (isShaderCompiled != GL_TRUE) {
        std::cerr << "Unable to compile shader, shader id: " << shader << std::endl;
        GLsizei logLength;
		GLchar log[1024];
        glGetShaderInfoLog(shader, sizeof(log), &logLength, log);
        std::cerr << log << std::endl;
        abort();
    }
    return shader;
}

void draw(SDL_Window* window) {
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    SDL_GL_SwapWindow(window);
}

glm::mat4 getTransformationMatrix(int window_width, int window_height) {
    if (window_width < window_height) {
        // scale by X
        return glm::scale(glm::dmat4(1.0),
            glm::dvec3(((double) window_height / window_width), 1.0, 1.0));
    } else {
        // scale by Y
        return glm::scale(glm::dmat4(1.0),
            glm::dvec3(1.0, ((double) window_width / window_height), 1.0));
    }
}

void updateCursorCoords(double& xGL, double& yGL, int width, int height) {
    int xCursor, yCursor;
    SDL_GetMouseState(&xCursor, &yCursor);
    xGL = (2.0*xCursor/width)-1.0;
    yGL = 1.0-(2.0*yCursor)/height;
}

void updateZoomMatrix(SDL_Window * window, glm::dmat4& zoomMat, bool out) {
    double xGL, yGL;
    int window_width, window_height;
    SDL_GetWindowSize(window, &window_width, &window_height);
    updateCursorCoords(xGL, yGL, window_width, window_height);
    double scaleFactor = SCALE_FACTOR;
    if (out) {
        scaleFactor = 1.0 / SCALE_FACTOR;
    }
    
    // Translate -> scale -> reverse translate
    glm::dmat4 applyMat(1.0);
    applyMat = glm::translate(applyMat, glm::dvec3(xGL, yGL, 0.0));
    applyMat = glm::scale(applyMat, glm::dvec3(scaleFactor, scaleFactor, 1.0));
    applyMat = glm::translate(applyMat, glm::dvec3(-xGL, -yGL, 0.0));
    zoomMat = applyMat * zoomMat;
}

void setMatrix(GLint glProgram, const glm::dmat4& transformMat) {
    GLuint MatrixId = glGetUniformLocation(glProgram, MATRIX);
    glUniformMatrix4dv(MatrixId, 1, GL_FALSE, &transformMat[0][0]);
    std::cout << transformMat << std::endl;
}

int main() {
    // Set OpenGL Version
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    bool exit = false;
    bool moveModeOn = false;
    int window_width, window_height;
    glm::dmat4 resizeMat(1.0);
    glm::dmat4 zoomMat(1.0);
    SDL_Event event;
    double xGL, yGL;
    
    // Create SDL window and context
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "Video initialization failed: %s\n", SDL_GetError();
        return 1;
    }
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_Window *window = SDL_CreateWindow(TITLE, 
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
        DEFAULT_WIDTH, DEFAULT_HEIGHT, SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL);
    SDL_GLContext glcontext = SDL_GL_CreateContext(window);
    if (glcontext == NULL) {
        std::cerr << "GLContext is null" << std::endl;
        return 1;
    }

    // Initialize GLEW
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cerr << "Error initializing GLEW" << std::endl;
        return 1;
    }

    std::cout << "GLEW initialized" << std::endl;

    // Create buffer
    GLuint arrayBuffer = createBuffer(GL_ARRAY_BUFFER, vertices, sizeof(vertices));

    // Create and attach shaders
    GLint glProgram = glCreateProgram();
    GLint vertexShader = compileShader(true);
    glAttachShader(glProgram, vertexShader);
    GLint fragmentShader = compileShader(false);
    glAttachShader(glProgram, fragmentShader);
    glLinkProgram(glProgram);
    glUseProgram(glProgram);

    std::cout << "Created and attached shaders" << std::endl;

    // Resize image 
    SDL_GetWindowSize(window, &window_width, &window_height);
    resizeMat = getTransformationMatrix(window_width, window_height);
    glViewport(0, 0, window_width, window_height);
    setMatrix(glProgram, resizeMat);

    // Set the cordinates
    GLint vertexPos = glGetAttribLocation(glProgram, POSITION);
    glVertexAttribLPointer(vertexPos, 2, GL_DOUBLE, 0, 0);
    glEnableVertexAttribArray(vertexPos);
    draw(window);

    while (!exit) {
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_WINDOWEVENT: {
                    if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                        glm::dmat4 tempMat(1.0);
                        SDL_GetWindowSize(window, &window_width, &window_height);
                        resizeMat = getTransformationMatrix(window_width, window_height);
                        tempMat *= zoomMat;
                        tempMat *= resizeMat;
                        glViewport(0, 0, window_width, window_height);
                        setMatrix(glProgram, tempMat);
                        draw(window);
                    }
                    break;
                }
                case SDL_MOUSEWHEEL: {
                    if (event.wheel.y > 0) {
                        // Zoom in
                        updateZoomMatrix(window, zoomMat, false);
                    }
                    else if (event.wheel.y < 0) {
                        // Zoom out
                        updateZoomMatrix(window, zoomMat, true);
                    }
                    glm::dmat4 tempMat(1.0);
                    tempMat *= zoomMat;
                    tempMat *= resizeMat;
                    setMatrix(glProgram, tempMat);
                    draw(window);
                    break;
                }
                case SDL_MOUSEMOTION: {
                    // TODO move image
                    double xDiff = xGL;
                    double yDiff = yGL;
                    SDL_GetWindowSize(window, &window_width, &window_height);
                    updateCursorCoords(xGL, yGL, window_width, window_height);
                    xDiff = xGL - xDiff;
                    yDiff = yGL - yDiff;
                    if (moveModeOn && (xDiff != 0 || yDiff != 0)) {
                        glm::dmat4 tempMat(1.0);
                        zoomMat = glm::translate(glm::dmat4(1.0), glm::dvec3(xDiff, yDiff, 0.0)) * zoomMat;
                        tempMat *= zoomMat;
                        tempMat *= resizeMat;
                        setMatrix(glProgram, tempMat);
                        draw(window);
                    }
                    break;
                }
                case SDL_MOUSEBUTTONDOWN:
                    moveModeOn = true;
                    std::cout << "move mode on" << std::endl;
                    SDL_GetWindowSize(window, &window_width, &window_height);
                    updateCursorCoords(xGL, yGL, window_width, window_height);
                    break;
                case SDL_MOUSEBUTTONUP:
                    moveModeOn = false;
                    std::cout << "move mode off" << std::endl;
                    SDL_GetWindowSize(window, &window_width, &window_height);
                    updateCursorCoords(xGL, yGL, window_width, window_height);
                    break;
                case SDL_QUIT:
                    exit = true;
                    break;
                default:
                    break;
            }
        }
    }

    // Clean up resources
    glDeleteProgram(glProgram);
    glDeleteShader(fragmentShader);
    glDeleteShader(vertexShader);
    glDeleteBuffers(1, &arrayBuffer);

    SDL_GL_DeleteContext(glcontext);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
