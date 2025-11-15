/*
File:   app_save.c
Author: Taylor Robbins
Date:   08\16\2025
Description: 
	** Holds functions that help us save information to a few files
	** in the AppData folder to track things like history and
	** preferences so we can restore this information when the
	** application is closed and re-opened
*/

// +--------------------------------------------------------------+
// |                          Serialize                           |
// +--------------------------------------------------------------+

Str8 SerializeHistory(Arena* arena, const VarArray* historyList, bool addNullTerm)
{
	TwoPassStr8Loop(result, arena, addNullTerm)
	{
		VarArrayLoop(historyList, hIndex)
		{
			VarArrayLoopGet(HistoryItem, item, historyList, hIndex);
			if (item->finished)
			{
				if (result.index > 0)
				{
					TwoPassChar(&result, '\n');
				}
				TwoPassPrint(&result, "# %s %s %.*s\n", item->failed ? "Failed" : "Succeeded", GetHttpVerbStr(item->verb), StrPrint(item->url));
				if (item->failed)
				{
					TwoPassPrint(&result, "FailureReason: %s\n", GetResultStr(item->failureReason));
				}
				TwoPassPrint(&result, "Status: %u\n", item->responseStatusCode);
				TwoPassPrint(&result, "NumHeaders: %llu\n", item->numHeaders);
				for (uxx headerIndex = 0; headerIndex < item->numHeaders; headerIndex++)
				{
					TwoPassPrint(&result, "\t%.*s: %.*s\n", StrPrint(item->headers[headerIndex].key), StrPrint(item->headers[headerIndex].value));
				}
				TwoPassPrint(&result, "NumContent: %llu\n", item->numContentItems);
				for (uxx cIndex = 0; cIndex < item->numContentItems; cIndex++)
				{
					//NOTE: Content key/value pairs have more characters allowed (like colon) so we need to put key and value on their own lines and surround (with quotes to avoid stripping trailing/leading whitespace)
					TwoPassPrint(&result, "\tKey: \"%.*s\"\n", StrPrint(item->contentItems[cIndex].key));
					TwoPassPrint(&result, "\tValue: \"%.*s\"\n", StrPrint(item->contentItems[cIndex].value));
				}
				//TODO: Should we somehow save response and responseHeaders?
			}
		}
		
		TwoPassStr8LoopEnd(&result);
	}
	return result.str;
}

void SaveHistory(const VarArray* historyList)
{
	ScratchBegin(scratch);
	
	FilePath saveFolderPath = OsGetSettingsSavePath(scratch, Str8_Empty, StrLit(PROJECT_FOLDER_NAME_STR), true);
	NotNull(saveFolderPath.chars);
	FilePath historyFilePath = JoinStringsInArena3(scratch, saveFolderPath, StrLit("/"), StrLit(HISTORY_FILENAME), false);
	
	Str8 serializedHistory = SerializeHistory(scratch, historyList, false);
	if (serializedHistory.length > 0)
	{
		bool writeSuccess = OsWriteTextFile(historyFilePath, serializedHistory);
		if (!writeSuccess)
		{
			PrintLine_E("Failed to save %llu byte history to \"%.*s\"", serializedHistory.length, StrPrint(historyFilePath));
		}
	}
	else if (OsDoesFileExist(historyFilePath))
	{
		//TODO: We need to add a OsDeleteFile function!
		bool writeSuccess = OsWriteTextFile(historyFilePath, StrLit(" "));
		if (!writeSuccess)
		{
			PrintLine_E("Failed to write empty history to \"%.*s\"", StrPrint(historyFilePath));
		}
	}
	
	ScratchEnd(scratch);
}

