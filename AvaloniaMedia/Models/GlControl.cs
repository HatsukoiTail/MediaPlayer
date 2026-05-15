

using System;
using System.Runtime.InteropServices;
using System.Threading;
using Avalonia.OpenGL;
using Avalonia.OpenGL.Controls;
using FFmpeg.AutoGen;
using Vortice.Direct3D11;
using Vortice.DXGI;

namespace AvaloniaMedia.Models;

public class EglD3D11Extractor
{
    // EGL常量
    const int EGL_DEVICE_EXT = 0x322C;
    const int EGL_D3D11_DEVICE_ANGLE = 0x33A1;

    [DllImport("libEGL.dll")]
    static extern IntPtr eglGetCurrentDisplay();

    [DllImport("libEGL.dll")]
    static extern bool eglQueryDisplayAttribEXT(
        IntPtr display, int attribute, out IntPtr value);

    [DllImport("libEGL.dll")]
    static extern bool eglQueryDeviceAttribEXT(
        IntPtr device, int attribute, out IntPtr value);

    public static IntPtr GetD3D11DevicePtr()
    {
        var display = eglGetCurrentDisplay();
        if (display == IntPtr.Zero)
        {
            Console.WriteLine("ERROR: 无法获取EGL Display，可能不在GL线程中");
            return IntPtr.Zero;
        }

        if (!eglQueryDisplayAttribEXT(display, EGL_DEVICE_EXT, out var eglDevice))
        {
            Console.WriteLine("ERROR: eglQueryDisplayAttribEXT 失败");
            return IntPtr.Zero;
        }

        if (!eglQueryDeviceAttribEXT(eglDevice, EGL_D3D11_DEVICE_ANGLE, out var d3d11Device))
        {
            Console.WriteLine("ERROR: eglQueryDeviceAttribEXT 失败，可能不是ANGLE后端");
            return IntPtr.Zero;
        }

        Console.WriteLine($"SUCCESS: D3D11Device ptr = {d3d11Device}");
        return d3d11Device; // 这就是 ID3D11Device*
    }
}

public class GlControl : OpenGlControlBase
{
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    delegate IntPtr EglGetCurrentDisplayDelegate();

    private Vortice.Direct3D11.ID3D11Device? _device;
    private Vortice.Direct3D11.ID3D11DeviceContext? _context;
    private Vortice.Direct3D11.ID3D11VideoDevice? _videoDevice;
    private Vortice.Direct3D11.ID3D11VideoContext? _videoContext;
    private Vortice.Direct3D11.ID3D11VideoProcessor? _processor;
    private Vortice.Direct3D11.ID3D11Texture2D? _outputTex;
    private ID3D11VideoProcessorOutputView? _outputView;
    private ID3D11VideoProcessorEnumerator? _enumerator;

    public IntPtr D3D11DevicePtr { get; private set; }


    const int Width = 1920;
    const int Height = 960;


    // 委托定义
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    delegate IntPtr EglCreateImageDelegate(IntPtr display, IntPtr context, uint target, IntPtr buffer, int[] attribs);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    delegate bool EglDestroyImageDelegate(IntPtr display, IntPtr image);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    delegate void GlEGLImageTargetTexture2DDelegate(uint target, IntPtr image);

    // EGL常量
    const uint EGL_D3D_TEXTURE_ANGLE = 0x33A3;
    const uint GL_TEXTURE_2D = 0x0DE1;

    EglCreateImageDelegate? _eglCreateImage;
    EglDestroyImageDelegate? _eglDestroyImage;
    GlEGLImageTargetTexture2DDelegate? _glEGLImageTargetTexture2D;
    IntPtr _eglDisplay;
    uint _glTexture;

    GlInterface? _gl;
    private volatile bool _hasNewFrame = false;
    int _currentFramebuffer;

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    delegate IntPtr EglQueryStringDelegate(IntPtr display, int name);

