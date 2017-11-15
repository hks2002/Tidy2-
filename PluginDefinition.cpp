//this file is part of notepad++
//Copyright (C)2003 Don HO <donho@altern.org>
//
//This program is free software; you can redistribute it and/or
//modify it under the terms of the GNU General Public License
//as published by the Free Software Foundation; either
//version 2 of the License, or (at your option) any later version.
//
//This program is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//GNU General Public License for more details.
//
//You should have received a copy of the GNU General Public License
//along with this program; if not, write to the Free Software
//Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#include "Shlwapi.h"

#include "PluginDefinition.h"
#include "menuCmdID.h"

#include "Js/jsformatter.h"
#include "Tidy/tidy.h"
#include "astyle/astyle_main.h"
#include <regex>
#include <list>
#include <string>
#include <sstream>
#include <fstream>
#include <io.h> // if file exist

#pragma comment(lib,"Tidy\\libTidy.lib")
#pragma comment(lib,"Astyle\\AStyleLib.lib")

using namespace std;

//
// The plugin data that Notepad++ needs
//
FuncItem funcItem[nbFunc];

//
// The data of Notepad++ that you can use in your plugin commands
//
NppData nppData;

//
// Initialize your plugin data here
// It will be called while plugin loading   
void pluginInit(HANDLE hModule)
{
}

//
// Here you can do the clean up, save the parameters (if any) for the next session
//
void pluginCleanUp()
{
}

//
// Initialization of your plugin commands
// You should fill your plugins commands here
void commandMenuInit()
{

	//--------------------------------------------//
	//-- STEP 3. CUSTOMIZE YOUR PLUGIN COMMANDS --//
	//--------------------------------------------//
	// with function :
	// setCommand(int index,                      // zero based number to indicate the order of command
	//            TCHAR *commandName,             // the command name that you want to see in plugin menu
	//            PFUNCPLUGINCMD functionPointer, // the symbol of function (function pointer) associated with this command. The body should be defined below. See Step 4.
	//            ShortcutKey *shortcut,          // optional. Define a shortcut to trigger this command
	//            bool check0nInit                // optional. Make this menu item be checked visually
	//            );
	
	
	setCommand(0, TEXT("Tidy"), doTidy, NULL, false); 
    setCommand(1, TEXT("HTML Cfg"), HTMLcfg, NULL, false);
	setCommand(2, TEXT("JS Cfg"), JScfg, NULL, false);
	setCommand(3, TEXT("Astyle Cfg"), ASTYLEcfg, NULL, false);
	setCommand(4, TEXT("About"), about, NULL, false);
}

//
// Here you can do the clean up (especially for the shortcut)
//
void commandMenuCleanUp()
{
	// Don't forget to deallocate your shortcut here
}


//
// This function help you to initialize your plugin commands
//
bool setCommand(size_t index, TCHAR *cmdName, PFUNCPLUGINCMD pFunc, ShortcutKey *sk, bool check0nInit)
{
	if (index >= nbFunc)
		return false;

	if (!pFunc)
		return false;

	lstrcpy(funcItem[index]._itemName, cmdName);
	funcItem[index]._pFunc = pFunc;
	funcItem[index]._init2Check = check0nInit;
	funcItem[index]._pShKey = sk;

	return true;
}

//----------------------------------------------//
//-- STEP 4. DEFINE YOUR ASSOCIATED FUNCTIONS --//
//----------------------------------------------//

void about()
{
	::MessageBox(NULL, TEXT("ERB Tidy could format the mixed HTML/js/C++/php/jsp content."), TEXT("About"), MB_OK);
}

HWND getCurrentHScintilla()
{
	int which;
	::SendMessage(nppData._nppHandle, NPPM_GETCURRENTSCINTILLA, 0, reinterpret_cast<LPARAM>(&which));
	return (which == 0) ? nppData._scintillaMainHandle : nppData._scintillaSecondHandle;
}

class TidyReadContext {
public:
	int nextPosition;
	int length;
	HWND hScintilla;
};

class TidyWriteContext {
public:
	HWND hScintilla;
};

int TIDY_CALL getByte(void *context) {
	TidyReadContext* tidyContext = reinterpret_cast<TidyReadContext*>(context);

	int returnByte = static_cast<unsigned char>(::SendMessage(tidyContext->hScintilla, SCI_GETCHARAT, tidyContext->nextPosition, 0));

	++(tidyContext->nextPosition);

	return returnByte;
}

