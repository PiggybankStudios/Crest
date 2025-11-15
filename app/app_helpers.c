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
	Result readFileResult = TryReadAppResource(&app->resources, scratch, MakeFilePathNt(path), false, &fileContents);
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
		FontCharRange_LatinSupplementAccent,
		MakeFontCharRangeSingle(UNICODE_ELLIPSIS_CODEPOINT),
		MakeFontCharRangeSingle(UNICODE_RIGHT_ARROW_CODEPOINT),
	};
	
	Result attachResult = Result_None;
	Result bakeResult = Result_None;
	
	PigFont newUiFont = ZEROED;
	{
		newUiFont = InitFont(stdHeap, StrLit("uiFont"));
		attachResult = TryAttachOsTtfFileToFont(&newUiFont, StrLit(UI_FONT_NAME), app->uiFontSize, UI_FONT_STYLE); Assert(attachResult == Result_Success);
		attachResult = TryAttachOsTtfFileToFont(&newUiFont, StrLit(UI_FONT_NAME), app->uiFontSize, UI_FONT_STYLE|FontStyleFlag_Bold); Assert(attachResult == Result_Success);
		
		bakeResult = TryBakeFontAtlas(&newUiFont, app->uiFontSize, UI_FONT_STYLE, 256, 1024, ArrayCount(fontCharRanges), &fontCharRanges[0]);
		if (bakeResult != Result_Success && bakeResult != Result_Partial)
		{
			DebugAssert(bakeResult == Result_Success || bakeResult == Result_Partial);
			FreeFont(&newUiFont);
			return false;
		}
		FillFontKerningTable(&newUiFont);
		// RemoveAttachedFontFiles(&newUiFont);
		
		bakeResult = TryBakeFontAtlas(&newUiFont, app->uiFontSize, UI_FONT_STYLE|FontStyleFlag_Bold, 256, 1024, ArrayCount(fontCharRanges), &fontCharRanges[0]);
		if (bakeResult != Result_Success && bakeResult != Result_Partial)
		{
			DebugAssert(bakeResult == Result_Success || bakeResult == Result_Partial);
			FreeFont(&newUiFont);
			return false;
		}
		
		MakeFontActive(&newUiFont, 256, 1024, 16, 0, 0);
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
bool ClayBtnStrEx(Str8 idStr, Str8 btnText, Str8 hotkeyStr, bool isEnabled, bool hasError, bool growWidth, Texture* icon)
{
	Str8 fullIdStr = PrintInArenaStr(uiArena, "Btn_%.*s", StrPrint(idStr));
	Str8 hotkeyIdStr = PrintInArenaStr(uiArena, "Btn_%.*s_Hotkey", StrPrint(idStr));
	ClayId btnId = ToClayId(fullIdStr);
	ClayId hotkeyId = ToClayId(hotkeyIdStr);
	bool isHovered = IsMouseOverClay(btnId);
	bool isPressed = (isHovered && !hasError && IsMouseBtnDown(&appIn->mouse, MouseBtn_Left));
	Color32 backgroundColor = !isEnabled ? MonokaiBack : (isPressed ? MonokaiGray2 : ((isHovered && !hasError) ? MonokaiGray1 : MonokaiDarkGray));
	Color32 borderColor = hasError ? MonokaiMagenta : (isEnabled ? MonokaiWhite : MonokaiGray1);
	Color32 textColor = hasError ? MonokaiMagenta : ((isEnabled && isHovered) ? MonokaiDarkGray : MonokaiWhite);
	u16 borderWidth = (!isEnabled || hasError || isHovered || isPressed) ? 1 : 0;
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
			PrintInArenaStr(uiArena, "%.*s", StrPrint(btnText)),
			CLAY_TEXT_CONFIG({
				.fontId = app->clayUiBoldFontId,
				.fontSize = (u16)app->uiFontSize,
				.textColor = textColor,
				.wrapMode = CLAY_TEXT_WRAP_NONE,
				.textAlignment = CLAY_TEXT_ALIGN_SHRINK,
				.userData = { .contraction = TextContraction_ClipRight },
			})
		);
		if (!IsEmptyStr(hotkeyStr) && !hasError)
		{
			CLAY({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(0) }, } }) {}
			
			CLAY({ .id=hotkeyId, .layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT } })
			{
				CLAY_TEXT(
					hotkeyStr,
					CLAY_TEXT_CONFIG({
						.fontId = app->clayUiFontId,
						.fontSize = (u16)app->uiFontSize,
						.textColor = (isEnabled && isHovered) ? MonokaiDarkGray : MonokaiLightGray,
						.wrapMode = CLAY_TEXT_WRAP_NONE,
						.textAlignment = CLAY_TEXT_ALIGN_SHRINK,
						.userData = { .contraction = TextContraction_ClipRight },
					})
				);
			}
		}
	}
	return (isHovered && isEnabled && !hasError && IsMouseBtnPressed(&appIn->mouse, MouseBtn_Left));
}
bool ClayBtnStr(Str8 btnText, Str8 hotkeyStr, bool isEnabled, bool growWidth, Texture* icon)
{
	return ClayBtnStrEx(btnText, btnText, hotkeyStr, isEnabled, false, growWidth, icon);
}
bool ClayBtn(const char* btnText, const char* hotkeyStr, bool isEnabled, bool growWidth, Texture* icon)
{
	return ClayBtnStr(MakeStr8Nt(btnText), MakeStr8Nt(hotkeyStr), isEnabled, growWidth, icon);
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

void DoErrorHoverable(UiWidgetContext* uiContext, Str8 uiElementIdStr, StrErrorList* errorList, bool openOverride)
{
	NotNull(uiContext);
	NotNull(errorList);
	ClayId uiElementId = ToClayId(uiElementIdStr);
	if (errorList->numErrors > 0)
	{
		CLAY({
			.floating = {
				.attachTo = CLAY_ATTACH_TO_ELEMENT_WITH_ID,
				.parentId = uiElementId.id,
				.attachPoints = { .parent = CLAY_ATTACH_POINT_RIGHT_CENTER, .element = CLAY_ATTACH_POINT_RIGHT_CENTER },
				.offset = MakeV2(UI_R32(-8), 0),
			},
		})
		{
			Str8 hoverableId = PrintInArenaStr(uiArena, "%.*s_ErrorIcon", StrPrint(uiElementIdStr));
			DoUiHoverableInterleaved(section, uiContext, hoverableId, Dir2_Up, ToV2Fromi(appIn->screenSize), openOverride)
			{
				DoUiHoverableSection(section, HoverArea)
				{
					CLAY({
						.layout = {
							.sizing = { .width = CLAY_SIZING_FIT(UI_R32(18)), .height = CLAY_SIZING_FIXED(UI_R32(18)) },
							.childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
						},
						.border = { .width = UI_BORDER(1), .color = MonokaiMagenta },
						.cornerRadius = CLAY_CORNER_RADIUS(UI_R32(18/2)),
					})
					{
						CLAY_TEXT(
							StrLit("!"),
							CLAY_TEXT_CONFIG({
								.fontId = app->clayUiBoldFontId,
								.fontSize = (u16)app->uiFontSize,
								.textColor = MonokaiMagenta,
								.wrapMode = CLAY_TEXT_WRAP_NONE,
								.textAlignment = CLAY_TEXT_ALIGN_LEFT,
						}));
					}
				}
				DoUiHoverableSection(section, Tooltip)
				{
					CLAY({
						.layout = {
							.layoutDirection = CLAY_TOP_TO_BOTTOM,
							.padding = CLAY_PADDING_ALL(UI_U16(8)),
							.childGap = UI_U16(4),
						},
						.border = { .width = CLAY_BORDER_OUTSIDE(UI_BORDER(2)), .color = MonokaiMagenta },
						.cornerRadius = CLAY_CORNER_RADIUS(UI_R32(5)),
						.backgroundColor = MonokaiDarkGray,
					})
					{
						for (uxx eIndex = 0; eIndex < errorList->numErrors; eIndex++)
						{
							if (errorList->errors[eIndex].duplicateIndex == UINTXX_MAX)
							{
								CLAY_TEXT(
									errorList->errors[eIndex].error,
									CLAY_TEXT_CONFIG({
										.fontId = app->clayUiBoldFontId,
										.fontSize = (u16)app->uiFontSize,
										.textColor = MonokaiMagenta,
										.wrapMode = CLAY_TEXT_WRAP_NONE,
										.textAlignment = CLAY_TEXT_ALIGN_LEFT,
								}));
							}
						}
					}
				}
			}
		}
	}
}

void HighlightErrorsInTextbox(UiTextbox* tbox, StrErrorList* errorList)
{
	UiTextboxClearSyntaxRanges(tbox);
	tbox->displayRedOutline = (errorList->numErrors > 0);
	if (errorList->numErrors > 0)
	{
		ScratchBegin(scratch);
		uxx numMergedRanges = errorList->numErrors;
		RangeUXX* mergedRanges = AllocArray(RangeUXX, scratch, errorList->numErrors);
		for (uxx eIndex = 0; eIndex < errorList->numErrors; eIndex++)
		{
			mergedRanges[eIndex] = errorList->errors[eIndex].range;
		}
		numMergedRanges = CombineOverlappingAndConsecutiveRangesUXX(numMergedRanges, mergedRanges);
		for (uxx eIndex = 0; eIndex < numMergedRanges; eIndex++)
		{
			UiTextboxAddSyntaxRange(tbox, mergedRanges[eIndex], MakeRichStrStyleChangeColor(MonokaiMagenta, false));
		}
		ScratchEnd(scratch);
	}
}

Color32 GetColorForHttpStatusCode(u16 statusCode)
{
	if (statusCode >= 200 && statusCode < 300) { return MonokaiGreen; } //success
	if (statusCode >= 300 && statusCode < 400) { return MonokaiYellow; } //redirection
	if (statusCode >= 400 && statusCode < 500) { return MonokaiOrange; } //client errors
	if (statusCode >= 500 && statusCode < 600) { return MonokaiMagenta; } //server errors
	return MonokaiPurple;
}

void FreeHistoryItem(HistoryItem* item)
{
	NotNull(item);
	if (item->arena != nullptr)
	{
		for (uxx hIndex = 0; hIndex < item->numHeaders; hIndex++)
		{
			FreeStr8(item->arena, &item->headers[hIndex].key);
			FreeStr8(item->arena, &item->headers[hIndex].value);
		}
		if (item->headers != nullptr) { FreeArray(Str8Pair, item->arena, item->numHeaders, item->headers); }
		for (uxx cIndex = 0; cIndex < item->numContentItems; cIndex++)
		{
			FreeStr8(item->arena, &item->contentItems[cIndex].key);
			FreeStr8(item->arena, &item->contentItems[cIndex].value);
		}
		if (item->contentItems != nullptr) { FreeArray(Str8Pair, item->arena, item->numContentItems, item->contentItems); }
		FreeStr8(item->arena, &item->response);
		FreeUiLargeText(&item->responseLargeText);
		VarArrayLoop(&item->responseHeaders, hIndex)
		{
			VarArrayLoopGet(Str8Pair, header, &item->responseHeaders, hIndex);
			FreeStr8(item->arena, &header->key);
			FreeStr8(item->arena, &header->value);
		}
		FreeVarArray(&item->responseHeaders);
	}
	ClearPointer(item);
}

#endif //BUILD_WITH_SOKOL_GFX