    protected override void OnOpenGlInit(GlInterface gl)
    {
        if (_hasNewFrame)
        {
            _hasNewFrame = false;
            BindOutputTexToGl(gl); // 在GL线程里绑定
        }

        const int EGL_DEVICE_EXT = 0x322C;
        const int EGL_D3D11_DEVICE_ANGLE = 0x33A1;

        // 获取所有需要的函数指针
        var fnGetDisplay = GetDelegate<EglGetCurrentDisplay>(gl, "eglGetCurrentDisplay");
        var fnQueryDisplay = GetDelegate<EglQueryDisplayAttrib>(gl, "eglQueryDisplayAttribEXT");
        var fnQueryDevice = GetDelegate<EglQueryDeviceAttrib>(gl, "eglQueryDeviceAttribEXT");

        if (fnGetDisplay == null || fnQueryDisplay == null || fnQueryDevice == null)
        {
            Console.WriteLine("ERROR: 缺少必要的EGL扩展函数");
            return;
        }

        var display = fnGetDisplay();
        Console.WriteLine($"EGL Display: {display}");

        if (!fnQueryDisplay(display, EGL_DEVICE_EXT, out var eglDevice))
        {
            Console.WriteLine("ERROR: eglQueryDisplayAttribEXT 失败");
            return;
        }
        Console.WriteLine($"EGL Device: {eglDevice}");

        if (!fnQueryDevice(eglDevice, EGL_D3D11_DEVICE_ANGLE, out var d3d11DevicePtr))
        {
            Console.WriteLine("ERROR: eglQueryDeviceAttribEXT 失败");
            return;
        }
        Console.WriteLine($"SUCCESS: D3D11Device ptr = {d3d11DevicePtr}");
        D3D11DevicePtr = d3d11DevicePtr;

        // 用Vortice包装指针
        var device = new Vortice.Direct3D11.ID3D11Device(d3d11DevicePtr);

        _device = device;
        _context = device.ImmediateContext;
        _videoDevice = _device.QueryInterface<Vortice.Direct3D11.ID3D11VideoDevice>();
        _videoContext = _context.QueryInterface<Vortice.Direct3D11.ID3D11VideoContext>();


        InitVideoProcessor();


        // 获取EGL函数
        _eglCreateImage = GetDelegate<EglCreateImageDelegate>(gl, "eglCreateImageKHR");
        _eglDestroyImage = GetDelegate<EglDestroyImageDelegate>(gl, "eglDestroyImageKHR");
        _glEGLImageTargetTexture2D = GetDelegate<GlEGLImageTargetTexture2DDelegate>(gl, "glEGLImageTargetTexture2DOES");

        // 获取EGL display
        // var fnGetDisplay = GetDelegate<EglGetCurrentDisplay>(gl, "eglGetCurrentDisplay");
        // _eglDisplay = fnGetDisplay();

        _gl = gl;

        _eglDisplay = display;

        // 创建GL纹理
        unsafe
        {
            int texture = 0;
            gl.GenTextures(1, &texture);
            _glTexture = (uint)texture;
        }

        Console.WriteLine($"EGL函数获取: CreateImage={_eglCreateImage != null}, DestroyImage={_eglDestroyImage != null}, TargetTexture={_glEGLImageTargetTexture2D != null}");


        // 获取EGL扩展列表
        var fnQueryString = GetDelegate<EglQueryStringDelegate>(gl, "eglQueryString");
        if (fnQueryString != null)
        {
            var extensions = Marshal.PtrToStringAnsi(fnQueryString(_eglDisplay, 0x3055)); // EGL_EXTENSIONS
            Console.WriteLine($"EGL Extensions: {extensions}");
        }

        Console.WriteLine($"Thread id = {Environment.CurrentManagedThreadId}");

        // 测试：在OnOpenGlInit里直接调用eglGetCurrentContext
        var fnGetContext = GetDelegate<EglGetCurrentContextDelegate>(gl, "eglGetCurrentContext");
        var ctx = fnGetContext!();
        Console.WriteLine($"OnOpenGlInit中的EGL Context: {ctx}");

        // 同时测试eglGetCurrentDisplay
        Console.WriteLine($"OnOpenGlInit中的EGL Display: {display}");


        var clientApis = Marshal.PtrToStringAnsi(fnQueryString!(_eglDisplay, 0x308D)); // EGL_CLIENT_APIS
        Console.WriteLine($"Client APIs: {clientApis}");

        InitShader(gl);
    }

