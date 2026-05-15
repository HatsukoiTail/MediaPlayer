#ifndef OPENGLRENDER_H
#define OPENGLRENDER_H

#include "SmartStruct.h"

#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLWidget>

class OpenGLRender : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT
public:
    explicit OpenGLRender(QWidget* parent = nullptr);
    ~OpenGLRender() override;

public:
    void draw(AVFramePointer frame);

private:
    void draw();

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int width, int height) override;

private:
    void initShaderPrograms();
    void initTextures();
    void initVertexBuffer();

private:
    void updateVertices(int image_width, int image_height);

private:
    std::unique_ptr<QOpenGLShaderProgram> makeShaderProgram(const std::string& source);
    void handleNV12(AVFrame* frame);
    void handleYUV420P(AVFrame* frame);
    void handleRGBA8888(AVFrame* frame);

private:
    using TextureHandleMethod = void(OpenGLRender::*)(AVFrame* frame);
    GLfloat vertices[8];
    GLuint vbo = 0;
    GLuint textures[4] = { 0 };
    std::unordered_map<AVPixelFormat, std::string> shader_sources;
    std::unordered_map<AVPixelFormat, std::unique_ptr<QOpenGLShaderProgram>> shader_programs;
    std::unordered_map<AVPixelFormat, TextureHandleMethod> texture_handlers;

private:
    int64_t frame_flag = 0; // 避免对同一帧反复渲染，优化性能
    bool texture_uploaded = false;

private:
    std::mutex mutex;
    SharedFramePointer frame;
    QSize last_frame_size;
};

#endif // OPENGLRENDER_H
