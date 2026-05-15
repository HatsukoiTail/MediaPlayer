#include "OpenGLRender.h"

OpenGLRender::OpenGLRender(QWidget* parent)
    :QOpenGLWidget{parent}
{
    this->shader_sources =
        {
            {AV_PIX_FMT_NV12, std::string("varying vec2 textureOut;"
                                          "uniform sampler2D texY;"
                                          "uniform sampler2D texUV;"
                                          "void main(void) {"
                                          "    float y = texture2D(texY, textureOut).r;"
                                          "    float u = texture2D(texUV, textureOut).r - 0.5;"
                                          "    float v = texture2D(texUV, textureOut).g - 0.5;"
                                          "    y = 1.1643 * (y - 0.0625);"
                                          "    float r = y + 1.5958 * v;"
                                          "    float g = y - 0.39173 * u - 0.81290 * v;"
                                          "    float b = y + 2.017 * u;"
                                          "    gl_FragColor = vec4(r, g, b, 1.0);"
                                          "}")},
            {AV_PIX_FMT_YUV420P, std::string("varying vec2 textureOut;"
                                             "uniform sampler2D texY;"
                                             "uniform sampler2D texU;"
                                             "uniform sampler2D texV;"
                                             "void main() {"
                                             "    float y = texture2D(texY, textureOut).r;"
                                             "    float u = texture2D(texU, textureOut).r - 0.5;"
                                             "    float v = texture2D(texV, textureOut).r - 0.5;"
                                             "    y = 1.1643 * (y - 0.0625);"
                                             "    vec3 rgb = vec3("
                                             "        y + 1.5958 * v,"
                                             "        y - 0.39173 * u - 0.81290 * v,"
                                             "        y + 2.017 * u"
                                             "    );"
                                             "    gl_FragColor = vec4(rgb, 1.0);"
                                             "}")},
            {AV_PIX_FMT_RGB32, std::string("varying vec2 textureOut;"
                                           "uniform sampler2D texY;"
                                           "uniform sampler2D texUV;"
                                           "void main(void) {"
                                           "    float y = texture2D(texY, textureOut).r;"
                                           "    float u = texture2D(texUV, textureOut).r - 0.5;"
                                           "    float v = texture2D(texUV, textureOut).g - 0.5;"
                                           "    y = 1.1643 * (y - 0.0625);"
                                           "    float r = y + 1.5958 * v;"
                                           "    float g = y - 0.39173 * u - 0.81290 * v;"
                                           "    float b = y + 2.017 * u;"
                                           "    gl_FragColor = vec4(r, g, b, 1.0);"
                                           "}")}
        };
    this->texture_handlers =
        {
         {AV_PIX_FMT_NV12, &OpenGLRender::handleNV12},
         {AV_PIX_FMT_YUV420P, &OpenGLRender::handleYUV420P},
         {AV_PIX_FMT_RGB32, &OpenGLRender::handleRGBA8888},
         };
}

OpenGLRender::~OpenGLRender()
{
    makeCurrent();
    glDeleteBuffers(1, &this->vbo);
    glDeleteTextures(4, this->textures);
    doneCurrent();
}

void OpenGLRender::draw(AVFramePointer frame)
{
    std::lock_guard<std::mutex> lock(this->mutex);
    this->frame = make_shared_frame(std::move(frame));
    update();
}

void OpenGLRender::initializeGL()
{
    initializeOpenGLFunctions();

    this->initShaderPrograms();
    this->initTextures();
    this->initVertexBuffer();
}

void OpenGLRender::paintGL()
{
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    SharedFramePointer local_frame;

    {
        std::lock_guard<std::mutex> locker(this->mutex);
        local_frame = this->frame;
    }

    if (!local_frame)
        return;

    assert(this->shader_programs.find((AVPixelFormat)local_frame->format) != this->shader_programs.end());
    assert(this->texture_handlers.find((AVPixelFormat)local_frame->format) != this->texture_handlers.end());

    this->updateVertices(local_frame->width, local_frame->height);

    auto& program = this->shader_programs.find((AVPixelFormat)local_frame->format)->second;

    program->bind();

    glBindBuffer(GL_ARRAY_BUFFER, this->vbo);

    // 设置顶点属性
    GLuint vertexLoc = program->attributeLocation("vertexIn");
    GLuint texCoordLoc = program->attributeLocation("textureIn");

    glEnableVertexAttribArray(vertexLoc);
    glVertexAttribPointer(vertexLoc, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);

    glEnableVertexAttribArray(texCoordLoc);
    glVertexAttribPointer(texCoordLoc, 2, GL_FLOAT, GL_FALSE, 0, (void*)(8 * sizeof(GLfloat)));

    if (this->frame_flag != local_frame->pts || !this->texture_uploaded)
    {
        this->frame_flag = local_frame->pts;
        // 上传纹理
        auto handler_it = this->texture_handlers.find((AVPixelFormat)local_frame->format);
        if (handler_it == this->texture_handlers.end())
            return;

        (this->*(handler_it->second))(local_frame.get());
        this->texture_uploaded = true;
    }

    // 绘制
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    // 清理状态
    glDisableVertexAttribArray(vertexLoc);
    glDisableVertexAttribArray(texCoordLoc);
    program->release();
}

void OpenGLRender::resizeGL(int width, int height)
{
    this->texture_uploaded = false;
    QOpenGLFunctions::glViewport(0, 0, width, height);
    this->updateVertices(width, height);
}