    void InitVideoProcessor()
    {
        var desc = new Vortice.Direct3D11.VideoProcessorContentDescription
        {
            Usage = VideoUsage.PlaybackNormal,
            InputFrameFormat = VideoFrameFormat.Progressive,
            InputWidth = (uint)Width,
            InputHeight = (uint)Height,
            OutputWidth = (uint)Width,
            OutputHeight = (uint)Height,
            InputFrameRate = new Rational(60, 1),
            OutputFrameRate = new Rational(60, 1),
        };
        _videoDevice!.CreateVideoProcessorEnumerator(desc, out _enumerator).CheckError();
        _videoDevice.CreateVideoProcessor(_enumerator, 0, out _processor).CheckError();

        Console.WriteLine("VideoProcessor初始化成功");

        var texDesc = new Texture2DDescription
        {
            Width = Width,
            Height = Height,
            MipLevels = 1,
            ArraySize = 1,
            Format = Format.R8G8B8A8_UNorm,
            SampleDescription = new SampleDescription(1, 0),
            Usage = ResourceUsage.Default,
            BindFlags = BindFlags.ShaderResource | BindFlags.RenderTarget,
            CPUAccessFlags = CpuAccessFlags.None,
            MiscFlags = ResourceOptionFlags.Shared | ResourceOptionFlags.SharedNTHandle,
        };
        _outputTex = _device!.CreateTexture2D(texDesc);

        // 创建OutputView
        var ovDesc = new VideoProcessorOutputViewDescription
        {
            ViewDimension = VideoProcessorOutputViewDimension.Texture2D,
            Texture2D = new Texture2DVideoProcessorOutputView { MipSlice = 0 }
        };
        _videoDevice.CreateVideoProcessorOutputView(
            _outputTex, _enumerator, ovDesc, out _outputView).CheckError();

        Console.WriteLine("输出纹理和OutputView创建成功");

        Console.WriteLine($"OutputTex MiscFlags: {_outputTex.Description.MiscFlags}");

        // Console.WriteLine
    }

    protected override void OnOpenGlRender(GlInterface gl, int framebuffer)
    {
        if (_hasNewFrame)
        {
            _hasNewFrame = false;
            BindOutputTexToGl(gl);
        }

        gl.BindFramebuffer(GlConsts.GL_FRAMEBUFFER, framebuffer);
        gl.Viewport(0, 0, (int)Bounds.Width, (int)Bounds.Height);
        gl.ClearColor(0f, 0f, 0f, 1f);
        gl.Clear(GlConsts.GL_COLOR_BUFFER_BIT);

        if (_glTexture == 0 || _shaderProgram == 0) return;

        gl.UseProgram(_shaderProgram);
        CheckGlError(gl, "UseProgram");

        gl.ActiveTexture(GlConsts.GL_TEXTURE0);
        gl.BindTexture((int)GL_TEXTURE_2D, (int)_glTexture);
        CheckGlError(gl, "BindTexture");

        unsafe
        {
            var nameBytes = System.Text.Encoding.UTF8.GetBytes("uTexture\0");
            fixed (byte* namePtr = nameBytes)
            {
                var loc = gl.GetUniformLocation(_shaderProgram, (IntPtr)namePtr);
                Console.WriteLine($"uniform loc: {loc}");
                gl.Uniform1i(loc, 0);
            }
        }
        CheckGlError(gl, "Uniform");

        gl.BindVertexArray(_vao);
        CheckGlError(gl, "BindVertexArray");

        gl.DrawArrays(GlConsts.GL_TRIANGLES, 0, 6);
        CheckGlError(gl, "DrawArrays");

        gl.BindVertexArray(0);
        gl.UseProgram(0);

        return;

        Console.WriteLine($"OnOpenGlRender: hasNewFrame={_hasNewFrame}, glTexture={_glTexture}");
        _currentFramebuffer = framebuffer;
        if (_hasNewFrame)
        {
            _hasNewFrame = false;
            BindOutputTexToGl(gl); // 在GL线程里绑定
        }

        RenderTexture(gl);

        // 渲染纹理到屏幕
        // RenderTexture(gl);
        // base.OnOpenGlRender(gl, framebuffer);
        // 暂时只清屏，确认EGLImage创建成功后再写渲染
        // gl.ClearColor(0f, 0f, 1f, 1f);
        // gl.Clear(GlConsts.GL_COLOR_BUFFER_BIT);
    }