void TIDY_CALL ungetByte(void *context, byte b) {
	TidyReadContext* tidyContext = reinterpret_cast<TidyReadContext*>(context);

	--(tidyContext->nextPosition);

}

Bool TIDY_CALL isInputEof(void *context) {
	TidyReadContext* tidyContext = reinterpret_cast<TidyReadContext*>(context);
	if (tidyContext->nextPosition >= tidyContext->length)
	{
		return yes;  // Urgh. Tidy enum.
	}
	else
	{
		return no;
	}
}

void TIDY_CALL putByte(void *context, byte b) {
	TidyWriteContext* tidyContext = reinterpret_cast<TidyWriteContext*>(context);
	::SendMessage(tidyContext->hScintilla, SCI_APPENDTEXT, 1, reinterpret_cast<LPARAM>(&b));
}

static void formatRunProcCallback(const char *in, const char *out, HWND hwin)
{
	if (0 != strcmp(in, out))
		::SendMessage(hwin, SCI_SETTEXT, 0, (LPARAM)out);
}

void createDefaultConfig(const char *configPath, CFG cfgF)
{
	if (cfgF == CFG::HTML) {
		FILE *defaultConfig = fopen(configPath, "wb");
		if (NULL != defaultConfig)
		{
			fprintf(defaultConfig, "%s",
				"indent: auto\r\n"
				"indent-spaces: 2\r\n"
				"wrap: 120\r\n"
				"markup: yes\r\n"
				"\r\n"
				"output-xml: yes\r\n"
				"input-xml: yes\r\n"
				"wrap-asp: no\r\n"
				"wrap-jste: no\r\n"
				"wrap-php: no\r\n"
				"\r\n"
				"doctype: omit\r\n"
				"show-body-only: yes\r\n"
				"omit-optional-tags: yes\r\n"
				"\r\n"
				"drop-empty-elements: no\r\n"
				"drop-empty-paras: no\r\n"
				"join-styles: no\r\n"
				"merge-divs: no\r\n"
				"merge-emphasis: no\r\n"
				"merge-spans: no\r\n"
				"quote-ampersand: no\r\n"
				"quote-nbsp: no\r\n"
				"quote-marks: no\r\n"
				"numeric-entities: yes\r\n");
			fclose(defaultConfig);
		}
	}
	if (cfgF == CFG::JS) {
		FILE *defaultConfig = fopen(configPath, "wb");
		if (NULL != defaultConfig)
		{
			fprintf(defaultConfig, "%s",
				"indentNumber=4\r\n"
				"bracNewLine=false\r\n"
				"indentEmpytLine=false\r\n");
			fclose(defaultConfig);
		}
	}
	if (cfgF == CFG::ASTYLE) {
		FILE *defaultConfig = fopen(configPath, "wb");
		if (NULL != defaultConfig)
		{
			fprintf(defaultConfig, "%s",
				"mode=c\r\n"
				"A3\r\n"
				"s4\r\n"
				"S\r\n"
				"K\r\n"
				"f\r\n"
				"xg\r\n"
				"p\r\n"
				"H\r\n"
				"U\r\n"
				"o\r\n"
				"O\r\n"
				"c\r\n"
				"k3\r\n"
				"W3\r\n"
				"j\r\n"
				"J\r\n"
				"xL\r\n"
				"n\r\n"
				"Z\r\n"
				"z1\r\n");
			fclose(defaultConfig);
		}
	}
}

void getConfigPath(char *result, int bufferSize, const char *configName)
{
#ifdef UNICODE
	TCHAR wideConfigPath[MAX_PATH];
	::SendMessage(nppData._nppHandle, NPPM_GETPLUGINSCONFIGDIR, MAX_PATH, reinterpret_cast<LPARAM>(&wideConfigPath));


	WideCharToMultiByte(CP_UTF8, 0, wideConfigPath, -1, result, bufferSize, NULL, NULL);
	PathCombineA(result, result, configName);

#else

	::SendMessage(nppData._nppHandle, NPPM_GETPLUGINSCONFIGDIR, MAX_PATH, reinterpret_cast<LPARAM>(&result));
	PathCombineA(result, result, configName);
#endif


}

void HTMLcfg()
{
	cfg(CFG::HTML);
}

void JScfg()
{
	cfg(CFG::JS);
}

void ASTYLEcfg()
{
	cfg(CFG::ASTYLE);
}

