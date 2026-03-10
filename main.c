#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <raylib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

// TYPES

typedef enum {
	TOOL_BRUSH,
	TOOL_ERASER,
	TOOL_FILL,
	TOOL_PICKER,
	TOOL_SHAPE,
	TOOL_COUNT
} ToolType;

typedef enum {
	UI_SLIDER,
	UI_TOGGLE,	
	UI_CHOICE
} OptionUIType;

typedef struct {
	char *name;
	OptionUIType type;
	union {
		struct { float value; float min; float max; } slider;
		struct { bool enabled; } toggle;
		struct { int active; int count; const char **labels; bool isOpen; } choice;
	} data;
} ToolOption;

#define MAX_TOOL_OPTIONS 4

typedef struct {
	ToolType id;
	Rectangle spriteCutout;
	int optionCount;
	ToolOption options[MAX_TOOL_OPTIONS];
} ToolDef;

typedef struct {
	ToolType selected;
	Vector2 lastMouse;
	bool isDrawing;
	ToolDef tools[TOOL_COUNT];
} ToolboxState;

typedef struct {
	RenderTexture2D tex;
	Vector2 pos;
	float zoom;
	Color current;
	Color background;
} Canvas;

#define MAX_HISTORY 25

typedef struct {
	Image states[MAX_HISTORY];
	int current;
	int count;
} CanvasHistory;

// COLORS

#define WIN_BG     (Color){ 212, 208, 200, 255 }
#define WIN_HI     (Color){ 255, 255, 255, 255 }
#define WIN_SHADOW (Color){ 128, 128, 128, 255 }
#define WIN_DARK   (Color){  64,  64,  64, 255 }

const Color PALETTE_COLORS[28] = {
	{0,0,0,255},       {128,128,128,255}, {128,0,0,255},     {128,128,0,255},
	{0,128,0,255},     {0,128,128,255},   {0,0,128,255},     {128,0,128,255},
	{128,128,64,255},  {0,64,64,255},     {0,128,255,255},   {0,64,128,255},
	{64,0,255,255},    {128,64,0,255},

	{255,255,255,255}, {192,192,192,255}, {255,0,0,255},     {255,255,0,255},
	{0,255,0,255},     {0,255,255,255},   {0,0,255,255},     {255,0,255,255},
	{255,255,128,255}, {0,255,128,255},   {128,255,255,255}, {128,128,255,255},
	{255,0,128,255},   {255,128,64,255}
};

// UTIL

Vector2 CanvasPos(Vector2 pos, Vector2 offset, Canvas *canvas) {
	return (Vector2){
		(pos.x - offset.x - canvas->pos.x) / canvas->zoom,
		(pos.y - offset.y - canvas->pos.y) / canvas->zoom,
	};
}

bool ColorsEqual(Color a, Color b) {
	return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

Rectangle CutTop(Rectangle *rect, float amount) {
	Rectangle cut = { rect->x, rect->y, rect->width, amount };
	rect->y += amount;
	rect->height -= amount;
	return cut;
}

Rectangle CutBottom(Rectangle *rect, float amount) {
	rect->height -= amount;
	Rectangle cut = { rect->x, rect->y + rect->height, rect->width, amount };
	return cut;
}

Rectangle CutLeft(Rectangle *rect, float amount) {
	Rectangle cut = { rect->x, rect->y, amount, rect->height };
	rect->x += amount;
	rect->width -= amount;
	return cut;
}

Rectangle CutRight(Rectangle *rect, float amount) {
	rect->width -= amount;
	Rectangle cut = { rect->x + rect->width, rect->y, amount, rect->height};
	return cut;
}

Rectangle PadAll(Rectangle *rect, float amount) {
	Rectangle pad = { rect->x+amount, rect->y+amount, rect->width-amount*2, rect->height-amount*2 };	
	return pad;
}

Rectangle GetGridRectCol(Rectangle bounds, int index, int cols, int padding, int cellW, int cellH) {
	int col = index % cols;
	int row = index / cols;
	return (Rectangle){
		bounds.x + padding + col * (cellW + padding),
		bounds.y + padding + row * (cellH + padding),
		cellW, cellH
	};
}

Rectangle GetGridRectRow(Rectangle bounds, int index, int rows, int padding, int cellW, int cellH) {
	int col = index / rows;
	int row = index % rows;
	return (Rectangle){
		bounds.x + padding + col * (cellW + padding),
		bounds.y + padding + row * (cellH + padding),
		cellW, cellH
	};
}

ToolDef *GetActiveTool(ToolboxState *state) {
	return &state->tools[state->selected];
}

// GUI

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

bool GuiSlider(Rectangle bounds, float *value, float min, float max) {
	bool changed = false;
	
	Vector2 mousePos = GetMousePosition();
	Rectangle track = { bounds.x, bounds.y, bounds.width, 8 };
	DrawSunkenPanel(track);

	float percent = (*value - min) / (max - min);
	Rectangle handle = { track.x + (percent * (track.width-10)), track.y - 2, 10, 12 };
	DrawRaisedPanel(handle);

	if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mousePos, track)) {
		float newPercent = (mousePos.x - track.x) / track.width;

		if (newPercent < 0.0f) newPercent = 0.0f;
		if (newPercent > 1.0f) newPercent = 1.0f;

		*value = min + (newPercent * (max - min));
		changed = true;
	}

	return changed;
}

