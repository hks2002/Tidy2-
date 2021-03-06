#include "jsformatter.h"
#include <cstring>
#include <string>
#include <iostream>

using namespace std;

JSFormatter::JSFormatter(char *input,  const FormatterOption &option)
{
    in = input;
    inLen = strlen(in);
    getPos = 0;
    Option = option;
    //for parse-----------------------
    lineCount = 1; // 行号从 1 开始
    tokenCount = 0;
    strBeforeReg = "(,=:[!&|?+{};\n";
    isRegular = false;
    isNeg = false;
    regBracketLevel = 0;
    isFirstToken = true;
    //for parse-----------------------

    initIndentString = ' '; // 起始缩进
    indentCount = 0;

    formattedLineCount = 1;
    lineFormattedVec.resize(1000, -1);

    isLineTemplate = false;
    lineBuffer = "";
    lineIndents = 0;

    isNewLine = false;
    isBlockStmt = false;
    isAssigned = false;
    isEmptyBracket = false;
    isCommentPuted = false;
    isTemplatePuted = false;

    questOperCount = 0;


    blockMap[string("if")] = JS_IF;
    blockMap[string("else")] = JS_ELSE;
    blockMap[string("for")] = JS_FOR;
    blockMap[string("do")] = JS_DO;
    blockMap[string("while")] = JS_WHILE;
    blockMap[string("switch")] = JS_SWITCH;
    blockMap[string("case")] = JS_CASE;
    blockMap[string("default")] = JS_CASE;
    blockMap[string("try")] = JS_TRY;
    blockMap[string("finally")] = JS_TRY; // 等同于 try
    blockMap[string("catch")] = JS_CATCH;
    blockMap[string("function")] = JS_FUNCTION;
    blockMap[string("{")] = JS_BLOCK;
    blockMap[string("(")] = JS_BRACKET;
    blockMap[string("[")] = JS_SQUARE;
    blockMap[string("=")] = JS_ASSIGN;

    specKeywordSet.insert("if");
    specKeywordSet.insert("for");
    specKeywordSet.insert("while");
    specKeywordSet.insert("switch");
    specKeywordSet.insert("catch");
    specKeywordSet.insert("function");
    specKeywordSet.insert("with");
    specKeywordSet.insert("return");
    specKeywordSet.insert("throw");
    specKeywordSet.insert("delete");
}

string JSFormatter::Trim(const string &str)
{
    std::string ret(str);
    ret = ret.erase(ret.find_last_not_of(" \r\n\t") + 1);
    return ret.erase(0, ret.find_first_not_of(" \r\n\t"));
}

string JSFormatter::TrimSpace(const string &str)
{
    std::string ret(str);
    ret = ret.erase(ret.find_last_not_of(" \t") + 1);
    return ret.erase(0, ret.find_first_not_of(" \t"));
}

string JSFormatter::TrimRightSpace(const string &str)
{
    std::string ret(str);
    return ret.erase(ret.find_last_not_of(" \t") + 1);
}

void JSFormatter::StringReplace(string &strBase, const string &strSrc, const string &strDes)
{
    string::size_type pos = 0;
    string::size_type srcLen = strSrc.size();
    string::size_type desLen = strDes.size();
    pos = strBase.find(strSrc, pos);

    while ((pos != string::npos)) {
        strBase.replace(pos, srcLen, strDes);
        pos = strBase.find(strSrc, pos + desLen);
    }
}

