/*
File:   app_main.c
Author: Taylor Robbins
Date:   01\19\2025
Description: 
	** Contains the dll entry point and all exported functions that the platform
	** layer can lookup by name and call. Also includes all other source files
	** required for the application to compile.
*/

#include "build_config.h"
#include "defines.h"
#define PIG_CORE_IMPLEMENTATION BUILD_INTO_SINGLE_UNIT

#include "base/base_all.h"
#include "std/std_all.h"
#include "os/os_all.h"
#include "mem/mem_all.h"
#include "struct/struct_all.h"
#include "misc/misc_all.h"
#include "input/input_all.h"
#include "file_fmt/file_fmt_all.h"
#include "ui/ui_all.h"
#include "gfx/gfx_all.h"
#include "gfx/gfx_system_global.h"
#include "phys/phys_all.h"

// +--------------------------------------------------------------+
// |                         Header Files                         |
// +--------------------------------------------------------------+
#include "platform_interface.h"
#include "app_resources.h"
#include "app_main.h"

#include "main2d_shader.glsl.h"

// +--------------------------------------------------------------+
// |                           Globals                            |
// +--------------------------------------------------------------+
static AppData* app = nullptr;
static AppInput* appIn = nullptr;
static Arena* uiArena = nullptr;

#if !BUILD_INTO_SINGLE_UNIT //NOTE: The platform layer already has these globals
static PlatformInfo* platformInfo = nullptr;
static PlatformApi* platform = nullptr;
static Arena* stdHeap = nullptr;
#endif

// +--------------------------------------------------------------+
// |                         Source Files                         |
// +--------------------------------------------------------------+
#include "app_resources.c"
#include "app_helpers.c"
//TODO: Add source files here!

// +==============================+
// |           DllMain            |
// +==============================+
#if (TARGET_IS_WINDOWS && !BUILD_INTO_SINGLE_UNIT)
BOOL WINAPI DllMain(
	HINSTANCE hinstDLL, // handle to DLL module
	DWORD fdwReason,    // reason for calling function
	LPVOID lpReserved)
{
	UNUSED(hinstDLL);
	UNUSED(lpReserved);
	switch(fdwReason)
	{ 
		case DLL_PROCESS_ATTACH: break;
		case DLL_PROCESS_DETACH: break;
		case DLL_THREAD_ATTACH: break;
		case DLL_THREAD_DETACH: break;
		default: break;
	}
	//If we don't return TRUE here then the LoadLibraryA call will return a failure!
	return TRUE;
}
#endif //(TARGET_IS_WINDOWS && !BUILD_INTO_SINGLE_UNIT)

void UpdateDllGlobals(PlatformInfo* inPlatformInfo, PlatformApi* inPlatformApi, void* memoryPntr, AppInput* appInput)
{
	#if !BUILD_INTO_SINGLE_UNIT
	platformInfo = inPlatformInfo;
	platform = inPlatformApi;
	stdHeap = inPlatformInfo->platformStdHeap;
	#else
	UNUSED(inPlatformApi);
	UNUSED(inPlatformInfo);
	#endif
	app = (AppData*)memoryPntr;
	appIn = appInput;
}

// +==============================+
// |           AppInit            |
// +==============================+
// void* AppInit(PlatformInfo* inPlatformInfo, PlatformApi* inPlatformApi)
EXPORT_FUNC APP_INIT_DEF(AppInit)
{
	TracyCZoneN(Zone_Func, "AppInit", true);
	#if !BUILD_INTO_SINGLE_UNIT
	InitScratchArenasVirtual(Gigabytes(4));
	#endif
	ScratchBegin(scratch);
	ScratchBegin1(scratch2, scratch);
	ScratchBegin2(scratch3, scratch, scratch2);
	
	AppData* appData = AllocType(AppData, inPlatformInfo->platformStdHeap);
	ClearPointer(appData);
	UpdateDllGlobals(inPlatformInfo, inPlatformApi, (void*)appData, nullptr);
	
	#if 0
	PrintLine_D("scratch1: %p-%p", scratch->mainPntr, (u8*)scratch->mainPntr + scratch->size);
	PrintLine_D("scratch2: %p-%p", scratch2->mainPntr, (u8*)scratch2->mainPntr + scratch2->size);
	PrintLine_D("scratch3: %p-%p", scratch3->mainPntr, (u8*)scratch3->mainPntr + scratch3->size);
	PrintLine_D("appData: %p-%p", appData, (appData + 1));
	#endif
	
	InitAppResources(&app->resources);
	
	#if BUILD_WITH_SOKOL_APP
	platform->SetWindowTitle(StrLit(PROJECT_READABLE_NAME_STR));
	LoadWindowIcon();
	#endif
	
	InitRandomSeriesDefault(&app->random);
	SeedRandomSeriesU64(&app->random, OsGetCurrentTimestamp(false));
	
	InitCompiledShader(&app->mainShader, stdHeap, main2d);
	
	app->uiFontSize = DEFAULT_UI_FONT_SIZE;
	app->uiScale = 1.0f;
	bool fontBakeSuccess = AppCreateFonts();
	Assert(fontBakeSuccess);
	UNUSED(fontBakeSuccess);
	
	InitClayUIRenderer(stdHeap, V2_Zero, &app->clay);
	app->clayUiFontId = AddClayUIRendererFont(&app->clay, &app->uiFont, UI_FONT_STYLE);
	app->clayUiBoldFontId = AddClayUIRendererFont(&app->clay, &app->uiFont, UI_FONT_STYLE|FontStyleFlag_Bold);
	
	InitUiTextbox(stdHeap, StrLit("UrlTextbox"), StrLit("https://www.kagi.com/"), &app->urlTextbox);
	InitUiListView(stdHeap, StrLit("HeadersListView"), &app->headersListView);
	InitUiTextbox(stdHeap, StrLit("HeaderKeyTextbox"), StrLit(""), &app->headerKeyTextbox);
	InitUiTextbox(stdHeap, StrLit("HeaderValueTextbox"), StrLit(""), &app->headerValueTextbox);
	InitUiListView(stdHeap, StrLit("ContentListView"), &app->contentListView);
	InitUiTextbox(stdHeap, StrLit("ContentKeyTextbox"), StrLit(""), &app->contentKeyTextbox);
	InitUiTextbox(stdHeap, StrLit("ContentValueTextbox"), StrLit(""), &app->contentValueTextbox);
	InitUiListView(stdHeap, StrLit("HistoryListView"), &app->historyListView);
	
	InitVarArray(Str8Pair, &app->httpHeaders, stdHeap);
	InitVarArray(Str8Pair, &app->httpContent, stdHeap);
	InitVarArray(HistoryItem, &app->history, stdHeap);
	app->nextHistoryId = 1;
	
	app->httpVerb = HttpVerb_POST;
	app->currentResultTab = ResultTab_Raw;
	
	app->initialized = true;
	ScratchEnd(scratch);
	ScratchEnd(scratch2);
	ScratchEnd(scratch3);
	TracyCZoneEnd(Zone_Func);
	return (void*)app;
}