    public unsafe void ProcessAndVerifyFrame(AVFrame* frame)
    {
        // 从AVFrame提取D3D11纹理指针
        var decoderTexPtr = (IntPtr)frame->data[0];
        var arrayIndex = (int)(nint)frame->data[1];
        Console.WriteLine($"DECODER纹理ptr: {decoderTexPtr}, ArrayIndex: {arrayIndex}");

        var decoderTex = new Vortice.Direct3D11.ID3D11Texture2D(decoderTexPtr);

        // 诊断：打印DECODER纹理的实际描述
        var desc = decoderTex.Description;
        Console.WriteLine($"DECODER纹理信息:");
        Console.WriteLine($"  Format: {desc.Format}");
        Console.WriteLine($"  Width: {desc.Width}, Height: {desc.Height}");
        Console.WriteLine($"  ArraySize: {desc.ArraySize}");
        Console.WriteLine($"  BindFlags: {desc.BindFlags}");
        Console.WriteLine($"  MiscFlags: {desc.MiscFlags}");
        Console.WriteLine($"  Usage: {desc.Usage}");
        Console.WriteLine($"  ArrayIndex传入值: {arrayIndex}");

        // 诊断：确认ArrayIndex不越界
        if (arrayIndex >= desc.ArraySize)
        {
            Console.WriteLine($"ERROR: ArrayIndex({arrayIndex}) >= ArraySize({desc.ArraySize})");
            return;
        }

        // 诊断：确认VideoProcessorEnumerator与纹理尺寸匹配
        Console.WriteLine($"VideoProcessor初始化时的尺寸: {Width}x{Height}");
        Console.WriteLine($"实际纹理尺寸: {desc.Width}x{desc.Height}");

        // 创建InputView
        var ivDesc = new VideoProcessorInputViewDescription
        {
            FourCC = 0x3231564E,
            ViewDimension = VideoProcessorInputViewDimension.Texture2D,
            Texture2D = new Texture2DVideoProcessorInputView
            {
                MipSlice = 0,
                ArraySlice = (uint)arrayIndex  // 这里对应D3D11的ArrayIndex
            }
        };

        // 不存在GetVideoProcessorCaps方法
        // 打印enumerator的完整能力
        // _videoDevice.GetVideoProcessorCaps(_enumerator, out var caps);
        // Console.WriteLine($"VideoProcessorCaps:");
        // Console.WriteLine($"  DeviceCaps: {caps.DeviceCaps}");
        // Console.WriteLine($"  FeatureCaps: {caps.FeatureCaps}");
        // Console.WriteLine($"  InputFormatCaps: {caps.InputFormatCaps}");
        // Console.WriteLine($"  MaxInputStreams: {caps.MaxInputStreams}");

        // 同时打印ivDesc的完整内容确认
        Console.WriteLine($"ivDesc:");
        Console.WriteLine($"  FourCC: {ivDesc.FourCC}");
        Console.WriteLine($"  ViewDimension: {ivDesc.ViewDimension}");
        Console.WriteLine($"  MipSlice: {ivDesc.Texture2D.MipSlice}");
        Console.WriteLine($"  ArraySlice: {ivDesc.Texture2D.ArraySlice}");

        _videoDevice!.CreateVideoProcessorInputView(
            decoderTex, _enumerator, ivDesc, out var inputView).CheckError();

        // 告诉VideoProcessor输出RGBA而不是BGRA
        _videoContext!.VideoProcessorSetOutputColorSpace(_processor, new VideoProcessorColorSpace
        {
            Usage = 0,
            RGB_Range = 1,
            YCbCr_Matrix = 0,
            YCbCr_xvYCC = 0,
            Nominal_Range = 1,
        });


        // 配置stream
        var stream = new VideoProcessorStream
        {
            Enable = true,
            InputSurface = inputView
        };

        // 执行转换
        _videoContext!.VideoProcessorBlt(
            _processor, _outputView, 0, 1, [stream]).CheckError();

        Console.WriteLine("VideoProcessorBlt执行成功");
        inputView.Dispose();

        // CPU回读验证像素
        // VerifyOutputPixels();

        // BindOutputTexToGl(_gl!);

        // VideoProcessorBlt完成后，标记有新帧
        _hasNewFrame = true;
        RequestNextFrameRendering(); // 触发OnOpenGlRender

    }

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    delegate int EglGetErrorDelegate();

