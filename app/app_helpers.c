/*
File:   app_helpers.c
Author: Taylor Robbins
Date:   02\25\2025
Description: 
	** None
*/

#if BUILD_WITH_SOKOL_GFX

#define UI_R32(pixels) UISCALE_R32(app->uiScale, (pixels))
#define UI_U16(pixels) UISCALE_U16(app->uiScale, (pixels))
#define UI_BORDER(pixels) UISCALE_BORDER(app->uiScale, (pixels))

bool IsMouseOverClay(ClayId clayId)
{
	return appIn->mouse.isOverWindow && Clay_PointerOver(clayId);
}
bool IsMouseOverClayInContainer(ClayId containerId, ClayId clayId)
{
	return appIn->mouse.isOverWindow && Clay_PointerOver(containerId) && Clay_PointerOver(clayId);
}

ImageData LoadImageData(Arena* arena, const char* path)
{
	ScratchBegin1(scratch, arena);
	Slice fileContents = Slice_Empty;
	Result readFileResult = TryReadAppResource(&app->resources, scratch, FilePathLit(path), false, &fileContents);
	Assert(readFileResult == Result_Success);
	ImageData imageData = ZEROED;
	Result parseResult = TryParseImageFile(fileContents, arena, &imageData);
	Assert(parseResult == Result_Success);
	ScratchEnd(scratch);
	return imageData;
}

#if BUILD_WITH_SOKOL_APP
void LoadWindowIcon()
{
	ScratchBegin(scratch);
	ImageData iconImageDatas[6];
	iconImageDatas[0] = LoadImageData(scratch, "resources/image/icon_16.png");
	iconImageDatas[1] = LoadImageData(scratch, "resources/image/icon_24.png");
	iconImageDatas[2] = LoadImageData(scratch, "resources/image/icon_32.png");
	iconImageDatas[3] = LoadImageData(scratch, "resources/image/icon_64.png");
	iconImageDatas[4] = LoadImageData(scratch, "resources/image/icon_120.png");
	iconImageDatas[5] = LoadImageData(scratch, "resources/image/icon_256.png");
	platform->SetWindowIcon(ArrayCount(iconImageDatas), &iconImageDatas[0]);
	ScratchEnd(scratch);
}
#endif //BUILD_WITH_SOKOL_APP

bool AppCreateFonts()
{
	FontCharRange fontCharRanges[] = {
		FontCharRange_ASCII,
		FontCharRange_LatinExt,
		NewFontCharRangeSingle(UNICODE_ELLIPSIS_CODEPOINT),
		NewFontCharRangeSingle(UNICODE_RIGHT_ARROW_CODEPOINT),
	};
	
	PigFont newUiFont = ZEROED;
	{
		newUiFont = InitFont(stdHeap, StrLit("uiFont"));
		Result attachResult = AttachOsTtfFileToFont(&newUiFont, StrLit(UI_FONT_NAME), app->uiFontSize, UI_FONT_STYLE);
		Assert(attachResult == Result_Success);
		UNUSED(attachResult);
		Result bakeResult = BakeFontAtlas(&newUiFont, app->uiFontSize, UI_FONT_STYLE, NewV2i(256, 256), ArrayCount(fontCharRanges), &fontCharRanges[0]);
		if (bakeResult != Result_Success)
		{
			bakeResult = BakeFontAtlas(&newUiFont, app->uiFontSize, UI_FONT_STYLE, NewV2i(512, 512), ArrayCount(fontCharRanges), &fontCharRanges[0]);
			if (bakeResult != Result_Success)
			{
				RemoveAttachedTtfFile(&newUiFont);
				FreeFont(&newUiFont);
				return false;
			}
		}
		Assert(bakeResult == Result_Success);
		FillFontKerningTable(&newUiFont);
		RemoveAttachedTtfFile(&newUiFont);
	}
	
	if (app->uiFont.arena != nullptr) { FreeFont(&app->uiFont); }
	app->uiFont = newUiFont;
	
	return true;
}

bool AppChangeFontSize(bool increase)
{
	if (increase)
	{
		app->uiFontSize += 1;
		app->uiScale = app->uiFontSize / (r32)DEFAULT_UI_FONT_SIZE;
		if (!AppCreateFonts())
		{
			app->uiFontSize -= 1;
			app->uiScale = app->uiFontSize / (r32)DEFAULT_UI_FONT_SIZE;
		}
		return true;
	}
	else if (AreSimilarOrGreaterR32(app->uiFontSize - 1.0f, MIN_UI_FONT_SIZE, DEFAULT_R32_TOLERANCE))
	{
		app->uiFontSize -= 1;
		app->uiScale = app->uiFontSize / (r32)DEFAULT_UI_FONT_SIZE;
		AppCreateFonts();
		return true;
	}
	else { return false; }
}

#define CLAY_ICON(texturePntr, size, color) CLAY({      \
	.layout = {                                         \
		.sizing = {                                     \
			.width = CLAY_SIZING_FIXED((size).Width),   \
			.height = CLAY_SIZING_FIXED((size).Height), \
		},                                              \
	},                                                  \
	.image = {                                          \
		.imageData = (texturePntr),                     \
		.sourceDimensions = {                           \
			.Width = (r32)((texturePntr)->Width),       \
			.Height = (r32)((texturePntr)->Height),     \
		},                                              \
	},                                                  \
	.backgroundColor = color,                           \
}) {}