// +==============================+
// |       RenderHeaderItem       |
// +==============================+
// void RenderHeaderItem(UiListView* list, void* item, uxx index, bool isSelected, bool isHovered)
UI_LIST_VIEW_ITEM_RENDER_DEF(RenderHeaderItem)
{
	Str8Pair* header = (Str8Pair*)((UiListViewItem*)item)->contextPntr;
	if (app->removedHeaderThisFrame) { header--; }
	CLAY_TEXT(
		PrintInArenaStr(uiArena, "%.*s: %.*s", StrPrint(header->key), StrPrint(header->value)),
		CLAY_TEXT_CONFIG({
			.fontId = app->clayUiFontId,
			.fontSize = (u16)app->uiFontSize,
			.textColor = isSelected ? MonokaiDarkGray : MonokaiWhite,
			.wrapMode = CLAY_TEXT_WRAP_NONE,
			.textAlignment = CLAY_TEXT_ALIGN_SHRINK,
			.userData = { .contraction = TextContraction_EllipseRight },
	}));
	
	CLAY({ .layout = { .sizing = { .width=CLAY_SIZING_GROW(0) } } } ) {}
	
	Str8 btnIdStr = PrintInArenaStr(uiArena, "Header_Item%llu_%.*s_DeleteBtn", index, StrPrint(header->key));
	if (ClayBtnStrEx(btnIdStr, StrLit("Del"), Str8_Empty, true, false, nullptr))
	{
		if (!app->removedHeaderThisFrame)
		{
			VarArray* headerArray = (VarArray*)list->contextPntr;
			FreeStr8(stdHeap, &header->key);
			FreeStr8(stdHeap, &header->value);
			VarArrayRemoveAt(Str8Pair, headerArray, index);
			app->removedHeaderThisFrame = true;
		}
	} Clay__CloseElement();
}

// +==============================+
// |      RenderContentItem       |
// +==============================+
// void RenderContentItem(UiListView* list, void* item, uxx index, bool isSelected, bool isHovered)
UI_LIST_VIEW_ITEM_RENDER_DEF(RenderContentItem)
{
	Str8Pair* contentItem = (Str8Pair*)((UiListViewItem*)item)->contextPntr;
	if (app->removedContentThisFrame) { contentItem--; }
	CLAY_TEXT(
		PrintInArenaStr(uiArena, "%.*s=%.*s", StrPrint(contentItem->key), StrPrint(contentItem->value)),
		CLAY_TEXT_CONFIG({
			.fontId = app->clayUiFontId,
			.fontSize = (u16)app->uiFontSize,
			.textColor = isSelected ? MonokaiDarkGray : MonokaiWhite,
			.wrapMode = CLAY_TEXT_WRAP_NONE,
			.textAlignment = CLAY_TEXT_ALIGN_SHRINK,
			.userData = { .contraction = TextContraction_EllipseRight },
	}));
	
	CLAY({ .layout = { .sizing = { .width=CLAY_SIZING_GROW(0) } } } ) {}
	
	Str8 btnIdStr = PrintInArenaStr(uiArena, "Content_Item%llu_%.*s_DeleteBtn", index, StrPrint(contentItem->key));
	if (ClayBtnStrEx(btnIdStr, StrLit("Del"), Str8_Empty, true, false, nullptr))
	{
		if (!app->removedContentThisFrame)
		{
			VarArray* contentItemArray = (VarArray*)list->contextPntr;
			FreeStr8(stdHeap, &contentItem->key);
			FreeStr8(stdHeap, &contentItem->value);
			VarArrayRemoveAt(Str8Pair, contentItemArray, index);
			app->removedContentThisFrame = true;
		}
	} Clay__CloseElement();
}

