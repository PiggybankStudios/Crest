/*
File:   app_main.h
Author: Taylor Robbins
Date:   02\25\2025
*/

#ifndef _APP_MAIN_H
#define _APP_MAIN_H

typedef enum ResultTab ResultTab;
enum ResultTab
{
	ResultTab_None = 0,
	ResultTab_Raw,
	ResultTab_JSON,
	ResultTab_Image,
	ResultTab_Meta,
	ResultTab_Count,
};
const char* GetResultTabStr(ResultTab enumValue)
{
	switch (enumValue)
	{
		case ResultTab_Raw:   return "Raw";
		case ResultTab_JSON:  return "JSON";
		case ResultTab_Image: return "Image";
		case ResultTab_Meta:  return "Meta";
		default: return UNKNOWN_STR;
	}
}

typedef enum HttpVerb HttpVerb;
enum HttpVerb
{
	HttpVerb_None = 0,
	HttpVerb_GET,
	HttpVerb_POST,
	HttpVerb_DELETE,
	HttpVerb_Count,
};
const char* GetHttpVerbStr(HttpVerb enumValue)
{
	switch (enumValue)
	{
		case HttpVerb_GET:    return "GET";
		case HttpVerb_POST:   return "POST";
		case HttpVerb_DELETE: return "DELETE";
		default: return UNKNOWN_STR;
	}
}

typedef plex HistoryItem HistoryItem;
plex HistoryItem
{
	Arena* arena;
	Str8 url;
	HttpVerb verb;
	uxx numHeaders;
	Str8Pair* headers;
	uxx numContentItems;
	Str8Pair* contentItems;
};

typedef struct AppData AppData;
struct AppData
{
	bool initialized;
	RandomSeries random;
	AppResources resources;
	
	Shader mainShader;
	PigFont uiFont;
	PigFont largeFont;
	
	ClayUIRenderer clay;
	r32 uiScale;
	r32 uiFontSize;
	u16 clayUiFontId;
	
	UiTextbox* focusedTextbox;
	HttpVerb httpVerb;
	
	UiTextbox urlTextbox;
	UiTextbox headerKeyTextbox;
	UiTextbox headerValueTextbox;
	UiTextbox contentKeyTextbox;
	UiTextbox contentValueTextbox;
	
	VarArray httpHeaders; //Str8Pair
	VarArray httpContent; //Str8Pair
	
	VarArray history; //HistoryItem
	
	ResultTab currentResultTab;
};

#endif //  _APP_MAIN_H