    [DllImport("kernel32.dll")]
    static extern bool CloseHandle(IntPtr handle);

    void BindOutputTexToGl(GlInterface gl)
    {

        Console.WriteLine($"Thread id = {Environment.CurrentManagedThreadId}");
        // 确认context
        var fnGetContext = GetDelegate<EglGetCurrentContextDelegate>(gl, "eglGetCurrentContext");
        var fnGetError = GetDelegate<EglGetErrorDelegate>(gl, "eglGetError");
        var eglContext = fnGetContext!();
        Console.WriteLine($"EGL Context: {eglContext}");
        fnGetError!(); // 清除错误

        const uint EGL_D3D11_TEXTURE_ANGLE = 0x3484;
        const uint EGL_D3D11_TEXTURE_ARRAY_SLICE_ANGLE = 0x3493;  // 修正：0x3491 -> 0x3493
        const uint EGL_NONE = 0x3038;

        int[] attribs =
        [
            (int)EGL_D3D11_TEXTURE_ARRAY_SLICE_ANGLE, 0,
            (int)EGL_NONE
        ];

        var eglImage = _eglCreateImage!(
            _eglDisplay,
            IntPtr.Zero,           // EGL_NO_CONTEXT
            EGL_D3D11_TEXTURE_ANGLE,
            _outputTex!.NativePointer,
            attribs);

        var error = fnGetError();
        Console.WriteLine($"EGLImage: {eglImage}, EGL Error: 0x{error:X4}");

        if (eglImage == IntPtr.Zero) return;

        unsafe
        {
            gl.BindTexture((int)GL_TEXTURE_2D, (int)_glTexture);
            _glEGLImageTargetTexture2D!(GL_TEXTURE_2D, eglImage);
        }
        _eglDestroyImage!(_eglDisplay, eglImage);
        Console.WriteLine("纹理绑定成功");

        gl.BindTexture((int)GL_TEXTURE_2D, (int)_glTexture);
        _glEGLImageTargetTexture2D!(GL_TEXTURE_2D, eglImage);
        _eglDestroyImage!(_eglDisplay, eglImage);

        // 设置纹理参数，EGLImage绑定后必须设置
        gl.TexParameteri((int)GL_TEXTURE_2D, GlConsts.GL_TEXTURE_MIN_FILTER, GlConsts.GL_LINEAR);
        gl.TexParameteri((int)GL_TEXTURE_2D, GlConsts.GL_TEXTURE_MAG_FILTER, GlConsts.GL_LINEAR);
        gl.TexParameteri((int)GL_TEXTURE_2D, 0x2802, 0x812F);
        gl.TexParameteri((int)GL_TEXTURE_2D, 0x2803, 0x812F);

        var err = gl.GetError();
        Console.WriteLine($"BindOutputTexToGl GL Error: 0x{err:X4}");
    }

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    delegate IntPtr EglGetCurrentContextDelegate();

