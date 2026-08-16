/* 
    Copyright 2026 Marco Viscontini <mviscontini@gmail.com>

    Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the
    “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish,
    distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to
    the following conditions:

    The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
    WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
    CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.

*/

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>
#include <math.h>
#include <string.h>

#define MAX_NATIVE_OPS         256
#define CODE_STACK_SIZE      65536
#define DATA_STACK_SIZE        256
#define RETURN_STACK_SIZE        2
#define MAX_OP_NAME_SIZE        16
#define MAX_IF_NESTED_CYCLES  1024

typedef struct tf_context tf_context;

typedef void (*tf_func)(tf_context *);

typedef int32_t tf_int;
typedef float tf_float;
typedef uint32_t tf_pointer;
typedef unsigned char tf_char;
typedef unsigned char tf_bool;
typedef uint8_t tf_byte;

typedef enum {
    TF_INT,
    TF_FLT,
    TF_BLN,
    TF_CHR,
    TF_PTR,
    TF_OPN
} tf_type;

typedef struct {
    char name[MAX_OP_NAME_SIZE+1];
} tf_op;

typedef struct {
    tf_op items[MAX_NATIVE_OPS];
    tf_byte size;
} tf_op_list;

typedef struct {
    tf_type type;
    union {
        tf_int as_int;
        tf_float as_float;
        tf_pointer as_pointer;
        tf_char as_char;
        tf_bool as_bool;
        tf_byte as_op;
    } data;    
} tf_word;

typedef struct {
    tf_word* words;
    size_t cursor;
    size_t max_size;
} tf_stack;

typedef struct {
    tf_byte* bytes;
    size_t cursor;
} tf_byte_stack;

struct tf_context {
    tf_op_list ops;
    tf_byte_stack code_stack;
    tf_stack data_stack;
    tf_stack return_stack;
    // used for if / loops
    tf_pointer jumped_to;
    tf_int* word_address;
};

typedef struct tf_token_list_item {
    char value[MAX_OP_NAME_SIZE+1];
    struct tf_token_list_item* prev;
    struct tf_token_list_item* next;
} tf_token_list_item, *tf_token_list;

typedef struct {
    tf_pointer if_pos;
    tf_pointer else_pos;
    tf_token_list if_node;
    tf_token_list else_node;
    tf_pointer then_pos;
} tf_if_box;

void tf_init_ops(tf_op_list* operations) {
    operations -> size = 0;
}

void tf_add_op(tf_op_list* operations, const char* name) {
    if (operations == NULL) return;
    if (operations -> size + 1 >= MAX_NATIVE_OPS) return;
    strcpy(operations -> items[(operations -> size)++].name, name);
}

tf_int tf_code_pop_int(tf_context* ctx) {
    ctx -> code_stack.cursor--; // remove type byte
    tf_byte b1 = ctx -> code_stack.bytes[ctx->code_stack.cursor--];
    tf_byte b2 = ctx -> code_stack.bytes[ctx->code_stack.cursor--];
    tf_byte b3 = ctx -> code_stack.bytes[ctx->code_stack.cursor--];
    tf_byte b4 = ctx -> code_stack.bytes[ctx->code_stack.cursor--];
    tf_int num = b1 << 24 | b2 << 16 | b3 << 8 | b4;
    return num;
}

void tf_code_push_int(tf_context* ctx, tf_int n) {
    if (ctx -> code_stack.cursor >= CODE_STACK_SIZE) {
        return;
    }
    ctx -> code_stack.bytes[ctx->code_stack.cursor++] = TF_INT;
    ctx -> code_stack.bytes[ctx->code_stack.cursor++] = (n >> (0 * 8)) & 0xFF;
    ctx -> code_stack.bytes[ctx->code_stack.cursor++] = (n >> (1 * 8)) & 0xFF;
    ctx -> code_stack.bytes[ctx->code_stack.cursor++] = (n >> (2 * 8)) & 0xFF;
    ctx -> code_stack.bytes[ctx->code_stack.cursor++] = (n >> (3 * 8)) & 0xFF;
}

