#include "../include/GameCore.hpp"

#include "SDL3/SDL_init.h"

#include <chrono>
#include <iostream>

namespace GameCore
{
	SDL_Window* g_Window = nullptr;

	int RunApplication(Game& game, const char* commandLineArgs)
	{
		// 1. Initialize SDL
		if (!SDL_Init(SDL_INIT_VIDEO))
		{
			std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
			return -1;
		}

		// 2. Create Window
		// Convert wstring name to string for SDL
		std::wstring wName = game.GetName();
		std::string title(wName.begin(), wName.end());

		g_Window = SDL_CreateWindow(title.c_str(), game.GetWidth(), game.GetHeight(), SDL_WINDOW_RESIZABLE);
		if (!g_Window)
		{
			std::cerr << "SDL_CreateWindow Error: " << SDL_GetError() << std::endl;
			return -1;
		}

		// 3. App Startup
		game.Startup();

		// 4. Main Loop
		bool isRunning = true;
		auto t0 = std::chrono::high_resolution_clock::now();

		while (isRunning)
		{
			// --- Event Polling ---
			SDL_Event event;
			while (SDL_PollEvent(&event))
			{
				switch (event.type)
				{
				case SDL_EVENT_QUIT:
					isRunning = false;
					break;

				case SDL_EVENT_KEY_DOWN:
					if (event.key.key == SDLK_ESCAPE) isRunning = false;
					game.OnKeyDown(event.key.key);
					break;

				case SDL_EVENT_KEY_UP:
					game.OnKeyUp(event.key.key);
					break;

				case SDL_EVENT_WINDOW_RESIZED:
					game.OnResize(event.window.data1, event.window.data2);
					break;
				}
			}
			// --- Update Timer ---
			auto t1 = std::chrono::high_resolution_clock::now();
			float deltaTime = std::chrono::duration<float>(t1 - t0).count();
			t0 = t1;

			// --- Game Logic & Render ---
			game.Update(deltaTime);
			game.Render();
		}

		// 5. Cleanup
		game.Cleanup();

		SDL_DestroyWindow(g_Window);
		SDL_Quit();

		return 0;
	}

	void ToggleFullscreen()
	{
		static bool isFullscreen = false;
		isFullscreen = !isFullscreen;
		SDL_SetWindowFullscreen(g_Window, isFullscreen); 
	}
}