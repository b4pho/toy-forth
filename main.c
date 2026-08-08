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
#include <string.h>

#define MAX_NATIVE_OPS 256
#define MAX_WORDS      65536

typedef struct tf_context tf_context;

typedef void (*tf_func)(tf_context *);

typedef int32_t tf_int;
typedef float tf_float;
typedef u_int32_t tf_pointer;
typedef unsigned char tf_char;
typedef unsigned char tf_bool;

typedef struct {
    char name[16];
    tf_func code;
} tf_op;

typedef struct {
    tf_op* items;
    u_int8_t size;
} tf_op_list;

typedef u_int8_t tf_word;

struct tf_context {
    tf_op_list ops;
    tf_word* words;
    size_t cursor;
};

typedef enum {
    TF_INT,
    TF_FLT,
    TF_BLN,
    TF_CHR,
    TF_PTR,
    TF_OPN
} tf_type;

void tf_init_ops(tf_op_list* operations) {
    operations -> items = malloc(sizeof(tf_op) * MAX_NATIVE_OPS);
    if (operations -> items == NULL) {
        perror("failed to define operations list\n");
        exit(-1);
    }
    operations -> size = 0;
}

void tf_add_op(tf_op_list* operations, const char* name, tf_func code) {
    if (operations == NULL) return;
    if (operations -> size + 1 >= MAX_NATIVE_OPS) return;
    strncpy(operations -> items[(operations -> size)++].name, name, strlen(name));
    operations -> items[(operations -> size) - 1].code = code;
}

tf_int tf_pop_int(tf_context* ctx) {
    ctx -> cursor--; // remove type byte
    tf_word b1 = ctx -> words[ctx->cursor--];
    tf_word b2 = ctx -> words[ctx->cursor--];
    tf_word b3 = ctx -> words[ctx->cursor--];
    tf_word b4 = ctx -> words[ctx->cursor--];
    tf_int num = b1 << 24 | b2 << 16 | b3 << 8 | b4;
    // printf("[%d %d %d %d]\n", b1, b2, b3, b4);
    return num;
}

void tf_push_int(tf_context* ctx, tf_int n) {
    if (ctx -> cursor >= MAX_WORDS) {
        return;
    }
    ctx -> words[ctx->cursor++] = TF_INT;
    ctx -> words[ctx->cursor++] = (n >> (0 * 8)) & 0xFF;
    ctx -> words[ctx->cursor++] = (n >> (1 * 8)) & 0xFF;
    ctx -> words[ctx->cursor++] = (n >> (2 * 8)) & 0xFF;
    ctx -> words[ctx->cursor++] = (n >> (3 * 8)) & 0xFF;
}

void tf_push_float(tf_context* ctx, float n) {
    if (ctx -> cursor >= MAX_WORDS) {
        return;
    }
    ctx -> words[ctx->cursor++] = TF_FLT;
    ctx -> words[ctx->cursor++] = ((u_int32_t) n >> (0 * 8)) & 0xFF;
    ctx -> words[ctx->cursor++] = ((u_int32_t) n >> (1 * 8)) & 0xFF;
    ctx -> words[ctx->cursor++] = ((u_int32_t) n >> (2 * 8)) & 0xFF;
    ctx -> words[ctx->cursor++] = ((u_int32_t) n >> (3 * 8)) & 0xFF;
}

void tf_push_bool(tf_context* ctx, u_int32_t n) {
    if (ctx -> cursor >= MAX_WORDS) {
        return;
    }
    ctx -> words[ctx->cursor++] = TF_BLN;
    ctx -> words[ctx->cursor++] = (tf_word) n;
}

void tf_push_char(tf_context* ctx, unsigned char c) {
    if (ctx -> cursor >= MAX_WORDS) {
        return;
    }
    ctx -> words[ctx->cursor++] = TF_CHR;
    ctx -> words[ctx->cursor++] = (tf_word) c;
}

void tf_push_op(tf_context* ctx, u_int8_t op) {
    if (ctx -> cursor >= MAX_WORDS) {
        return;
    }
    ctx -> words[ctx->cursor++] = TF_OPN;
    ctx -> words[ctx->cursor++] = (tf_word) op;
}

void tf_op_add(tf_context* ctx) {
    // tf_debug_stack(ctx);
    tf_int b = tf_pop_int(ctx);
    tf_int a = tf_pop_int(ctx);
    // printf("%d %d\n", a, b);
    tf_push_int(ctx, a+b);
}

void tf_op_print_int(tf_context* ctx) {
    // tf_debug_stack(ctx);
    tf_int a = tf_pop_int(ctx);
    // printf("%d %d\n", a, b);
    printf("%d\n", a);
}

void tf_init(tf_context* ctx) {
    tf_init_ops(&(ctx -> ops));
    tf_add_op(&(ctx -> ops), "+", tf_op_add);
    tf_add_op(&(ctx -> ops), "print_int", tf_op_print_int);
    ctx -> words = malloc(sizeof(tf_word) * MAX_WORDS);
    ctx -> cursor = 0;
}