bool inline JSFormatter:: IsNormalChar(int ch)
{
    // 一般字符
    return ((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') ||
            (ch >= 'A' && ch <= 'Z') || ch == '_' || ch == '$' ||
            ch > 126 || ch < 0);
}

bool inline JSFormatter::IsNumChar(int ch)
{
    // 数字和.
    return ((ch >= '0' && ch <= '9') || ch == '.');
}

bool inline JSFormatter::IsBlankChar(int ch)
{
    // 空白字符
    return (ch == ' ' || ch == '\t' || ch == '\r');
}

bool inline JSFormatter::IsSingleOper(int ch)
{
    // 单字符符号
    return (ch == '.' || ch == '(' || ch == ')' ||
            ch == '[' || ch == ']' || ch == '{' || ch == '}' ||
            ch == ',' || ch == ';' || ch == '~' ||
            ch == '\n');
}

bool inline JSFormatter::IsQuote(int ch)
{
    // 引号
    return (ch == '\'' || ch == '\"' || ch == '`');
}

bool JSFormatter::IsInlineComment(const Token &token)
{
    if (token.type != TOKENTYPE::COMMENT_TYPE_MULTLINE)
    { return false; }

    return token.inlineComment;
}

bool JSFormatter::IsComment()
{
    // 注释
    return (charA == '/' && (charB == '/' || charB == '*'));
}

int JSFormatter::GetChar()
{
    if (getPos < inLen) {
        getPos++;
        return *in++;
    } else
    { return 0; }
}

void JSFormatter::PutChar(int ch)
{
    out.append(1, ch);
}

void JSFormatter::GetTokenRaw()
{
    if (isFirstToken) {
        charB = GetChar();
    }

    // normal procedure
    if (!isRegular && !isNeg) {
        tokenB.code = "";
        tokenB.type = TOKENTYPE::STRING_TYPE;
        tokenB.line = lineCount;
    } else if (isRegular) {
        //tokenB.push_back('/');
        tokenB.type = TOKENTYPE::REGULAR_TYPE; // 正则
    } else {
        tokenB.type = TOKENTYPE::STRING_TYPE; // 正负数
    }

    bool isQuote = false;
    bool isComment = false;
    bool isRegularFlags = false;
    bool isFirst = true;
    bool isNum = false; // 是不是数字
    bool isLineBegin = false;
    char quote; // 记录引号类型 ' 或 "
    char comment; // 注释类型 / 或 *

    while (1) {
        charA = charB;

        if (charA == 0) {
            isRegular = false; // js content error
            return;
        }

        charB = GetChar();

        // \r\n -> \n(next char)
        if (charA == '\r' && charB == '\n') {
            charA = '\n';
            charB = GetChar();
        }

        // \r -> \n
        if (charA == '\r' && charB != '\n') {
            charA = '\n';
        }

        if (charA == '\n')
        { ++lineCount; }

        /*
         * 参考 charB 来处理 charA
         * 输出或不输出 charA
         * 下一次循环时自动会用 charB 覆盖 charA
         */

        // 正则需要在 token 级别判断
        if (isRegular) {
            // 正则状态全部输出，直到 /
            tokenB.code.push_back(charA);

            if (charA == '\\' &&
                    (charB == '/' || charB == '\\' ||
                     charB == '[' || charB == ']')) { // 转义字符
                tokenB.code.push_back(charB);
                charB = GetChar();
            }

            if (charA == '[' && regBracketLevel == 0) {
                ++regBracketLevel;
            }

            if (charA == ']' && regBracketLevel > 0) {
                --regBracketLevel;

                if (isRegularFlags)
                { isRegularFlags = false; }
            }

            if (charA == '/' &&
                    (charB != '*' && charB != '|' && charB != '?')) { // 正则可能结束
                if (!isRegularFlags &&
                        (IsNormalChar(charB) || regBracketLevel > 0)) {
                    // 正则的 flags 部分
                    // /g /i /ig...
                    isRegularFlags = true;
                    continue;
                } else {
                    // 正则结束
                    isRegular = false;
                    return;
                }
            }

            if (isRegularFlags && !IsNormalChar(charB)) {
                // 正则结束
                isRegularFlags = false;
                isRegular = false;
                return;
            }

            continue;
        }

        if (isQuote) {
            // 引号状态，全部输出，直到引号结束
            tokenB.code.push_back(charA);

            if (charA == '\\' && (charB == quote || charB == '\\')) { // 转义字符
                tokenB.code.push_back(charB);
                charB = GetChar();
            }

            if (charA == quote) // 引号结束
            { return; }

            continue;
        }

        if (isComment) {
            // 注释状态，全部输出
            if (tokenB.type == TOKENTYPE::COMMENT_TYPE_MULTLINE) {
                // /*...*/每行前面的\t, ' '都要删掉
                if (isLineBegin && (charA == '\t' || charA == ' '))
                { continue; }
                else if (isLineBegin && charA == '*')
                { tokenB.code.push_back(' '); }

                isLineBegin = false;

                if (charA == '\n')
                { isLineBegin = true; }
            }

            tokenB.code.push_back(charA);

            if (comment == '*') {
                // 直到 */
                tokenB.type = TOKENTYPE::COMMENT_TYPE_MULTLINE;
                tokenB.inlineComment = false;

                if (charA == '*' && charB == '/') {
                    tokenB.code.push_back(charB);
                    charB = GetChar();

                    tokenABeforeComment = tokenA;

                    return;
                }
            } else {
                // 直到换行
                tokenB.type = TOKENTYPE::COMMENT_TYPE_ONELINE;
                tokenB.inlineComment = false;

                if (charA == '\n')
                { return; }
            }

            continue;
        }

        if (IsNormalChar(charA)) {
            tokenB.type = TOKENTYPE::STRING_TYPE;
            tokenB.code.push_back(charA);

            // 解决类似 82e-2, 442e+6, 555E-6 的问题
            // 因为这是立即数，所以只能符合以下的表达形式
            bool isNumOld = isNum;

            if (isFirst || isNumOld) { // 只有之前是数字才改变状态
                isNum = IsNumChar(charA);
                isFirst = false;
            }

            if (isNumOld && !isNum && (charA == 'e' || charA == 'E') &&
                    (charB == '-' || charB == '+' || IsNumChar(charB))) {
                isNum = true;

                if (charB == '-' || charB == '+') {
                    tokenB.code.push_back(charB);
                    charB = GetChar();
                }
            }

            if (!IsNormalChar(charB)) { // loop until charB is not normal char
                isNeg = false;
                return;
            }
        } else {
            if (IsBlankChar(charA))
            { continue; } // 忽略空白字符

            if (IsQuote(charA)) {
                // 引号
                isQuote = true;
                quote = charA;

                tokenB.type = TOKENTYPE::STRING_TYPE;
                tokenB.code.push_back(charA);
                continue;
            }

            if (IsComment()) {
                // 注释
                isComment = true;
                comment = charB;

                //tokenBType = COMMENT_TYPE;
                tokenB.code.push_back(charA);
                continue;
            }

            if (IsSingleOper(charA) ||
                    IsNormalChar(charB) || IsBlankChar(charB) || IsQuote(charB)) {
                tokenB.type = TOKENTYPE::OPER_TYPE;
                tokenB.code = charA; // 单字符符号
                return;
            }

            // 多字符符号
            if ((charB == '=' || charB == charA) ||
                    ((charA == '-' || charA == '=') && charB == '>')) {
                // 的确是多字符符号
                tokenB.type = TOKENTYPE::OPER_TYPE;
                tokenB.code.push_back(charA);
                tokenB.code.push_back(charB);
                charB = GetChar();

                if ((tokenB.code == "==" || tokenB.code == "!=" ||
                        tokenB.code == "<<" || tokenB.code == ">>") && charB == '=') {
                    // 三字符 ===, !==, <<=, >>=
                    tokenB.code.push_back(charB);
                    charB = GetChar();
                } else if (tokenB.code == ">>" && charB == '>') {
                    // >>>, >>>=
                    tokenB.code.push_back(charB);
                    charB = GetChar();

                    if (charB == '=') { // >>>=
                        tokenB.code.push_back(charB);
                        charB = GetChar();
                    }
                }

                return;
            } else {
                // 还是单字符的
                tokenB.type = TOKENTYPE::OPER_TYPE;
                tokenB.code = charA; // 单字符符号
                return;
            }

            // What? How could we come here?
            charA = 0;
            return;
        }
    }
}

bool JSFormatter::GetToken()
{
    if (isFirstToken) {
        // 第一次多调用一次 GetTokenRaw
        GetTokenRaw();
        isFirstToken = false;
    }

    PrepareRegular(); // 判断正则
    PreparePosNeg(); // 判断正负数

    ++tokenCount;
    tokenA = tokenB;

    if (tokenBQueue.size() == 0) {
        GetTokenRaw();
        PrepareTokenB(); // 看看是不是要跳过换行
    } else {
        // 有排队的换行
        tokenB = tokenBQueue.front();
        tokenBQueue.pop();
    }

    return (charA != 0 || tokenA.code != ""); //终止While信号
}

void JSFormatter::PrepareRegular()
{
    /*
     * 先处理一下正则
     * tokenB[0] == /，且 tokenB 不是注释
     * tokenA 不是 STRING (除了 tokenA == return)
     * 而且 tokenA 的最后一个字符是下面这些
    */
    //size_t last = tokenA.size() > 0 ? tokenA.size() - 1 : 0;
    char tokenALast = tokenA.code.size() > 0 ? tokenA.code[tokenA.code.size() - 1] : 0;
    char tokenBFirst = tokenB.code[0];

    if (tokenBFirst == '/' && tokenB.type != TOKENTYPE::COMMENT_TYPE_ONELINE &&
            tokenB.type != TOKENTYPE::COMMENT_TYPE_MULTLINE &&
            ((tokenA.type != TOKENTYPE::STRING_TYPE && strBeforeReg.find(tokenALast) != string::npos) ||
             tokenA.code == "return")) {
        isRegular = true;
        GetTokenRaw(); // 把正则内容加到 tokenB
    }
}

void JSFormatter::PreparePosNeg()
{
    /*
     * 如果 tokenB 是 -,+ 号
     * 而且 tokenA 不是字符串型也不是正则表达式
     * 而且 tokenA 不是 ++, --, ], )
     * 而且 charB 是一个 NormalChar
     * 那么 tokenB 实际上是一个正负数
     */
    Token tokenRealPre = tokenA;

    if (tokenA.type == TOKENTYPE::COMMENT_TYPE_MULTLINE)
    { tokenRealPre = tokenABeforeComment; }

    if (tokenB.type == TOKENTYPE::OPER_TYPE && (tokenB.code == "-" || tokenB.code == "+") &&
            (tokenRealPre.type != TOKENTYPE::STRING_TYPE ||
             tokenRealPre.code == "return" || tokenRealPre.code == "case" ||
             tokenRealPre.code == "delete" || tokenRealPre.code == "throw") &&
            tokenRealPre.type != TOKENTYPE::REGULAR_TYPE &&
            tokenRealPre.code != "++" && tokenRealPre.code != "--" &&
            tokenRealPre.code != "]" && tokenRealPre.code != ")" &&
            IsNormalChar(charB)) {
        // tokenB 实际上是正负数
        isNeg = true;
        GetTokenRaw();
    }
}

void JSFormatter::PrepareTokenB()
{

    /*
     * 跳过 else, while, catch, ',', ';', ')', { 之前的换行
     * 如果最后读到的不是上面那几个，再把去掉的换行补上
     */
    int c = 0;

    while (tokenB.code == "\n" || tokenB.code == "\r\n") {
        ++c;
        GetTokenRaw();
    }

    if (c == 0 &&
            tokenA.type != TOKENTYPE::COMMENT_TYPE_ONELINE &&
            tokenB.type == TOKENTYPE::COMMENT_TYPE_MULTLINE &&
            tokenB.code.find("\r") == string::npos &&
            tokenB.code.find("\n") == string::npos) {
        // COMMENT_TYPE_MULTLINE 之前没有换行, 自己也没有换行
        tokenB.inlineComment = true;
    }

    if (tokenB.code != "else" && tokenB.code != "while" && tokenB.code != "catch" &&
     tokenB.code != "," && tokenB.code != ";" && tokenB.code != ")" && tokenB.code != "}") {
        // 将去掉的换行压入队列，先处理
        if (tokenA.code == "{" && tokenB.code == "}")
        { return; } // 空 {}
                
        Token temp;
        c = c > 2 ? 2 : c;


        for (; c > 0; --c) {
            temp.code = string("\n");
            temp.type = TOKENTYPE::OPER_TYPE;
            tokenBQueue.push(temp);
        }

        tokenBQueue.push(tokenB);
        temp = tokenBQueue.front();
        tokenBQueue.pop();
        tokenB = temp;
    }
}


void JSFormatter::PutToken(const Token &token, const string &leftStyle, const string &rightStyle)
{
    // debug
    /*size_t length = token.size();
    for(size_t i = 0; i < length; ++i)
        PutChar(token[i]);
    PutChar('\n');*/
    // debug
    PutString(leftStyle);
    PutString(token);
    PutString(rightStyle);

    if (!(isCommentPuted && isNewLine))
    { isCommentPuted = false; } // 这个一定会发生在注释之后的任何输出后面
}

void JSFormatter::PutLineBuffer()
{
    // Map original line count to formatted line count
    size_t i = 0;

    while (1) {
        if (i >= lineWaitVec.size()) {
            lineWaitVec.clear();
            break;
        }

        unsigned int oldLine = lineWaitVec[i];

        if (oldLine >= lineFormattedVec.size()) {
            lineFormattedVec.resize(lineFormattedVec.size() * 2, -1);
            continue;
        }

        if (lineFormattedVec[oldLine] == -1)
        { lineFormattedVec[oldLine] = formattedLineCount; }

        ++i;
    }

    string line;

    if (!isTemplatePuted)
    { line.append(TrimRightSpace(lineBuffer)); }
    else
    { line.append(lineBuffer); } // 原样输出 Template String

    if ((!isTemplatePuted || lineBuffer[0] == '`') &&
            (line != "" || Option.indentEmpytLine == true)) { // Fix "JSLint unexpect space" bug
        for (size_t i = 0; i < initIndentString.length(); ++i)
        { PutChar(initIndentString[i]); } // 先输出预缩进

        for (int c = 0; c < lineIndents; ++c)
            for (int c2 = 0; c2 < Option.indentNumber; ++c2)
            { PutChar(Option.indentChar); } // 输出缩进
    }

    // 加上换行
    if (Option.CRPut == true)
    { line.append("\r"); } //PutChar('\r');

    line.append("\n"); //PutChar('\n');

    // 输出 line
    for (size_t i = 0; i < line.length(); ++i) {
        int ch = line[i];
        PutChar(ch);

        if (ch == '\n')
        { ++formattedLineCount; }
    }
}

void JSFormatter::PutString(const Token &token)
{
    size_t length = token.code.size();

    //char topStack = blockStack.top();
    for (size_t i = 0; i < length; ++i) {
        if (isNewLine && (isCommentPuted ||
                          ((Option.bracNewLine == true || token.code[i] != '{') &&
                           token.code[i] != ',' && token.code[i] != ';' && !IsInlineComment(token)))) {
            // 换行后面不是紧跟着 {,; 才真正换
            PutLineBuffer(); // 输出行缓冲

            lineBuffer = "";
            isTemplatePuted = false;
            isNewLine = false;
            indentCount = indentCount < 0 ? 0 : indentCount; // 出错修正
            lineIndents = indentCount;

            if (token.code[i] == '{' || token.code[i] == ',' || token.code[i] == ';') // 行结尾是注释，使得{,;不得不换行
            { --lineIndents; }
        }

        if (isNewLine && !isCommentPuted &&
                ((Option.bracNewLine == false && token.code[i] == '{') ||
                 token.code[i] == ',' || token.code[i] == ';' || IsInlineComment(token)))
        { isNewLine = false; }

        if (lineBuffer.length() == 0 && isTemplatePuted)
        { isLineTemplate = true; }

        if (token.code[i] == '\n') {
            isNewLine = true;
        } else {
            lineBuffer += token.code[i];
            int tokenLine = token.line;

            if (tokenLine != -1)
            { lineWaitVec.push_back(token.line); }
        }
    }
}

void JSFormatter::PutString(const string &str)
{
    Token tokenWrapper;
    tokenWrapper.type = TOKENTYPE:: NOT_TOKEN;
    tokenWrapper.code = str;
    tokenWrapper.inlineComment = false;
    tokenWrapper.line = -1;

    PutString(tokenWrapper);
}

void JSFormatter::PopMultiBlock(char previousStackTop)
{
    if (tokenB.code == ";") // 如果 tokenB 是 ;，弹出多个块的任务留给它
    { return; }

    if (!((previousStackTop == JS_IF && tokenB.code == "else") ||
            (previousStackTop == JS_DO && tokenB.code == "while") ||
            (previousStackTop == JS_TRY && tokenB.code == "catch"))) {
        char topStack;

        if (!GetStackTop(blockStack, topStack))
        { return; }

        // ; 还可能可能结束多个 if, do, while, for, try, catch
        while (topStack == JS_IF || topStack == JS_FOR || topStack == JS_WHILE ||
                topStack == JS_DO || topStack == JS_ELSE || topStack == JS_TRY || topStack == JS_CATCH) {
            if (topStack == JS_IF || topStack == JS_FOR ||
                    topStack == JS_WHILE || topStack == JS_CATCH ||
                    topStack == JS_ELSE || topStack == JS_TRY) {
                blockStack.pop();
                --indentCount;
            } else if (topStack == JS_DO) {
                --indentCount;
            }

            if ((topStack == JS_IF && tokenB.code == "else") ||
                    (topStack == JS_DO /*&& tokenB.code == "while"*/) ||
                    (topStack == JS_TRY && tokenB.code == "catch"))
            { break; } // 直到刚刚结束一个 if...else, do..., try...catch

            if (!GetStackTop(blockStack, topStack))
            { break; }
        }
    }
}

void JSFormatter::ProcessOper(bool isNewLine)
{

    char topStack;
    GetStackTop(blockStack, topStack);
    string strRight(" ");

    if (tokenA.code == "(" ||
            tokenA.code == ")" ||
            tokenA.code == "[" ||
            tokenA.code == "]" ||
            tokenA.code == "!" ||
            tokenA.code == "!!" ||
            tokenA.code == "~" ||
            tokenA.code == ".") {
        // ()[]!. 都是前后没有样式的运算符
        if ((tokenA.code == ")" || tokenA.code == "]") &&
                (topStack == JS_ASSIGN || topStack == JS_HELPER)) {
            if (topStack == JS_ASSIGN)
            { --indentCount; }

            blockStack.pop();
        }

        GetStackTop(blockStack, topStack);

        if ((tokenA.code == ")" && topStack == JS_BRACKET) ||
                (tokenA.code == "]" && topStack == JS_SQUARE)) {
            // )] 需要弹栈，减少缩进
            blockStack.pop();
            --indentCount;
            GetStackTop(blockStack, topStack);

            if (topStack == JS_ASSIGN || topStack == JS_HELPER)
            { blockStack.pop(); }
        }

        GetStackTop(blockStack, topStack);

        if (tokenA.code == ")" && !brcNeedStack.top() &&
                (topStack == JS_IF || topStack == JS_FOR || topStack == JS_WHILE ||
                 topStack == JS_SWITCH || topStack == JS_CATCH)) {
            // 栈顶的 if, for, while, switch, catch 正在等待 )，之后换行增加缩进
            // 这里的空格和下面的空格是留给 { 的，bNLBracket 为 true 则不需要空格了
            string rightDeco = tokenB.code != ";" ? strRight : "";

            if (!isNewLine)
            { rightDeco.append("\n"); }

            PutToken(tokenA, string(""), rightDeco);
            //bBracket = true;
            brcNeedStack.pop();
            isBlockStmt = false; // 等待 statment

            if (StackTopEq(blockStack, JS_WHILE)) {
                blockStack.pop();

                if (StackTopEq(blockStack, JS_DO)) {
                    // 结束 do...while
                    blockStack.pop();

                    PopMultiBlock(JS_WHILE);
                } else {
                    blockStack.push(JS_WHILE);
                    ++indentCount;
                }
            } else
            { ++indentCount; }
        } else if (tokenA.code == ")" &&
                   (tokenB.code == "{" || IsInlineComment(tokenB) || isNewLine))
        { PutToken(tokenA, string(""), strRight); } // { 或者/**/或者换行之前留个空格
        else
        { PutToken(tokenA); } // 正常输出

        if (tokenA.code == "(" || tokenA.code == "[") {
            // ([ 入栈，增加缩进
            GetStackTop(blockStack, topStack);

            if (topStack == JS_ASSIGN) {
                if (!isAssigned)
                { --indentCount; }
                else
                { blockStack.push(JS_HELPER); }
            }

            blockStack.push(blockMap[tokenA.code]);
            ++indentCount;
        }

        return;
    }

    if (tokenA.code == ";") {
        GetStackTop(blockStack, topStack);

        if (topStack == JS_ASSIGN) {
            --indentCount;
            blockStack.pop();
        }

        GetStackTop(blockStack, topStack);

        // ; 结束 if, else, while, for, try, catch
        if (topStack == JS_IF || topStack == JS_FOR ||
                topStack == JS_WHILE || topStack == JS_CATCH) {
            blockStack.pop();
            --indentCount;
            // 下面的 } 有同样的处理
            PopMultiBlock(topStack);
        }

        if (topStack == JS_ELSE || topStack == JS_TRY) {
            blockStack.pop();
            --indentCount;
            PopMultiBlock(topStack);
        }

        if (topStack == JS_DO) {
            --indentCount;
            PopMultiBlock(topStack);
        }

        // do 在读取到 while 后才修改计数
        // 对于 do{} 也一样

        GetStackTop(blockStack, topStack);

        if (topStack != JS_BRACKET && !isNewLine && !IsInlineComment(tokenB))
        { PutToken(tokenA, string(""), strRight.append("\n")); } // 如果不是 () 里的 ; 就换行
        else if (topStack == JS_BRACKET || tokenB.type == TOKENTYPE::COMMENT_TYPE_ONELINE ||
                 IsInlineComment(tokenB))
        { PutToken(tokenA, string(""), strRight); } // (; ) 空格
        else
        { PutToken(tokenA); }

        return; // ;
    }

    if (tokenA.code == ",") {
        if (StackTopEq(blockStack, JS_ASSIGN)) {
            --indentCount;
            blockStack.pop();
        }

        if (StackTopEq(blockStack, JS_BLOCK) && !isNewLine)
        { PutToken(tokenA, string(""), strRight.append("\n")); } // 如果是 {} 里的
        else
        { PutToken(tokenA, string(""), strRight); }

        return; // ,
    }

    if (tokenA.code == "{") {
        GetStackTop(blockStack, topStack);

        if (topStack == JS_IF || topStack == JS_FOR ||
                topStack == JS_WHILE || topStack == JS_DO ||
                topStack == JS_ELSE || topStack == JS_SWITCH ||
                topStack == JS_TRY || topStack == JS_CATCH ||
                topStack == JS_ASSIGN) {
            if (!isBlockStmt || topStack == JS_ASSIGN) { //(topStack == JS_ASSIGN && !bAssign))
                //blockStack.pop(); // 不把那个弹出来，遇到 } 时一起弹
                --indentCount;
                isBlockStmt = true;
            } else {
                blockStack.push(JS_HELPER); // 压入一个 JS_HELPER 统一状态
            }
        }

        // 修正({...}) 中多一次缩进
        bool bPrevFunc = (topStack == JS_FUNCTION);
        char fixTopStack = topStack;

        if (bPrevFunc) {
            blockStack.pop(); // 弹掉 JS_FUNCTION
            GetStackTop(blockStack, fixTopStack);
        }

        if (fixTopStack == JS_BRACKET) {
            --indentCount; // ({ 减掉一个缩进
        }

        if (bPrevFunc) {
            blockStack.push(JS_FUNCTION); // 压回 JS_FUNCTION
        }

        // 修正({...}) 中多一次缩进 end

        blockStack.push(blockMap[tokenA.code]); // 入栈，增加缩进
        ++indentCount;

        /*
         * { 之间的空格都是由之前的符号准备好的
         * 这是为了解决 { 在新行时，前面会多一个空格的问题
         * 因为算法只能向后，不能向前看
         */
        if (tokenB.code == "}") {
            // 空 {}
            isEmptyBracket = true;

            if (isNewLine == false && Option.bracNewLine == true &&
                    (topStack == JS_IF || topStack == JS_FOR ||
                     topStack == JS_WHILE || topStack == JS_SWITCH ||
                     topStack == JS_CATCH || topStack == JS_FUNCTION)) {
                PutToken(tokenA, string(" ")); // 这些情况下，前面补一个空格
            } else {
                PutToken(tokenA);
            }
        } else {
            string strLeft = (Option.bracNewLine == true && !isNewLine) ? string("\n") : string("");

            if (!isNewLine && !IsInlineComment(tokenB)) // 需要换行
            { PutToken(tokenA, strLeft, strRight.append("\n")); }
            else
            { PutToken(tokenA, strLeft, strRight); }
        }

        return; // {
    }

    if (tokenA.code == "}") {
        // 激进的策略，} 一直弹到 {
        // 这样做至少可以使得 {} 之后是正确的
        while (GetStackTop(blockStack, topStack)) {
            if (topStack == JS_BLOCK)
            { break; }

            blockStack.pop();

            switch (topStack) {
                case JS_IF:
                case JS_FOR:
                case JS_WHILE:
                case JS_CATCH:
                case JS_DO:
                case JS_ELSE:
                case JS_TRY:
                case JS_SWITCH:
                case JS_ASSIGN:
                case JS_FUNCTION:
                case JS_HELPER:
                    --indentCount;
                    break;
            }

            /*if(!GetStackTop(blockStack, topStack))
                break;*/
        }

        if (topStack == JS_BLOCK) {
            // 弹栈，减小缩进
            blockStack.pop();
            --indentCount;
            bool bGetTop = GetStackTop(blockStack, topStack);

            if (bGetTop) {
                switch (topStack) {
                    case JS_IF:
                    case JS_FOR:
                    case JS_WHILE:
                    case JS_CATCH:
                    case JS_ELSE:
                    case JS_TRY:
                    case JS_SWITCH:
                    case JS_ASSIGN:
                    case JS_FUNCTION:
                    case JS_HELPER:
                        blockStack.pop();
                        break;

                    case JS_DO:
                        // 缩进已经处理，do 留给 while
                        break;
                }
            }
        }

        string leftStyle("");

        if (!isNewLine)
        { leftStyle = "\n"; }

        if (isEmptyBracket) {
            leftStyle = "";
            strRight.append("\n");
            isEmptyBracket = false;
        }

        if ((!isNewLine &&
                tokenB.code != ";" && tokenB.code != "," && tokenB.code != "=" &&
                !IsInlineComment(tokenB)) &&
                (Option.bracNewLine == true ||
                 !((topStack == JS_DO && tokenB.code == "while") ||
                   (topStack == JS_IF && tokenB.code == "else") ||
                   (topStack == JS_TRY && tokenB.code == "catch") ||
                   tokenB.code == ")"))) {
            if (strRight.length() == 0 || strRight[strRight.length() - 1] != '\n')
            { strRight.append("\n"); } // 一些情况换行, 不要重复换行

            PutToken(tokenA, leftStyle, strRight);
        } else if (tokenB.type == TOKENTYPE::STRING_TYPE ||
                   tokenB.type == TOKENTYPE::COMMENT_TYPE_ONELINE ||
                   IsInlineComment(tokenB)) {
            PutToken(tokenA, leftStyle, strRight); // 为 else 准备的空格
        } else {
            PutToken(tokenA, leftStyle); // }, }; })
        }

        // 注意 ) 不要在输出时仿照 ,; 取消前面的换行

        //char tmpTopStack;
        //GetStackTop(blockStack, tmpTopStack);
        // 修正({...}) 中多一次缩进
        if (topStack != JS_ASSIGN && StackTopEq(blockStack, JS_BRACKET))
        { ++indentCount; }

        // 修正({...}) 中多一次缩进 end

        PopMultiBlock(topStack);

        return; // }
    }

    if (tokenA.code == "++" || tokenA.code == "--") {
        PutToken(tokenA);
        return;
    }

    if (tokenA.code == "\n" || tokenA.code == "\r\n") {
        PutToken(tokenA);
        return;
    }

    if (tokenA.code == ":" && StackTopEq(blockStack, JS_CASE)) {
        // case, default
        if (!isNewLine)
        { PutToken(tokenA, string(""), strRight.append("\n")); }
        else
        { PutToken(tokenA, string(""), strRight); }

        blockStack.pop();
        return;
    }

    if (tokenA.code == "::" || tokenA.code == "->") {
        PutToken(tokenA);
        return;
    }

    if (StackTopEq(blockStack, JS_ASSIGN))
    { isAssigned = true; }

    if (tokenA.code == "=" && !StackTopEq(blockStack, JS_ASSIGN)) {
        blockStack.push(blockMap[tokenA.code]);
        ++indentCount;
        isAssigned = false;
    }

    if (tokenA.code == "?") {
        ++questOperCount;
        questOperStackCount.push(blockStack.size());
    }

    if (tokenA.code == ":") {
        if (questOperCount > 0 &&
                (questOperStackCount.top() >= blockStack.size() ||
                 StackTopEq(blockStack, JS_ASSIGN))) {
            --questOperCount;
            questOperStackCount.pop();
        } else {
            PutToken(tokenA, string(""), string(" "));
            return;
        }
    }

    PutToken(tokenA, string(" "), string(" ")); // 剩余的操作符都是 空格oper空格
}

void JSFormatter::ProcessQuote(Token &token)
{
    char chFirst = token.code[0];
    char chLast = token.code[token.code.length() - 1];

    if (token.type == TOKENTYPE::STRING_TYPE &&
            ((chFirst == '"' && chLast == '"') ||
             (chFirst == '\'' && chLast == '\''))) {
        string tokenNewCode;
        string tokenLine;
        size_t tokenLen = token.code.length();

        for (size_t i = 0; i < tokenLen; ++i) {
            char ch = token.code[i];
            tokenLine += ch;

            if (ch == '\n' || i == (tokenLen - 1)) {
                tokenNewCode.append(TrimSpace(tokenLine));
                tokenLine = "";
            }
        }

        token.code = tokenNewCode;
    }
}

void JSFormatter::ProcessString(bool isNewLine)
{
    if (tokenA.code == "case" || tokenA.code == "default") {
        // case, default 往里面缩一格
        --indentCount;
        string rightDeco = tokenA.code != "default" ? string(" ") : string();
        PutToken(tokenA, string(""), rightDeco);
        ++indentCount;
        blockStack.push(blockMap[tokenA.code]);
        return;
    }

    if (tokenA.code == "do" || (tokenA.code == "else" && tokenB.code != "if") ||
            tokenA.code == "try" || tokenA.code == "finally") {
        // do, else (NOT else if), try
        PutToken(tokenA);

        blockStack.push(blockMap[tokenA.code]);
        ++indentCount; // 无需 ()，直接缩进
        isBlockStmt = false; // 等待 block 内部的 statment

        PutString(string(" "));

        if ((tokenB.type == TOKENTYPE::STRING_TYPE || Option.bracNewLine == true) && !isNewLine)
        { PutString(string("\n")); }

        return;
    }

    if (tokenA.code == "function") {
        if (StackTopEq(blockStack, JS_ASSIGN)) {
            --indentCount;
            blockStack.pop();
        }

        blockStack.push(blockMap[tokenA.code]); // 把 function 也压入栈，遇到 } 弹掉
    }

    if (StackTopEq(blockStack, JS_ASSIGN))
    { isAssigned = true; }

    if (tokenB.type == TOKENTYPE::STRING_TYPE ||
            tokenB.type == TOKENTYPE::COMMENT_TYPE_ONELINE ||
            tokenB.type == TOKENTYPE::COMMENT_TYPE_MULTLINE ||
            tokenB.code == "{") {
        PutToken(tokenA, string(""), string(" "));

        //if(blockStack.top() != 't' && IsType(tokenA))
        //blockStack.push('t'); // 声明变量
        return;
    }

    if (specKeywordSet.find(tokenA.code) != specKeywordSet.end() &&
            tokenB.code != ";") {
        PutToken(tokenA, string(""), string(" "));
    } else if (tokenA.code[0] == '`' && tokenA.code[tokenA.code.length() - 1] == '`') {
        isTemplatePuted = true;
        PutToken(tokenA);
        isTemplatePuted = false;
    } else {
        ProcessQuote(tokenA);
        PutToken(tokenA);
    }

    if (tokenA.code == "if" || tokenA.code == "for" ||
            tokenA.code == "while" || tokenA.code == "catch") {
        // 等待 ()，() 到来后才能加缩进
        brcNeedStack.push(false);
        blockStack.push(blockMap[tokenA.code]);

    }

    if (tokenA.code == "switch") {
        //bBracket = false;
        brcNeedStack.push(false);
        blockStack.push(blockMap[tokenA.code]);
    }
}

void JSFormatter::Go()
{
    blockStack.push(JS_STUB);
    brcNeedStack.push(true);

    bool needNewLine;
    char tokenAFirst;
    char tokenBFirst;

    while (GetToken()) {
        needNewLine = false; // needNewLine 表示后面将要换行，isNewLine 表示已经换行了
        tokenAFirst = tokenA.code[0];
        tokenBFirst = tokenB.code.size() ? tokenB.code[0] : 0;

        if (tokenBFirst == '\r')
        { tokenBFirst = '\n'; }

        if (tokenBFirst == '\n' || tokenB.type == TOKENTYPE::COMMENT_TYPE_ONELINE)
        { needNewLine = true; }

        if (!isBlockStmt && tokenA.code != "{" && tokenA.code != "\n"
                && tokenA.type != TOKENTYPE::COMMENT_TYPE_ONELINE && tokenA.type != TOKENTYPE::COMMENT_TYPE_MULTLINE)
        { isBlockStmt = true; }

        bool isCommentInline = false;

        /*
         * 参考 tokenB 来处理 tokenA
         * 输出或不输出 tokenA
         * 下一次循环时自动会用 tokenB 覆盖 tokenA
         */
        //PutToken(tokenA);
        switch (tokenA.type) {
            case TOKENTYPE::REGULAR_TYPE:
                PutToken(tokenA); // 正则表达式直接输出，前后没有任何样式
                break;

            case TOKENTYPE::COMMENT_TYPE_ONELINE:
            case TOKENTYPE::COMMENT_TYPE_MULTLINE:
                if (tokenA.code[1] == '*') {
                    // 多行注释
                    if (!needNewLine) {
                        if (IsInlineComment(tokenA))
                        { isCommentInline = true; }

                        if (!isCommentInline) {
                            PutToken(tokenA, string(""), string("\n")); // 需要换行
                        } else if (tokenB.type != OPER_TYPE || tokenB.code == "{") { // { 依靠前一个 token 提供空格
                            PutToken(tokenA, string(""), string(" ")); // 不需要换行
                        } else {
                            PutToken(tokenA); // 不需要换行, 也不要空格
                        }
                    } else {
                        PutToken(tokenA);
                    }
                } else {
                    // 单行注释
                    PutToken(tokenA); // 肯定会换行的
                }

                // 内联注释就当作没有注释, 透明的
                if (!isCommentInline)
                { isCommentPuted = true; }

                break;

            case TOKENTYPE::OPER_TYPE:
                ProcessOper(needNewLine);
                break;

            case TOKENTYPE::STRING_TYPE:
                ProcessString(needNewLine);
                break;
        }
    }

    if (!isLineTemplate)
    { lineBuffer = Trim(lineBuffer); }

    if (lineBuffer.length())
    { PutLineBuffer(); }

}

string JSFormatter::GetFormattedJs()
{
    return out;
}