    void VerifyOutputPixels()
    {
        // 创建Staging纹理
        var stagingDesc = new Texture2DDescription
        {
            Width = Width,
            Height = Height,
            MipLevels = 1,
            ArraySize = 1,
            Format = Format.B8G8R8A8_UNorm,
            SampleDescription = new SampleDescription(1, 0),
            Usage = ResourceUsage.Staging,
            BindFlags = BindFlags.None,
            CPUAccessFlags = CpuAccessFlags.Read,
            MiscFlags = ResourceOptionFlags.None,
        };
        var staging = _device!.CreateTexture2D(stagingDesc);
        _context!.CopyResource(staging, _outputTex);

        var mapped = _context.Map(staging, 0, MapMode.Read, Vortice.Direct3D11.MapFlags.None);
        // 读取左上角4个像素
        var pixels = new byte[16];
        Marshal.Copy(mapped.DataPointer, pixels, 0, 16);
        _context.Unmap(staging, 0);
        staging.Dispose();

        Console.WriteLine("左上角像素 (BGRA):");
        for (int i = 0; i < 4; i++)
            Console.WriteLine($"  像素{i}: B={pixels[i * 4]}, G={pixels[i * 4 + 1]}, R={pixels[i * 4 + 2]}, A={pixels[i * 4 + 3]}");
    }

    int _shaderProgram;
    int _vao;
    int _vbo;