bool GuiCheckbox(Rectangle bounds, bool *value) {
	bool changed = false;

	Vector2 mousePos = GetMousePosition();
	DrawSunkenPanel(bounds);

	if (*value) {
		DrawRectangleRec(PadAll(&bounds, 2), BLACK);
	}

	if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mousePos, bounds)) {
		*value = !(*value);
		changed = true;
	}

	return changed;
}

bool GuiButton(Rectangle bounds, bool isActive) {
	Vector2 mousePos = GetMousePosition();

	bool isHovering = CheckCollisionPointRec(mousePos, bounds);
	bool isDown = isHovering && IsMouseButtonDown(MOUSE_LEFT_BUTTON);
	bool isClicked = isHovering && IsMouseButtonReleased(MOUSE_LEFT_BUTTON);

	if (isActive || isDown || isClicked) DrawSunkenPanel(bounds);
	else DrawRaisedPanel(bounds);	

	return isClicked;
}

bool GuiButtonTxt(Rectangle bounds, const char *text, bool isActive) {
	bool clicked = GuiButton(bounds, isActive);
	int textWidth = MeasureText(text, 10);
	DrawText(text, bounds.x + (bounds.width - textWidth) / 2, bounds.y + (bounds.height-12), 10, BLACK);
	return clicked;
}
bool GuiButtonImg(Rectangle bounds, Texture2D tex, Rectangle texSource, bool isActive) {
	bool clicked = GuiButton(bounds, isActive);
	Vector2 iconPos = {
		bounds.x + (bounds.width - texSource.width) / 2.0f,
		bounds.y + (bounds.height - texSource.height) / 2.0f,
	};

	DrawTextureRec(tex, texSource, iconPos, WHITE);
	return clicked;
}

bool GuiAccordeon(Rectangle bounds, int *active, const char **labels, int count, bool *isOpen) {
	float itemHeight = bounds.height;
	float totalHeight = itemHeight;

	if (GuiButtonTxt(bounds, labels[*active], *isOpen)) {
		*isOpen = !(*isOpen);
	};

	if (!(*isOpen)) return totalHeight;

	for (int i = 0; i < count; i++) {
		Rectangle item = { bounds.x + 8, bounds.y + totalHeight, bounds.width - 8, itemHeight };	
		bool isSelected = (*active == i);
		if (GuiButtonTxt(item, labels[i], isSelected)) {
			*active = i;
			*isOpen = false;
		}

		totalHeight += itemHeight;
	}

	return totalHeight;
}

// CANVAS MANAGEMENT

Canvas CreateCanvas(Vector2 size, Color background) {
	RenderTexture2D tex = LoadRenderTexture(size.x, size.y);
	BeginTextureMode(tex);
	ClearBackground(background);
	EndTextureMode();

	return (Canvas){
		tex,
		(Vector2){0, 0},
		1.0,
		BLACK,
		background
	};
}

void SaveHistory(CanvasHistory *hist, Canvas *canvas) {
	for (int i = hist->current+1; i < hist->count; i++) {
		UnloadImage(hist->states[i]);
	}

	hist->current++;

	if (hist->current >= MAX_HISTORY) {
		UnloadImage(hist->states[0]);
		for (int i = 1; i < MAX_HISTORY; i++) {
			hist->states[i-1] = hist->states[i];
		}
		hist->current = MAX_HISTORY - 1;
	}

	hist->count = hist->current+1;

	hist->states[hist->current] = LoadImageFromTexture(canvas->tex.texture);
}