// +==============================+
// |          AppUpdate           |
// +==============================+
// bool AppUpdate(PlatformInfo* inPlatformInfo, PlatformApi* inPlatformApi, void* memoryPntr, AppInput* appInput)
EXPORT_FUNC APP_UPDATE_DEF(AppUpdate)
{
	TracyCZoneN(Zone_Func, "AppUpdate", true);
	ScratchBegin(scratch);
	ScratchBegin1(scratch2, scratch);
	ScratchBegin2(scratch3, scratch, scratch2);
	bool renderedFrame = true;
	UpdateDllGlobals(inPlatformInfo, inPlatformApi, memoryPntr, appInput);
	v2i screenSizei = appIn->screenSize;
	v2 screenSize = ToV2Fromi(screenSizei);
	v2 screenCenter = Div(screenSize, 2.0f);
	v2 mousePos = appIn->mouse.position;
	
	UiTextbox* focusableTextboxes[] = {
		&app->urlTextbox,
		&app->headerKeyTextbox, &app->headerValueTextbox, &app->contentKeyTextbox, &app->contentValueTextbox
	};
	
	bool addHeader = false;
	bool canAddHeader = (app->headerKeyTextbox.text.length > 0 && app->headerValueTextbox.text.length > 0);
	bool addContent = false;
	bool canAddContent = (app->contentKeyTextbox.text.length > 0 && app->contentValueTextbox.text.length > 0);
	bool makeRequest = false;
	bool canMakeRequest = (app->urlTextbox.text.length > 0);
	
	// +==============================+
	// |            Update            |
	// +==============================+
	TracyCZoneN(Zone_Update, "Update", true);
	{
		// +==================================+
		// | Handle Ctrl+Plus/Minus/0/Scroll  |
		// +==================================+
		if (IsKeyboardKeyPressed(&appIn->keyboard, Key_Plus) && IsKeyboardKeyDown(&appIn->keyboard, Key_Control))
		{
			AppChangeFontSize(true);
		}
		if (IsKeyboardKeyPressed(&appIn->keyboard, Key_Minus) && IsKeyboardKeyDown(&appIn->keyboard, Key_Control))
		{
			AppChangeFontSize(false);
		}
		if (IsKeyboardKeyPressed(&appIn->keyboard, Key_0) && IsKeyboardKeyDown(&appIn->keyboard, Key_Control))
		{
			app->uiFontSize = DEFAULT_UI_FONT_SIZE;
			app->uiScale = 1.0f;
			bool fontBakeSuccess = AppCreateFonts();
			Assert(fontBakeSuccess);
		}
		if (IsKeyboardKeyDown(&appIn->keyboard, Key_Control) && appIn->mouse.scrollDelta.Y != 0)
		{
			AppChangeFontSize(appIn->mouse.scrollDelta.Y > 0);
		}
		
		// +===============================+
		// | Ctrl+Tilde Toggles Clay Debug |
		// +===============================+
		#if DEBUG_BUILD
		if (IsKeyboardKeyPressed(&appIn->keyboard, Key_Tilde) && IsKeyboardKeyDown(&appIn->keyboard, Key_Control))
		{
			Clay_SetDebugModeEnabled(!Clay_IsDebugModeEnabled());
		}
		#endif
		
		// +==============================+
		// | Tab Cycles Through Textboxes |
		// +==============================+
		if (IsKeyboardKeyPressed(&appIn->keyboard, Key_Tab))
		{
			uxx currentFocusIndex = ArrayCount(focusableTextboxes);
			for (uxx fIndex = 0; fIndex < ArrayCount(focusableTextboxes); fIndex++)
			{
				if (focusableTextboxes[fIndex] == app->focusedTextbox)
				{
					currentFocusIndex = fIndex;
					break;
				}
			}
			
			if (currentFocusIndex < ArrayCount(focusableTextboxes))
			{
				if (IsKeyboardKeyDown(&appIn->keyboard, Key_Shift))
				{
					app->focusedTextbox = currentFocusIndex > 0
						? focusableTextboxes[currentFocusIndex-1]
						: focusableTextboxes[ArrayCount(focusableTextboxes)-1];
				}
				else
				{
					app->focusedTextbox = focusableTextboxes[(currentFocusIndex+1) % ArrayCount(focusableTextboxes)];
				}
			}
			else
			{
				app->focusedTextbox = focusableTextboxes[0];
			}
		}
		
		// +==============================+
		// |    Handle Enter to Commit    |
		// +==============================+
		if (IsKeyboardKeyPressed(&appIn->keyboard, Key_Enter))
		{
			if (IsKeyboardKeyDown(&appIn->keyboard, Key_Control))
			{
				makeRequest = true;
			}
			else
			{
				if (app->focusedTextbox == &app->headerKeyTextbox || app->focusedTextbox == &app->headerValueTextbox)
				{
					addHeader = true;
					if (app->focusedTextbox->text.length > 0)
					{
						if (app->focusedTextbox == &app->headerValueTextbox) { app->focusedTextbox = &app->headerKeyTextbox; }
						else { app->focusedTextbox = &app->headerValueTextbox; }
					}
				}
				if (app->focusedTextbox == &app->contentKeyTextbox || app->focusedTextbox == &app->contentValueTextbox)
				{
					addContent = true;
					if (app->focusedTextbox->text.length > 0)
					{
						if (app->focusedTextbox == &app->contentValueTextbox) { app->focusedTextbox = &app->contentKeyTextbox; }
						else { app->focusedTextbox = &app->contentValueTextbox; }
					}
				}
			}
		}
	}
	TracyCZoneEnd(Zone_Update);
	
	// +==============================+
	// |            Render            |
	// +==============================+
	TracyCZoneN(Zone_Render, "Render", true);
	SetTextBackgroundColor(MonokaiBack);
	BeginFrame(platform->GetSokolSwapchain(), screenSizei, MonokaiBack, 1.0f);
	{
		BindShader(&app->mainShader);
		ClearDepthBuffer(1.0f);
		SetDepth(1.0f);
		mat4 projMat = Mat4_Identity;
		TransformMat4(&projMat, MakeScaleXYZMat4(1.0f/(screenSize.Width/2.0f), 1.0f/(screenSize.Height/2.0f), 1.0f));
		TransformMat4(&projMat, MakeTranslateXYZMat4(-1.0f, -1.0f, 0.0f));
		TransformMat4(&projMat, MakeScaleYMat4(-1.0f));
		SetProjectionMat(projMat);
		SetViewMat(Mat4_Identity);
		
		uiArena = scratch3;
		FlagSet(uiArena->flags, ArenaFlag_DontPop);
		uxx uiArenaMark = ArenaGetMark(uiArena);
		
		v2 scrollContainerInput = IsKeyboardKeyDown(&appIn->keyboard, Key_Control) ? V2_Zero : appIn->mouse.scrollDelta;
		UpdateClayScrolling(&app->clay.clay, 16.6f, false, scrollContainerInput, false);
		BeginClayUIRender(&app->clay.clay, screenSize, false, mousePos, IsMouseBtnDown(&appIn->mouse, MouseBtn_Left));
		
		FontAtlas* uiFontAtlas = GetFontAtlas(&app->uiFont, app->uiFontSize, UI_FONT_STYLE);
		NotNull(uiFontAtlas);
		r32 fontHeight = uiFontAtlas->lineHeight;
		
		// +==============================+
		// |          Render UI           |
		// +==============================+
		CLAY({ .id = CLAY_ID("FullscreenContainer"),
			.layout = {
				.layoutDirection = CLAY_TOP_TO_BOTTOM,
				.sizing = { .width = CLAY_SIZING_GROW(0, screenSize.Width), .height = CLAY_SIZING_GROW(0, screenSize.Height) },
				.padding = CLAY_PADDING_ALL(UI_U16(8)),
			},
			.backgroundColor = MonokaiBack,
		})
		{
			// +==============================+
			// |         URL Textbox          |
			// +==============================+
			CLAY({ .id = CLAY_ID("UrlRow"),
				.layout = {
					.sizing = { .width = CLAY_SIZING_GROW(0) },
					.childGap = UI_U16(8),
					.layoutDirection = CLAY_LEFT_TO_RIGHT,
					.childAlignment = { .y = CLAY_ALIGN_Y_CENTER },
				},
			})
			{
				CLAY_TEXT(
					StrLit("URL:"),
					CLAY_TEXT_CONFIG({
						.fontId = app->clayUiBoldFontId,
						.fontSize = (u16)app->uiFontSize,
						.textColor = MonokaiWhite,
						.wrapMode = CLAY_TEXT_WRAP_NONE,
						.textAlignment = CLAY_TEXT_ALIGN_LEFT,
				}));
				
				DoUiTextbox(&app->urlTextbox, &app->clay, uiArena, &appIn->keyboard, &appIn->mouse, &app->focusedTextbox, &app->uiFont, UI_FONT_STYLE, app->uiFontSize, app->uiScale);
			}
			
			// +==============================+
			// |          Inputs Row          |
			// +==============================+
			CLAY({ .id = CLAY_ID("InputsRow"),
				.layout = {
					.sizing = { .width = CLAY_SIZING_FIXED(screenSize.Width), .height = CLAY_SIZING_PERCENT(0.40f) },
					.childGap = UI_U16(8),
					.layoutDirection = CLAY_LEFT_TO_RIGHT,
				},
			})
			{
				// +==============================+
				// |         Headers List         |
				// +==============================+
				CLAY({ .id = CLAY_ID("HeaderCol"),
					.layout = {
						.sizing = { .width = CLAY_SIZING_PERCENT(0.40f), .height = CLAY_SIZING_GROW(0) },
						.layoutDirection = CLAY_TOP_TO_BOTTOM,
					},
				})
				{
					CLAY_TEXT(
						StrLit("Headers:"),
						CLAY_TEXT_CONFIG({
							.fontId = app->clayUiBoldFontId,
							.fontSize = (u16)app->uiFontSize,
							.textColor = MonokaiWhite,
							.wrapMode = CLAY_TEXT_WRAP_NONE,
							.textAlignment = CLAY_TEXT_ALIGN_LEFT,
					}));
					
					{
						UiListViewItem* headerListItems = nullptr;
						if (app->httpHeaders.length > 0)
						{
							headerListItems = AllocArray(UiListViewItem, scratch, app->httpHeaders.length);
							NotNull(headerListItems);
							VarArrayLoop(&app->httpHeaders, hIndex)
							{
								VarArrayLoopGet(Str8Pair, header, &app->httpHeaders, hIndex);
								UiListViewItem* item = &headerListItems[hIndex];
								ClearPointer(item);
								item->idStr = AllocStr8(scratch, header->key);
								item->render = RenderHeaderItem;
								item->contextPntr = (void*)header;
							}
						}
						app->headersListView.contextPntr = (void*)&app->httpHeaders;
						app->removedHeaderThisFrame = false;
						DoUiListView(&app->headersListView, &app->clay, uiArena, &appIn->keyboard, &appIn->mouse,
							app->uiScale, CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0), 0,
							app->httpHeaders.length, headerListItems);
						
						if (app->headersListView.selectionChanged)
						{
							app->headersListView.selectionChanged = false;
							if (app->headersListView.selectionActive && !app->removedHeaderThisFrame)
							{
								if (!app->editedHeaderInputSinceFilled || (app->headerKeyTextbox.text.length == 0 && app->headerValueTextbox.text.length == 0))
								{
									Str8Pair* header = VarArrayGet(Str8Pair, &app->httpHeaders, app->headersListView.selectionIndex);
									UiTextboxSetText(&app->headerKeyTextbox, header->key);
									UiTextboxSetText(&app->headerValueTextbox, header->value);
									app->headerKeyTextbox.textChanged = false;
									app->headerValueTextbox.textChanged = false;
									app->editedHeaderInputSinceFilled = false;
								}
							}
							else if (!app->headersListView.selectionActive && !app->editedHeaderInputSinceFilled)
							{
								UiTextboxClear(&app->headerKeyTextbox);
								UiTextboxClear(&app->headerValueTextbox);
								app->headerKeyTextbox.textChanged = false;
								app->headerValueTextbox.textChanged = false;
							}
						}
					}
					
					CLAY({ .layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { .width=CLAY_SIZING_GROW(0) } } })
					{
						CLAY({ .layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { .width=CLAY_SIZING_GROW(0) } } })
						{
							CLAY_TEXT(
								StrLit("Key:"),
								CLAY_TEXT_CONFIG({
									.fontId = app->clayUiBoldFontId,
									.fontSize = (u16)app->uiFontSize,
									.textColor = MonokaiWhite,
									.wrapMode = CLAY_TEXT_WRAP_NONE,
									.textAlignment = CLAY_TEXT_ALIGN_LEFT,
							}));
							
							DoUiTextbox(&app->headerKeyTextbox, &app->clay, uiArena, &appIn->keyboard, &appIn->mouse, &app->focusedTextbox, &app->uiFont, UI_FONT_STYLE, app->uiFontSize, app->uiScale);
							if (app->headerKeyTextbox.textChanged)
							{
								app->headerKeyTextbox.textChanged = false;
								app->editedHeaderInputSinceFilled = true;
							}
						}
						
						CLAY({ .layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { .width=CLAY_SIZING_GROW(0) } } })
						{
							CLAY_TEXT(
								StrLit("Value:"),
								CLAY_TEXT_CONFIG({
									.fontId = app->clayUiBoldFontId,
									.fontSize = (u16)app->uiFontSize,
									.textColor = MonokaiWhite,
									.wrapMode = CLAY_TEXT_WRAP_NONE,
									.textAlignment = CLAY_TEXT_ALIGN_LEFT,
							}));
							
							DoUiTextbox(&app->headerValueTextbox, &app->clay, uiArena, &appIn->keyboard, &appIn->mouse, &app->focusedTextbox, &app->uiFont, UI_FONT_STYLE, app->uiFontSize, app->uiScale);
							if (app->headerValueTextbox.textChanged)
							{
								app->headerValueTextbox.textChanged = false;
								app->editedHeaderInputSinceFilled = true;
							}
						}
						
						CLAY({ .layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { .height=CLAY_SIZING_GROW(0) }, .childAlignment = { .y = CLAY_ALIGN_Y_CENTER } } })
						{
							CLAY({ .layout = { .sizing = { .height=CLAY_SIZING_FIXED(fontHeight) } } }) { }
							
							if (ClayBtnStrEx(StrLit("HeaderAddBtn"), StrLit("Add"), Str8_Empty, canAddHeader, false, nullptr))
							{
								addHeader = true;
							} Clay__CloseElement();
						}
					}
				}
				
				// +==============================+
				// |         Content List         |
				// +==============================+
				CLAY({ .id = CLAY_ID("ContentCol"),
					.layout = {
						.sizing = { .width = CLAY_SIZING_PERCENT(0.40f), .height = CLAY_SIZING_GROW(0) },
						.layoutDirection = CLAY_TOP_TO_BOTTOM,
					},
				})
				{
					CLAY_TEXT(
						StrLit("Content:"),
						CLAY_TEXT_CONFIG({
							.fontId = app->clayUiBoldFontId,
							.fontSize = (u16)app->uiFontSize,
							.textColor = MonokaiWhite,
							.wrapMode = CLAY_TEXT_WRAP_NONE,
							.textAlignment = CLAY_TEXT_ALIGN_LEFT,
					}));
					
					{
						UiListViewItem* contentListItems = nullptr;
						if (app->httpContent.length > 0)
						{
							contentListItems = AllocArray(UiListViewItem, scratch, app->httpContent.length);
							NotNull(contentListItems);
							VarArrayLoop(&app->httpContent, cIndex)
							{
								VarArrayLoopGet(Str8Pair, contentItem, &app->httpContent, cIndex);
								UiListViewItem* item = &contentListItems[cIndex];
								ClearPointer(item);
								item->idStr = AllocStr8(scratch, contentItem->key);
								item->render = RenderContentItem;
								item->contextPntr = (void*)contentItem;
							}
						}
						app->contentListView.contextPntr = (void*)&app->httpContent;
						app->removedContentThisFrame = false;
						DoUiListView(&app->contentListView, &app->clay, uiArena, &appIn->keyboard, &appIn->mouse,
							app->uiScale, CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0), 0,
							app->httpContent.length, contentListItems);
						
						if (app->contentListView.selectionChanged)
						{
							app->contentListView.selectionChanged = false;
							if (app->contentListView.selectionActive && !app->removedContentThisFrame)
							{
								if (!app->editedContentInputSinceFilled || (app->contentKeyTextbox.text.length == 0 && app->contentValueTextbox.text.length == 0))
								{
									Str8Pair* contentItem = VarArrayGet(Str8Pair, &app->httpContent, app->contentListView.selectionIndex);
									UiTextboxSetText(&app->contentKeyTextbox, contentItem->key);
									UiTextboxSetText(&app->contentValueTextbox, contentItem->value);
									app->contentKeyTextbox.textChanged = false;
									app->contentValueTextbox.textChanged = false;
									app->editedContentInputSinceFilled = false;
								}
							}
							else if (!app->contentListView.selectionActive && !app->editedContentInputSinceFilled)
							{
								UiTextboxClear(&app->contentKeyTextbox);
								UiTextboxClear(&app->contentValueTextbox);
								app->contentKeyTextbox.textChanged = false;
								app->contentValueTextbox.textChanged = false;
							}
						}
					}
					
					CLAY({ .layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { .width=CLAY_SIZING_GROW(0) } } })
					{
						CLAY({ .layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { .width=CLAY_SIZING_GROW(0) } } })
						{
							CLAY_TEXT(
								StrLit("Key:"),
								CLAY_TEXT_CONFIG({
									.fontId = app->clayUiBoldFontId,
									.fontSize = (u16)app->uiFontSize,
									.textColor = MonokaiWhite,
									.wrapMode = CLAY_TEXT_WRAP_NONE,
									.textAlignment = CLAY_TEXT_ALIGN_LEFT,
							}));
							
							DoUiTextbox(&app->contentKeyTextbox, &app->clay, uiArena, &appIn->keyboard, &appIn->mouse, &app->focusedTextbox, &app->uiFont, UI_FONT_STYLE, app->uiFontSize, app->uiScale);
							if (app->contentKeyTextbox.textChanged)
							{
								app->contentKeyTextbox.textChanged = false;
								app->editedContentInputSinceFilled = true;
							}
						}
						
						CLAY({ .layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { .width=CLAY_SIZING_GROW(0) } } })
						{
							CLAY_TEXT(
								StrLit("Value:"),
								CLAY_TEXT_CONFIG({
									.fontId = app->clayUiBoldFontId,
									.fontSize = (u16)app->uiFontSize,
									.textColor = MonokaiWhite,
									.wrapMode = CLAY_TEXT_WRAP_NONE,
									.textAlignment = CLAY_TEXT_ALIGN_LEFT,
							}));
							
							DoUiTextbox(&app->contentValueTextbox, &app->clay, uiArena, &appIn->keyboard, &appIn->mouse, &app->focusedTextbox, &app->uiFont, UI_FONT_STYLE, app->uiFontSize, app->uiScale);
							if (app->contentValueTextbox.textChanged)
							{
								app->contentValueTextbox.textChanged = false;
								app->editedContentInputSinceFilled = true;
							}
						}
						
						CLAY({ .layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { .height=CLAY_SIZING_GROW(0) } } })
						{
							CLAY({ .layout = { .sizing = { .height=CLAY_SIZING_FIXED(fontHeight) } } }) { }
							
							if (ClayBtnStrEx(StrLit("ContentAddBtn"), StrLit("Add"), Str8_Empty, canAddContent, false, nullptr))
							{
								addContent = true;
							} Clay__CloseElement();
						}
					}
				}
			}
			
			// +==============================+
			// |          Submit Row          |
			// +==============================+
			CLAY({ .id = CLAY_ID("SubmitRow"),
				.layout = {
					.sizing = { .width = CLAY_SIZING_GROW(0), },
					.layoutDirection = CLAY_LEFT_TO_RIGHT,
					.padding = CLAY_PADDING_ALL(UI_U16(8)),
					.childGap = UI_U16(8),
				},
			})
			{
				if (ClayBtn(GetHttpVerbStr(app->httpVerb), "", true, false, nullptr))
				{
					if (app->httpVerb+1 < HttpVerb_Count) { app->httpVerb = (HttpVerb)(app->httpVerb + 1); }
					else { app->httpVerb = (HttpVerb)1; }
				} Clay__CloseElement();
				
				if (ClayBtn("Make Request", "", canMakeRequest, true, nullptr))
				{
					makeRequest = true;
				} Clay__CloseElement();
			}
			
			CLAY({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(UI_R32(4)) } } }) {}
			CLAY({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(UI_R32(1)) } }, .backgroundColor = MonokaiGray1 })
			{
				CLAY({
					.layout = {
						.sizing = { .width = CLAY_SIZING_FIT(0), .height = CLAY_SIZING_FIT(0) },
						.padding = { .left = UI_U16(8), .right = UI_U16(8) },
						.childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
					},
					.floating = {
						.attachTo = CLAY_ATTACH_TO_PARENT,
						.attachPoints = {
							.parent = CLAY_ATTACH_POINT_CENTER_CENTER,
							.element = CLAY_ATTACH_POINT_CENTER_CENTER,
						},
					},
					.backgroundColor = MonokaiBack,
				})
				{
					CLAY_TEXT(
						StrLit("Result"),
						CLAY_TEXT_CONFIG({
							.fontId = app->clayUiBoldFontId,
							.fontSize = (u16)app->uiFontSize,
							.textColor = MonokaiGray1,
							.wrapMode = CLAY_TEXT_WRAP_NONE,
							.textAlignment = CLAY_TEXT_ALIGN_LEFT,
					}));
				}
			}
			CLAY({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(UI_R32(8)) } } }) {}
			
			// +==============================+
			// |          Result Row          |
			// +==============================+
			CLAY({ .id = CLAY_ID("ResultRow"),
				.layout = {
					.sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0) },
					.layoutDirection = CLAY_LEFT_TO_RIGHT,
					.childGap = UI_U16(8),
				},
			})
			{
				// +==============================+
				// |         History List         |
				// +==============================+
				UiListViewItem* historyListItems = nullptr;
				if (app->history.length > 0)
				{
					historyListItems = AllocArray(UiListViewItem, scratch, app->history.length);
					NotNull(historyListItems);
					for (uxx hIndex = app->history.length; hIndex > 0; hIndex--)
					{
						VarArrayLoopGet(HistoryItem, historyItem, &app->history, hIndex-1);
						UiListViewItem* item = &historyListItems[app->history.length - hIndex];
						ClearPointer(item);
						item->idStr = PrintInArenaStr(uiArena, "History%llu", historyItem->id);
						item->displayStr = AllocStr8(uiArena, historyItem->url);
						item->font = &app->uiFont;
						item->fontStyle = UI_FONT_STYLE;
						item->fontSize = app->uiFontSize;
						item->contraction = TextContraction_EllipseLeft;
					}
				}
				DoUiListView(&app->historyListView, &app->clay, uiArena, &appIn->keyboard, &appIn->mouse,
					app->uiScale, CLAY_SIZING_PERCENT(0.20f), CLAY_SIZING_GROW(0), 0,
					app->history.length, historyListItems);
				
				// +==============================+
				// |        Result TabView        |
				// +==============================+
				CLAY({ .id = CLAY_ID("ResultTabList"),
					.layout = {
						.sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0) },
						.layoutDirection = CLAY_TOP_TO_BOTTOM,
					},
				})
				{
					CLAY({ .layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT } })
					{
						for (uxx tIndex = 1; tIndex < ResultTab_Count; tIndex++)
						{
							ResultTab tab = (ResultTab)tIndex;
							ClayId tabId = ToClayIdPrint(uiArena, "%sTab", GetResultTabStr(tab));
							bool isHovered = IsMouseOverClay(tabId);
							
							if (isHovered && IsMouseBtnPressed(&appIn->mouse, MouseBtn_Left))
							{
								app->currentResultTab = tab;
							}
							
							CLAY({ .id = tabId,
								.layout = {
									.sizing = { .width = CLAY_SIZING_FIT(30, 0) },
									.padding = { .left = UI_U16(12), .right = UI_U16(12), .top = UI_U16(4), .bottom = UI_U16(4) },
									.childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
								},
								.backgroundColor = (app->currentResultTab == tab) ? MonokaiDarkGray : (isHovered ? MonokaiGray2 : MonokaiBack),
								.cornerRadius = { .topLeft = UI_R32(3), .topRight = UI_R32(3) },
							})
							{
								CLAY_TEXT(
									StrLit(GetResultTabStr(tab)),
									CLAY_TEXT_CONFIG({
										.fontId = app->clayUiBoldFontId,
										.fontSize = (u16)app->uiFontSize,
										.textColor = MonokaiWhite,
										.wrapMode = CLAY_TEXT_WRAP_NONE,
										.textAlignment = CLAY_TEXT_ALIGN_LEFT,
								}));
							}
						}
					}
					
					// +==============================+
					// |       Result Container       |
					// +==============================+
					CLAY({ .id = CLAY_ID("ResultContainer"),
						.layout = {
							.sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0) },
							.padding = CLAY_PADDING_ALL(UI_BORDER(2)),
						},
						.backgroundColor = MonokaiDarkGray,
					})
					{
						switch (app->currentResultTab)
						{
							case ResultTab_Raw:
							{
								CLAY_TEXT(
									StrLit("Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum."),
									CLAY_TEXT_CONFIG({
										.fontId = app->clayUiFontId,
										.fontSize = (u16)app->uiFontSize,
										.textColor = MonokaiWhite,
										.wrapMode = CLAY_TEXT_WRAP_WORDS,
										.textAlignment = CLAY_TEXT_ALIGN_LEFT,
								}));
							} break;
							
							default: 
							{
								CLAY_TEXT(
									StrLit("Not Implemented Yet!"),
									CLAY_TEXT_CONFIG({
										.fontId = app->clayUiFontId,
										.fontSize = (u16)app->uiFontSize,
										.textColor = MonokaiWhite,
										.wrapMode = CLAY_TEXT_WRAP_WORDS,
										.textAlignment = CLAY_TEXT_ALIGN_LEFT,
								}));
							} break;
						}
					}
				}
			}
		}
		
		
		Clay_RenderCommandArray clayRenderCommands = EndClayUIRender(&app->clay.clay);
		RenderClayCommandArray(&app->clay, &gfx, &clayRenderCommands);
		FlagUnset(uiArena->flags, ArenaFlag_DontPop);
		ArenaResetToMark(uiArena, uiArenaMark);
		uiArena = nullptr;
	}
	EndFrame();
	TracyCZoneEnd(Zone_Render);
	
	if (addHeader && canAddHeader)
	{
		uxx existingIndex = FindStr8PairInArray(&app->httpHeaders, app->headerKeyTextbox.text);
		if (existingIndex < app->httpHeaders.length)
		{
			Str8Pair* existingHeader = VarArrayGet(Str8Pair, &app->httpHeaders, existingIndex);
			if (!StrExactEquals(existingHeader->value, app->headerValueTextbox.text))
			{
				FreeStr8(stdHeap, &existingHeader->value);
				existingHeader->value = AllocStr8(stdHeap, app->headerValueTextbox.text);
			}
		}
		else
		{
			Str8Pair* newHeader = VarArrayAdd(Str8Pair, &app->httpHeaders);
			NotNull(newHeader);
			newHeader->key = AllocStr8(stdHeap, app->headerKeyTextbox.text);
			newHeader->value = AllocStr8(stdHeap, app->headerValueTextbox.text);
		}
		UiTextboxClear(&app->headerKeyTextbox);
		UiTextboxClear(&app->headerValueTextbox);
	}
	if (addContent && canAddContent)
	{
		uxx existingIndex = FindStr8PairInArray(&app->httpContent, app->contentKeyTextbox.text);
		if (existingIndex < app->httpContent.length)
		{
			Str8Pair* existingContentItem = VarArrayGet(Str8Pair, &app->httpContent, existingIndex);
			if (!StrExactEquals(existingContentItem->value, app->contentValueTextbox.text))
			{
				FreeStr8(stdHeap, &existingContentItem->value);
				existingContentItem->value = AllocStr8(stdHeap, app->contentValueTextbox.text);
			}
		}
		else
		{
			Str8Pair* newContentItem = VarArrayAdd(Str8Pair, &app->httpContent);
			NotNull(newContentItem);
			newContentItem->key = AllocStr8(stdHeap, app->contentKeyTextbox.text);
			newContentItem->value = AllocStr8(stdHeap, app->contentValueTextbox.text);
		}
		UiTextboxClear(&app->contentKeyTextbox);
		UiTextboxClear(&app->contentValueTextbox);
	}
	
	// +==============================+
	// |         Make Request         |
	// +==============================+
	if (makeRequest && canMakeRequest)
	{
		HttpRequestArgs args = ZEROED;
		args.verb = app->httpVerb;
		args.urlStr = app->urlTextbox.text;
		args.numHeaders = app->httpHeaders.length;
		args.headers = (Str8Pair*)app->httpHeaders.items;
		args.numContentItems = app->httpContent.length;
		args.contentItems = (Str8Pair*)app->httpContent.items;
		HttpRequest* request = OsMakeHttpRequest(platformInfo->http, &args, appIn->programTime);
		NotNull(request);
		
		HistoryItem* historyItem = VarArrayAdd(HistoryItem, &app->history);
		NotNull(historyItem);
		ClearPointer(historyItem);
		historyItem->arena = stdHeap;
		historyItem->id = app->nextHistoryId;
		app->nextHistoryId++;
		historyItem->httpId = request->id;
		historyItem->url = AllocStr8(stdHeap, app->urlTextbox.text);
		historyItem->verb = app->httpVerb;
		if (app->httpHeaders.length > 0)
		{
			historyItem->numHeaders = app->httpHeaders.length;
			historyItem->headers = AllocArray(Str8Pair, stdHeap, historyItem->numHeaders);
			NotNull(historyItem->headers);
			VarArrayLoop(&app->httpHeaders, hIndex)
			{
				VarArrayLoopGet(Str8Pair, entry, &app->httpHeaders, hIndex);
				historyItem->headers[hIndex].key = AllocStr8(stdHeap, entry->key);
				historyItem->headers[hIndex].value = AllocStr8(stdHeap, entry->value);
			}
		}
		if (app->httpContent.length > 0)
		{
			historyItem->numContentItems = app->httpContent.length;
			historyItem->contentItems = AllocArray(Str8Pair, stdHeap, historyItem->numContentItems);
			NotNull(historyItem->contentItems);
			VarArrayLoop(&app->httpContent, hIndex)
			{
				VarArrayLoopGet(Str8Pair, entry, &app->httpContent, hIndex);
				historyItem->contentItems[hIndex].key = AllocStr8(stdHeap, entry->key);
				historyItem->contentItems[hIndex].value = AllocStr8(stdHeap, entry->value);
			}
		}
	}
	
	ScratchEnd(scratch);
	ScratchEnd(scratch2);
	ScratchEnd(scratch3);
	TracyCZoneEnd(Zone_Func);
	return renderedFrame;
}

// +==============================+
// |          AppClosing          |
// +==============================+
// void AppClosing(PlatformInfo* inPlatformInfo, PlatformApi* inPlatformApi, void* memoryPntr)
EXPORT_FUNC APP_CLOSING_DEF(AppClosing)
{
	ScratchBegin(scratch);
	ScratchBegin1(scratch2, scratch);
	ScratchBegin2(scratch3, scratch, scratch2);
	UpdateDllGlobals(inPlatformInfo, inPlatformApi, memoryPntr, nullptr);
	
	#if BUILD_WITH_IMGUI
	igSaveIniSettingsToDisk(app->imgui->io->IniFilename);
	#endif
	
	ScratchEnd(scratch);
	ScratchEnd(scratch2);
	ScratchEnd(scratch3);
}

// +==============================+
// |          AppGetApi           |
// +==============================+
// AppApi AppGetApi()
EXPORT_FUNC APP_GET_API_DEF(AppGetApi)
{
	AppApi result = ZEROED;
	result.AppInit = AppInit;
	result.AppUpdate = AppUpdate;
	result.AppClosing = AppClosing;
	return result;
}