void cfg(CFG cfgf)
{
	char configPath[MAX_PATH];
	if (cfgf == CFG::HTML) {
		getConfigPath(configPath, MAX_PATH, "HTML.cfg");
	
		if (!tidyFileExists(NULL, configPath))
		{
			createDefaultConfig(configPath, CFG::HTML);
		}
	}
	else if (cfgf == CFG::JS) {
		getConfigPath(configPath, MAX_PATH, "JS.cfg");

		if (!tidyFileExists(NULL, configPath))
		{
			createDefaultConfig(configPath, CFG::JS);
		}
	}
	else if (cfgf == CFG::ASTYLE) {
		getConfigPath(configPath, MAX_PATH, "ASTYLE.cfg");

		if (!tidyFileExists(NULL, configPath))
		{
			createDefaultConfig(configPath, CFG::ASTYLE);
		}
	}
#ifdef UNICODE
	TCHAR wideConfigPath[MAX_PATH];
	MultiByteToWideChar(CP_UTF8, 0, configPath, -1, wideConfigPath, MAX_PATH);
	::SendMessage(nppData._nppHandle, NPPM_DOOPEN, 0, reinterpret_cast<LPARAM>(wideConfigPath));
#else
	::SendMessage(nppData._nppHandle, NPPM_DOOPEN, 0, reinterpret_cast<LPARAM>(configPath));
#endif

}

void doTidy()
{
	HWND currentScintilla = getCurrentHScintilla();
	int originalLength = static_cast<int>(::SendMessage(currentScintilla, SCI_GETLENGTH, 0, 0));
	LRESULT currentBufferID = ::SendMessage(nppData._nppHandle, NPPM_GETCURRENTBUFFERID, 0, 0);
	int encoding = static_cast<int>(::SendMessage(nppData._nppHandle, NPPM_GETBUFFERENCODING, currentBufferID, 0));
	TidyDoc tidyDoc = tidyCreate();

	char configPath[MAX_PATH];
	getConfigPath(configPath, MAX_PATH, "HTML.cfg");
	if (!tidyFileExists(tidyDoc, configPath))
	{
		createDefaultConfig(configPath,CFG::HTML);
	}
	tidyLoadConfigEnc(tidyDoc, configPath, "utf8");


	TidyReadContext context;
	context.nextPosition = 0;
	context.hScintilla = currentScintilla;
	context.length = originalLength;

	TidyInputSource inputSource;
	inputSource.getByte = getByte;
	inputSource.ungetByte = ungetByte;
	inputSource.sourceData = &context;
	inputSource.eof = isInputEof;
	char encodingString[30];

	switch (encoding)
	{
	case 1:  // UTF8
	case 2:  // UCS-2 BE, but stored in scintilla as UTF8
	case 3:  // UCS-2 LE, but stored in scintilla as UTF8
	case 6:  // UCS-2 BE No BOM - probably doesn't even exist
	case 7:  // UCS-2 LE No BOM - probably doesn't even exist
		strcpy(encodingString, "utf8");
		break;

	case 0:  // Ansi
	case 5:  // 7bit
		strcpy(encodingString, "raw");
		break;
	default:
		strcpy(encodingString, "raw");  // Assume it's some form of ASCII/Latin text
		break;

	}
	tidySetCharEncoding(tidyDoc, encodingString);

	::SendMessage(currentScintilla, SCI_BEGINUNDOACTION, 0, 0);

	list<string> scriptList = takeServerScriptTag(currentScintilla);
	list<string> jsList = takeClientScriptTag(currentScintilla);
	tidyParseSource(tidyDoc, &inputSource);
	

	TidyOutputSink outputSink;
	TidyWriteContext writeContext;


	writeContext.hScintilla = reinterpret_cast<HWND>(::SendMessage(nppData._nppHandle, NPPM_CREATESCINTILLAHANDLE, 0, 0));
	if (NULL == writeContext.hScintilla)
	{
		::MessageBox(nppData._nppHandle, TEXT("Could not create new Scintilla handle for tidied output (You're probably low on memory!)"), TEXT("Tidy Errors"), MB_ICONERROR | MB_OK);
	}
	else
	{
		
		
		::SendMessage(writeContext.hScintilla, SCI_ALLOCATE, originalLength, 0);

		outputSink.putByte = putByte;
		outputSink.sinkData = &writeContext;	

		tidyCleanAndRepair(tidyDoc);      
		tidySaveSink(tidyDoc, &outputSink);
		
		//int errorCount = tidyErrorCount(tidyDoc);

		//if (errorCount > 0)
		//{
		//	::MessageBox(nppData._nppHandle, TEXT("Errors were encountered tidying the document"), TEXT("Tidy Errors"), MB_ICONERROR | MB_OK);
		//}
		//else
		//{
			returnClientScriptTag(writeContext.hScintilla, jsList);
			returnServerScriptTag(writeContext.hScintilla, scriptList);

			char *text = reinterpret_cast<char*>(::SendMessage(writeContext.hScintilla, SCI_GETCHARACTERPOINTER, 0, 0));
			int length = static_cast<int>(::SendMessage(writeContext.hScintilla, SCI_GETLENGTH, 0, 0));
			
			::SendMessage(currentScintilla, SCI_CLEARALL, 0, 0);

			::SendMessage(currentScintilla, SCI_APPENDTEXT, static_cast<WPARAM>(length), reinterpret_cast<LPARAM>(text));
			::SendMessage(currentScintilla, SCI_ENDUNDOACTION, 0, 0);

		//}
		::SendMessage(nppData._nppHandle, NPPM_DESTROYSCINTILLAHANDLE, 0, reinterpret_cast<LPARAM>(&writeContext.hScintilla));
	}

	tidyRelease(tidyDoc);
}

