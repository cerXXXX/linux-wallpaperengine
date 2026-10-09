#include "GLFWOpenGLDriver.h"
#include "VideoFactories.h"
#include "WallpaperEngine/Logging/Log.h"
#include "WallpaperEngine/Render/Drivers/FrameHash.h"
#include "WallpaperEngine/Render/Drivers/Output/GLFWWindowOutput.h"
#ifdef ENABLE_X11
#include "WallpaperEngine/Render/Drivers/Output/X11Output.h"
#endif

#define GLFW_EXPOSE_NATIVE_X11
#include "WallpaperEngine/Debugging/CallStack.h"

#include <GLFW/glfw3native.h>

#include <cstdio>
#include <unistd.h>

using namespace WallpaperEngine::Render::Drivers;

void CustomGLFWErrorHandler (int errorCode, const char* reason) { sLog.error ("GLFW error ", errorCode, ": ", reason); }

GLFWOpenGLDriver::GLFWOpenGLDriver (const char* windowTitle, ApplicationContext& context, WallpaperApplication& app) :
    VideoDriver (app, m_mouseInput), m_context (context), m_mouseInput (*this) {
    glfwSetErrorCallback (CustomGLFWErrorHandler);

    // initialize glfw
    if (glfwInit () == GLFW_FALSE) {
	sLog.exception ("Failed to initialize glfw");
    }

    // set some window hints (opengl version to be used)
    glfwWindowHint (GLFW_SAMPLES, 4);
    glfwWindowHint (GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint (GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint (GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint (GLFW_VISIBLE, GLFW_FALSE);
    // set X11-specific hints
    glfwWindowHintString (GLFW_X11_CLASS_NAME, "linux-wallpaperengine");
    glfwWindowHintString (GLFW_X11_INSTANCE_NAME, "linux-wallpaperengine");

    // for forced window mode, we can set some hints that'll help position the window
    if (context.settings.render.mode == Application::ApplicationContext::EXPLICIT_WINDOW) {
	glfwWindowHint (GLFW_RESIZABLE, GLFW_FALSE);
	glfwWindowHint (GLFW_DECORATED, GLFW_FALSE);
	glfwWindowHint (GLFW_FLOATING, GLFW_TRUE);
    }

#if !NDEBUG
    glfwWindowHint (GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
#endif /* DEBUG */

    // create window, size doesn't matter as long as we don't show it. A streamed window is never shown, so its
    // framebuffer keeps the size it's created with
    glm::ivec2 windowSize = { 640, 480 };
    if (context.rendersOffscreen ()) {
	windowSize = { context.settings.render.window.geometry.z, context.settings.render.window.geometry.w };
    }
    this->m_window = glfwCreateWindow (windowSize.x, windowSize.y, windowTitle, nullptr, nullptr);

    if (this->m_window == nullptr) {
	sLog.exception ("Cannot create window");
    }

    // make context current, required for glew initialization
    glfwMakeContextCurrent (this->m_window);

    // initialize glew for rendering
    if (const GLenum result = glewInit (); result != GLEW_OK) {
	sLog.error ("Failed to initialize GLEW: ", glewGetErrorString (result));
    }

    if (context.rendersOffscreen ()) {
	if (context.settings.render.mode != ApplicationContext::EXPLICIT_WINDOW) {
	    sLog.exception ("--stream and --frame-file need --window to know the size to render at");
	}

	// the window is never shown, nothing must wait for it to be presented
	glfwSwapInterval (0);
    }

    if (!context.settings.render.streamTarget.empty ()) {
	this->m_stream = std::make_unique<FrameStream> (
	    context.settings.render.streamTarget, context.settings.render.maximumFPS
	);
    }

    // setup output
    if (context.settings.render.mode == ApplicationContext::EXPLICIT_WINDOW
	|| context.settings.render.mode == ApplicationContext::NORMAL_WINDOW) {
	m_output = new WallpaperEngine::Render::Drivers::Output::GLFWWindowOutput (context, *this);
    }
#ifdef ENABLE_X11
    else {
	m_output = new WallpaperEngine::Render::Drivers::Output::X11Output (context, *this);
    }
#else
    else {
	sLog.exception ("Trying to start GLFW in background mode without X11 support installed. Bailing out");
    }
#endif
}

GLFWOpenGLDriver::~GLFWOpenGLDriver () { glfwTerminate (); }

Output::Output& GLFWOpenGLDriver::getOutput () { return *this->m_output; }

float GLFWOpenGLDriver::getClockTime () const { return static_cast<float> (glfwGetTime ()); }

bool GLFWOpenGLDriver::closeRequested () { return glfwWindowShouldClose (this->m_window); }

void GLFWOpenGLDriver::resizeWindow (glm::ivec2 size) { glfwSetWindowSize (this->m_window, size.x, size.y); }

void GLFWOpenGLDriver::resizeWindow (glm::ivec4 sizeandpos) {
    glfwSetWindowPos (this->m_window, sizeandpos.x, sizeandpos.y);
    glfwSetWindowSize (this->m_window, sizeandpos.z, sizeandpos.w);
}

void GLFWOpenGLDriver::showWindow () { glfwShowWindow (this->m_window); }

void GLFWOpenGLDriver::hideWindow () { glfwHideWindow (this->m_window); }

glm::ivec2 GLFWOpenGLDriver::getFramebufferSize () const {
    glm::ivec2 size;

    glfwGetFramebufferSize (this->m_window, &size.x, &size.y);

    return size;
}

uint32_t GLFWOpenGLDriver::getFrameCounter () const { return this->m_frameCounter; }

void GLFWOpenGLDriver::dispatchEventQueue () {
    static float startTime, endTime;
    // the target rate can drop below --fps while only videos are shown, so re-read it every frame
    const float minimumTime = 1.0f / this->m_context.getTargetFrameRate ();
    // get the start time of the frame
    startTime = this->getClockTime ();
    // clear the screen
    glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    for (const auto& [screen, viewport] : this->m_output->getViewports ()) {
	this->getApp ().update (viewport);

	// nothing waits for a hidden window, a frame with content counts as shown (an --eco wallpaper is held
	// a moment after its first frame)
	if (viewport->swappingContent) {
	    this->getApp ().framePresented (viewport);
	}
    }

    // read the full texture into the image
    if (this->m_output->haveImageBuffer ()) {
	// 4.5 supports glReadnPixels, anything older doesn't...
	if (GLEW_VERSION_4_5) {
	    glReadnPixels (
		0, 0, this->m_output->getFullWidth (), this->m_output->getFullHeight (), GL_BGRA, GL_UNSIGNED_BYTE,
		this->m_output->getImageBufferSize (), this->m_output->getImageBuffer ()
	    );
	} else {
	    // fallback to old version
	    glReadPixels (
		0, 0, this->m_output->getFullWidth (), this->m_output->getFullHeight (), GL_BGRA, GL_UNSIGNED_BYTE,
		this->m_output->getImageBuffer ()
	    );
	}

	GLenum error = glGetError ();

	if (error != GL_NO_ERROR) {
	    sLog.exception ("OpenGL error when reading texture ", error);
	}
    }

    // TODO: FRAMETIME CONTROL SHOULD GO BACK TO THE CWALLPAPAERAPPLICATION ONCE ACTUAL PARTICLES ARE IMPLEMENTED
    // TODO: AS THOSE, MORE THAN LIKELY, WILL REQUIRE OF A DIFFERENT PROCESSING RATE
    // update the output with the given image
    this->m_output->updateRender ();

    if (this->m_stream != nullptr) {
	// hidden window: read the frame back from the back buffer, there's nothing to present
	this->m_stream->pushFrame (this->m_output->getFullWidth (), this->m_output->getFullHeight ());
    } else {
	// do buffer swapping first
	glfwSwapBuffers (this->m_window);
    }
    // poll for events
    glfwPollEvents ();
    // increase frame counter
    this->m_frameCounter++;
    // get the end time of the frame
    endTime = this->getClockTime ();

    // ensure the frame time is correct to not overrun FPS
    if ((endTime - startTime) < minimumTime) {
	usleep ((minimumTime - (endTime - startTime)) * CLOCKS_PER_SEC);
    }
}

void GLFWOpenGLDriver::renderHeldFrame () {
    // a new frame number, so the scene is rendered again instead of reusing the last frame
    this->m_frameCounter++;

    glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    for (const auto& [screen, viewport] : this->m_output->getViewports ()) {
	this->getApp ().update (viewport);
    }

    const glm::ivec4 area = { 0, 0, this->m_output->getFullWidth (), this->m_output->getFullHeight () };
    const uint64_t hash = hashFramebuffer (0, area, this->m_heldPixels);

    if (hash != this->m_heldFrameHash) {
	this->m_heldFrameHash = hash;

	if (!this->m_context.settings.render.frameFile.empty ()) {
	    this->writeFrameFile ();
	} else if (this->m_stream != nullptr) {
	    this->m_stream->pushFrame (area.z, area.w);
	} else {
	    glfwSwapBuffers (this->m_window);
	}
    }

    glfwPollEvents ();
}

void GLFWOpenGLDriver::forgetHeldFrames () { this->m_heldFrameHash = 0; }

void GLFWOpenGLDriver::writeFrameFile () const {
    const int width = this->m_output->getFullWidth ();
    const int height = this->m_output->getFullHeight ();
    const std::string& path = this->m_context.settings.render.frameFile;
    const std::string temporary = path + ".tmp";

    // RGBA bottom row first -> RGB top row first
    std::vector<uint8_t> rgb (static_cast<size_t> (width) * height * 3);

    for (int y = 0; y < height; y++) {
	const uint8_t* from = &this->m_heldPixels[static_cast<size_t> (height - 1 - y) * width * 4];
	uint8_t* to = &rgb[static_cast<size_t> (y) * width * 3];

	for (int x = 0; x < width; x++, from += 4, to += 3) {
	    to[0] = from[0];
	    to[1] = from[1];
	    to[2] = from[2];
	}
    }

    FILE* file = fopen (temporary.c_str (), "wb");

    if (file == nullptr) {
	sLog.error ("Cannot write the frame to ", temporary);
	return;
    }

    fprintf (file, "P6\n%d %d\n255\n", width, height);
    const bool written = fwrite (rgb.data (), 1, rgb.size (), file) == rgb.size ();
    fclose (file);

    // readers never see half a frame
    if (!written || rename (temporary.c_str (), path.c_str ()) != 0) {
	sLog.error ("Cannot write the frame to ", path);
	return;
    }

    sLog.out ("Frame written");
}

void* GLFWOpenGLDriver::getProcAddress (const char* name) const {
    return reinterpret_cast<void*> (glfwGetProcAddress (name));
}

GLFWwindow* GLFWOpenGLDriver::getWindow () const { return this->m_window; }

__attribute__ ((constructor)) void registerGLFWOpenGLDriver () {
    sVideoFactories.registerDriver (
	ApplicationContext::DESKTOP_BACKGROUND, "x11",
	[] (ApplicationContext& context, WallpaperApplication& application) -> std::unique_ptr<VideoDriver> {
	    return std::make_unique<GLFWOpenGLDriver> ("wallpaperengine", context, application);
	}
    );
    sVideoFactories.registerDriver (
	ApplicationContext::EXPLICIT_WINDOW, DEFAULT_WINDOW_NAME,
	[] (ApplicationContext& context, WallpaperApplication& application) -> std::unique_ptr<VideoDriver> {
	    return std::make_unique<GLFWOpenGLDriver> ("wallpaperengine", context, application);
	}
    );
    sVideoFactories.registerDriver (
	ApplicationContext::NORMAL_WINDOW, DEFAULT_WINDOW_NAME,
	[] (ApplicationContext& context, WallpaperApplication& application) -> std::unique_ptr<VideoDriver> {
	    return std::make_unique<GLFWOpenGLDriver> ("wallpaperengine", context, application);
	}
    );
}
