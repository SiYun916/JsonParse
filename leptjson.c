#include "leptjson.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <math.h>

#ifndef LEPT_PARSE_STACK_INIT_SIZE
#define LEPT_PARSE_STACK_INIT_SIZE 256
#endif 

#define EXPECT(c, ch)   do {assert(*c->json == (ch)); c->json++;} while(0)
#define ISDIGIT(ch)     ((ch) >= '0' && (ch) <= '9')
#define ISDIGIT1T09(ch) ((ch) >= '1' && (ch) <= '9')
#define PUTC(c, ch)     do { *(char *)lept_context_push(c, sizeof(char)) = (ch); } while(0)


typedef struct {
    const char *json;
    char *stack;
    size_t size, top;
}lept_context;

/*按照PUTC来说 这里的size 1字节  这里是返回地址 把值存到地址中去是PUTC*/
static void *lept_context_push(lept_context *c, size_t size) {
    void *ret;
    assert(size > 0);
    if (c->top + size > c->size) {
        if (c->size == 0)
            c->size = LEPT_PARSE_STACK_INIT_SIZE;
        while (c->top + size >= c->size)
            c->size += c->size >> 1;  /*c->size变成原来的1.5倍*/
        c->stack = (char *)realloc(c->stack, c->size);
    }
    ret = c->stack + c->top;
    c->top += size;
    return ret;
}

/*返回的是地址 char进行类型转换 这里为什么要把c->stack*/
static void *lept_context_pop(lept_context *c, size_t size) {
    assert(c->top >= size);
    return c->stack + (c->top -= size);
}

static void lept_parse_whitespace(lept_context *c) {
    const char *p = c->json;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
        p++;
    }
    c->json = p;
}

/*解析null true false*/
static int lept_parse_literal(lept_context *c, lept_value *v, const char *literal, lept_type type) {
    size_t i;
    EXPECT(c, literal[0]);
    /*literal[i+1]为空的时候终止，此时i为*/
    for (i = 0; literal[i + 1]; i++) {
        /*c已经后移了一位，所以比较的时候当前c要和literal的下一位比较*/
        if (c->json[i] != literal[i + 1])
            return LEPT_PARSE_INVALID_VALUE;
    }
    c->json += i;
    v->type = type;
    return LEPT_PARSE_OK;
}

 /*解析数字*/
static int lept_parse_number(lept_context *c, lept_value *v) {
    const char *p = c->json;
    if (*p == '-') p++;
    if (*p == '0') p++;
    else {
        if (!ISDIGIT1T09(*p)) return LEPT_PARSE_INVALID_VALUE;
        /*以p++为初始值的话，第一次循环的值是p+1而不是p。因为上面已经判断过一次了，这里直接从下一个开始*/
        for (p++; ISDIGIT(*p); p++);
    }
    if (*p == '.') {
       p++;
       if(!ISDIGIT(*p)) return LEPT_PARSE_INVALID_VALUE;
       /*for是初始化后先执行一次判断，所以这里是初始化p++后先执行一次ISDIGIT再去自增*/
       for (p++; ISDIGIT(*p); p++);
    }
    if (*p == 'e' || *p == 'E') {
        p++;
        if (*p =='+' || *p == '-') p++;
        if (!ISDIGIT(*p)) return LEPT_PARSE_INVALID_VALUE;
        for (p++; ISDIGIT(*p); p++);
    }
    errno = 0;
    v->u.n = strtod(c->json, NULL);
    if (errno == ERANGE && (v->u.n == HUGE_VAL || v->u.n == -HUGE_VAL))
        return LEPT_PARSE_NUMBER_TOO_BIG;
    v->type = LEPT_NUMBER;
    c->json = p;
    return LEPT_PARSE_OK;
}

static const char* lept_parse_hex4(const char *p, unsigned *u) {
    return p;
}

static void lept_encode_utf8(lept_context *c, unsigned *u) {

}

#define STRING_ERROR(ret) do { c->top = head; return ret; } while(0)