list<string> takeClientScriptTag(HWND currentScintilla) {
	
	list<string> scriptList;

	char *reg = "(<script\\b[^>]*>)([\\s\\S]*?)(</script>)";
	int startPos = 0;
	int endPos = SendMessage(currentScintilla, SCI_GETLENGTH, 0, 0);
	int searchFlags = SCFIND_REGEXP | SCFIND_POSIX;

	::SendMessage(currentScintilla, SCI_SETSEARCHFLAGS, (WPARAM)searchFlags, 0);
	::SendMessage(currentScintilla, SCI_SETTARGETRANGE, (WPARAM)startPos, (LPARAM)endPos);

	do {
		startPos = ::SendMessage(currentScintilla, SCI_SEARCHINTARGET, strlen(reg), (LPARAM)reg);

		if (startPos >= 0)
		{
			startPos = ::SendMessage(currentScintilla, SCI_GETTARGETSTART, 0, 0);
			endPos = ::SendMessage(currentScintilla, SCI_GETTARGETEND, 0, 0);
			
			char *JS = new char[endPos - startPos];
			::SendMessage(currentScintilla, SCI_GETTARGETTEXT, endPos - startPos, (LPARAM)JS);
			
			//FILE *fp;
			//fp = fopen("d:\\log.txt", "wb");
			//fprintf(fp, "JS LEN:%d\n%s\n",strlen(JS),JS);
			

			string js = JS;//SCI_GETTARGETTEXT issue? if content between <script></script> less than 3, will add some letter to the result
			smatch matches;
			regex_search(js,matches, regex("<script\\b[^>]*>([\\s\\S]*?)</script>", regex_constants::icase));
			js = matches[0];

			scriptList.push_back(js);
			js = regex_replace(js, regex("(<script\\b[^>]*>)([\\s\\S]*?)(</script>)"), "<div type='text/javascript'></div>");

			int len = js.length();
			//fprintf(fp, "js len:%d\n%s\n", len, js.c_str());
			//fclose(fp);

			::SendMessage(currentScintilla, SCI_REPLACETARGET, len, (LPARAM)js.c_str());
			
			startPos = startPos + len;

			
			::SendMessage(currentScintilla, SCI_SETTARGETSTART, (WPARAM)startPos, 0);
			endPos = SendMessage(currentScintilla, SCI_GETLENGTH, 0, 0);
			::SendMessage(currentScintilla, SCI_SETTARGETEND, (WPARAM)endPos, 0);

			delete[] JS;
		}
	} while (startPos >= 0);
	
	return scriptList;
}

