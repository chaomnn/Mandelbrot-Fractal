#include <iostream>

#include <SDL.h>
#include <GL/glew.h>

#include <glm/glm.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtx/io.hpp>

#define DEFAULT_WIDTH 1920
#define DEFAULT_HEIGHT 1080
#define TITLE "Mandelbrot Fractal"
#define POSITION "pos"
#define MATRIX "transformMat"
#define MATRIX_ZOOM "zoomMat"
#define BASE_COLOR "baseColor"
#define SCALE_FACTOR 1.05
#define CLRS_NUM 16

extern const char _binary_vertex_shader_start;
extern const char _binary_vertex_shader_end;
extern const char _binary_mandelbrot_shader_start;
extern const char _binary_mandelbrot_shader_end;

const GLfloat vertices[] = {
    -1.0f, -1.0f,
    -1.0f, 1.0f,
    1.0f,  -1.0f,
    1.0f,  1.0f
};

const glm::vec3 baseColor(3.4, 3.9, 5.0);

static GLuint createBuffer(GLenum type, const void *data, size_t size) {
    GLuint buffer;
    glGenBuffers(1, &buffer);
    glBindBuffer(type, buffer);
    glBufferData(type, size, data, GL_STATIC_DRAW);
    return buffer;
}

static GLint compileShader(const char *source_start, const char *source_end, GLenum type) {
    const GLint shader = glCreateShader(type);
    GLint source_length = (GLint) (source_end - source_start);
    glShaderSource(shader, 1, &source_start, &source_length);
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

static Uint32 callback(Uint32 interval, void *eventType) {
    Uint32 timerEventType = *(Uint32 *) eventType;
    if (timerEventType != ((Uint32)-1)) {
        SDL_Event event = { 0 };
        event.type = timerEventType;
        SDL_PushEvent(&event);
    }
    return interval;
}

class Surface {
    SDL_Window *window;
    GLint glProgram;
    SDL_GLContext glContext;
    GLuint arrayBuffer;
    GLint vertexShader, fragmentShader;
    int window_width;
    int window_height;
    Uint32 timerEventType;
    glm::mat4 resizeMat;
    glm::dmat4 zoomMat = glm::dmat4(1.0);
    glm::vec3 clr = baseColor;

public:
    Surface(Uint32 timerEventType) {
        // Create SDL window and context
        timerEventType = timerEventType;
        window = SDL_CreateWindow(TITLE,
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            DEFAULT_WIDTH, DEFAULT_HEIGHT, SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL);
        glContext = SDL_GL_CreateContext(window);
        if (glContext == NULL) {
            std::cerr << "GLContext is null" << std::endl;
            abort();
        }
        // Initialize GLEW
        glewExperimental = GL_TRUE;
        if (glewInit() != GLEW_OK) {
            std::cerr << "Error initializing GLEW" << std::endl;
            abort();
        }

        // Create buffer
        arrayBuffer = createBuffer(GL_ARRAY_BUFFER, vertices, sizeof(vertices));
        glProgram = glCreateProgram();
        // Create and attach shaders
        vertexShader = compileShader(&_binary_vertex_shader_start,
            &_binary_vertex_shader_end, GL_VERTEX_SHADER);
        glAttachShader(glProgram, vertexShader);
        fragmentShader = compileShader(&_binary_mandelbrot_shader_start,
            &_binary_mandelbrot_shader_end, GL_FRAGMENT_SHADER);
        glAttachShader(glProgram, fragmentShader);
        glLinkProgram(glProgram);
        glUseProgram(glProgram);

        // Set the coordinates
        GLint vertexPos = glGetAttribLocation(glProgram, POSITION);
        glVertexAttribPointer(vertexPos, 2, GL_FLOAT, GL_FALSE, 0, 0);
        glEnableVertexAttribArray(vertexPos);

        // Resize image
        resizeMat = getTransformationMatrix();
        glViewport(0, 0, window_width, window_height);
        setResizeMatrix(resizeMat);
        setZoomMatrix(zoomMat);
        setColor(clr);
        draw();
    }

    ~Surface() {
        // Clean up resources
        glDeleteProgram(glProgram);
        glDeleteShader(fragmentShader);
        glDeleteShader(vertexShader);
        glDeleteBuffers(1, &arrayBuffer);
        SDL_GL_DeleteContext(glContext);
        SDL_DestroyWindow(window);
    }

    void setResizeMatrix(const glm::mat4 &transformMat) {
        GLuint matrixId = glGetUniformLocation(glProgram, MATRIX);
        glUniformMatrix4fv(matrixId, 1, GL_FALSE, &transformMat[0][0]);
    }

    void setZoomMatrix(const glm::dmat4 &zoomMat) {
        GLuint matrixId = glGetUniformLocation(glProgram, MATRIX_ZOOM);
        glm::dmat4 invMat = glm::inverse(zoomMat);
        glUniformMatrix4dv(matrixId, 1, GL_FALSE, &invMat[0][0]);
        std::cout << "Zoom magnitude: " << zoomMat[0][0] << std::endl;
    }

    void setColor(const glm::vec3 &clr) {
        // Set color for color switching
        GLuint colorId = glGetUniformLocation(glProgram, BASE_COLOR);
        glUniform3fv(colorId, 1, &clr[0]);
    }

    void draw() {
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        SDL_GL_SwapWindow(window);
    }

    void updateCursorCoords(double& xGL, double& yGL) {
        SDL_GetWindowSize(window, &window_width, &window_height);
        int xCursor, yCursor;
        SDL_GetMouseState(&xCursor, &yCursor);
        bool diff = window_width < window_height;
        xGL = (2.0*xCursor/window_width)-1.0 *
            (diff ? (float) window_width / (float) window_height : 1);
        yGL = 1.0-(2.0*yCursor)/window_height *
            (diff ? 1 : (float) window_height / (float) window_width);
    }

    void updateZoomMatrix(glm::dmat4 &zoomMat, bool out) {
        double xGL, yGL;
        SDL_GetWindowSize(window, &window_width, &window_height);
        updateCursorCoords(xGL, yGL);
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

    glm::mat4 getTransformationMatrix() {
        SDL_GetWindowSize(window, &window_width, &window_height);
        if (window_width < window_height) {
            // scale by X
            return glm::scale(glm::mat4(1.0f),
                glm::vec3(((float) window_height / window_width), 1.0f, 1.0f));
        } else {
            // scale by Y
            return glm::scale(glm::mat4(1.0f),
                glm::vec3(1.0f, ((float) window_width / window_height), 1.0f));
        }
    }

    void mainLoop();
};

void Surface::mainLoop() {
    SDL_Event event;
    bool moveModeOn = false, switchColors = false, exit = false;
    double xGL, yGL;
    SDL_TimerID timerID;
    while (!exit) {
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_WINDOWEVENT: {
                    if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                        resizeMat = getTransformationMatrix();
                        glViewport(0, 0, window_width, window_height);
                        setResizeMatrix(resizeMat);
                        draw();
                    }
                    break;
                }
                case SDL_MOUSEWHEEL: {
                    if (event.wheel.y > 0) {
                        // Zoom in
                        updateZoomMatrix(zoomMat, false);
                    }
                    else if (event.wheel.y < 0) {
                        // Zoom out
                        updateZoomMatrix(zoomMat, true);
                    }
                    setZoomMatrix(zoomMat);
                    draw();
                    break;
                }
                case SDL_MOUSEMOTION: {
                    // move image
                    double xDiff = xGL;
                    double yDiff = yGL;
                    updateCursorCoords(xGL, yGL);
                    xDiff = xGL - xDiff;
                    yDiff = yGL - yDiff;
                    if (moveModeOn && (xDiff != 0 || yDiff != 0)) {
                        zoomMat = glm::translate(glm::dmat4(1.0),
                            glm::dvec3(xDiff, yDiff, 0.0f)) * zoomMat;
                        setZoomMatrix(zoomMat);
                        draw();
                    }
                    break;
                }
                case SDL_MOUSEBUTTONDOWN:
                    moveModeOn = true;
                    updateCursorCoords(xGL, yGL);
                    break;
                case SDL_MOUSEBUTTONUP:
                    moveModeOn = false;
                    updateCursorCoords(xGL, yGL);
                    break;
                case SDL_QUIT:
                    exit = true;
                    break;
                case SDL_KEYDOWN: {
                    // Colors changing
                    auto key = event.key.keysym.sym;
                    if (key == SDLK_c) {
                        switchColors = !switchColors;
                        if (switchColors) {
                            timerID = SDL_AddTimer(100, callback, &timerEventType);
                        } else {
                            SDL_RemoveTimer(timerID);
                        }
                    } else if (key == SDLK_r) {
                        if (switchColors) {
                            SDL_RemoveTimer(timerID);
                            switchColors = false;
                        }
                        clr = baseColor;
                        setColor(clr);
                        draw();
                    } else if (key == SDLK_1 || key == SDLK_2 || key == SDLK_3) {
                        clr += glm::vec3(key == SDLK_1 ? 0.05 : 0.00, key == SDLK_2 ? 0.05 : 0.00, 
                            key == SDLK_3 ? 0.05 : 0.00);
                        setColor(clr);
                        draw();
                    } else if (key == SDLK_z) {
                        zoomMat = glm::dmat4(1.0);
                        setZoomMatrix(zoomMat);
                        draw();
                    }
                    break;
                }
                default:
                    if (event.type == timerEventType) {
                        clr += glm::vec3(0.05, 0.05, 0.05);
                        setColor(clr);
                        draw();
                    }
                    break;
            }
        }
    }
}

int main() {
    // Set OpenGL Version, init SDL
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "Video initialization failed: %s\n", SDL_GetError();
        abort();
    }
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    Uint32 timerEventType = SDL_RegisterEvents(1);
    Surface surface(timerEventType);
    surface.mainLoop();

    SDL_Quit();
    return 0;
}
