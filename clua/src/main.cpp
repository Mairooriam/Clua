#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

extern "C" {
#include "ast.h"
#include "interpreter.h"
#include "lexer.h"
#include "mirua_module.h"
#include "mirua_types_internal.h"
#include "modules/file/file_module.h"
#include "open62541/types.h"
#include "parser.h"
}

#define MAX_INPUT_SIZE 1024
const char* CONFIG_PATH = "\\config";
const char* CONFIG_FILENAME = "config.mir";

extern "C" {
#include <open62541/client.h>
#include <open62541/client_config_default.h>
#include "mirua_module_internal.h"
#include "mirua_types.h"
}

void process_input(const std::string& input, Interpreter* interpreter) {
    if (input.empty()) return;

    int token_count;
    Token** tokens = lexer_tokenize_all(input.c_str(), &token_count);
    if (!tokens) {
        std::cout << "Error: Failed to tokenize input" << std::endl;
        return;
    }

    Parser* parser = parser_create(tokens, token_count);
    if (!parser) {
        std::cout << "Error: Failed to create parser" << std::endl;
        lexer_free_tokens(tokens, token_count);
        return;
    }

    AST* ast = parser_parse(parser);
    if (!ast) {
        std::cout << "Error: Failed to parse tokens" << std::endl;
        parser_free(parser);
        lexer_free_tokens(tokens, token_count);
        return;
    }

    interpreter_execute_ast(interpreter, ast);

    ast_free(ast);
    parser_free(parser);
    lexer_free_tokens(tokens, token_count);
}

std::string trim_whitespace(std::string str) {
    // Remove leading whitespace
    size_t start = str.find_first_not_of(" \t\n\r\f\v");
    if (start == std::string::npos) return "";
    
    // Remove trailing whitespace
    size_t end = str.find_last_not_of(" \t\n\r\f\v");
    return str.substr(start, end - start + 1);
}

void print_welcome() {
    std::cout << "=== MirWiz Command Line Interface ===" << std::endl;
    std::cout << "Type 'help' for available commands" << std::endl;
    std::cout << "Type 'exit' or 'quit' to exit" << std::endl;
    std::cout << "======================================" << std::endl << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc > 1 && strcmp(argv[1], "--test") == 0) {
        // Run tests directly
        MiruaContext* ctx = mirua_module_create();

        mirua_connect(ctx, "opc.tcp://127.0.0.1:4840");

        UA_String str = UA_String_fromChars("ns=5;s=::Program1:Mouses");
        UA_NodeId node;
        UA_StatusCode status = UA_NodeId_parse(&node, str);
        mirua_explore_structure(ctx->client, node);
        mirua_module_free(ctx);
        return 0;
    }

    char* str = mir_utils_getCurrentDirectory();
    if (str == nullptr) {
        std::cout << "Failed getting current directory" << std::endl;
        return -1;
    }

    size_t base_len = strlen(str);
    size_t config_len = strlen(CONFIG_PATH);
    size_t filename_len = strlen(CONFIG_FILENAME);
    size_t total_len = base_len + config_len + 1 + filename_len + 1;

    str = static_cast<char*>(realloc(str, total_len));
    if (str == nullptr) {
        std::cout << "Memory allocation failed" << std::endl;
        return -1;
    }

    strcat(str, CONFIG_PATH);
    std::cout << "Looking for opcua config in " << str << std::endl;

    if (mir_utils_fileExistsInDirectory(str, CONFIG_PATH)) {
        std::cout << "config.mir found!" << std::endl;
    }

    FILE* fPtr;
    strcat(str, "\\");
    strcat(str, CONFIG_FILENAME);

    fPtr = fopen(str, "r");
    if (fPtr == nullptr) {
        std::cout << "file couldn't be opened. File: " << str << std::endl;
    }

    free(str);

    Interpreter* interpreter = interpreter_create_ast();
    if (!interpreter) {
        std::cout << "Error: Failed to create interpreter" << std::endl;
        return 1;
    }

    // Check if command was passed as argument
    if (argc > 1) {
        std::string input;
        for (int i = 1; i < argc; i++) {
            input += argv[i];
            if (i < argc - 1) input += " ";
        }

        std::cout << "Executing: " << input << std::endl;
        process_input(input, interpreter);

        interpreter_free_ast(interpreter);
        return 0;
    }

    print_welcome();
    std::string input;
    while (true) {
        const char* prompt = (interpreter_get_mirua_state(interpreter) == MIRUA_STATE_NORMAL)
            ? "mirwiz"
            : "mirwiz>config";
        std::cout << prompt << "> ";
        std::cout.flush();

        if (!std::getline(std::cin, input)) {
            std::cout << "\nGoodbye!" << std::endl;
            break;
        }

        std::string trimmed = trim_whitespace(input);

        if (trimmed == "exit" || trimmed == "quit" || trimmed == "q") {
            std::cout << "Goodbye!" << std::endl;
            break;
        }

        if (trimmed.empty()) {
            continue;
        }

        process_input(trimmed, interpreter);
        std::cout << std::endl;
    }

    interpreter_free_ast(interpreter);
    return 0;
}