void LoadHistory(CanvasHistory *hist, Canvas *canvas, int stepDirection) {
	int target = hist->current + stepDirection;
	if (target >= 0 && target < hist->count) {
		hist->current = target;
		UpdateTexture(canvas->tex.texture, hist->states[hist->current].data);
	}
}

void SaveCanvasToPNG(Canvas *canvas) {
	FILE *f = popen("zenity --file-selection --save --confirm-overwrite --title=\"Save Artwork\" --filename=\"untitled.png\"", "r");
	if (!f) return;

	char path[1024] = {0};
	if (fgets(path, sizeof(path), f) != NULL) {
		path[strcspn(path, "\n")] = 0;
		if (!strstr(path, ".png")) strcat(path, ".png");

		Image img = LoadImageFromTexture(canvas->tex.texture);
		ImageFlipVertical(&img);
		ExportImage(img, path);
		UnloadImage(img);
	}

	pclose(f);
}

// UI COMPONENTS

void DrawCanvas(Rectangle rect, Canvas canvas) {
	DrawSunkenPanel(rect);
	BeginScissorMode(rect.x+2, rect.y+2, rect.width-4, rect.height-4);
		ClearBackground(GRAY);
		Rectangle sourceRec = { 0.0f, 0.0f, (float)canvas.tex.texture.width, -(float)canvas.tex.texture.height };
		Rectangle destRect = { rect.x + canvas.pos.x, rect.y + canvas.pos.y, canvas.tex.texture.width * canvas.zoom, canvas.tex.texture.height * canvas.zoom };
		DrawTexturePro(canvas.tex.texture, sourceRec, destRect, (Vector2){0, 0}, 0.0f, WHITE);
	EndScissorMode();
}

void DrawActiveShape(Vector2 start, Vector2 end, int type, float border, bool isFilled, Color color, Vector2 offset, float zoom) {
	Vector2 s = { start.x * zoom + offset.x, start.y * zoom + offset.y };
	Vector2 e = { end.x * zoom + offset.x, end.y * zoom + offset.y };
	float thick = border * zoom;

	switch (type) {
		case 0: { // RECTANGLE
			float rx = fminf(s.x, e.x);
			float ry = fminf(s.y, e.y);
			float rw = fabsf(s.x-e.x);
			float rh = fabsf(s.y-e.y);
			Rectangle rect = { rx, ry, rw, rh };
			if (isFilled) DrawRectangleRec(rect, color);
			else DrawRectangleLinesEx(rect, thick, color);
			break;
		}
		case 1: { // CIRCLE
			float dx = e.x - s.x;
			float dy = e.y - s.y;
			float radius = sqrtf(dx*dx + dy*dy);

			if (isFilled) DrawCircleV(s, radius, color);
			else {
				float inner = fmaxf(0.0f, radius - thick);
				DrawRing(s, inner, radius, 0, 360, 64, color);
			}
			break;
		}
		case 2: { // LINE
			DrawLineEx(s, e, thick, color);
			break;
		}
	}
}

void DrawToolOptions(Rectangle rect, ToolboxState *state) {
	DrawRaisedPanel(rect);
	ToolDef *activeTool = GetActiveTool(state);

	if (activeTool->optionCount == 0) return;

	float optHeight = 32;
	int padding = 4;
	float currentY = rect.y + padding;

	BeginScissorMode(rect.x, rect.y, rect.width, rect.height);
	for (int i = 0; i < activeTool->optionCount; i++) {
		ToolOption *opt = &activeTool->options[i];
		DrawText(opt->name, rect.x+padding, currentY, 10, BLACK);
		currentY += optHeight / 2;

		Rectangle control = { rect.x + padding, currentY, rect.width - (padding * 2), 16 };

		switch (opt->type) {
			case UI_SLIDER: {
				GuiSlider(control, &opt->data.slider.value, opt->data.slider.min, opt->data.slider.max);
				currentY += optHeight / 2 + padding;
				break;
			};
			case UI_TOGGLE: {
				Rectangle checkboxRec = CutLeft(&control, optHeight/2);
				GuiCheckbox(checkboxRec, &opt->data.toggle.enabled);
 			  currentY += optHeight / 2 + padding;
				break;
			};
			case UI_CHOICE: {
				float consumed = GuiAccordeon(control, &opt->data.choice.active, opt->data.choice.labels, opt->data.choice.count, &opt->data.choice.isOpen);
				 currentY += consumed + padding;
				 break;
			}
		}
	}
	EndScissorMode();
}



