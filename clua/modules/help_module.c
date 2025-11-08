// modules/help_module.c
// #include "help_module.h"
//
// #include <stdio.h>
// #include <stdlib.h>
//
// HelpContext* help_module_create(void) {
//     HelpContext* ctx = malloc(sizeof(HelpContext));
//     if (!ctx) return NULL;
//
//     ctx->base.module_name = "help";
//     ctx->help_topics = NULL;
//     ctx->topic_count = 0;
//
//     return ctx;
// }
//
// void help_execute(HelpContext* ctx, ASTNode* node) {
//     (void)ctx;
//     printf("=== Help System ===\n");
//
//     if (node->child_count > 0) {
//         // Help for specific topic
//         // char* topic = node->children[0]->value;
//         // printf("Help for: %s\n", topic);
//         // TODO: Look up specific help
//     } else {
//         // General help
//         printf("Available commands:\n");
//         printf("  help [topic] - Show help\n");
//         printf("  browse [path] - Browse directories\n");
//         printf("  save - Save configuration\n");
//         printf("  add <item> - Add item\n");
//         printf("  .. - Navigate up\n");
//         printf("  ls - List directories\n");
//     }
// }
//
// void help_module_free(HelpContext* ctx) {
//     if (ctx) {
//         // Free help_topics if allocated
//         free(ctx);
//     }
// }

// #include "help.h"

// #include <stdio.h>
// #include <string.h>
// static const char* lookup[] = {"read", "write"};
// static const char* helpTexts[] = {"this is help text for read", "this is help text for write"};

// // "read" "anytihngelse"
// char* help_get_help(const char* str) {
//     size_t idx = help_exists(str);
//     if (idx) {
//         printf("%s\n", helpTexts[idx]);
//     }
//     return "hihi";
// }

// size_t help_exists(const char* str) {
//     size_t lookup_n = sizeof(lookup) / sizeof(lookup[0]);
//     for (size_t i = 0; i < lookup_n; i++) {
//         if (strcmp(str, lookup[i])) {
//             printf("%s existed in lookup\n", str);
//             return i;
//         }
//     }
//     return 0;
// }
