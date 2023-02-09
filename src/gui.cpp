#include <imgui_vita.h>
#include <stdio.h>
#include <vita2d.h>
#include "windows.h"
#include "gui.h"
#include "textures.h"

bool done = false;
int gui_mode = GUI_MODE_BROWSER;

namespace GUI {
	int RenderLoop(void) 
	{
		ImGuiIO& io = ImGui::GetIO(); (void)io;

		Windows::Init();
		while (!done) {
			if (gui_mode == GUI_MODE_BROWSER)
			{
				ImGui_ImplVitaGL_NewFrame();
				Windows::HandleWindowInput();
				Windows::MainWindow();
				Windows::ExecuteActions();
				glViewport(0, 0, static_cast<int>(ImGui::GetIO().DisplaySize.x), static_cast<int>(ImGui::GetIO().DisplaySize.y));
				ImGui::Render();
				ImGui_ImplVitaGL_RenderDrawData(ImGui::GetDrawData());
				vglSwapBuffers(GL_FALSE);
			}
			else if (gui_mode == GUI_MODE_IME)
			{
				Windows::HandleImeInput();
			}
		}
		
		return 0;
	}
}