void tf_code_push_float(tf_context* ctx, tf_float n) {
    if (ctx -> code_stack.cursor >= CODE_STACK_SIZE) {
        return;
    }
    ctx -> code_stack.bytes[ctx->code_stack.cursor++] = TF_FLT;
    ctx -> code_stack.bytes[ctx->code_stack.cursor++] = ((tf_int) n >> (0 * 8)) & 0xFF;
    ctx -> code_stack.bytes[ctx->code_stack.cursor++] = ((tf_int) n >> (1 * 8)) & 0xFF;
    ctx -> code_stack.bytes[ctx->code_stack.cursor++] = ((tf_int) n >> (2 * 8)) & 0xFF;
    ctx -> code_stack.bytes[ctx->code_stack.cursor++] = ((tf_int) n >> (3 * 8)) & 0xFF;
}

void tf_code_push_bool(tf_context* ctx, tf_bool n) {
    if (ctx -> code_stack.cursor >= CODE_STACK_SIZE) {
        return;
    }
    ctx -> code_stack.bytes[ctx->code_stack.cursor++] = TF_BLN;
    ctx -> code_stack.bytes[ctx->code_stack.cursor++] = (tf_byte) n;
}

void tf_code_push_char(tf_context* ctx, tf_char c) {
    if (ctx -> code_stack.cursor >= CODE_STACK_SIZE) {
        return;
    }
    ctx -> code_stack.bytes[ctx->code_stack.cursor++] = TF_CHR;
    ctx -> code_stack.bytes[ctx->code_stack.cursor++] = (tf_byte) c;
}

void tf_code_push_op(tf_context* ctx, tf_byte op_index) {
    if (ctx -> code_stack.cursor >= CODE_STACK_SIZE) {
        return;
    }
    ctx -> code_stack.bytes[ctx->code_stack.cursor++] = TF_OPN;
    ctx -> code_stack.bytes[ctx->code_stack.cursor++] = op_index;
}

void tf_push(tf_stack* stack, tf_word word) {
    if (stack->cursor >= stack->max_size) {
        return;
    }
    stack->words[stack->cursor++] = word;
}

tf_word tf_pop(tf_stack* stack) {
    return stack->words[--stack->cursor];
}

void tf_push_int(tf_stack* stack, tf_int n) {
    if (stack->cursor >= stack->max_size) {
        return;
    }
    stack->words[stack->cursor++].type = TF_INT;
    stack->words[stack->cursor-1].data.as_int = n;
}

void tf_push_float(tf_stack* stack, tf_float n) {
    if (stack->cursor >= stack->max_size) {
        return;
    }
    stack->words[stack->cursor++].type = TF_FLT;
    stack->words[stack->cursor-1].data.as_float = n;
}

void tf_push_bool(tf_stack* stack, tf_bool n) {
    if (stack->cursor >= stack->max_size) {
        return;
    }
    stack->words[stack->cursor++].type = TF_BLN;
    stack->words[stack->cursor-1].data.as_bool = n;
}

void tf_push_char(tf_stack* stack, tf_char c) {
    if (stack->cursor >= stack->max_size) {
        return;
    }
    stack->words[stack->cursor++].type = TF_CHR;
    stack->words[stack->cursor-1].data.as_char = c;
}

void tf_push_pointer(tf_stack* stack, tf_int n) {
    if (stack->cursor >= stack->max_size) {
        return;
    }
    stack->words[stack->cursor++].type = TF_PTR;
    stack->words[stack->cursor-1].data.as_pointer = (tf_pointer) n;
}

void tf_push_op(tf_stack* stack, tf_byte op_index) {
    if (stack->cursor >= stack->max_size) {
        return;
    }
    stack->words[stack->cursor++].type = TF_OPN;
    stack->words[stack->cursor-1].data.as_op = op_index;
}

