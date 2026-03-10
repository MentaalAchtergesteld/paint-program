#ifndef GUI_H
#define GUI_H

#include "raylib.h"

#define WIN_BG     (Color){ 212, 208, 200, 255 }
#define WIN_HI     (Color){ 255, 255, 255, 255 }
#define WIN_SHADOW (Color){ 128, 128, 128, 255 }
#define WIN_DARK   (Color){  64,  64,  64, 255 }

typedef struct {
	unsigned int hotItem;
	unsigned int activeItem;
} UIState;

extern UIState uiState;

void DrawRaisedPanel(Rectangle r);
void DrawSunkenPanel(Rectangle r);

bool GuiButtonCore(const char *idStr, Rectangle bounds, bool isActive);
bool GuiButtonTxt(const char *text, Rectangle bounds, bool isActive);
bool GuiButtonImg(const char *idStr, Rectangle bounds, Texture2D tex, Rectangle texSource, bool isActive);
bool GuiSlider(const char *idStr, Rectangle bounds, float *value, float min, float max);
bool GuiCheckbox(const char *idStr, Rectangle bounds, bool *value);
float GuiAccordeon(Rectangle bounds, int *active, const char **labels, int count, bool *isOpen);

#endif

#ifdef GUI_IMPL

UIState uiState = {0};

unsigned int HashString(const char *str) {
	unsigned int hash = 5381;
	int c;
	while ((c = *str++)) {
		hash = ((hash << 5) + hash) + c;
	}
	return hash;
}

bool ShouldActivate() {
	return uiState.activeItem == 0 && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
}

bool ShouldDeactivate(unsigned int id) {
	return uiState.activeItem == id && IsMouseButtonReleased(MOUSE_LEFT_BUTTON);
}

void DrawRaisedPanel(Rectangle r) {
	DrawRectangleRec(r, WIN_BG);
	DrawRectangle(r.x, r.y, r.width, 1, WIN_HI);                 
	DrawRectangle(r.x, r.y, 1, r.height, WIN_HI);                
	DrawRectangle(r.x, r.y + r.height - 1, r.width, 1, WIN_DARK);
	DrawRectangle(r.x + r.width - 1, r.y, 1, r.height, WIN_DARK);
	DrawRectangle(r.x + 1, r.y + r.height - 2, r.width - 2, 1, WIN_SHADOW); 
	DrawRectangle(r.x + r.width - 2, r.y + 1, 1, r.height - 2, WIN_SHADOW);
}

void DrawSunkenPanel(Rectangle r) {
    DrawRectangleRec(r, WIN_BG);
    DrawRectangle(r.x, r.y, r.width, 1, WIN_SHADOW);             
    DrawRectangle(r.x, r.y, 1, r.height, WIN_SHADOW);            
    DrawRectangle(r.x, r.y + r.height - 1, r.width, 1, WIN_HI);  
    DrawRectangle(r.x + r.width - 1, r.y, 1, r.height, WIN_HI);  
    DrawRectangle(r.x + 1, r.y + 1, r.width - 2, 1, WIN_DARK);
    DrawRectangle(r.x + 1, r.y + 1, 1, r.height - 2, WIN_DARK);
}

bool GuiButtonCore(const char *idStr, Rectangle bounds, bool isActive) {
	unsigned int id = HashString(idStr);
	Vector2 mousePos = GetMousePosition();
	bool isHovering = CheckCollisionPointRec(mousePos, bounds);
	bool isClicked = false;

	if (isHovering) {
		uiState.hotItem = id;
		if (ShouldActivate()) uiState.activeItem = id;
	}

	if (ShouldDeactivate(id)) {
		if (isHovering) isClicked = true;
		uiState.activeItem = 0;
	}
	
	bool isVisualDown = isActive || (uiState.activeItem == id && isHovering) || isClicked;
	if (isVisualDown) DrawSunkenPanel(bounds);
	else DrawRaisedPanel(bounds);

	return isClicked;
}

bool GuiButtonTxt(const char *text, Rectangle bounds, bool isActive) {
	bool clicked = GuiButtonCore(text, bounds, isActive);
int textWidth = MeasureText(text, 10);
	DrawText(text, bounds.x + (bounds.width - textWidth) / 2, bounds.y + (bounds.height-12), 10, BLACK);
	return clicked;
}

bool GuiButtonImg(const char *idStr, Rectangle bounds, Texture2D tex, Rectangle texSource, bool isActive) {
	bool clicked = GuiButtonCore(idStr, bounds, isActive);
	Vector2 iconPos = { bounds.x + (bounds.width - texSource.width)/2, bounds.y + (bounds.height - texSource.height)/2 };
	DrawTextureRec(tex, texSource, iconPos, WHITE);
	return clicked;
}

bool GuiSlider(const char *idStr, Rectangle bounds, float *value, float min, float max) {
	unsigned int id = HashString(idStr);
	bool changed = false;
	Vector2 mousePos = GetMousePosition();

	Rectangle track = { bounds.x, bounds.y, bounds.width, 8 };
	float percent = (*value - min) / (max - min);
	Rectangle handle = { track.x + (percent * (track.width-10)), track.y - 2, 10, 12 };

	bool isHovering = CheckCollisionPointRec(mousePos, track) || CheckCollisionPointRec(mousePos, handle);

	if (isHovering) {
		uiState.hotItem = id;
		if (ShouldActivate()) uiState.activeItem = id;
	}

	if (uiState.activeItem == id) {
		float newPercent = (mousePos.x - track.x) / track.width;
		if (newPercent < 0.0f) newPercent = 0.0f;
		if (newPercent > 1.0f) newPercent = 1.0f;

		*value = min + (newPercent * (max-min));
		changed = true;

		if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) uiState.activeItem = 0;
	}

	DrawSunkenPanel(track);
	DrawRaisedPanel(handle);

	return changed;
}

bool GuiCheckbox(const char *idStr, Rectangle bounds, bool *value) {
	unsigned int id = HashString(idStr);
	bool changed = false;
	Vector2 mousePos = GetMousePosition();
	bool isHovering = CheckCollisionPointRec(mousePos, bounds);

	if (isHovering) {
		uiState.hotItem = id;
		if (ShouldActivate()) uiState.activeItem = id;
	}

	if (ShouldDeactivate(id)) {
		if (isHovering) {
			*value = !(*value);
			changed = true;
		}
		uiState.activeItem = 0;
	}

	DrawSunkenPanel(bounds);
	if (*value) {
		Rectangle padBounds = { bounds.x+2, bounds.y+2, bounds.width-4, bounds.height-4 };
		DrawRectangleRec(padBounds, BLACK);
	}

	return changed;
}

float GuiAccordeon(Rectangle bounds, int *active, const char **labels, int count, bool *isOpen) {
	float itemHeight = bounds.height;
	float totalHeight = itemHeight;

	if (GuiButtonTxt(labels[*active], bounds, *isOpen)) *isOpen = !(*isOpen);
	if (!(*isOpen)) return totalHeight;

	for (int i = 0; i < count; i++) {
		Rectangle item = { bounds.x+8, bounds.y+totalHeight, bounds.width - 8, itemHeight };
		if (GuiButtonTxt(labels[i], item, (*active == i))) {
			*active = i;
			*isOpen = false;
		}

		totalHeight += itemHeight;
	}

	return totalHeight;
} 

#endif