// +--------------------------------------------------------------+
// |                         Deserialize                          |
// +--------------------------------------------------------------+
Result TryDeserializeHistoryItem(Arena* arena, Str8 fileContents, HistoryItem* itemOut)
{
	Result result = Result_None;
	
	bool foundItemStart = false;
	bool foundStatus = false;
	bool foundFailureReason = false;
	bool foundNumHeaders = false;
	uxx headerIndex = 0;
	bool foundNumContent = false;
	uxx contentIndex = 0;
	bool expectingContentKey = false;
	
	TextParser parser = MakeTextParser(fileContents);
	parser.noComments = true;
	ParsingToken token = ZEROED;
	while (TextParserGetToken(&parser, &token))
	{
		switch (token.type)
		{
			case ParsingTokenType_FilePrefix:
			{
				if (foundItemStart) { result = Result_Duplicate; break; } //TODO: Better result code?
				
				bool spaceCount = 0;
				uxx prevSpaceIndex = 0;
				
				Str8 successPart = Str8_Empty;
				Str8 verbPart = Str8_Empty;
				Str8 urlPart = Str8_Empty;
				for (uxx cIndex = 0; cIndex < token.value.length; cIndex++)
				{
					if (token.value.chars[cIndex] == ' ')
					{
						if (spaceCount >= 2) { result = Result_WrongNumCharacters; break; }
						if (spaceCount == 0)
						{
							successPart = StrSlice(token.value, prevSpaceIndex, cIndex);
						}
						else if (spaceCount == 1)
						{
							verbPart = StrSlice(token.value, prevSpaceIndex, cIndex);
							urlPart = StrSliceFrom(token.value, cIndex+1);
						}
						spaceCount++;
						prevSpaceIndex = cIndex+1;
					}
				}
				if (result != Result_None) { break; }
				
				bool failed = false;
				if (StrAnyCaseEquals(successPart, StrLit("Succeeded"))) { failed = false; }
				else if (StrAnyCaseEquals(successPart, StrLit("Failed"))) { failed = true; }
				else { result = Result_UnknownString; break; }
				
				HttpVerb verb = HttpVerb_None;
				for (uxx vIndex = 0; vIndex < HttpVerb_Count; vIndex++)
				{
					Str8 verbStr = MakeStr8Nt(GetHttpVerbStr((HttpVerb)vIndex));
					if (StrAnyCaseEquals(verbPart, verbStr)) { verb = (HttpVerb)vIndex; break; }
				}
				if (verb == HttpVerb_None) { result = Result_UnknownString; break; }
				
				ClearPointer(itemOut);
				itemOut->arena = arena;
				itemOut->url = AllocStr8(arena, urlPart);
				itemOut->verb = verb;
				itemOut->finished = true;
				itemOut->failed = failed;
				itemOut->response = AllocStr8(arena, StrLit("Responses are not currently saved between sessions..."));
				InitUiLargeText(arena, itemOut->response, &itemOut->responseLargeText);
				InitVarArray(Str8Pair, &itemOut->responseHeaders, arena);
				foundItemStart = true;
			} break;
			
			case ParsingTokenType_KeyValuePair:
			{
				if (!foundItemStart) { result = Result_MissingFileHeader; break; }
				
				if (foundNumHeaders && headerIndex < itemOut->numHeaders)
				{
					itemOut->headers[headerIndex].key = AllocStr8(arena, token.key);
					itemOut->headers[headerIndex].value = AllocStr8(arena, token.value);
					headerIndex++;
				}
				else if (foundNumContent && contentIndex < itemOut->numContentItems)
				{
					if (!StrAnyCaseEquals(token.key, expectingContentKey ? StrLit("Key") : StrLit("Value"))) { result = Result_InvalidType; break; }
					Str8 valuePart = token.value;
					if (!expectingContentKey)
					{
						//strip quotes that prevented leading/trailing whitespace stripping
						if (StrExactStartsWith(valuePart, StrLit("\""))) { valuePart = StrSliceFrom(valuePart, 1); }
						if (StrExactEndsWith(valuePart, StrLit("\""))) { valuePart.length--; }
					}
					if (expectingContentKey) { itemOut->contentItems[contentIndex].key = AllocStr8(arena, valuePart); }
					else { itemOut->contentItems[contentIndex].value = AllocStr8(arena, valuePart); }
					if (!expectingContentKey) { contentIndex++; }
					expectingContentKey = !expectingContentKey;
				}
				else if (StrAnyCaseEquals(token.key, StrLit("Status")))
				{
					if (foundStatus) { result = Result_Duplicate; break; }
					Result parseError = Result_None;
					if (!TryParseU16(token.value, &itemOut->responseStatusCode, &parseError)) { result = parseError; break; }
					foundStatus = true;
				}
				else if (StrAnyCaseEquals(token.key, StrLit("FailureReason")))
				{
					if (foundFailureReason) { result = Result_Duplicate; break; }
					itemOut->failureReason = Result_Count;
					for (uxx rIndex = 0; rIndex < Result_Count; rIndex++)
					{
						Str8 resultStr = MakeStr8Nt(GetResultStr((Result)rIndex));
						if (StrAnyCaseEquals(token.value, resultStr)) { itemOut->failureReason = (Result)rIndex; break; }
					}
					if (itemOut->failureReason == Result_Count) { result = Result_UnknownString; break; }
					foundFailureReason = true;
				}
				else if (StrAnyCaseEquals(token.key, StrLit("NumHeaders")))
				{
					if (foundNumHeaders) { result = Result_Duplicate; break; }
					Result parseError = Result_None;
					if (!TryParseUXX(token.value, &itemOut->numHeaders, &parseError)) { result = parseError; break; }
					if (itemOut->numHeaders > 0)
					{
						itemOut->headers = AllocArray(Str8Pair, arena, itemOut->numHeaders);
						NotNull(itemOut->headers);
					}
					foundNumHeaders = true;
					headerIndex = 0;
				}
				else if (StrAnyCaseEquals(token.key, StrLit("NumContent")))
				{
					if (foundNumContent) { result = Result_Duplicate; break; }
					Result parseError = Result_None;
					if (!TryParseUXX(token.value, &itemOut->numContentItems, &parseError)) { result = parseError; break; }
					if (itemOut->numContentItems > 0)
					{
						itemOut->contentItems = AllocArray(Str8Pair, arena, itemOut->numContentItems);
						NotNull(itemOut->contentItems);
					}
					foundNumContent = true;
					expectingContentKey = true;
					contentIndex = 0;
				}
				else
				{
					PrintLine_E("Unknown key in history file: \"%.*s\"", StrPrint(token.key));
					result = Result_InvalidType;
					break;
				}
			} break;
			
			default: result = Result_InvalidSyntax; break; //treat other token types as invalid syntax
		}
		
		if (result != Result_None) { break; }
	}
	
	if (result == Result_None)
	{
		if (!foundItemStart) { result = Result_MissingFileHeader; }
		else if (!foundNumHeaders) { result = Result_MissingPart; }
		else if (!foundNumContent) { result = Result_MissingPart; }
		else if (foundNumHeaders && headerIndex < itemOut->numHeaders) { result = Result_MissingItems; }
		else if (foundNumContent && contentIndex < itemOut->numContentItems) { result = Result_MissingItems; }
	}
	
	if (result == Result_None) { result = Result_Success; }
	else if (foundItemStart && CanArenaFree(arena)) { FreeHistoryItem(itemOut); }
	return result;
}

