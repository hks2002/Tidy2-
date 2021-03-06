#ifndef JSFORMATTER_H
#define JSFORMATTER_H
#include <string>
#include <vector>
#include <map>
#include <set>
#include <stack>
#include <queue>

using namespace std;

struct FormatterOption {
    char indentChar;
    int indentNumber;
    bool CRRead;
    bool CRPut;
    bool bracNewLine;
    bool indentEmpytLine;
};
struct Token {
    string code;
    int type;
    bool inlineComment;
    long line;
};

enum TOKENTYPE {
    NOT_TOKEN = 0,
    STRING_TYPE,
    OPER_TYPE,
    REGULAR_TYPE,
    COMMENT_TYPE_ONELINE, // 单行注释
    COMMENT_TYPE_MULTLINE, // 多行注释
};
/*
 * if-i, else-e, else if-i,
 * for-f, do-d, while-w,
 * switch-s, case-c, default-c
 * try-r, catch-h
 * {-BLOCK, (-BRACKET
 * 0-empty
 */
#define JS_IF 'i'
#define JS_ELSE 'e'
#define JS_FOR 'f'
#define JS_DO 'd'
#define JS_WHILE 'w'
#define JS_SWITCH 's'
#define JS_CASE 'c'
#define JS_TRY 'r'
#define JS_CATCH 'h'
#define JS_FUNCTION 'n'
#define JS_BLOCK '{'
#define JS_BRACKET '('
#define JS_SQUARE '['
#define JS_ASSIGN '='
#define JS_QUEST_MARK '?'
#define JS_HELPER '\\'
#define JS_STUB ' '
#define JS_EMPTY 0

typedef map<string, char> StrCharMap;
typedef set<string> StrSet;
typedef vector<int> IntVector;
typedef stack<char> CharStack;
typedef stack<bool> BoolStack;
typedef stack<int> IntStack;
typedef stack<size_t> SizeStack;
typedef queue<Token> TokenQueue;

template<class T>
bool GetStackTop(const stack<T> &stk, T &ret)
{
    if (stk.size() == 0)
    { return false; }

    ret = stk.top();
    return true;
}

template<class T>
bool StackTopEq(const stack<T> &stk, T eq)
{
    if (stk.size() == 0)
    { return false; }

    return (eq == stk.top());
}



class JSFormatter
{
public:
    JSFormatter(char *input, const FormatterOption &fmtoption);
    ~JSFormatter() {}

    //some string and char utils
    static string Trim(const string &str);
    static string TrimSpace(const string &str);
    static string TrimRightSpace(const string &str);
    void StringReplace(string &strBase, const string &strSrc, const string &strDes);

    inline void SetInitIndent(const string &initIndent) { initIndentString = initIndent; }
    void Go();
    string GetFormattedJs();

private:
    int  GetChar();
    void PutChar(int ch);
    //for parse----------------------------------
    bool inline IsNormalChar(int ch);
    bool inline IsNumChar(int ch);
    bool inline IsBlankChar(int ch);
    bool inline IsSingleOper(int ch);
    bool inline IsQuote(int ch);
    bool inline IsComment(); // 要联合判断 charA, charB
    bool IsInlineComment(const Token &token);

    bool GetToken(); // 处理过负数, 正则等等的 GetToken 函数
    void GetTokenRaw();
    void PrepareRegular(); // 通过词法判断 tokenB 正则
    void PreparePosNeg(); // 通过词法判断 tokenB 正负数
    void PrepareTokenB();
    //for parse----------------------------------

    void PutToken(const Token &token,  const string &leftStyle = string(""), const string &rightStyle = string("")); // Put a token out with style
    void PutLineBuffer();
    void PutString(const Token &str);
    void PutString(const string &str);
    void PopMultiBlock(char previousStackTop);
    void ProcessOper(bool isNewLine);
    void ProcessQuote(Token &token);
    void ProcessString(bool isNewLine);


    //for parse----------------------------------
    int charA;
    int charB;
    Token tokenA;
    Token tokenB;
    Token tokenABeforeComment;
    long lineCount;
    long tokenCount;

    bool isFirstToken; // 是否是第一次执行 GetToken
    TokenQueue tokenBQueue;
    bool isNeg; // tokenB 实际是正负数
    bool isRegular; // tokenB 实际是正则 GetToken 用到的两个成员状态
    string strBeforeReg; // 判断正则时，正则前面可以出现的字符
    int regBracketLevel; // 正则表达式中出现的 [] 深度
    //for parse----------------------------------

    //for format
    string initIndentString; // 起始缩进
    FormatterOption Option;

    int lineIndents;
    int lineTemplate;
    string lineBuffer;

    int formattedLineCount;
    IntVector lineFormattedVec;
    IntVector lineWaitVec;

    StrSet specKeywordSet; // 后面要跟着括号的关键字集合
    StrCharMap blockMap;
    CharStack blockStack;
    int indentCount; // 缩进数量，不用计算 blockStack，效果不好

    // 使用栈是为了解决在判断条件中出现循环的问题
    BoolStack brcNeedStack; // if 之类的后面的括号

    bool isNewLine; // 准备换行的标志
    bool isBlockStmt; // block 真正开始了
    bool isAssigned;
    bool isEmptyBracket; // 空 {}
    bool isCommentPuted; // 刚刚输出了注释
    bool isTemplatePuted; // Template String
    bool isLineTemplate;

    int questOperCount;
    SizeStack questOperStackCount;

    //读写
    char *in;
    string out;
    size_t getPos;
    size_t inLen;
private:
    // 改写初始化, 用引用方式代替默认的复制方式
    JSFormatter(const JSFormatter &);
    JSFormatter &operator=(const JSFormatter &);
};
#endif // JSFORMATTER_H