    void InitShader(GlInterface gl)
    {
        // 顶点shader：全屏四边形
        // string vertSrc = """
        //     #version 300 es
        //     precision mediump float;
        //     layout(location = 0) in vec2 aPos;
        //     layout(location = 1) in vec2 aTexCoord;
        //     out vec2 vTexCoord;
        //     void main()
        //     {
        //         gl_Position = vec4(aPos, 0.0, 1.0);
        //         vTexCoord = aTexCoord;
        //     }
        // """;

        // // 片段shader：采样纹理
        // string fragSrc = """
        // #version 300 es
        // precision mediump float;
        // in vec2 vTexCoord;
        // uniform sampler2D uTexture;
        // out vec4 fragColor;
        // void main()
        // {
        //     fragColor = texture(uTexture, vTexCoord);
        // }
        // """;

        string vertSrc = """
    #version 300 es
    precision mediump float;
    out vec2 vTexCoord;
    void main()
    {
        vec2 pos[6];
        pos[0] = vec2(-1.0,  1.0);
        pos[1] = vec2(-1.0, -1.0);
        pos[2] = vec2( 1.0, -1.0);
        pos[3] = vec2(-1.0,  1.0);
        pos[4] = vec2( 1.0, -1.0);
        pos[5] = vec2( 1.0,  1.0);

        vec2 tex[6];
        tex[0] = vec2(0.0, 0.0);
        tex[1] = vec2(0.0, 1.0);
        tex[2] = vec2(1.0, 1.0);
        tex[3] = vec2(0.0, 0.0);
        tex[4] = vec2(1.0, 1.0);
        tex[5] = vec2(1.0, 0.0);

        gl_Position = vec4(pos[gl_VertexID], 0.0, 1.0);
        vTexCoord = tex[gl_VertexID];
    }
    """;

        string fragSrc = """
    #version 300 es
    precision mediump float;
    in vec2 vTexCoord;
    uniform sampler2D uTexture;
    out vec4 fragColor;
    void main()
    {
        fragColor = texture(uTexture, vTexCoord);
    }
    """;

        unsafe
        {
            var vert = gl.CreateShader(GlConsts.GL_VERTEX_SHADER);
            var vertBytes = System.Text.Encoding.UTF8.GetBytes(vertSrc + "\0");
            fixed (byte* ptr = vertBytes)
            {
                var strPtr = (IntPtr)ptr;
                int len = vertBytes.Length;
                gl.ShaderSource(vert, 1, new IntPtr(&strPtr), new IntPtr(&len));
            }
            gl.CompileShader(vert);

            // GetShaderInfoLog
            byte[] logBuf = new byte[1024];
            fixed (byte* logPtr = logBuf)
            {
                gl.GetShaderInfoLog(vert, 1024, out int logLen, logPtr);
                Console.WriteLine($"Vert log: {System.Text.Encoding.UTF8.GetString(logBuf, 0, logLen)}");
            }

            var frag = gl.CreateShader(GlConsts.GL_FRAGMENT_SHADER);
            var fragBytes = System.Text.Encoding.UTF8.GetBytes(fragSrc + "\0");
            fixed (byte* ptr = fragBytes)
            {
                var strPtr = (IntPtr)ptr;
                int len = fragBytes.Length;
                gl.ShaderSource(frag, 1, new IntPtr(&strPtr), new IntPtr(&len));
            }
            gl.CompileShader(frag);
            fixed (byte* logPtr = logBuf)
            {
                gl.GetShaderInfoLog(frag, 1024, out int logLen, logPtr);
                Console.WriteLine($"Frag log: {System.Text.Encoding.UTF8.GetString(logBuf, 0, logLen)}");
            }

            _shaderProgram = gl.CreateProgram();
            gl.AttachShader(_shaderProgram, vert);
            gl.AttachShader(_shaderProgram, frag);
            gl.LinkProgram(_shaderProgram);
            fixed (byte* logPtr = logBuf)
            {
                gl.GetProgramInfoLog(_shaderProgram, 1024, out int logLen, logPtr);
                Console.WriteLine($"Program log: {System.Text.Encoding.UTF8.GetString(logBuf, 0, logLen)}");
            }

            gl.DeleteShader(vert);
            gl.DeleteShader(frag);
        }
        // 全屏四边形顶点数据：position(2) + texcoord(2)
        float[] vertices =
        [
            -1f,  1f,  0f, 0f,  // 左上
        -1f, -1f,  0f, 1f,  // 左下
         1f, -1f,  1f, 1f,  // 右下
        -1f,  1f,  0f, 0f,  // 左上
         1f, -1f,  1f, 1f,  // 右下
         1f,  1f,  1f, 0f,  // 右上
    ];

        // 创建VAO和VBO
        unsafe
        {
            int vao, vbo;
            gl.GenVertexArrays(1, &vao);
            gl.GenBuffers(1, &vbo);
            _vao = vao;
            _vbo = vbo;
            Console.WriteLine($"-------------------VAO: {_vao}, VBO: {_vbo}");
        }

        gl.BindVertexArray(_vao);
        gl.BindBuffer(GlConsts.GL_ARRAY_BUFFER, _vbo);

        unsafe
        {
            fixed (float* ptr = vertices)
            {
                gl.BufferData(GlConsts.GL_ARRAY_BUFFER,
                    new IntPtr(vertices.Length * sizeof(float)),
                    (IntPtr)ptr,
                    GlConsts.GL_STATIC_DRAW);
            }
        }

        // position attribute
        gl.VertexAttribPointer(0, 2, GlConsts.GL_FLOAT, 0, 4 * sizeof(float), IntPtr.Zero);
        gl.EnableVertexAttribArray(0);

        // texcoord attribute
        gl.VertexAttribPointer(1, 2, GlConsts.GL_FLOAT, 0, 4 * sizeof(float), new IntPtr(2 * sizeof(float)));
        gl.EnableVertexAttribArray(1);

        // int bufferSize = 0;
        // unsafe
        // {
        //     gl.GetBufferParameteriv(GlConsts.GL_ARRAY_BUFFER, GlConsts.GL_BUFFER_SIZE, &bufferSize);
        // }
        // Console.WriteLine($"VBO size: {bufferSize}"); // 应该是 6*4*4 = 96
        // Console.WriteLine($"VAO: {_vao}, VBO: {_vbo}, Program: {_shaderProgram}");

        gl.BindVertexArray(0);
    }

    void CheckGlError(GlInterface gl, string location)
    {
        var err = gl.GetError();
        if (err != 0)
            Console.WriteLine($"GL Error at {location}: 0x{err:X4}");
    }

