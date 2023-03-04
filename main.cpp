#include <iostream>
#include <fstream>
#include <string>

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
#define COLORS "colors"
#define SCALE_FACTOR 1.05
#define CONFIG_FILE "colors.txt"
#define CLRS_NUM 16

const GLchar* vertexCode = R"glsl(
    #version 450 core
    in vec2 pos;
    uniform mat4 transformMat;
    out vec2 point;

    void main() {
        point = pos.xy;
        gl_Position = transformMat * vec4(pos.xy, 0, 1);
    })glsl";

const GLchar* fragmentCode = R"glsl(
    #version 450 core

    #define LIMIT 1000
    #define CLRS_NUM 16

    out vec4 outColor;
    in vec2 point;
    uniform dmat4 zoomMat;
    uniform vec3 colors[CLRS_NUM];

    vec3 getColor(float iter) {
        int num = int(mod(iter, colors.length()));
        vec3 clr1 = colors[num];
        vec3 clr2 = colors[num + 1];
        vec3 finalClr = mix(clr1, clr2, mod(iter, 1));
        return finalClr / 255.0;
    }

    void main() {
        dvec4 c = dvec4(double(point.x), double(point.y), 0, 1);
        c = zoomMat * c;
        dvec2 zn = dvec2(0.0, 0.0);
        int iter = 0;

        double xSqr = zn.x * zn.x;
        double ySqr = zn.y * zn.y;

        while (iter <= LIMIT) {

            double temp = zn.x;
            zn.x = xSqr - ySqr + c.x;
            zn.y = 2.0*temp*zn.y + c.y;

            xSqr = zn.x * zn.x;
            ySqr = zn.y * zn.y;
            double sum = xSqr + ySqr;

            if (sum > 4.0) {
                float lsum = log(float(sum));
                float fIter = float(iter) + 1.0 - log(lsum) / log(2.0);
                outColor = vec4(getColor(fIter), 1);
                break;
            } else if (iter == LIMIT) {
                outColor = vec4(0, 0, 0, 1);
            }
            ++iter;
        }
    })glsl";

    // x is real part and y is imaginary part

const GLfloat vertices[] = {
    -1.0f, -1.0f,
    -1.0f, 1.0f,
    1.0f,  -1.0f,
    1.0f,  1.0f
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
        return glm::scale(glm::mat4(1.0f),
            glm::vec3(((float) window_height / window_width), 1.0f, 1.0f));
    } else {
        // scale by Y
        return glm::scale(glm::mat4(1.0f),
            glm::vec3(1.0f, ((float) window_width / window_height), 1.0f));
    }
}

void updateCursorCoords(double& xGL, double& yGL, int width, int height) {
    int xCursor, yCursor;
    SDL_GetMouseState(&xCursor, &yCursor);
    bool diff = width < height;
    xGL = (2.0*xCursor/width)-1.0 * (diff ? (float) width / (float) height : 1);
    yGL = 1.0-(2.0*yCursor)/height * (diff ? 1 : (float) height / (float) width);
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

void setResizeMatrix(GLint glProgram, const glm::mat4& transformMat) {
    GLuint matrixId = glGetUniformLocation(glProgram, MATRIX);
    glUniformMatrix4fv(matrixId, 1, GL_FALSE, &transformMat[0][0]);
}

void setZoomMatrix(GLint glProgram, const glm::dmat4& zoomMat) {
    GLuint matrixId = glGetUniformLocation(glProgram, MATRIX_ZOOM);
    glm::dmat4 invMat = glm::inverse(zoomMat);
    glUniformMatrix4dv(matrixId, 1, GL_FALSE, &invMat[0][0]);
    std::cout << "Zoom magnitude: " << zoomMat[0][0] << std::endl;
}

int main() {
    // Set OpenGL Version
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    bool exit = false;
    bool moveModeOn = false;
    int window_width, window_height;
    glm::mat4 resizeMat(1.0f);
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

    // Read 16 RGB colors from file
    std::ifstream fin(CONFIG_FILE);
    GLfloat colorVecs[3 * CLRS_NUM];
    int num = 0;
    for (std::string line; std::getline(fin, line);) {
        std::istringstream currStr;
        currStr.str(line);
        for (std::string temp; std::getline(currStr, temp, ',');) {
            colorVecs[num] = std::stoi(temp);
            num++;
        }
    }
    fin.close();

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

    // Resize image 
    SDL_GetWindowSize(window, &window_width, &window_height);
    resizeMat = getTransformationMatrix(window_width, window_height);
    glViewport(0, 0, window_width, window_height);
    setResizeMatrix(glProgram, resizeMat);
    setZoomMatrix(glProgram, zoomMat); 

    // Set colors
    GLuint arrayId = glGetUniformLocation(glProgram, COLORS);
    glUniform3fv(arrayId, sizeof(colorVecs) / sizeof(*colorVecs), colorVecs);

    // Set the cordinates
    GLint vertexPos = glGetAttribLocation(glProgram, POSITION);
    glVertexAttribPointer(vertexPos, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(vertexPos);
    draw(window);

    while (!exit) {
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_WINDOWEVENT: {
                    if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                        SDL_GetWindowSize(window, &window_width, &window_height);
                        resizeMat = getTransformationMatrix(window_width, window_height);
                        glViewport(0, 0, window_width, window_height);
                        setResizeMatrix(glProgram, resizeMat);
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
                    setZoomMatrix(glProgram, zoomMat);
                    draw(window);
                    break;
                }
                case SDL_MOUSEMOTION: {
                    // move image
                    double xDiff = xGL;
                    double yDiff = yGL;
                    SDL_GetWindowSize(window, &window_width, &window_height);
                    updateCursorCoords(xGL, yGL, window_width, window_height);
                    xDiff = xGL - xDiff;
                    yDiff = yGL - yDiff;
                    if (moveModeOn && (xDiff != 0 || yDiff != 0)) {
                        zoomMat = glm::translate(glm::dmat4(1.0), glm::dvec3(xDiff, yDiff, 0.0f)) * zoomMat;
                        setZoomMatrix(glProgram, zoomMat);
                        draw(window);
                    }
                    break;
                }
                case SDL_MOUSEBUTTONDOWN:
                    moveModeOn = true;
                    SDL_GetWindowSize(window, &window_width, &window_height);
                    updateCursorCoords(xGL, yGL, window_width, window_height);
                    break;
                case SDL_MOUSEBUTTONUP:
                    moveModeOn = false;
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