void returnClientScriptTag(HWND currentScintilla, list<string> &jslist) {
	char configPath[MAX_PATH];

	getConfigPath(configPath, MAX_PATH, "JS.cfg");
	if (access(configPath, 0) == -1)
	{
		createDefaultConfig(configPath, CFG::JS);
	}
	int indentNumber = GetPrivateProfileInt(NULL, (LPCTSTR)"indentNumber", 4, (LPTSTR)configPath);
	int bracNewLine = GetPrivateProfileInt(NULL, (LPCTSTR)"bracNewLine", 0, (LPTSTR)configPath);
	int indentEmpytLine = GetPrivateProfileInt(NULL, (LPCTSTR)"indentEmpytLine", 0, (LPTSTR)configPath);

	bool CRPut = true;
	int eolMode = (int)::SendMessage(currentScintilla, SCI_GETEOLMODE, 0, 0);
	switch (eolMode)
	{
	case SC_EOL_CRLF:
		CRPut = true;
		break;
	case SC_EOL_CR:
	case SC_EOL_LF:
		CRPut = false;
	}
	FormatterOption option;
	option.indentChar = ' ';
	option.indentNumber = 4;
	option.CRPut = CRPut;
	option.bracNewLine = bracNewLine;
	option.indentEmpytLine = indentEmpytLine;


	char* reg = "<div type='text/javascript'></div>";
	int startPos = 0;
	int endPos = SendMessage(currentScintilla, SCI_GETLENGTH, 0, 0);
	int searchFlags = SCFIND_REGEXP | SCFIND_POSIX;

	::SendMessage(currentScintilla, SCI_SETSEARCHFLAGS, (WPARAM)searchFlags, 0);
	::SendMessage(currentScintilla, SCI_SETTARGETSTART, (WPARAM)startPos, 0);
	::SendMessage(currentScintilla, SCI_SETTARGETEND, (WPARAM)endPos, 0);


	do {
		startPos = ::SendMessage(currentScintilla, SCI_SEARCHINTARGET, strlen(reg), (LPARAM)reg);

		if (startPos >= 0)
		{
			startPos = ::SendMessage(currentScintilla, SCI_GETTARGETSTART, 0, 0);
			endPos = ::SendMessage(currentScintilla, SCI_GETTARGETEND, 0, 0);

			//FILE *fp;
			//fp = fopen("d:\\log.txt", "wb");


			int column = ::SendMessage(currentScintilla, SCI_GETCOLUMN, startPos, 0);
			//fprintf(fp, "COLUMN :%d\n", column);

			char iniIndent[MAX_PATH] = "";
			for (int i = 0; i < column; ++i) {
				strcat(iniIndent, " ");
			}
			//fclose(fp);

			string jsOrig = jslist.front();
			string jsPreTag, jsContent, jsAfterTag;
			jsPreTag = regex_replace(jsOrig, regex("(<script\\b[^>]*>)([\\s\\S]*?)(</script>)"), "$1");
			jsContent = regex_replace(jsOrig, regex("(<script\\b[^>]*>)([\\s\\S]*?)(</script>)"), "$2");
			jsAfterTag = regex_replace(jsOrig, regex("(<script\\b[^>]*>)([\\s\\S]*?)(</script>)"), "$3");

			jslist.pop_front();

			JSFormatter jsformatter((char*)jsContent.c_str(), option);
			jsformatter.SetInitIndent(iniIndent);
			jsformatter.Go();

			string jsFormatted;
			jsFormatted = jsformatter.GetFormattedJs();

			switch (eolMode)
			{
			case SC_EOL_CRLF:
				jsFormatted = regex_replace(jsFormatted, regex("([\\r\\n]{2})([\\s]*)(\\})"), "\r\n$2$3");//fix two newline before '}'
				break;
			case SC_EOL_CR:
				jsFormatted = regex_replace(jsFormatted, regex("([\\n]{2})([\\s]*)(\\})"), "\n$2$3");//fix two newline before '}'
				break;
			case SC_EOL_LF:
				jsFormatted = regex_replace(jsFormatted, regex("([\\r]{2})([\\s]*)(\\})"), "\r$2$3");//fix two newline before '}'

			}

			if (jsFormatted.length() > 0) {
				jsAfterTag.insert(0, iniIndent);
				jsFormatted.insert(0, jsPreTag).append(jsAfterTag);
			}
			else {
				jsFormatted.insert(0, jsPreTag).append(jsAfterTag);
			}

			int len = jsFormatted.length();
			::SendMessage(currentScintilla, SCI_REPLACETARGET, len, (LPARAM)jsFormatted.c_str());

			startPos = startPos + len;
			::SendMessage(currentScintilla, SCI_SETTARGETSTART, (WPARAM)startPos, 0);
			endPos = SendMessage(currentScintilla, SCI_GETLENGTH, 0, 0);
			::SendMessage(currentScintilla, SCI_SETTARGETEND, (WPARAM)endPos, 0);

		}
	} while (startPos >= 0);

}

