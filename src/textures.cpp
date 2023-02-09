#include <vitaGL.h>
#include <imgui_vita.h>
#include <vita2d.h>
#include "textures.h"

GLuint connect_icon;
GLuint disconnect_icon;
GLuint search_icon;
GLuint refresh_icon;

GLuint folder_icon;
GLuint file_icon;
GLuint update_icon;

namespace Textures {
	
	void LoadFonts()
	{
		// Build and load the texture atlas into a texture
		uint32_t* pixels = NULL;
		int width, height;
		ImGuiIO& io = ImGui::GetIO();
		io.Fonts->AddFontFromFileTTF(
					"ux0:app/SMLA00001/Ubuntu-R.ttf",
					16.0f,
					0,
					io.Fonts->GetGlyphRangesDefault());
	}

	bool LoadImageFile(const std::string filename, GLuint *texture)
	{
		// Load from file
		vita2d_texture *image = vita2d_load_PNG_file(filename.c_str());
		if (image == NULL) {
				return false;
		}
		int width = vita2d_texture_get_width(image);
		int height = vita2d_texture_get_height(image);

		glGenTextures(1, texture);
		glBindTexture(GL_TEXTURE_2D, *texture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, vita2d_texture_get_datap(image));
		vita2d_free_texture(image);
		return true;
	}
	
	void Init(void) {
		Textures::LoadImageFile("ux0:app/WDVCLI001/icons/connect.png", &connect_icon);
		Textures::LoadImageFile("ux0:app/WDVCLI001/icons/disconnect.png", &disconnect_icon);
		Textures::LoadImageFile("ux0:app/WDVCLI001/icons/search.png", &search_icon);
		Textures::LoadImageFile("ux0:app/WDVCLI001/icons/refresh.png", &refresh_icon);
		Textures::LoadImageFile("ux0:app/WDVCLI001/icons/folder.png", &folder_icon);
		Textures::LoadImageFile("ux0:app/WDVCLI001/icons/file.png", &file_icon);
		Textures::LoadImageFile("ux0:app/WDVCLI001/icons/update.png", &update_icon);
	}

	void Exit(void) {
	}

	void Free(Tex *texture) {
		vita2d_free_texture(texture->id);
	}
	
}
