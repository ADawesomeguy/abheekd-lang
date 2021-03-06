#include <fstream>
#include <iostream>
#include <sstream>

#include "Lexer/Lexer.hpp"
#include "Parser/Parser.hpp"
#include "Token/Token.hpp"

const char *out_file = "out.ll";

// todo: add much better logging for parsed stuff
static void HandleDefinition() {
    if (auto FnAST = Parser::ParseFuncDefinition()) {
        if (auto *FnIR = FnAST->codegen()) {
            // fprintf(stderr, "Read function definition:\n");
            // FnIR->print(llvm::errs());
            // fprintf(stderr, "\n");
        }
        //printf("Parsed a function definition.\n");
    } else {
        // Skip token for error recovery.
        Parser::getNextToken();
    }
}

static void HandleExtern() {
    if (auto ProtoAST = Parser::ParseExtern()) {
        if (auto *ProtoIR = ProtoAST->codegen()) {
            // fprintf(stderr, "Read proto definition:\n");
            // ProtoIR->print(llvm::errs());
            // fprintf(stderr, "\n");
        }

        //printf("Parsed an extern\n");
    } else {
        // Skip token for error recovery.
        Parser::getNextToken();
    }
}

static void HandleTopLevelExpression() {
    // Evaluate a top-level expression into an anonymous function.
    if (auto StAST = Parser::ParseStatement()) {
        if (auto *FnIR = StAST->codegen()) {
            // fprintf(stderr, "Read top-level expr (statement):\n");
            // FnIR->print(llvm::errs());
            // fprintf(stderr, "\n");
        }
        // printf("Parsed a top-level expr (statement)\n");
    } else {
        // Skip token for error recovery.
        Parser::getNextToken();
    }
}

static void MainLoop() {
    while (true) {
        switch (Parser::CurrentToken.type) {
            case Token::type::tok_eof:
                return;
            case Token::type::tok_func:
                HandleDefinition();
                break;
            case Token::type::tok_extern:
                HandleExtern();
                break;
            default:
                if (Parser::CurrentToken.value == ";")
                    Parser::getNextToken();
                else {
                    HandleTopLevelExpression();
                }
                break;
        }
        fflush(stdout);
        fflush(stderr);
    }
}


int main(int argc, char **argv) {
    if (argc < 2) {
        std::cerr << "please specify a file to compile!\n" << argv[0] << " [path to file]\n" << std::flush;
        exit(EXIT_FAILURE);
    }

    std::ifstream ifs(argv[1]);
    if (!ifs) {
        std::cerr << "invalid file!" << std::endl;
    }

    std::stringstream temp;
    temp << ifs.rdbuf();
    Lexer(temp.str());

    // Lexer(
    //       //"extern puts(in : s1*) : s4;\n"
    //       "extern printf(fmt : s1*, ...) : s4;"
    //       "func main() : s4 {\n"
    //       //"\tputs(\"hello world!\");\n"
    //       "\tprintf(\"hello world!\");\n"
    //       "\treturn 0;\n"
    //       "}\n");
    //       // "func thing(arg1 : s4, arg2 : s4) : s4 {\n"
    //       // "\targ1 + arg2;\n"
    //       // "\treturn arg1 + arg2 * 6.0;\n"
    //       // "};\n"
    //       // "\n"
    //       // "thing(1.0, 2.0);"); // todo: fix function stuff bc this call
    //                            // is nested within the function instead
    //                            // of being outside

    std::cout << "SOURCE:\n---\n" << Lexer::Source << "\n---\n" << std::endl;

    std::cout << "TOKENS:\n";

    printf("%3s:%-3s %10s %10s\n", "ROW", "COL", "TOKEN", "TYPE");
    fflush(stdout);

    Token::InitBinOps();

    Token currentTok;
    while ((currentTok = Lexer::getTok()).type != Token::type::tok_eof) {
        printf("%3d:%-3d %10s %10d\n",
               currentTok.pos.row,
               currentTok.pos.column,
               currentTok.value.c_str(),
               currentTok.type
        );
    }
    printf("\n");
    fflush(stdout);

    Lexer::CharIdx = 0; // reset

    // set parser precedences
    Parser();

    // initialize module
    InitializeModule();

    // Prime the first token.
    Parser::getNextToken();

    MainLoop();

#ifdef DEBUG
    SaveModuleToFile(out_file);
    std::cout << "saved compiled LLVM IR to \"" << out_file << "\"!\n" << std::flush;
#endif
    SaveObjectToFile("out.o");
    std::cout << "saved object file to \"" << "out.o" << "\"!\n" << std::flush;
}
