/*
File:   platform_api.c
Author: Taylor Robbins
Date:   02\25\2025
Description: 
	** Holds implementations for the PlatformApi functions defined in platform_interface.h
*/

// +==============================+
// |  Plat_GetNativeWindowHandle  |
// +==============================+
// OsWindowHandle Plat_GetNativeWindowHandle()
GET_NATIVE_WINDOW_HANDLE_DEF(Plat_GetNativeWindowHandle)
{
	OsWindowHandle result = nullptr;
	#if TARGET_IS_WINDOWS
	{
		#if BUILD_WITH_SOKOL_APP
		result = (OsWindowHandle)sapp_win32_get_hwnd();
		#else
		AssertMsg(false, "Plat_GetNativeWindowHandle doesn't have an implementation for the current window library!");
		#endif
	}
	#else
	AssertMsg(false, "Plat_GetNativeWindowHandle doesn't have an implementation for the current TARGET!");
	#endif
	return result;
}

#if BUILD_WITH_SOKOL_APP

// +==============================+
// |    Plat_GetSokolSwapchain    |
// +==============================+
GET_SOKOL_SWAPCHAIN_DEF(Plat_GetSokolSwapchain)
{
	return GetSokolAppSwapchain();
}

// +==============================+
// |     Plat_SetMouseLocked      |
// +==============================+
// void Plat_SetMouseLocked(bool isMouseLocked)
SET_MOUSE_LOCKED_DEF(Plat_SetMouseLocked)
{
	NotNull(platformData);
	NotNull(platformData->oldAppInput);
	NotNull(platformData->currentAppInput);
	if (platformData->currentAppInput->mouse.isLocked != isMouseLocked)
	{
		sapp_lock_mouse(isMouseLocked);
		//Change the value in both old and current AppInput so the application immediately sees the value change in the AppInput it has a handle to
		platformData->oldAppInput->mouse.isLocked = isMouseLocked;
		platformData->currentAppInput->mouse.isLocked = isMouseLocked;
	}
}

// +==============================+
// |   Plat_SetMouseCursorType    |
// +==============================+
// void Plat_SetMouseCursorType(sapp_mouse_cursor cursorType)
SET_MOUSE_CURSOR_TYPE_DEF(Plat_SetMouseCursorType)
{
	NotNull(platformData);
	NotNull(platformData->oldAppInput);
	NotNull(platformData->currentAppInput);
	sapp_set_mouse_cursor(cursorType);
	if (platformData->currentAppInput->cursorType != cursorType)
	{
		platformData->oldAppInput->cursorType = cursorType;
		platformData->currentAppInput->cursorType = cursorType;
	}
}

// +==============================+
// |     Plat_SetWindowTitle      |
// +==============================+
// void Plat_SetWindowTitle(Str8 windowTitle)
SET_WINDOW_TITLE_DEF(Plat_SetWindowTitle)
{
	ScratchBegin(scratch);
	Str8 windowTitleNt = AllocStrAndCopy(scratch, windowTitle.length, windowTitle.chars, true);
	NotNull(windowTitleNt.chars);
	sapp_set_window_title(windowTitleNt.chars);
	ScratchEnd(scratch);
}

// +==============================+
// |      Plat_SetWindowIcon      |
// +==============================+
// void Plat_SetWindowIcon(uxx numIconSizes, const ImageData* iconSizes)
SET_WINDOW_ICON_DEF(Plat_SetWindowIcon)
{
	Assert(numIconSizes <= SAPP_MAX_ICONIMAGES);
	Assert(iconSizes != nullptr || numIconSizes == 0);
	sapp_icon_desc iconDesc = ZEROED;
	iconDesc.sokol_default = (numIconSizes == 0);
	for (uxx iIndex = 0; iIndex < numIconSizes; iIndex++)
	{
		const ImageData* imageData = &iconSizes[iIndex];
		iconDesc.images[iIndex].width = (int)imageData->size.Width;
		iconDesc.images[iIndex].height = (int)imageData->size.Height;
		iconDesc.images[iIndex].pixels = (sapp_range){ imageData->pixels, sizeof(u32) * imageData->numPixels };
	}
	sapp_set_icon(&iconDesc);
}

// +==============================+
// |     Plat_SetCursorShape      |
// +==============================+
// void Plat_SetCursorShape(CursorShape shape)
SET_CURSOR_SHAPE_DEF(Plat_SetCursorShape)
{
	// TODO: What is SAPP_MOUSECURSOR_ARROW?
	// TODO: What is SAPP_MOUSECURSOR_CROSSHAIR?
	// TODO: Support SAPP_MOUSECURSOR_RESIZE_ALL?
	sapp_mouse_cursor sokolCursor = SAPP_MOUSECURSOR_DEFAULT;
	switch (shape)
	{
		case CursorShape_Default: sokolCursor = SAPP_MOUSECURSOR_DEFAULT; break;
		case CursorShape_Point: sokolCursor = SAPP_MOUSECURSOR_POINTING_HAND; break;
		case CursorShape_IBeam: sokolCursor = SAPP_MOUSECURSOR_IBEAM; break;
		case CursorShape_ResizeHori: sokolCursor = SAPP_MOUSECURSOR_RESIZE_EW; break;
		case CursorShape_ResizeVert: sokolCursor = SAPP_MOUSECURSOR_RESIZE_NS; break;
		case CursorShape_ResizeDiagNwSe: sokolCursor = SAPP_MOUSECURSOR_RESIZE_NWSE; break;
		case CursorShape_ResizeDiagSwNe: sokolCursor = SAPP_MOUSECURSOR_RESIZE_NESW; break;
	}
	sapp_set_mouse_cursor(sokolCursor);
}

#endif //BUILD_WITH_SOKOL_APP