list<string> takeServerScriptTag(HWND currentScintilla) {

	list<string> SCRIPTList;

	char *reg = "<%([\\s\\S]*?)%>";
	int startPos = 0;
	int endPos = SendMessage(currentScintilla, SCI_GETLENGTH, 0, 0);
	int searchFlags = SCFIND_REGEXP | SCFIND_POSIX;

	::SendMessage(currentScintilla, SCI_SETSEARCHFLAGS, (WPARAM)searchFlags, 0);
	::SendMessage(currentScintilla, SCI_SETTARGETRANGE, (WPARAM)startPos, (LPARAM)endPos);

	do {
		startPos = ::SendMessage(currentScintilla, SCI_SEARCHINTARGET, strlen(reg), (LPARAM)reg);
		
		if (startPos >= 0)
		{
			startPos = ::SendMessage(currentScintilla, SCI_GETTARGETSTART, 0, 0);
			endPos = ::SendMessage(currentScintilla, SCI_GETTARGETEND, 0, 0);

			char *SCRIPT = new char[endPos - startPos];
			::SendMessage(currentScintilla, SCI_GETTARGETTEXT, endPos - startPos, (LPARAM)SCRIPT);

			//FILE *fp;
			//fp = fopen("d:\\log.txt", "wb");
			//fprintf(fp, "JS LEN:%d\n%s\n",strlen(JS),JS);


			string script = SCRIPT;//SCI_GETTARGETTEXT issue? if content between <script></script> less than 3, will add some letter to the result
			smatch matches;
			regex_search(script, matches, regex("<%([\\s\\S]*?)%>", regex_constants::icase));
			script = matches[0];
			
			SCRIPTList.push_back(script);

			script = regex_replace(script, regex("<%([\\s\\S]*?)%>"), "<%%>");

			int len = script.length();
			//fprintf(fp, "js len:%d\n%s\n", len, js.c_str());
			//fclose(fp);

			::SendMessage(currentScintilla, SCI_REPLACETARGET, len, (LPARAM)script.c_str());

			startPos = startPos + len;


			::SendMessage(currentScintilla, SCI_SETTARGETSTART, (WPARAM)startPos, 0);
			endPos = SendMessage(currentScintilla, SCI_GETLENGTH, 0, 0);
			::SendMessage(currentScintilla, SCI_SETTARGETEND, (WPARAM)endPos, 0);

			delete[] SCRIPT;
		}
	} while (startPos >= 0);

	return SCRIPTList;
}