void DrawToolPreview(Rectangle rect, Canvas *canvas, ToolboxState *state) {
	if (!state->isDrawing) return;

	ToolDef *activeTool = GetActiveTool(state);
	switch (activeTool->id) {
		case TOOL_SHAPE: {
			Vector2 mousePos = GetMousePosition();
			Vector2 canvasMousePos = CanvasPos(mousePos, (Vector2){rect.x, rect.y}, canvas);

			float border = activeTool->options[0].data.slider.value;
			bool isFilled = activeTool->options[1].data.toggle.enabled;
			int shapeType = activeTool->options[2].data.choice.active;
			BeginScissorMode(rect.x, rect.y, rect.width, rect.height);

			Vector2 offset = { rect.x + canvas->pos.x, rect.y + canvas->pos.y };
			DrawActiveShape(state->lastMouse, canvasMousePos, shapeType, border, isFilled, canvas->current, offset, canvas->zoom);

			EndScissorMode();
			}
		default: return;
	}
}

void DrawTopBar(Rectangle rect, Canvas *canvas, CanvasHistory *hist) {
	DrawRaisedPanel(rect);
	Rectangle inner = PadAll(&rect, 4);

	Rectangle btnSave  = CutLeft(&inner, 60); CutLeft(&inner, 4);
	Rectangle btnLoad  = CutLeft(&inner, 60); CutLeft(&inner, 4);
	Rectangle btnUndo  = CutLeft(&inner, 60); CutLeft(&inner, 4);
	Rectangle btnRedo  = CutLeft(&inner, 60); CutLeft(&inner, 4);
	Rectangle btnClear = CutLeft(&inner, 60); CutLeft(&inner, 4);

	if (GuiButtonTxt(btnSave, "Save", false)) {
		SaveCanvasToPNG(canvas);
	}
	if (GuiButtonTxt(btnLoad, "Load", false)) {
		printf("Load!\n");
	}
	if (GuiButtonTxt(btnUndo, "Undo", false)) {
		LoadHistory(hist, canvas, -1);
	}
	if (GuiButtonTxt(btnRedo, "Redo", false)) {
		LoadHistory(hist, canvas, 1);
	}
	if (GuiButtonTxt(btnClear, "Clear", false)) {
		BeginTextureMode(canvas->tex);
		ClearBackground(canvas->background);
		EndTextureMode();
		SaveHistory(hist, canvas);
	}
}

void DrawToolbox(Rectangle rect, ToolboxState *state, Texture2D icons, int cols, int padding, int btnSize) {
	DrawRaisedPanel(rect);

	for (int i = 0; i < TOOL_COUNT; i++) {
		Rectangle btnRect = GetGridRectCol(rect, i, cols, padding, btnSize, btnSize);
		bool clicked = GuiButtonImg(btnRect, icons, state->tools[i].spriteCutout, state->selected == state->tools[i].id);
		if (clicked) state->selected = state->tools[i].id;
	}
}

void DrawColorPalette(Rectangle rect, Canvas *canvas) {
	DrawRaisedPanel(rect);
	Vector2 mousePos = GetMousePosition();

	int rows = 2;
	int swatchSize = 28;
	int padding = 2;

	for (int i = 0; i < 28; i++) {
		Rectangle gridRect = GetGridRectRow(rect, i, rows, padding, swatchSize, swatchSize);
		Rectangle swatch = PadAll(&gridRect, 2);

		if (GuiButton(gridRect, ColorsEqual(canvas->current, PALETTE_COLORS[i]))) {
			canvas->current = PALETTE_COLORS[i];
		}
		if (CheckCollisionPointRec(mousePos, gridRect) && IsMouseButtonReleased(MOUSE_RIGHT_BUTTON)) {
			canvas->background = PALETTE_COLORS[i];
		}

		DrawRectangleRec(swatch, PALETTE_COLORS[i]);
	}
}

// APP LOGIC