//Call Clay__CloseElement once after if statement
bool ClayBtnStrEx(Str8 idStr, Str8 btnText, Str8 hotkeyStr, bool isEnabled, bool growWidth, Texture* icon)
{
	ScratchBegin(scratch);
	Str8 fullIdStr = PrintInArenaStr(scratch, "%.*s_Btn", StrPrint(idStr));
	Str8 hotkeyIdStr = PrintInArenaStr(scratch, "%.*s_Hotkey", StrPrint(idStr));
	ClayId btnId = ToClayId(fullIdStr);
	ClayId hotkeyId = ToClayId(hotkeyIdStr);
	bool isHovered = IsMouseOverClay(btnId);
	bool isPressed = (isHovered && IsMouseBtnDown(&appIn->mouse, MouseBtn_Left));
	Color32 backgroundColor = !isEnabled ? MonokaiBack : (isPressed ? MonokaiGray2 : (isHovered ? MonokaiGray1 : MonokaiDarkGray));
	Color32 borderColor = isEnabled ? MonokaiWhite : MonokaiGray1;
	Color32 textColor = (isEnabled && isHovered) ? MonokaiDarkGray : MonokaiWhite;
	u16 borderWidth = (!isEnabled || isHovered) ? 1 : 0;
	Clay__OpenElement();
	Clay__ConfigureOpenElement((Clay_ElementDeclaration){
		.id = btnId,
		.layout = {
			.padding = { .top = UI_U16(6), .bottom = UI_U16(6), .left = UI_U16(10), .right = UI_U16(10), },
			.sizing = { .width = growWidth ? CLAY_SIZING_GROW(0) : CLAY_SIZING_FIT(0), .height = CLAY_SIZING_FIT(0), },
		},
		.backgroundColor = backgroundColor,
		.cornerRadius = CLAY_CORNER_RADIUS(UI_R32(4)),
		.border = { .width=CLAY_BORDER_OUTSIDE(UI_BORDER(borderWidth)), .color=borderColor },
	});
	CLAY({
		.layout = {
			.layoutDirection = CLAY_LEFT_TO_RIGHT,
			.childGap = UI_U16(2),
			.sizing = { .width = CLAY_SIZING_GROW(0) },
			.padding = { .right = 0 },
		},
	})
	{
		if (icon != nullptr)
		{
			CLAY_ICON(icon, FillV2(16 * app->uiScale), MonokaiWhite);
		}
		CLAY_TEXT(
			btnText,
			CLAY_TEXT_CONFIG({
				.fontId = app->clayUiFontId,
				.fontSize = (u16)app->uiFontSize,
				.textColor = MonokaiWhite,
				.wrapMode = CLAY_TEXT_WRAP_NONE,
				.textAlignment = CLAY_TEXT_ALIGN_SHRINK,
				.userData = { .contraction = TextContraction_ClipRight },
			})
		);
		if (!IsEmptyStr(hotkeyStr))
		{
			CLAY({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(0) }, } }) {}
			
			CLAY({ .id=hotkeyId,
				.layout = {
					.layoutDirection = CLAY_LEFT_TO_RIGHT,
					.padding = CLAY_PADDING_ALL(UI_U16(2)),
				},
				.border = { .width=CLAY_BORDER_OUTSIDE(UI_BORDER(1)), .color = MonokaiLightGray },
				.cornerRadius = CLAY_CORNER_RADIUS(UI_R32(5)),
			})
			{
				CLAY_TEXT(
					hotkeyStr,
					CLAY_TEXT_CONFIG({
						.fontId = app->clayUiFontId,
						.fontSize = (u16)app->uiFontSize,
						.textColor = MonokaiLightGray,
						.wrapMode = CLAY_TEXT_WRAP_NONE,
						.textAlignment = CLAY_TEXT_ALIGN_SHRINK,
						.userData = { .contraction = TextContraction_ClipRight },
					})
				);
			}
		}
	}
	ScratchEnd(scratch);
	return (isHovered && isEnabled && IsMouseBtnPressed(&appIn->mouse, MouseBtn_Left));
}
bool ClayBtnStr(Str8 btnText, Str8 hotkeyStr, bool isEnabled, r32 growWidth, Texture* icon)
{
	return ClayBtnStrEx(btnText, btnText, hotkeyStr, isEnabled, growWidth, icon);
}
bool ClayBtn(const char* btnText, const char* hotkeyStr, bool isEnabled, r32 growWidth, Texture* icon)
{
	return ClayBtnStr(StrLit(btnText), StrLit(hotkeyStr), isEnabled, growWidth, icon);
}

uxx FindStr8PairInArray(VarArray* array, Str8 key)
{
	VarArrayLoop(array, iIndex)
	{
		VarArrayLoopGet(Str8Pair, item, array, iIndex);
		if (StrExactEquals(item->key, key)) { return iIndex; }
	}
	return array->length;
}

#endif //BUILD_WITH_SOKOL_GFX