/*解析字符串*/
static int lept_parse_string(lept_context *c, lept_value *v) {
    size_t head = c->top;
    size_t len;
    unsigned u;
    const char *p;
    EXPECT(c, '\"');
    p = c->json;
    for (;;) {
        char ch = *p++;
        switch (ch) {
            case '\"':
                /*len表示字符串的长度*/
                len = c->top - head;
                lept_set_string(v, (const char *)lept_context_pop(c, len), len);
                c->json = p;
                return LEPT_PARSE_OK;
            case '\\':
                switch (*p++) {
                    case '\"': PUTC(c, '\"');break;
                    case '\\': PUTC(c, '\\'); break;
                    case '/':  PUTC(c, '/' ); break;
                    case 'b':  PUTC(c, '\b'); break;
                    case 'f':  PUTC(c, '\f'); break;
                    case 'n':  PUTC(c, '\n'); break;
                    case 'r':  PUTC(c, '\r'); break;
                    case 't':  PUTC(c, '\t'); break;
                    case 'u':
                        if (!(p = lept_parse_hex4(p, &u)))
                            STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_HEX);
                        lept_encode_utf8(c, u);
                        break;
                    default:
                        STRING_ERROR(LEPT_PARSE_INVALID_STRING_ESCAPE);
                }
                break;
            case '\0':
                STRING_ERROR(LEPT_PARSE_MISS_QUOTATION_MARK);
            default:
                if ((unsigned char)ch < 0x20) { 
                    STRING_ERROR(LEPT_PARSE_INVALID_STRING_CHAR);
                }
                PUTC(c, ch); 
        }
    }
}

/********************************************************
解析，根据c中json解析，结果存到v中。如果第一个值是字母开头，对
应解析null true false即可，如果是"开头，说明是值是字符串。如
果是\0说明值是空，返回的解析状态就不是OK。数字开头就是解析数字
*********************************************************
*/

static int lept_parse_value(lept_context *c, lept_value *v) {
    switch (*c->json) {
        case 'n' : return lept_parse_literal(c, v, "null", LEPT_NULL);
        case 't' : return lept_parse_literal(c, v, "true", LEPT_TRUE);
        case 'f' : return lept_parse_literal(c, v, "false", LEPT_FALSE);
        default : return lept_parse_number(c, v);
        case '"' : return lept_parse_string(c, v);
        case '\0' : return LEPT_PARSE_EXPECT_VALUE;
    }
}

/********************************************************
解析主函数，解析输入的json串，将其类型和值设进自定义的类型 v，
v即可用于后续的操作。
*********************************************************
*/
int lept_parse(lept_value *v, const char *json) {
    /*目前的理解是，如果是一个很长的串{'name':"bob",age:18}这种，需要用一个栈来保存每个值*/
    lept_context c;
    /*解析value状态*/
    int ret;
    /*如果v为空，说明没创建*/
    assert(v != NULL);
    /*设一个串来临时保存json，逐步解析*/
    c.json = json;
    c.stack = NULL;
    c.size = c.top = 0;
    /*初始化v*/
    lept_init(v);
    /*取c的地址，解析c中json串的空白*/
    lept_parse_whitespace(&c);
    /*解析值，如果正确解析了一个值*/
    if ((ret = lept_parse_value(&c, v)) == LEPT_PARSE_OK) {
        lept_parse_whitespace(&c);
        /*如果value不是单个值，设类型是NULL*/
        if (*c.json != '\0') {
            v->type = LEPT_NULL;
            ret = LEPT_PARSE_ROOT_NOT_SINGULAR;
        }     
    }
    assert(c.top == 0);
    free(c.stack);
    return ret;
} 

void lept_free(lept_value *v) {
    assert(v != NULL);
    if (v->type == LEPT_STRING)
        free(v->u.s.s);
    v->type = LEPT_NULL;
}

lept_type lept_get_type(const lept_value *v) {
    assert(v != NULL);
    return v->type;
}

int lept_get_boolean(const lept_value *v) {
    assert(v != NULL && (v->type == LEPT_TRUE || v->type == LEPT_FALSE));
    return v->type == LEPT_TRUE;
}

void lept_set_boolean(lept_value *v, int b) {
    lept_free(v);
    v->type = b ? LEPT_TRUE : LEPT_FALSE;
}

double lept_get_number(const lept_value *v) {
    assert(v != NULL && v->type == LEPT_NUMBER);
    return v->u.n;
}

void lept_set_number(lept_value *v, double n) {
    lept_free(v);
    v->u.n = n;
    v->type = LEPT_NUMBER;
}

const char *lept_get_string(const lept_value *v) {
    assert(v != NULL && v->type == LEPT_STRING);
    return v->u.s.s;
}

size_t lept_get_string_length(const lept_value *v) {
    assert(v != NULL && v->type == LEPT_STRING);
    return v->u.s.len;
}

/*将s串中的值拷贝到v中*/
void lept_set_string(lept_value *v, const char *s, size_t len) {
    assert(v != NULL && (s != NULL || len == 0));
    lept_free(v);
    v->u.s.s = (char*)malloc(len+1);
    memcpy(v->u.s.s, s, len);
    v->u.s.s[len] = '\0';
    v->u.s.len = len;
    v->type = LEPT_STRING;
}