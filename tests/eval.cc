#include <cstdlib>

#include <gtest/gtest.h>

#include <curv/analyser.h>
#include <curv/program.h>
#include <curv/exception.h>
#include <curv/parser.h>
#include <curv/phrase.h>
#include <curv/string.h>
#include <curv/system.h>

using namespace std;
using namespace curv;

std::stringstream console;

curv::System&
make_system()
{
    try {
        static curv::System_Impl sys(console);
        sys.load_library(curv::make_string("../lib/std.curv"));
        return sys;
    } catch (curv::Exception& e) {
        std::cerr << "ERROR: " << e << "\n";
        exit(EXIT_FAILURE);
    } catch (std::exception& e) {
        std::cerr << "ERROR: " << e.what() << "\n";
        exit(EXIT_FAILURE);
    }
}

struct CString_Script : public curv::Script
{
    const char* buffer_;

    CString_Script(const char* name, const char* buffer)
    :
        curv::Script(make_string(name), buffer, buffer + strlen(buffer)),
        buffer_(buffer)
    {}
};

struct Evaluator
{
    Evaluator(const char* source)
    :
        script_(make<CString_Script>("", source)),
        failmsg_(nullptr),
        failall_(nullptr),
        success_(nullptr)
    {
        try {
            console.str("");
            console.clear(); // Clear state flags.
            curv::Program prog{*script_, make_system()};
            prog.compile();
            auto den = prog.denotes();

            String_Builder buf;
            bool first = true;
            if (den.first) {
                for (auto f : *den.first) {
                    if (!first) buf << "\n";
                    buf << f.first << "=" << f.second;
                    first = false;
                }
            }
            if (den.second) {
                for (auto e : *den.second) {
                    if (!first) buf << "\n";
                    buf << e;
                    first = false;
                }
            }

            success_str_ = buf.get_string();
            success_ = success_str_->c_str();
        } catch (curv::Exception& e) {
            failmsg_str_ = e.shared_what();
            failmsg_ = failmsg_str_->c_str();
            failall_str_ = stringify(e);
            failall_ = failall_str_->c_str();
        } catch (std::exception& e) {
            failmsg_str_ = make_string(e.what());
            failmsg_ = failmsg_str_->c_str();
            failall_ = failmsg_;
        }
    }

    Shared<CString_Script> script_;

    const char* failmsg_;
    Shared<const curv::String> failmsg_str_;

    const char* failall_;
    Shared<const curv::String> failall_str_;

    const char* success_;
    Shared<curv::String> success_str_;
};

enum Expectation { x_success, x_failmsg, x_failall };

testing::AssertionResult
evaltest(const char* expr, const char* expected, Expectation expectation)
{
    Evaluator result(expr);
    if (result.failmsg_) {
        // the test failed
        switch (expectation) {
        case x_success:
            return testing::AssertionFailure()
                << "in expr '" << expr << "'\n"
                << "expected value: '" << expected << "'\n"
                << "  actual error: '" << result.failall_ << "'\n";
        case x_failmsg:
            if (strcmp(expected, result.failmsg_) == 0) {
                return testing::AssertionSuccess();
            } else {
                return testing::AssertionFailure()
                    << "in expr '" << expr << "'\n"
                    << "expected error: '" << expected << "'\n"
                    << "  actual error: '" << result.failall_ << "'\n";
            }
        case x_failall:
            if (strcmp(expected, result.failall_) == 0) {
                return testing::AssertionSuccess();
            } else {
                return testing::AssertionFailure()
                    << "in expr '" << expr << "'\n"
                    << "expected error: '" << expected << "'\n"
                    << "  actual error: '" << result.failall_ << "'\n";
            }
        default:
            throw std::logic_error("can't happen");
        }
    } else {
        // the test succeeded
        switch (expectation) {
        case x_success:
            if (strcmp(expected, result.success_) == 0) {
                return testing::AssertionSuccess();
            } else {
                return testing::AssertionFailure()
                    << "in expr '" << expr << "'\n"
                    << "expected value: '" << expected << "'\n"
                    << "  actual value: '" << result.success_ << "'\n";
            }
        case x_failmsg:
        case x_failall:
            return testing::AssertionFailure()
                << "in expr '" << expr << "'\n"
                << "expected error: '" << expected << "'\n"
                << "  actual value: '" << result.success_ << "'\n";
        default:
            throw std::logic_error("can't happen");
        }
    }
}