void UpdateActiveTool(ToolboxState *state, Canvas *canvas, CanvasHistory *hist, Vector2 canvasOffset) {
	Vector2 mousePos = GetMousePosition();
	Vector2 canvasMousePos = CanvasPos(mousePos, canvasOffset, canvas);

	bool isPressed = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
	bool isDown    = IsMouseButtonDown(MOUSE_LEFT_BUTTON);
	bool isReleased = IsMouseButtonReleased(MOUSE_LEFT_BUTTON);

	ToolDef *activeTool = GetActiveTool(state);

	switch (activeTool->id) {
		case TOOL_BRUSH:
		case TOOL_ERASER: {
			if (isPressed) {
				state->isDrawing = true;
				state->lastMouse = canvasMousePos;
			}

			if (isDown && state->isDrawing) {
				BeginTextureMode(canvas->tex);
				Color color = (activeTool->id == TOOL_ERASER) ? canvas->background : canvas->current;
				float currentSize = activeTool->options[0].data.slider.value;

				if (state->lastMouse.x >= 0 && state->lastMouse.y >= 0) {
					DrawLineEx(state->lastMouse, canvasMousePos, currentSize, color);
					DrawCircleV(state->lastMouse, currentSize/2.0f, color);
				}
				DrawCircleV(canvasMousePos, currentSize/2.0f, color);
				EndTextureMode();

				state->lastMouse = canvasMousePos;
			}

			if (isReleased && state->isDrawing) {
				state->isDrawing = false;
				state->lastMouse = (Vector2){-1, -1};
				SaveHistory(hist, canvas);
			}
			break;
		}
		case TOOL_FILL: {
			if (!isPressed) return;
			int startX = (int)canvasMousePos.x;
			int startY = canvas->tex.texture.height - 1 - (int)canvasMousePos.y;

			if (startX < 0 || startX >= canvas->tex.texture.width ||
					startY < 0 || startY >= canvas->tex.texture.height) return;

			Image img = LoadImageFromTexture(canvas->tex.texture);
			Color *pixels = (Color*)img.data;

			Color targetColor = pixels[startY * img.width + startX];
			Color replacementColor = canvas->current;

			if (ColorsEqual(targetColor, replacementColor)) {
				UnloadImage(img);
				return;
			}

			typedef struct { int x, y; } Point;
			int maxPixels = img.width * img.height;
			Point *stack = malloc(maxPixels * 4 * sizeof(Point));
			int stackPtr = 0;

			stack[stackPtr++] = (Point){startX, startY};

			while (stackPtr > 0) {
				Point p = stack[--stackPtr];
				if (p.x < 0 || p.x >= img.width || p.y < 0 || p.y >= img.height) continue;

				int index = p.y * img.width + p.x;
				if (ColorsEqual(pixels[index], targetColor)) {
					pixels[index] = replacementColor;

					stack[stackPtr++] = (Point){p.x+1, p.y};
					stack[stackPtr++] = (Point){p.x-1, p.y};
					stack[stackPtr++] = (Point){p.x, p.y+1};
					stack[stackPtr++] = (Point){p.x, p.y-1};
				}
			}

			UpdateTexture(canvas->tex.texture, pixels);
			free(stack);
			UnloadImage(img);
			SaveHistory(hist, canvas);
			break;
		}
		case TOOL_PICKER: {
			if (!isPressed) return;
			Image img = LoadImageFromTexture(canvas->tex.texture);
			int flippedY = canvas->tex.texture.height-1 - (int)canvasMousePos.y;
			canvas->current = GetImageColor(img, canvasMousePos.x, flippedY);
			UnloadImage(img);
			break;
		}
		case TOOL_SHAPE: {
			if (isPressed) {
				state->isDrawing = true;
				state->lastMouse = canvasMousePos;
			};

			if (isReleased && state->isDrawing) {
				state->isDrawing = false;

				float border = activeTool->options[0].data.slider.value;
				bool isFilled = activeTool->options[1].data.toggle.enabled;
				int shapeType = activeTool->options[2].data.choice.active;

				BeginTextureMode(canvas->tex);
				DrawActiveShape(state->lastMouse, canvasMousePos, shapeType, border, isFilled, canvas->current, (Vector2){0,0}, 1.0);
				EndTextureMode();

				SaveHistory(hist, canvas);
				state->lastMouse = (Vector2){-1, -1};
			}
		}
		default: return;
	}
}