void OpenGLRender::initShaderPrograms()
{
    this->shader_programs[AV_PIX_FMT_NV12] = this->makeShaderProgram(this->shader_sources[AV_PIX_FMT_NV12]);
    this->shader_programs[AV_PIX_FMT_YUV420P] = this->makeShaderProgram(this->shader_sources[AV_PIX_FMT_YUV420P]);
    this->shader_programs[AV_PIX_FMT_RGB32] = this->makeShaderProgram(this->shader_sources[AV_PIX_FMT_RGB32]);
}

void OpenGLRender::initTextures()
{
    glGenTextures(4, this->textures);

    // 公用纹理参数设置
    for (int i = 0; i < 4; ++i)
    {
        glBindTexture(GL_TEXTURE_2D, this->textures[i]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
}

void OpenGLRender::initVertexBuffer()
{
    GLfloat vertices[] = {
        -1.0f, -1.0f,
        +1.0f, -1.0f,
        -1.0f, +1.0f,
        +1.0f, +1.0f,
    };
    GLfloat texCoords[] = {
        0.0f, 1.0f,
        1.0f, 1.0f,
        0.0f, 0.0f,
        1.0f, 0.0f,
    };

    // 创建VBO
    glGenBuffers(1, &this->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, this->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices) + sizeof(texCoords), nullptr, GL_STATIC_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
    glBufferSubData(GL_ARRAY_BUFFER, sizeof(vertices), sizeof(texCoords), texCoords);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    memcpy(this->vertices, vertices, sizeof(vertices));
}

void OpenGLRender::updateVertices(int image_width, int image_height)
{
    const float image_aspect = static_cast<float>(image_width) / image_height;

    const float window_aspect = static_cast<float>(width()) / height();

    if (window_aspect > image_aspect)
    {
        const float x_scale = image_aspect / window_aspect;
        this->vertices[0] = -x_scale; this->vertices[1] = -1.0f;
        this->vertices[2] =  x_scale; this->vertices[3] = -1.0f;
        this->vertices[4] = -x_scale; this->vertices[5] =  1.0f;
        this->vertices[6] =  x_scale; this->vertices[7] =  1.0f;
    }
    else
    {
        const float y_scale = window_aspect / image_aspect;
        this->vertices[0] = -1.0f; this->vertices[1] = -y_scale;
        this->vertices[2] =  1.0f; this->vertices[3] = -y_scale;
        this->vertices[4] = -1.0f; this->vertices[5] =  y_scale;
        this->vertices[6] =  1.0f; this->vertices[7] =  y_scale;
    }

    glBindBuffer(GL_ARRAY_BUFFER, this->vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(this->vertices), this->vertices);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

std::unique_ptr<QOpenGLShaderProgram> OpenGLRender::makeShaderProgram(const std::string& source)
{
    auto program = std::make_unique<QOpenGLShaderProgram>();
    program->addShaderFromSourceCode(QOpenGLShader::Vertex,
                                     "attribute vec4 vertexIn;"
                                     "attribute vec2 textureIn;"
                                     "varying vec2 textureOut;"
                                     "void main(void) {"
                                     "    gl_Position = vertexIn;"
                                     "    textureOut = textureIn;"
                                     "}");
    program->addShaderFromSourceCode(QOpenGLShader::Fragment, source.c_str());
    program->link();
    return program;
}

void OpenGLRender::handleNV12(AVFrame* frame)
{
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, this->textures[0]); // Y
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED,
                 frame->width, frame->height, 0,
                 GL_RED, GL_UNSIGNED_BYTE, frame->data[0]);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, this->textures[1]); // UV
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG,
                 frame->width/2, frame->height/2, 0,
                 GL_RG, GL_UNSIGNED_BYTE, frame->data[1]);

    this->shader_programs[AV_PIX_FMT_NV12]->setUniformValue("texY", 0);
    this->shader_programs[AV_PIX_FMT_NV12]->setUniformValue("texUV", 1);
}

void OpenGLRender::handleYUV420P(AVFrame* frame)
{
    glPixelStorei(GL_UNPACK_ROW_LENGTH, frame->linesize[0]);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, this->textures[0]); // Y
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED,
                 frame->width, frame->height, 0,
                 GL_RED, GL_UNSIGNED_BYTE, frame->data[0]);

    glPixelStorei(GL_UNPACK_ROW_LENGTH, frame->linesize[1]);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, this->textures[2]); // U
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED,
                 frame->width/2, frame->height/2, 0,
                 GL_RED, GL_UNSIGNED_BYTE, frame->data[1]);

    glPixelStorei(GL_UNPACK_ROW_LENGTH, frame->linesize[2]);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, this->textures[3]); // V
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED,
                 frame->width/2, frame->height/2, 0,
                 GL_RED, GL_UNSIGNED_BYTE, frame->data[2]);

    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

    this->shader_programs[AV_PIX_FMT_YUV420P]->setUniformValue("texY", 0);
    this->shader_programs[AV_PIX_FMT_YUV420P]->setUniformValue("texU", 1);
    this->shader_programs[AV_PIX_FMT_YUV420P]->setUniformValue("texV", 2);
}

void OpenGLRender::handleRGBA8888(AVFrame* frame)
{
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, this->textures[0]);

    GLenum format = GL_RGBA;
    if(frame->format == AV_PIX_FMT_RGB32){
        format = GL_BGRA; // 根据实际字节顺序调整
    }

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                 frame->width, frame->height, 0,
                 format, GL_UNSIGNED_BYTE, frame->data[0]);

    this->shader_programs[AV_PIX_FMT_RGB32]->setUniformValue("texRGB", 0);
}