u_int8_t tf_look_for_op(tf_context* ctx, const char* name, tf_op** ret_op) {
    for(u_int8_t i = ctx -> ops.size; i >= 0; i--) {
        tf_op op = ctx -> ops.items[i];
        if (strcmp(op.name, name) == 0) {
            *ret_op = ctx->ops.items + i;
            return i;
        }
    }
    *ret_op = NULL;
    return 0;
} 

int tf_parse(tf_context* ctx, const char *input) {
    size_t input_len = strlen(input);
    char* code = malloc(sizeof(char) * input_len);
    if (code == NULL) {
        return -1;
    }
    if (strncpy(code, input, input_len) == 0) {
        return -2;
    }
    
    char *token = strtok(code, " \t\n");

    u_int32_t integer_value;
    float float_value;
    unsigned char char_value;
    unsigned char bool_value;
    u_int32_t pointer_value;
    tf_op* op;
    
    while (token != NULL) {
        int read_integer = sscanf(token, "%d", &integer_value);
        int read_float = sscanf(token, "%f", &float_value);
        int read_char = sscanf(token, "'%c'", &char_value);
        int read_true = sscanf(token, "true");
        int read_false = sscanf(token, "false");
        int read_pointer = sscanf(token, "%x", &pointer_value);
        
        if (read_integer) {
            // printf("read_integer: %d\n", integer_value);
            tf_push_int(ctx, integer_value);
        } else if (read_float) {
            // printf("read_float: %f\n", float_value);
            tf_push_float(ctx, float_value);
        } else if (read_char) {
            // printf("read_char: %c\n", char_value);
            tf_push_char(ctx, char_value);
        } else if (read_true || read_false) {
            // printf("read_bool: %s\n", read_true ? "true" : "false");
            tf_push_bool(ctx, read_true);
        } else if (read_pointer) {
            // printf("read_pointer: %x\n", pointer_value);
            tf_push_int(ctx, pointer_value);
        } else {
            u_int8_t op_index = tf_look_for_op(ctx, token, &op);
            if (op != NULL) {
                // printf("read operand: %s --> <%s>\n", token, op->name);
                tf_push_op(ctx, op_index);
            } else {
                printf("Unknown token: %s\n", token);
                exit(-1);
            }
        }
        //ctx->words[(ctx -> cursor)++].name = 
        
        token = strtok(NULL, " \t\n");
    }

    free(code);
    return 0;
}

unsigned char* tf_get_bytecode(tf_context* ctx, size_t* bytecode_size) {
    *bytecode_size = ctx->cursor;
    unsigned char* bytecode = calloc(sizeof(unsigned char), ctx->cursor);
    if (bytecode == NULL) {
        return NULL;
    }
    
    for (int i = 0; i < ctx->cursor; i++) {
        bytecode[i] = (unsigned char) ctx->words[i];
    }
    return bytecode;
}

void tf_debug_stack(tf_context* ctx) {
    for (int i = 0; i < ctx->cursor; i++) {
        unsigned char byte = (unsigned char) ctx->words[i];
        printf("%02X ", byte);
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

void tf_exec(tf_context* ctx) {
    // tf_exec_state state = TF_EX_TYPE;
    int i = 0;
    int code_size = ctx->cursor;
    while (i < code_size) {
        tf_word type = ctx->words[i++];
        switch (type) {
            case TF_OPN:
                tf_word op_index = ctx->words[i++];
                // printf("--> %d\n", op_index);
                tf_op op = ctx->ops.items[op_index];
                if (op.code == NULL) {
                    printf("Error: '%s' operator is not defined\n", op.name);
                    exit(-2);
                } else {
                    op.code(ctx);
                }
                break;
            case TF_INT:
                tf_word b4 = ctx->words[i++];
                tf_word b3 = ctx->words[i++];
                tf_word b2 = ctx->words[i++];
                tf_word b1 = ctx->words[i++];
                tf_int num = b1 << 24 | b2 << 16 | b3 << 8 | b4;
                // printf("--> %d [%d %d %d %d]\n", num, b1, b2, b3, b4);
                tf_push_int(ctx, num);
                break;
            // TODO OTHER TYPES
            default:
                printf("Invalid bytecode\n");
                exit(-2);
        }
    }
    printf("\n");
}


int main(int argc, char** argv) {
    tf_context ctx;
    tf_init(&ctx);
    tf_parse(&ctx, "1 41 + print_int");
    // size_t bytecode_size = 0;
    // unsigned char* bytecode = tf_get_bytecode(&ctx, &bytecode_size);
    // tf_debug_byte_array(bytecode, bytecode_size);
    // free(bytecode);
    // tf_debug_stack(&ctx);
    tf_exec(&ctx);
    // tf_debug_stack(&ctx);
    return 0;
}