void tf_init(tf_context* ctx) {
    tf_init_ops(&(ctx -> ops));
    tf_add_op(&(ctx -> ops), "DUP");
    tf_add_op(&(ctx -> ops), "OVER");
    tf_add_op(&(ctx -> ops), "SWAP");
    tf_add_op(&(ctx -> ops), "DROP");
    tf_add_op(&(ctx -> ops), "ROT");
    tf_add_op(&(ctx -> ops), "+");
    tf_add_op(&(ctx -> ops), "-");
    tf_add_op(&(ctx -> ops), "*");
    tf_add_op(&(ctx -> ops), "/");
    tf_add_op(&(ctx -> ops), "POW");
    tf_add_op(&(ctx -> ops), "MOD");
    tf_add_op(&(ctx -> ops), "=");
    tf_add_op(&(ctx -> ops), "<>");
    tf_add_op(&(ctx -> ops), "<");
    tf_add_op(&(ctx -> ops), ">");
    tf_add_op(&(ctx -> ops), "<=");
    tf_add_op(&(ctx -> ops), ">=");
    tf_add_op(&(ctx -> ops), "0=");
    tf_add_op(&(ctx -> ops), "AND");
    tf_add_op(&(ctx -> ops), "OR");
    tf_add_op(&(ctx -> ops), ".");
    tf_add_op(&(ctx -> ops), "CR");
    tf_add_op(&(ctx -> ops), "JZ");
    tf_add_op(&(ctx -> ops), "JE");
    tf_add_op(&(ctx -> ops), "JMP");
    ctx -> code_stack.bytes = malloc(sizeof(tf_byte) * CODE_STACK_SIZE);
    ctx -> code_stack.cursor = 0;
    ctx -> data_stack.words = malloc(sizeof(tf_word) * DATA_STACK_SIZE);
    ctx -> data_stack.cursor = 0;
    ctx -> data_stack.max_size = DATA_STACK_SIZE;
    ctx -> return_stack.words = malloc(sizeof(tf_word) * RETURN_STACK_SIZE);
    ctx -> return_stack.cursor = 0;
    ctx -> return_stack.max_size = DATA_STACK_SIZE;
    ctx -> jumped_to = -1;
    ctx -> word_address = malloc(sizeof(tf_int) * CODE_STACK_SIZE / 2);
}

void tf_destroy(tf_context* ctx) {
    free(ctx->code_stack.bytes);
    free(ctx->data_stack.words);
    free(ctx->return_stack.words);
    free(ctx->word_address);
}

tf_byte tf_look_for_op(tf_context* ctx, const char* name, tf_op** ret_op) {
    for(tf_byte i = 0; i <= ctx -> ops.size; i++) {
        tf_op op = ctx -> ops.items[i];
        if (strcmp(op.name, name) == 0) {
            *ret_op = ctx->ops.items + i;
            return i;
        }
    }
    *ret_op = NULL;
    return 0;
}

bool tf_copy_substring(const char* src, int start, int end, char* output, int max_len) {
    if (!src || start > end) {
        return false;
    }
    
    int len = end - start + 1;
    if (len > max_len) {
        return false;
    }

    memcpy(output, src + start, len);
    output[len] = '\0';

    return true;
}

bool tf_get_next_token(const char* input, int* start, char* token) {
    if (*start < 0) {
        return false;
    }

    int token_size = 0;
    for (int i = *start; input[i] != '\0'; i++) {
        if (!isspace(input[i])) {
            token_size++;
        } else if (token_size > 0) {
            tf_copy_substring(input, *start, i-1, token, MAX_OP_NAME_SIZE);        
            *start = *start + token_size;
            return true;
        } else {
            (*start)++;
        }
    }

    return false;
}

void tf_parse_to_code_list(const char* input, tf_token_list* list) {
    tf_token_list_item* item = malloc(sizeof(tf_token_list_item));
    if (item == NULL) exit(-1);
    item->next = NULL;
    item->prev = NULL;
    *list = item;
    tf_token_list_item* prev_item;
    
    int start = 0;
    while(tf_get_next_token(input, &start, item->value)) {       
        prev_item = item;
        item = malloc(sizeof(tf_token_list_item));
        if (item == NULL) exit(-1);
        item->next = NULL;
        item->prev = prev_item;
        prev_item->next = item;
    }
    prev_item->next = NULL;
    free(item);
}

void tf_destroy_code_list(tf_token_list* list) {
    if (*list == NULL) {
        return;
    }
    while (*list == NULL) {
        tf_token_list elem = *list;
        *list = elem -> next;
        free(elem);
    }
}

void tf_debug_code_list(tf_token_list list) {
    tf_token_list code = list;
    while (code != NULL) {
        if (code-> prev != NULL) {
            printf("<%s> -> ", code->prev->value); 
        }
        printf("<%s>", code->value); 
        if (code-> next != NULL) {
            printf(" -> <%s>", code->next->value); 
        }
        printf("\n"); 
        code = code -> next;
    }
    printf("---\n");
    code = list;
    int t = 0;
    while (code != NULL) {
        printf("%04d -> <%s>\n", t, code->value); 
        code = code -> next;
        t++;
    }
}