Result TryDeserializeHistoryList(Arena* arena, Str8 fileContents, VarArray* listOut, uxx* nextHistoryId)
{
	Result result = Result_None;
	
	#define PARSE_HISTORY_ITEM(slice) do                                                        \
	{                                                                                           \
		HistoryItem item = ZEROED;                                                              \
		Result itemResult = TryDeserializeHistoryItem(arena, slice, &item);                     \
		if (itemResult == Result_Success)                                                       \
		{                                                                                       \
			item.id = *nextHistoryId;                                                           \
			*nextHistoryId = (*nextHistoryId) + 1;                                              \
			                                                                                    \
			HistoryItem* newSpace = VarArrayAdd(HistoryItem, listOut);                          \
			NotNull(newSpace);                                                                  \
			MyMemCopy(newSpace, &item, sizeof(HistoryItem));                                    \
		}                                                                                       \
		else                                                                                    \
		{                                                                                       \
			PrintLine_E("Failed to parse item[%llu]: %s", itemIndex, GetResultStr(itemResult)); \
			result = itemResult;                                                                \
			break;                                                                              \
		}                                                                                       \
		itemIndex++;                                                                            \
	} while(0)
	
	uxx itemStartIndex = 0;
	bool insideItem = false;
	uxx itemIndex = 0;
	TextParser parser = MakeTextParser(fileContents);
	parser.noComments = true;
	ParsingToken token = ZEROED;
	while (TextParserGetToken(&parser, &token))
	{
		switch (token.type)
		{
			case ParsingTokenType_FilePrefix:
			{
				uxx lineStartIndex = (uxx)(token.str.chars - fileContents.chars);
				if (insideItem)
				{
					Str8 prevItemSlice = StrSlice(fileContents, itemStartIndex, lineStartIndex);
					PARSE_HISTORY_ITEM(prevItemSlice);
				}
				itemStartIndex = lineStartIndex;
				insideItem = true;
			} break;
			
			default: break;
		}
		if (result != Result_None) { break; }
	}
	
	if (result == Result_None && insideItem && itemStartIndex < fileContents.length)
	{
		Str8 lastItemSlice = StrSlice(fileContents, itemStartIndex, fileContents.length);
		PARSE_HISTORY_ITEM(lastItemSlice);
	}
	
	if (result == Result_None) { result = (itemIndex > 0) ? Result_Success : Result_EmptyFile; }
	return result;
}

bool LoadHistory(Arena* arena, VarArray* historyList, uxx* nextHistoryId)
{
	ScratchBegin1(scratch, arena);
	bool result = false;
	
	FilePath saveFolderPath = OsGetSettingsSavePath(scratch, Str8_Empty, StrLit(PROJECT_FOLDER_NAME_STR), true);
	NotNull(saveFolderPath.chars);
	FilePath historyFilePath = JoinStringsInArena3(scratch, saveFolderPath, StrLit("/"), StrLit(HISTORY_FILENAME), false);
	
	if (OsDoesFileExist(historyFilePath))
	{
		Str8 historyFileContents = Str8_Empty;
		if (OsReadTextFile(historyFilePath, scratch, &historyFileContents))
		{
			Result parseResult = TryDeserializeHistoryList(arena, historyFileContents, historyList, nextHistoryId);
			if (parseResult == Result_Success || parseResult == Result_EmptyFile)
			{
				PrintLine_D("Loaded %llu history items from \"%.*s\"", historyList->length, StrPrint(historyFilePath));
				result = true;
			}
			else { PrintLine_E("Failed to parse %llu byte history file contents at \"%.*s\"! Error: %s", historyFileContents.length, StrPrint(historyFilePath), GetResultStr(parseResult)); }
		}
		else { PrintLine_W("Failed to open and read history file at \"%.*s\"", StrPrint(historyFilePath)); }
	}
	else { PrintLine_D("No history file at \"%.*s\"", StrPrint(historyFilePath)); }
	
	ScratchEnd(scratch);
	return result;
}
