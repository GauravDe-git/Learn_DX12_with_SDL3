#pragma once

#include <SDL3/SDL_keyboard.h>

#include <string>
#include <memory>

class Game
{
public:
	Game(const std::wstring& name, int width, int height, bool vSync)
		: m_Name {name}, m_Width{width}, m_Height{height}, m_VSync{vSync}
	{ }
	virtual ~Game() { }

	// --- Interface (MiniEngine Style) ----
	virtual void Startup() = 0;
	virtual void Cleanup() = 0;
	
	virtual void Update(float deltaTime) = 0;
	virtual void Render() = 0;

	// Input handlers (MiniEngine polls input, but using callbacks here)
	virtual void OnKeyDown(SDL_Keycode key) {}
	virtual void OnKeyUp(SDL_Keycode key) {}
	virtual void OnResize(int width, int height) {}

	// Accessors
	int GetWidth() const { return m_Width; }
	int GetHeight() const { return m_Height; }
	const std::wstring& GetName() const { return m_Name; }
	bool IsVSync() const { return m_VSync; }

protected:
	// -- Window States -- //
	std::wstring m_Name;
	int m_Width;
	int m_Height;
	bool m_VSync;
};

namespace GameCore
{
	// The entry point that main.cpp will call
	int RunApplication(Game& game, const char* commandLineArgs);

	// Global access to the window (if needed by other systems)
	extern SDL_Window* g_Window;

	void ToggleFullscreen();
}