void tf_remove_node_from_code_list(tf_token_list* list, int i) {
    tf_token_list item;
    if (i == 0) {
        item = *list;
        *list = item->next;
        (*list) -> prev = item->prev;
        free(item);
        return;
    }

    int t = 0;
    tf_token_list code = *list;
    while (code != NULL) {
        if (t == i) {
            item = code;
            code = item->prev;
            code -> next = item->next;
            (item->next)->prev = code;
            free(item);
            return;
        }
        code = code -> next;
        t++;
    }
}

void tf_update_node_value_in_code_list(tf_token_list* list, int i, const char* value) {
    tf_token_list code = *list;
    int t = 0;
    while (code != NULL) {
        if (t == i) {
            memcpy(code->value, value, strlen(value));
            code->value[strlen(value)] = '\0';
        }
        code = code -> next;
        t++;
    }    
}

void tf_add_node_to_code_list(tf_token_list* list, int i, const char* value) {
    tf_token_list code = *list;
    tf_token_list item = malloc(sizeof(tf_token_list_item));
    if (item == NULL) exit(-1);

    // Add new head
    if (i == -1) {
        item->next = code;
        item->prev = NULL;
        memcpy(item->value, value, strlen(value));
        item->value[strlen(value)] = '\0';
        *list = item; 
        return;
    }

    int t = 0;
    while (code != NULL) {
        if (t == i) {
            tf_token_list next_node = code->next;
            item->next = next_node;
            next_node->prev = item;
            code->next = item;
            memcpy(item->value, value, strlen(value));
            item->value[strlen(value)] = '\0';
            break;
        }
        code = code -> next;
        t++;
    }
}

void tf_macro_parse(tf_context* ctx, tf_token_list* code) {
    int i = 0;
    int t = 0;

    // Max cyclomatic complexity allowed here is A LOT!
    tf_if_box if_stack[MAX_IF_NESTED_CYCLES] = {0};
    int if_i = 0;

    tf_token_list token = *code;
    while(token != NULL) {
        if (strcmp(token->value, "IF") == 0) {
            if (if_i >= MAX_IF_NESTED_CYCLES) {
                printf("Error: Max cyclomatic complexity reached!\n");
                exit(-1);
            }
            if_stack[if_i++].if_pos = t;
            if_stack[if_i-1].if_node = token;
            strcpy(token->value, "10");
            tf_add_node_to_code_list(code, t, "JZ");
            token = token -> next;
            t++;
        } else if (strcmp(token->value, "ELSE") == 0) {
            if_stack[if_i-1].else_pos = t + 2;
            if_stack[if_i-1].else_node = token;
            strcpy(token->value, "20");
            tf_add_node_to_code_list(code, t, "JMP");
            token = token -> next;
            t++;
        } else if (strcmp(token->value, "THEN") == 0) {
            if_stack[if_i-1].then_pos = t;
            char else_address[32] = {0};
            char then_address[32] = {0};
            snprintf(else_address, sizeof(else_address), "%d", if_stack[if_i-1].else_pos);
            snprintf(then_address, sizeof(then_address), "%d", if_stack[if_i-1].then_pos);
            
            tf_remove_node_from_code_list(code, t);
            token = token -> prev;
            t--;
            strcpy(if_stack[if_i-1].if_node->value, else_address);
            strcpy(if_stack[if_i-1].else_node->value, then_address);
            if_i--;
        }
        
        token = token -> next;
        t++;
    }
}

void tf_base_parse(tf_context* ctx, tf_token_list list) {
    tf_int integer_value;
    tf_float float_value;
    tf_char char_value;
    tf_bool bool_value;
    tf_pointer pointer_value;
    tf_op* op;
    int j = 0;
    
    tf_token_list code = list;
    while (code != NULL) {
        char* token = code -> value;
        int read_integer = sscanf(token, "%d", &integer_value);
        int read_float = sscanf(token, "%f", &float_value);
        int read_char = sscanf(token, "'%c'", &char_value);
        int read_true = sscanf(token, "true");
        int read_false = sscanf(token, "false");
        int read_pointer = sscanf(token, "0x%x", &pointer_value);
        
        // keeping track of word addresses
        ctx->word_address[j++] = ctx->code_stack.cursor;

        if (read_integer) {
            tf_code_push_int(ctx, integer_value);
        } else if (read_float) {
            tf_code_push_float(ctx, float_value);
        } else if (read_char) {
            tf_code_push_char(ctx, char_value);
        } else if (read_true || read_false) {
            tf_code_push_bool(ctx, read_true);
        } else if (read_pointer) {
            tf_code_push_int(ctx, pointer_value);
        } else {
            tf_byte op_index = tf_look_for_op(ctx, token, &op);
            if (op != NULL) {
                tf_code_push_op(ctx, op_index);
            } else {
                printf("Unknown token: %s\n", token);
                exit(-1);
            }
        }
        code = code -> next;
    }
}