    void RenderTexture(GlInterface gl)
    {
        if (_glTexture == 0)
        {
            Console.WriteLine("RenderTexture: _glTexture为0，跳过");
            return;
        }

        gl.BindFramebuffer(GlConsts.GL_FRAMEBUFFER, _currentFramebuffer);
        gl.Viewport(0, 0, (int)Bounds.Width, (int)Bounds.Height);
        gl.ClearColor(0f, 0f, 0f, 1f);
        gl.Clear(GlConsts.GL_COLOR_BUFFER_BIT);

        if (_glTexture == 0 || _shaderProgram == 0) return;

        gl.UseProgram(_shaderProgram);
        gl.ActiveTexture(GlConsts.GL_TEXTURE0);
        gl.BindTexture((int)GL_TEXTURE_2D, (int)_glTexture);

        unsafe
        {
            var nameBytes = System.Text.Encoding.UTF8.GetBytes("uTexture\0");
            fixed (byte* namePtr = nameBytes)
            {
                var loc = gl.GetUniformLocation(_shaderProgram, (IntPtr)namePtr);
                gl.Uniform1i(loc, 0);
            }
        }

        gl.DrawArrays(GlConsts.GL_TRIANGLES, 0, 6);

        var err = gl.GetError();
        if (err != 0)
            Console.WriteLine($"GL Error: 0x{err:X4}");

        return;

        // 绑定Avalonia的FBO而不是默认FBO(0)
        gl.BindFramebuffer(GlConsts.GL_FRAMEBUFFER, _currentFramebuffer);

        gl.ClearColor(0f, 0f, 0f, 1f);
        gl.Clear(GlConsts.GL_COLOR_BUFFER_BIT);
        gl.UseProgram(_shaderProgram);
        gl.ActiveTexture(GlConsts.GL_TEXTURE0);
        gl.BindTexture((int)GL_TEXTURE_2D, (int)_glTexture);

        unsafe
        {
            var nameBytes = System.Text.Encoding.UTF8.GetBytes("uTexture\0");
            fixed (byte* namePtr = nameBytes)
            {
                var loc = gl.GetUniformLocation(_shaderProgram, (IntPtr)namePtr);
                gl.Uniform1i(loc, 0);
            }
        }

        gl.BindVertexArray(_vao);
        gl.DrawArrays(GlConsts.GL_TRIANGLES, 0, 6);
        gl.BindVertexArray(0);
        Console.WriteLine($"RenderTexture完成: program={_shaderProgram}, vao={_vao}, tex={_glTexture}");

    }




    [UnmanagedFunctionPointer(CallingConvention.StdCall)]
    delegate int GetFeatureLevelDelegate(IntPtr device);

    // 委托定义
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    delegate IntPtr EglGetCurrentDisplay();

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    delegate bool EglQueryDisplayAttrib(IntPtr display, int attribute, out IntPtr value);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    delegate bool EglQueryDeviceAttrib(IntPtr device, int attribute, out IntPtr value);

    // 辅助方法
    T? GetDelegate<T>(GlInterface gl, string name) where T : Delegate
    {
        var ptr = gl.GetProcAddress(name);
        if (ptr == IntPtr.Zero) return null;
        return Marshal.GetDelegateForFunctionPointer<T>(ptr);
    }

    // protected override void OnOpenGlInit(GlInterface gl)
    // {
    //     base.OnOpenGlInit(gl);

    //     var eglGetCurrentDisplayPtr = gl.GetProcAddress("eglGetCurrentDisplay");
    //     Console.WriteLine($"eglGetCurrentDisplay ptr: {eglGetCurrentDisplayPtr}");
    //     var eglGetCurrentDisplay = Marshal.GetDelegateForFunctionPointer<EglGetCurrentDisplayDelegate>(eglGetCurrentDisplayPtr);
    //     var display = eglGetCurrentDisplay();
    //     Console.WriteLine($"EGL Display: {display}");
    // }
}