testing::AssertionResult
eval_success(
    const char* e1, const char* e2,
    const char* expr, const char* expected)
{
    return evaltest(expr, expected, x_success);
}

testing::AssertionResult
eval_failmsg(
    const char* e1, const char* e2,
    const char* expr, const char* expected)
{
    return evaltest(expr, expected, x_failmsg);
}

testing::AssertionResult
eval_failall(
    const char* e1, const char* e2,
    const char* expr, const char* expected)
{
    return evaltest(expr, expected, x_failall);
}

int reps()
{
    const char* rs = getenv("REPS");
    if (rs == nullptr)
        return 1;
    int r = atoi(rs);
    if (r <= 0)
        return 1;
    return r;
}

#define SUCCESS(expr,result) EXPECT_PRED_FORMAT2(eval_success,expr,result)
#define FAILMSG(expr,result) EXPECT_PRED_FORMAT2(eval_failmsg,expr,result)
#define FAILALL(expr,result) EXPECT_PRED_FORMAT2(eval_failall,expr,result)

TEST(curv, eval)
{
  int r = reps();
  for (int i = 0; i < r; ++i) {
    // constructors
    SUCCESS("42.7", "42.7");
    SUCCESS(".1", "0.1");
    SUCCESS("1.", "1");
    SUCCESS(".1e-1", "0.01");
    SUCCESS("1.e+1", "10");
    SUCCESS("1e1", "10");
    FAILMSG("0x", "bad numeral");
    SUCCESS("0xFF", "255");
    SUCCESS("\"abc\"", "\"abc\"");
    SUCCESS("[1,2,3]", "[1,2,3]");
    SUCCESS("{x:1}", "{x:1}");

    // builtins
    SUCCESS("pi",  "3.141592653589793");
    SUCCESS("tau", "6.283185307179586");
    SUCCESS("inf", "inf");
    SUCCESS("null", "null");
    SUCCESS("false", "false");
    SUCCESS("true", "true");
    SUCCESS("sqrt", "<function>");

    // runtime operations
    SUCCESS("-0", "-0");
    SUCCESS("-inf", "-inf");
    FAILMSG("1+null", "1+null: domain error");
    SUCCESS("[10,20]-3", "[7,17]");
    SUCCESS("5-[1,2]", "[4,3]");
    SUCCESS("[1,2]-[10,20]", "[-9,-18]");
    FAILMSG("inf-inf","inf-inf: domain error");
    FAILMSG("[]-[1]","-: mismatched list sizes (0,1) in array operation");
    FAILMSG("0/0", "0/0: domain error");
    SUCCESS("1/0", "inf");
    SUCCESS("sqrt(2)", "1.4142135623730951");
    SUCCESS("max(1,2,)", "2"); // test syntax: trailing , after last argument
    SUCCESS("sqrt << sqrt 16", "2");
    FAILALL("let f=()->sqrt(true);\nin f()",
        "sqrt(true): domain error\n"
        "line 1(columns 11-20)\n"
        "  let f=()->sqrt(true);\n"
        "            ^--------- \n"
        "line 2(columns 4-6)\n"
        "  in f()\n"
        "     ^--");
    SUCCESS("count()", "0");
    FAILALL("count 0",
        "not a list or string\n"
        "line 1(column 7)\n"
        "  count 0\n"
        "        ^");
    SUCCESS("true||false", "true");
    SUCCESS("false||true", "true");
    SUCCESS("false||false", "false");
    SUCCESS("true||null", "true");
    FAILMSG("null||true", "not a boolean value");

    SUCCESS("false&&true", "false");
    SUCCESS("false&&null", "false");
    SUCCESS("true&&false", "false");
    FAILMSG("true&&null", "not a boolean value");
    SUCCESS("true&&true", "true");

    FAILMSG("count(if (true) [])",
        "if: not an expression (missing else clause)");

    SUCCESS("null==null", "true");
    SUCCESS("null==false", "false");
    SUCCESS("false==false", "true");
    SUCCESS("42==42.0", "true");
    SUCCESS("0==false", "false");
    SUCCESS("[1,2]==[1,2]", "true");
    SUCCESS("[1,true]==[1,2]", "false");
    SUCCESS("{x:1,y:2}=={x:1,y:2}", "true");
    SUCCESS("sqrt==sqrt", "true");
    SUCCESS("!true", "false");
    SUCCESS("!false", "true");
    SUCCESS("![false,true,[false]]","[true,false,[true]]");
    SUCCESS("![]","[]");
    FAILMSG("!null", "!null: domain error");
    SUCCESS("null!=null", "false");
    SUCCESS("null!=false", "true");
    SUCCESS("0 < 1", "true");
    SUCCESS("-0 < +0", "false");
    FAILMSG("0 < null", "0<null: domain error");
    SUCCESS("0 <= 1", "true");
    SUCCESS("1 > 0", "true");
    SUCCESS("1 >= 0", "true");
    SUCCESS("{f:sqrt}.f(4)", "2");
    SUCCESS("4^0.5", "2");
    SUCCESS("4^-1", "0.25");
    SUCCESS("-2^2", "-4");
    SUCCESS("[1,2,3]'1","2");
    FAILALL("[1,2,3]'1.1",
        "1.1 is not an integer\n"
        "line 1(columns 9-11)\n"
        "  [1,2,3]'1.1\n"
        "          ^--");
    SUCCESS("(0..10)'(3..1 by -1)", "[3,2,1]");
    SUCCESS("[false,true]'[[0,1],[1,0]]", "[[false,true],[true,false]]");
    SUCCESS("let x=1;y=2; in x+y", "3");
    SUCCESS("let a=c+1;b=1;c=b+1; in a", "3");
    SUCCESS("let x=1 in let y=2 in let z=3 in x+y+z", "6");
    FAILALL("let x=x in x",
        "illegal recursive reference\n"
        "line 1(column 7)\n"
        "  let x=x in x\n"
        "        ^     ");
    SUCCESS("let f=x->(let a=x+1 in a) in f 2", "3");
    FAILMSG("let f(x,y)=x in f()",
        "list has wrong size: expected 2, got 0");
    SUCCESS("let add=(x,y)->x+y in add(1,2)", "3");
    SUCCESS("let add=x->y->x+y in add 1 2", "3");
    SUCCESS("let add x y = x+y in add 1 2", "3");
    SUCCESS(
        "let sum = (list,i,f)->if (i < count list) list'i+f(list,i+1,f) else 0;"
        "in sum([1,2,3],0,sum)",
        "6");
    SUCCESS(
        "/* tail-recursive function */"
        "let sum = (list,i)->if (i < count list) list'i+sum(list,i+1) else 0;"
        "in sum([1,2,3],0)",
        "6");
    SUCCESS(
        "// factorial (non-tail-recursive function)\n"
        "let f = x->if (x <= 1) 1 else x * f(x-1);\n"
        "in f(3)",
        "6");
    FAILALL("let f=x->x x in f 0",
        "0: not a function\n"
        "line 1(column 10)\n"
        "  let f=x->x x in f 0\n"
        "           ^         \n"
        "line 1(columns 17-19)\n"
        "  let f=x->x x in f 0\n"
        "                  ^--");

    // file
    FAILALL("file(\"bad_token.curv\")",
        "unterminated comment\n"
        "file \"bad_token.curv\", lines 1(column 5)-2(column 3)\n"
        "  x + /********\n"
        "      ^--------\n"
        "line 1(columns 1-22)\n"
        "  file(\"bad_token.curv\")\n"
        "  ^---------------------");
    FAILALL("file(\n1,2)",
        "value is not a string\n"
        "lines 1(column 5)-2(column 4)\n"
        "  file(\n"
        "      ^");
    FAILALL("file \"nonexistent\"",
        "can't open file nonexistent\n"
        "line 1(columns 6-18)\n"
        "  file \"nonexistent\"\n"
        "       ^------------");
    SUCCESS("let std = file \"std.curv\" in std.concat([1], [2,3], [4])",
        "[1,2,3,4]");
    SUCCESS("file \"curv.curv\"", "null");

    // range generator
    SUCCESS("1..4", "[1,2,3,4]");
    SUCCESS("1..3 by 0.5", "[1,1.5,2,2.5,3]");
    SUCCESS("1..1", "[1]");
    SUCCESS("1..0", "[]");
    SUCCESS("1..-1", "[]");
    SUCCESS("1..3 by -1", "[]");
    SUCCESS("3..1 by -1", "[3,2,1]");
    FAILMSG("1..inf", "1..inf: too many elements in range");
    FAILMSG("1..true", "1..true: domain error");

    // for
    FAILMSG("for", "syntax error: expecting '(' after 'for'");
    FAILMSG("for (i in a)", "missing expression");
    FAILMSG("for (i = a) x", "syntax error: expecting 'in'");
    FAILMSG("for x x", "syntax error: expecting '(' after 'for'");
    FAILMSG("for () x", "unexpected token when expecting 'for' pattern");
    FAILMSG("for (i in a,j in b) x", "syntax error: expecting ')'");
    FAILMSG("for (i) x", "syntax error: expecting 'in'");
    FAILMSG("for (42 in i) x", "not a pattern");
    SUCCESS("[for (i in [1,2,3]) i+1]", "[2,3,4]");

    // generalized actions
    SUCCESS("do (let a=-2 in for(b in a..2) if(b>0) print b);"
            "   for(x in -1..1) if(x<0) print \"-\" else if(x>0) print \"+\";"
            "in 0",
        "0");
    EXPECT_EQ(console.str(),
        "1\n"
        "2\n"
        "-\n"
        "+\n");

    // The spread operator (a sequence generator)
    SUCCESS("[for (i in [1,2,3]) if (i==2) ...(\"two\", \"2!\") else i]", 
        "[1,\"two\",\"2!\",3]");
    SUCCESS("...[1,2,3]", "1\n2\n3");

    // let operator
    SUCCESS("(let a=1; print \"$(a)\" in a)+1", "2");
    EXPECT_EQ(console.str(), "1\n");

    // print action
    SUCCESS("print \"$(17,42)\"", "");
    EXPECT_EQ(console.str(), "[17,42]\n");

    // lexical errors
    FAILMSG("\\foo", "illegal character '\\'");
    FAILMSG("\177", "illegal character 0x7F");
    FAILMSG("42e+", "bad numeral");
    FAILALL("/* foo",
        "unterminated comment\n"
        "line 1(columns 1-6)\n"
        "  /* foo\n"
        "  ^-----");

    // analysis errors
    FAILMSG("fnord", "fnord: not defined");
    /*
    FAILALL("{x:1,x:2}",
        "x: multiply defined\n"
        "line 1(column 6)\n"
        "  {x:1,x:2}\n"
        "       ^   ");
     */
    FAILALL("x+",
        "missing expression\n"
        "line 1(column 3), at end of script\n"
        "  x+\n"
        "    ^");
    FAILALL("x+\n",
        "missing expression\n"
        "line 1(column 3), at end of script\n"
        "  x+\n"
        "    ^");
    FAILMSG("(a=0)+1", "not an operation");

    // max, min
    SUCCESS("max()", "-inf");
    SUCCESS("max(1,)", "1");
    SUCCESS("max(1,2)", "2");
    SUCCESS("min()", "inf");
    SUCCESS("min(1,)", "1");
    SUCCESS("min(1,2)", "1");
    SUCCESS("(max([1,100],[10,20]), max(20,[5,17,30]), max([1,2],1.5))",
        "[[10,100],[20,20,30],[1.5,2]]");

    SUCCESS("abs(-inf)", "inf");
    SUCCESS("abs(-2)", "2");
    SUCCESS("abs(-0)", "0");
    SUCCESS("abs(inf)", "inf");
    SUCCESS("abs(2)", "2");
    SUCCESS("abs(0)", "0");

    SUCCESS("(mag(), mag(2,), mag(3,4))",
        "[0,2,5]");

    SUCCESS("is_list 0","false");
    SUCCESS("is_list ()","true");

    FAILALL("1,2",
        "syntax error\n"
        "line 1(column 2)\n"
        "  1,2\n"
        "   ^ ");

    SUCCESS("let a=2; f x={print(g 2); g y=a*x*b*y; b=3} in f(5).g(7)", "210");
    EXPECT_EQ(console.str(),
        "60\n");

    FAILMSG("let var a:=2 in a", "wrong style of definition for this block");
    FAILMSG("do a=2 in a", "wrong style of definition for this block");
  }
}