void tf_parse(tf_context* ctx, const char* input) {
    tf_token_list code = NULL;
    tf_parse_to_code_list(input, &code);
    tf_macro_parse(ctx, &code);
    tf_base_parse(ctx, code);
    tf_destroy_code_list(&code);
}

unsigned char* tf_get_bytecode(tf_context* ctx, size_t* bytecode_size) {
    *bytecode_size = ctx->code_stack.cursor;
    unsigned char* bytecode = calloc(sizeof(unsigned char), ctx->code_stack.cursor);
    if (bytecode == NULL) {
        return NULL;
    }
    
    for (int i = 0; i < ctx->code_stack.cursor; i++) {
        bytecode[i] = (unsigned char) ctx->code_stack.bytes[i];
    }
    return bytecode;
}

void tf_debug_code_stack(tf_context* ctx) {
    for (int i = 0; i < ctx->code_stack.cursor; i++) {
        unsigned char byte = (unsigned char) ctx->code_stack.bytes[i];
        printf("%02X ", byte);
    }
    printf("\n");
}

void tf_debug_stack(tf_stack* stack) {
    for (int i = 0; i < stack->cursor; i++) {
        unsigned char* bytes = (unsigned char*) &(stack->words[i]);
        for(int b = 0; b < sizeof(stack->words[i]); b++) {
            printf("%02X", bytes[b]);
        }
        printf(" ");
    }
    printf("\n");
}

void tf_debug_byte_array(const unsigned char* array, size_t size) {
    printf("DEBUG: Byte array (size: %ld): [ ", size); 
    for (size_t i = 0; i < size; i++) {
        printf("%02X ", (unsigned char) array[i]);
    }
    printf("]\n");
}

tf_int tf_find_jump_address(tf_context* ctx) {
    tf_int index = ctx -> jumped_to;
    if (index < 0) {
        return -1; // no jump
    }
    return ctx->word_address[index];
}

void tf_clean_jump(tf_context* ctx) {
    ctx -> jumped_to = -1;
}

int tf_save_to_file(tf_context* ctx, const char* filename) {
    size_t bytecode_size = 0;
    unsigned char* bytecode = tf_get_bytecode(ctx, &bytecode_size);
    FILE *file = fopen(filename, "wb");
    if (file == NULL) {
        printf("Error: Cannot open '%s'.\n", filename);
        return 0; 
    }
    size_t written_bytes = fwrite(bytecode, sizeof(unsigned char), bytecode_size, file);
    fclose(file);
    free(bytecode);
    return (written_bytes == bytecode_size) ? 1 : 0;
}

bool tf_parse_file(tf_context* ctx, const char* filename) {
    FILE* f = fopen(filename, "rb");
    if (!f) return false;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size < 0) {
        fclose(f);
        return false;
    }

    char* buffer = malloc(size + 1);
    if (!buffer) {
        fclose(f);
        return false;
    }

    size_t read_bytes = fread(buffer, 1, size, f);
    buffer[read_bytes] = '\0';

    fclose(f);

    tf_parse(ctx, buffer);
    free(buffer);

    return true;
}

int main(int argc, char** argv) {
    if (argc != 3) {
        printf("USAGE: %s [input-file] [output-file]\n", argv[0]);
        exit(0);
    }

    char* input_filename = argv[1];
    char* output_filename = argv[2];

    tf_context ctx;
    tf_init(&ctx);
    if (!tf_parse_file(&ctx, input_filename)) {
        printf("Error failed to parse file '%s'\n", input_filename);
        exit(-1);
    }
    if (!tf_save_to_file(&ctx, output_filename)) {
        printf("Error while saving file!\n");
        exit(-1);
    }
    tf_destroy(&ctx);
    return 0;
}