void returnServerScriptTag(HWND currentScintilla, list<string> &scriptlist) {
		
	char* reg = "<[ ]{0,2}%%[ ]{0,2}>"; //js formatter will change <%%> to <  %%  >, so consider this condition
	int startPos = 0;
	int endPos = SendMessage(currentScintilla, SCI_GETLENGTH, 0, 0);
	int searchFlags = SCFIND_REGEXP | SCFIND_POSIX;
	int lastEndPos = 0;
	int lastIndent = 0;

	::SendMessage(currentScintilla, SCI_SETSEARCHFLAGS, (WPARAM)searchFlags, 0);
	::SendMessage(currentScintilla, SCI_SETTARGETSTART, (WPARAM)startPos, 0);
	::SendMessage(currentScintilla, SCI_SETTARGETEND, (WPARAM)endPos, 0);


	do {
		startPos = ::SendMessage(currentScintilla, SCI_SEARCHINTARGET, strlen(reg), (LPARAM)reg);

		if (startPos >= 0)
		{
			startPos = ::SendMessage(currentScintilla, SCI_GETTARGETSTART, 0, 0);
			endPos = ::SendMessage(currentScintilla, SCI_GETTARGETEND, 0, 0);

			//FILE *fp;
			//fp = fopen("d:\\log.txt", "ab+");

			int line= ::SendMessage(currentScintilla, SCI_LINEFROMPOSITION, startPos, 0);
			int column = ::SendMessage(currentScintilla, SCI_GETCOLUMN, startPos, 0);
			
			string iniIndent;
			if (startPos == lastEndPos) {				
				for (int i = 0; i < lastIndent; ++i) {
					iniIndent.append(" ");
				}
			}
			else {
				lastIndent = column;
				for (int i = 0; i < column; ++i) {
					iniIndent.append(" ");
				}
			}
		
			string scriptOrig = scriptlist.front();
			string scriptPreTag, scriptContent, scriptAfterTag;

			//fprintf(fp, "scriptOrig:%s\n", scriptOrig.c_str());

			scriptPreTag = regex_replace(scriptOrig, regex("(<%[\\S]*[\\s\\r\\n])([\\s\\S]*?)([\\s\\r\\n]%>)"), "$1");
			scriptContent = regex_replace(scriptOrig, regex("(<%[\\S]*[\\s\\r\\n])([\\s\\S]*?)([\\s\\r\\n]%>)"), "$2");
			scriptAfterTag = regex_replace(scriptOrig, regex("(<%[\\S]*[\\s\\r\\n])([\\s\\S]*?)([\\s\\r\\n]%>)"), "$3");
			
			//fprintf(fp, "scriptPreTag:%s\n", scriptPreTag.c_str());			
			//fprintf(fp, "scriptAfterTag:%s\n", scriptAfterTag.c_str());
			//fprintf(fp, "scriptContent:%s\n", scriptContent.c_str());

			scriptlist.pop_front();
			//use Astyle format
			string scriptFormatted;
			//fprintf(fp, "scriptFormatted:%s\n", scriptFormatted.c_str());
			

			if (startPos == lastEndPos) {
				scriptFormatted = AStyleCode(scriptContent.c_str(), iniIndent.c_str());
				scriptFormatted.insert(0, scriptPreTag);
				scriptFormatted.insert(0, iniIndent);
				scriptFormatted.insert(0, "\n");
				scriptFormatted.append(scriptAfterTag);
			}
			else {
				scriptFormatted = AStyleCode(scriptContent.c_str());
				if (scriptPreTag.substr(0, 3) == "<%=") {
					scriptFormatted.insert(0, scriptPreTag);
					scriptFormatted.append(scriptAfterTag);
				}
				else {			
					scriptFormatted.insert(0, scriptPreTag);
				    scriptFormatted.insert(0, "\n");
					scriptFormatted.append(scriptAfterTag);
				}
			}
			
			//fclose(fp);

			int len = scriptFormatted.length();
			::SendMessage(currentScintilla, SCI_REPLACETARGET, len, (LPARAM)scriptFormatted.c_str());

			lastEndPos = startPos + len;			
			::SendMessage(currentScintilla, SCI_SETTARGETSTART, (WPARAM)lastEndPos, 0);
			endPos = SendMessage(currentScintilla, SCI_GETLENGTH, 0, 0);
			::SendMessage(currentScintilla, SCI_SETTARGETEND, (WPARAM)endPos, 0);

		}
	} while (startPos >= 0);

}

string AStyleCode(const char *input, const char *iniIndent)
{

	astyle::ASFormatter formatter;
	astyle::ASOptions option(formatter);//need define ASTYLE_LIB first, then use it
	vector<string> optionsVector;

	char configPath[MAX_PATH];


	getConfigPath(configPath, MAX_PATH, "ASTYLE.cfg");
	if (access(configPath, 0) == -1)
	{
		createDefaultConfig(configPath, CFG::ASTYLE);
	}

	filebuf fb;
	if (fb.open(configPath, std::ios::in)) {
		istream is(&fb);
		option.importOptions(is, optionsVector);
		fb.close();
	}
	
	string errorInfo;
	option.parseOptions(optionsVector,errorInfo);
	if (errorInfo.length() > 0) {
		::MessageBox(NULL, (LPCTSTR)errorInfo.c_str(), TEXT("Warnning"), MB_OK);
	}

	istringstream in(input);
	astyle::ASStreamIterator<std::istringstream> streamIterator(&in);
	ostringstream out;
	formatter.init(&streamIterator);


	while (formatter.hasMoreLines())
	{
		out << iniIndent <<  formatter.nextLine();
		if (formatter.hasMoreLines())
			out << streamIterator.getOutputEOL();
		else
		{
			// this can happen if the file if missing a closing bracket and break-blocks is requested
			if (formatter.getIsLineReady())
			{
				out << streamIterator.getOutputEOL();
				out << formatter.nextLine();
			}
		}
	}
	
	string str = out.str();
	str = str.erase(0, str.find_first_not_of(" \x03\r\n\t"));//remove fisrt line leading blank
	str = str = str.erase(str.find_last_not_of(" \r\n\t") + 1);
	return str;	
}
