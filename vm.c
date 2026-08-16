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
#include <math.h>
#include <string.h>

#define MAX_NATIVE_OPS       256
#define CODE_STACK_SIZE    65536
#define DATA_STACK_SIZE      256
#define RETURN_STACK_SIZE      2

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
    tf_func code;
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

void tf_init_ops(tf_op_list* operations) {
    operations -> size = 0;
}

void tf_add_op(tf_op_list* operations, tf_func code) {
    if (operations == NULL) return;
    if (operations -> size + 1 >= MAX_NATIVE_OPS) return;
    operations -> items[(operations -> size)++].code = code;
}

tf_int tf_code_pop_int(tf_context* ctx) {
    ctx -> code_stack.cursor--; // remove type byte
    tf_byte b1 = ctx -> code_stack.bytes[ctx->code_stack.cursor--];
    tf_byte b2 = ctx -> code_stack.bytes[ctx->code_stack.cursor--];
    tf_byte b3 = ctx -> code_stack.bytes[ctx->code_stack.cursor--];
    tf_byte b4 = ctx -> code_stack.bytes[ctx->code_stack.cursor--];
    tf_int num = b1 << 24 | b2 << 16 | b3 << 8 | b4;
    // printf("[%d %d %d %d]\n", b1, b2, b3, b4);
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

void tf_op_add(tf_context* ctx) {
    tf_int b = tf_pop(&ctx->data_stack).data.as_int;
    tf_int a = tf_pop(&ctx->data_stack).data.as_int;
    tf_push_int(&ctx->data_stack, a+b);
}

void tf_op_sub(tf_context* ctx) {
    tf_int b = tf_pop(&ctx->data_stack).data.as_int;
    tf_int a = tf_pop(&ctx->data_stack).data.as_int;
    tf_push_int(&ctx->data_stack, a-b);
}

void tf_op_mul(tf_context* ctx) {
    tf_int b = tf_pop(&ctx->data_stack).data.as_int;
    tf_int a = tf_pop(&ctx->data_stack).data.as_int;
    tf_push_int(&ctx->data_stack, a*b);
}

void tf_op_div(tf_context* ctx) {
    tf_int b = tf_pop(&ctx->data_stack).data.as_int;
    tf_int a = tf_pop(&ctx->data_stack).data.as_int;
    tf_push_int(&ctx->data_stack, a/b);
}

void tf_op_pow(tf_context* ctx) {
    tf_int b = tf_pop(&ctx->data_stack).data.as_int;
    tf_int a = tf_pop(&ctx->data_stack).data.as_int;
    tf_push_int(&ctx->data_stack, pow(a,b));
}

void tf_op_mod(tf_context* ctx) {
    tf_int b = tf_pop(&ctx->data_stack).data.as_int;
    tf_int a = tf_pop(&ctx->data_stack).data.as_int;
    tf_push_int(&ctx->data_stack, a % b);
}

void tf_op_dup(tf_context* ctx) {
    tf_word a = tf_pop(&ctx->data_stack);
    tf_push(&ctx->data_stack, a);
    tf_push(&ctx->data_stack, a);
}

void tf_op_swap(tf_context* ctx) {
    tf_word b = tf_pop(&ctx->data_stack);
    tf_word a = tf_pop(&ctx->data_stack);
    tf_push(&ctx->data_stack, b);
    tf_push(&ctx->data_stack, a);
}

void tf_op_over(tf_context* ctx) {
    tf_word b = tf_pop(&ctx->data_stack);
    tf_word a = tf_pop(&ctx->data_stack);
    tf_push(&ctx->data_stack, a);
    tf_push(&ctx->data_stack, b);
    tf_push(&ctx->data_stack, a);
}

void tf_op_drop(tf_context* ctx) {
    tf_pop(&ctx->data_stack);
}

void tf_op_rot(tf_context* ctx) {
    tf_word c = tf_pop(&ctx->data_stack);
    tf_word b = tf_pop(&ctx->data_stack);
    tf_word a = tf_pop(&ctx->data_stack);
    tf_push(&ctx->data_stack, b);
    tf_push(&ctx->data_stack, c);
    tf_push(&ctx->data_stack, a);
}

void tf_op_eq(tf_context* ctx) {
    tf_word b = tf_pop(&ctx->data_stack);
    tf_word a = tf_pop(&ctx->data_stack);
    tf_bool c = false;
    if (a.type == b.type) {
        c = a.data.as_pointer == b.data.as_pointer;
    }
    tf_push_bool(&ctx->data_stack, c);
}

void tf_op_neq(tf_context* ctx) {
    tf_word b = tf_pop(&ctx->data_stack);
    tf_word a = tf_pop(&ctx->data_stack);
    tf_bool c = true;
    if (a.type == b.type) {
        c = !(a.data.as_pointer == b.data.as_pointer);
    }
    tf_push_bool(&ctx->data_stack, c);
}

void tf_op_lt(tf_context* ctx) {
    tf_word b = tf_pop(&ctx->data_stack);
    tf_word a = tf_pop(&ctx->data_stack);
    tf_bool c = false;
    if (a.type == b.type) {
        switch (a.type) {
            case TF_INT:
                c = a.data.as_int < b.data.as_int;
                break;
            case TF_FLT:
                c = a.data.as_float < b.data.as_float;
                break;
            case TF_CHR:
                c = a.data.as_char < b.data.as_char;
                break;
            case TF_BLN:
                c = false; // because it doesn't make sense 
                break;
            case TF_PTR:
                c = a.data.as_pointer < b.data.as_pointer;
                break;
            case TF_OPN:
                c = false; // because it doesn't make sense
                break;
        }
    }
    tf_push_bool(&ctx->data_stack, c);
}

void tf_op_gt(tf_context* ctx) {
    tf_word b = tf_pop(&ctx->data_stack);
    tf_word a = tf_pop(&ctx->data_stack);
    tf_bool c = false;
    if (a.type == b.type) {
        switch (a.type) {
            case TF_INT:
                c = a.data.as_int > b.data.as_int;
                break;
            case TF_FLT:
                c = a.data.as_float > b.data.as_float;
                break;
            case TF_CHR:
                c = a.data.as_char > b.data.as_char;
                break;
            case TF_BLN:
                c = false; // because it doesn't make sense 
                break;
            case TF_PTR:
                c = a.data.as_pointer > b.data.as_pointer;
                break;
            case TF_OPN:
                c = false; // because it doesn't make sense
                break;
        }
    }
    tf_push_bool(&ctx->data_stack, c);
}

void tf_op_lte(tf_context* ctx) {
    tf_word b = tf_pop(&ctx->data_stack);
    tf_word a = tf_pop(&ctx->data_stack);
    tf_bool c = false;
    if (a.type == b.type) {
        switch (a.type) {
            case TF_INT:
                c = a.data.as_int <= b.data.as_int;
                break;
            case TF_FLT:
                c = a.data.as_float <= b.data.as_float;
                break;
            case TF_CHR:
                c = a.data.as_char <= b.data.as_char;
                break;
            case TF_BLN:
                c = false; // because it doesn't make sense 
                break;
            case TF_PTR:
                c = a.data.as_pointer <= b.data.as_pointer;
                break;
            case TF_OPN:
                c = false; // because it doesn't make sense
                break;
        }
    }
    tf_push_bool(&ctx->data_stack, c);
}

void tf_op_gte(tf_context* ctx) {
    tf_word b = tf_pop(&ctx->data_stack);
    tf_word a = tf_pop(&ctx->data_stack);
    tf_bool c = false;
    if (a.type == b.type) {
        switch (a.type) {
            case TF_INT:
                c = a.data.as_int >= b.data.as_int;
                break;
            case TF_FLT:
                c = a.data.as_float >= b.data.as_float;
                break;
            case TF_CHR:
                c = a.data.as_char >= b.data.as_char;
                break;
            case TF_BLN:
                c = false; // because it doesn't make sense 
                break;
            case TF_PTR:
                c = a.data.as_pointer >= b.data.as_pointer;
                break;
            case TF_OPN:
                c = false; // because it doesn't make sense
                break;
        }
    }
    tf_push_bool(&ctx->data_stack, c);
}

void tf_op_not(tf_context* ctx) {
    tf_bool a = tf_pop(&ctx->data_stack).data.as_bool;
    tf_push_bool(&ctx->data_stack, !a);
}

void tf_op_and(tf_context* ctx) {
    tf_bool b = tf_pop(&ctx->data_stack).data.as_bool;
    tf_bool a = tf_pop(&ctx->data_stack).data.as_bool;
    tf_push_bool(&ctx->data_stack, a && b);
}

void tf_op_or(tf_context* ctx) {
    tf_bool b = tf_pop(&ctx->data_stack).data.as_bool;
    tf_bool a = tf_pop(&ctx->data_stack).data.as_bool;
    tf_push_bool(&ctx->data_stack, a || b);
}

void tf_op_print_word(tf_context* ctx) {
    tf_word a = tf_pop(&ctx->data_stack);
    switch (a.type) {
        case TF_INT:
            printf("%d ", a.data.as_int);
            break;
        case TF_FLT:
            printf("%f ", a.data.as_float);
            break;
        case TF_BLN:
            printf("%s ", a.data.as_bool ? "true" : "false");
            break;
        case TF_CHR:
            printf("%c ", a.data.as_char);
            break;
        case TF_PTR:
            printf("%x ", a.data.as_pointer);
            break;
        case TF_OPN:
            printf("%d ", a.data.as_op);
            break;
    }
}

void tf_op_print_newline(tf_context* ctx) {
    printf("\n");
}

void tf_op_jump_true(tf_context* ctx) {
    tf_pointer a = tf_pop(&ctx->data_stack).data.as_pointer;
    tf_bool b = tf_pop(&ctx->data_stack).data.as_bool;
    if (b) {
        ctx->jumped_to = a;
    }
}

void tf_op_jump_false(tf_context* ctx) {
    tf_pointer a = tf_pop(&ctx->data_stack).data.as_pointer;
    tf_bool b = tf_pop(&ctx->data_stack).data.as_bool;
    if (!b) {
        ctx->jumped_to = a;
    }
}

void tf_op_jump(tf_context* ctx) {
    tf_pointer a = tf_pop(&ctx->data_stack).data.as_pointer;
    ctx -> jumped_to = a;
}

void tf_init(tf_context* ctx) {
    tf_init_ops(&(ctx -> ops));
    tf_add_op(&(ctx -> ops), tf_op_dup);
    tf_add_op(&(ctx -> ops), tf_op_over);
    tf_add_op(&(ctx -> ops), tf_op_swap);
    tf_add_op(&(ctx -> ops), tf_op_drop);
    tf_add_op(&(ctx -> ops), tf_op_rot);
    tf_add_op(&(ctx -> ops), tf_op_add);
    tf_add_op(&(ctx -> ops), tf_op_sub);
    tf_add_op(&(ctx -> ops), tf_op_mul);
    tf_add_op(&(ctx -> ops), tf_op_div);
    tf_add_op(&(ctx -> ops), tf_op_pow);
    tf_add_op(&(ctx -> ops), tf_op_mod);
    tf_add_op(&(ctx -> ops), tf_op_eq);
    tf_add_op(&(ctx -> ops), tf_op_neq);
    tf_add_op(&(ctx -> ops), tf_op_lt);
    tf_add_op(&(ctx -> ops), tf_op_gt);
    tf_add_op(&(ctx -> ops), tf_op_lte);
    tf_add_op(&(ctx -> ops), tf_op_gte);
    tf_add_op(&(ctx -> ops), tf_op_not);
    tf_add_op(&(ctx -> ops), tf_op_and);
    tf_add_op(&(ctx -> ops), tf_op_or);
    tf_add_op(&(ctx -> ops), tf_op_print_word);
    tf_add_op(&(ctx -> ops), tf_op_print_newline);
    tf_add_op(&(ctx -> ops), tf_op_jump_false);
    tf_add_op(&(ctx -> ops), tf_op_jump_true);
    tf_add_op(&(ctx -> ops), tf_op_jump);
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

void tf_exec(tf_context* ctx) {
    int i = 0;
    int code_size = ctx->code_stack.cursor;
    while (i < code_size) {
        tf_byte type = ctx->code_stack.bytes[i++];
        tf_byte b1, b2, b3, b4;
        tf_int num;
        tf_float fnum;
        switch (type) {
            case TF_OPN:
                tf_byte op_index = ctx->code_stack.bytes[i++];
                tf_op op = ctx->ops.items[op_index];
                if (op.code == NULL) {
                    printf("Error: #%x operator is not defined\n", op_index);
                    exit(-2);
                } else {
                    op.code(ctx);
                    // this is used for IF and LOOPS
                    tf_int new_address = tf_find_jump_address(ctx);
                    if (new_address > -1) {
                        i = new_address;
                        tf_clean_jump(ctx);
                    }
                }
                break;
            case TF_INT:
                b4 = ctx->code_stack.bytes[i++];
                b3 = ctx->code_stack.bytes[i++];
                b2 = ctx->code_stack.bytes[i++];
                b1 = ctx->code_stack.bytes[i++];
                num = b1 << 24 | b2 << 16 | b3 << 8 | b4;
                tf_push_int(&ctx->data_stack, num);
                break;
            case TF_FLT:
                b4 = ctx->code_stack.bytes[i++];
                b3 = ctx->code_stack.bytes[i++];
                b2 = ctx->code_stack.bytes[i++];
                b1 = ctx->code_stack.bytes[i++];
                fnum = b1 << 24 | b2 << 16 | b3 << 8 | b4;
                tf_push_float(&ctx->data_stack, (tf_float) fnum);
                break;
            case TF_CHR:
                b1 = ctx->code_stack.bytes[i++];
                tf_push_char(&ctx->data_stack, (tf_char) b1);
                break;
            case TF_BLN:
                b1 = ctx->code_stack.bytes[i++];
                tf_push_bool(&ctx->data_stack, (tf_bool) b1);
                break;
            case TF_PTR:
                b4 = ctx->code_stack.bytes[i++];
                b3 = ctx->code_stack.bytes[i++];
                b2 = ctx->code_stack.bytes[i++];
                b1 = ctx->code_stack.bytes[i++];
                num = b1 << 24 | b2 << 16 | b3 << 8 | b4;
                tf_push_pointer(&ctx->data_stack, (tf_pointer) num);
                break;
            default:
                printf("Invalid bytecode\n");
                exit(-2);
        }
    }
    printf("\n");
}

void tf_load(tf_context* ctx, const unsigned char* bytecode, size_t bytecode_size) {
    int j = 0;
    int next_bytes = 0;
    tf_type type;
    ctx->code_stack.cursor = 0;
    for (size_t i = 0; i < bytecode_size; i++) {
        if (next_bytes == 0) {
            ctx->word_address[j++] = i;
            type = (tf_type) bytecode[i];
            switch (type) {
                case TF_INT: next_bytes = 5; break;
                case TF_FLT: next_bytes = 5; break;
                case TF_BLN: next_bytes = 2; break;
                case TF_CHR: next_bytes = 2; break;
                case TF_PTR: next_bytes = 5; break;
                case TF_OPN: next_bytes = 2; break;
            }
        }
        ctx->code_stack.bytes[ctx->code_stack.cursor++] = bytecode[i];
        next_bytes--;
    }
}

size_t tf_load_from_file(tf_context* ctx, const char* filename) {
    FILE *file = fopen(filename, "rb");
    if (file == NULL) {
        printf("Error: Cannot open '%s'.\n", filename);
        return 0; 
    }

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    rewind(file);

    unsigned char* buffer = (unsigned char *) malloc(size);
    if (buffer == NULL) {
        fclose(file);
        return 0;
    }
    size_t buffer_size = fread(buffer, 1, size, file);
    fclose(file);

    tf_load(ctx, buffer, buffer_size);
    free(buffer);
    
    return buffer_size;
}

int main(int argc, char** argv) {
    if (argc != 2) {
        printf("USAGE: %s [input-file]\n", argv[0]);
        exit(0);
    }

    char* input_filename = argv[1];

    tf_context ctx;
    tf_init(&ctx);
    tf_load_from_file(&ctx, input_filename);
    tf_exec(&ctx);
    tf_destroy(&ctx);
    return 0;
}