int main() {
	int screenW = 800;
	int screenH = 600;
	InitWindow(screenW, screenH, "Paint");
	SetWindowMonitor(0);
	SetConfigFlags(FLAG_WINDOW_RESIZABLE);
	SetTargetFPS(60);

	int menuHeight = 24;
	int paletteHeight = 64;
	int sidebarWidth = 32*2+4*3;

	Vector2 initialCanvasSize = (Vector2){ screenW-sidebarWidth-8, screenH-menuHeight-paletteHeight-8 };
	Canvas canvas = CreateCanvas(initialCanvasSize, RAYWHITE);

	CanvasHistory canvasHistory = {
		{{0}},
		-1,
		0
	};
	SaveHistory(&canvasHistory, &canvas);

	Texture2D toolIcons = LoadTexture("tools_spritesheet.png");
	ToolboxState toolboxState = {
		TOOL_BRUSH, 
		{-1, -1},
		false, {
		{ TOOL_BRUSH,  { 0,   0, 16, 16 }, 1, {
			{ "Size", UI_SLIDER, .data.slider = { 5.0, 1.0f, 20.0f } },
		}},
		{ TOOL_ERASER, { 16,  0, 16, 16 }, 1, {
			{ "Size", UI_SLIDER, .data.slider = { 5.0, 1.0f, 20.0f } }
		}},
		{ TOOL_FILL,   { 32,  0, 16, 16 }, 0, {{0}} },
		{ TOOL_PICKER, { 48,  0, 16, 16 }, 0, {{0}} },
		{ TOOL_SHAPE , { 0,  16, 16, 16 }, 3, {
			{ "Border", UI_SLIDER, .data.slider = { 1.0, 1.0f, 10.0f } },
			{ "Filled", UI_TOGGLE, .data.toggle = { false } },
			{ "Shape",  UI_CHOICE, .data.choice = { 0, 3, (const char*[]){ "Box", "Circle", "Line" }, false } },
		}},
	}};

	while (!WindowShouldClose()) {
		Rectangle remainingSpace = { 0, 0, GetScreenWidth(), GetScreenHeight() };

		Rectangle menuZone  = CutTop(&remainingSpace, menuHeight);
		Rectangle paletteZone = CutBottom(&remainingSpace, paletteHeight);
		Rectangle sidebar = CutLeft(&remainingSpace, sidebarWidth);
		Rectangle toolboxZone = CutTop(&sidebar, remainingSpace.height/2);
		Rectangle optionsZone = sidebar;
		Rectangle canvasZone = remainingSpace;

		Vector2 mousePos = GetMousePosition();
		bool isMouseOverCanvas = CheckCollisionPointRec(mousePos, canvasZone);
		if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE) && isMouseOverCanvas) {
		}

		if (isMouseOverCanvas) {
			float wheel = GetMouseWheelMove();
			if (wheel != 0.0f) {
				Vector2 mouseOnCanvas = {
					(mousePos.x - canvasZone.x - canvas.pos.x) / canvas.zoom,
					(mousePos.y - canvasZone.y - canvas.pos.y) / canvas.zoom,
				};
				canvas.zoom += wheel * 0.1f;
				if (canvas.zoom < 0.1f) canvas.zoom = 0.1f;
				if (canvas.zoom > 10.f) canvas.zoom = 10.f;

				canvas.pos.x = (mousePos.x - canvasZone.x) - (mouseOnCanvas.x * canvas.zoom);
				canvas.pos.y = (mousePos.y - canvasZone.y) - (mouseOnCanvas.y * canvas.zoom);
			}

			if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)) {
				Vector2 delta = GetMouseDelta();
				canvas.pos.x += delta.x;
				canvas.pos.y += delta.y;
			}

			UpdateActiveTool(&toolboxState, &canvas, &canvasHistory, (Vector2){canvasZone.x, canvasZone.y});
		} else {
			if (toolboxState.isDrawing) {
				toolboxState.isDrawing = false;
				toolboxState.lastMouse = (Vector2){-1, -1};
				SaveHistory(&canvasHistory, &canvas);
			}
		}

		if (IsKeyDown(KEY_LEFT_CONTROL)) {
			if (IsKeyPressed(KEY_Z)) LoadHistory(&canvasHistory, &canvas, -1);
			if (IsKeyPressed(KEY_Y)) LoadHistory(&canvasHistory, &canvas, 1);
			if (IsKeyPressed(KEY_S)) SaveCanvasToPNG(&canvas);
		}
		

		BeginDrawing();
		ClearBackground(WIN_BG); 

		DrawCanvas(canvasZone, canvas);	
		DrawToolPreview(canvasZone, &canvas, &toolboxState);

		DrawTopBar(menuZone, &canvas, &canvasHistory);

		DrawToolbox(toolboxZone, &toolboxState, toolIcons, 2, 4, 32);
		DrawToolOptions(optionsZone, &toolboxState);

		DrawColorPalette(paletteZone, &canvas);

		EndDrawing();
	}

	UnloadRenderTexture(canvas.tex);
	CloseWindow();
	return 0;
}
