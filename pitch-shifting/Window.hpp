#pragma once
/* NOTE: the original Windows class source code was duplicated from Iolive project */

#define GLFW_EXPOSE_NATIVE_WIN32
/* for 2D texture */
//#include <GL/glew.h>
#include <GLFW/glfw3.h>
#ifdef _WIN32
#include <GLFW/glfw3native.h>
#endif

/* require opengl dependencies from operation system */
#pragma comment(lib, "opengl32.lib")
//#pragma comment(lib, "glu32.lib") // from old school style, the glew is enough for 2d texture

// libglew32 lib was from Iolive project, probably built its own by cmake, offical lib is glew32(s).lib
/* require GLEW_STATIC */
//#ifdef _DEBUG
//#pragma comment(lib, "./Debug/libglew32d.lib")
//#else
//#pragma comment(lib, "./Release/libglew32.lib")
//#endif

#ifdef _DEBUG
#pragma comment(lib, "glfw3dll.lib")
#pragma comment(lib, "glfw3.lib")
#else
//#pragma comment(lib, "glew32.lib") // offical GLEW lib, has been used by libglew32.lib
#pragma comment(lib, "glfw3_mt.lib")
#endif

#include <mutex>

namespace GLUI {
	class Window {
	private:
		/* inline static keywords combination require c++17 */
		inline static Window* s_Window = nullptr; // static this instance
		inline static std::mutex s_MtxGetInstance;
	public:
		void Destroy();

		static Window* Create(const char* title, int width, int height);
		static Window* Get();

		/*
		* poll window events and prepare frame context
		* \return bool isWindowShouldClose?
		*/
		bool PrepareFrame();
		/* update frame to graphic output */
		void SwapWindow();

		void SetWindowOpacity(float value);

		GLFWwindow* GetGlfwWindow();
		void GetWindowSize(int* width, int* height);
		double GetDeltaTime() const;

	private:
		Window(const char* title, int width, int height);

	public:
		/*
		* callback for Window basic operation (partial by setting GLFWwindow callbacks)
		*/
		/* when user raise close window event, mainly from caption top-right close button */
		void(*OnWindowClosing)(Window* window) = nullptr;
		/* minor callbacks for playground */
		void(*OnFrameResizedCallback)(int width, int height) = nullptr;
		void(*OnScrollCallback)(double xoffset, double yoffset) = nullptr;
		void(*OnCursorPosCallback)(bool pressed, double xpos, double ypos) = nullptr;

		void(*OnRenderFrame)(Window* window) = nullptr;

		float MaxFPS = 60.0f;

	private:
		GLFWwindow* m_GlfwWindow = nullptr;

		double m_CurrentFrame = 0.0;
		double m_LastFrame = 0.0;
		double m_DeltaTime = 0.02;	//seconds
	};
}
