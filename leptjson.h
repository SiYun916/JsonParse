#ifndef LEPTJSON_H__
#define LEPTJSON_H__

#include <stddef.h> /*size_t引入*/

typedef enum {LEPT_NULL, LEPT_FALSE, LEPT_TRUE, LEPT_NUMBER, LEPT_STRING, LEPT_ARRAY, LEPT_OBJECT} lept_type;

/*************************************************************************
当解析json时，按照json的标准，可以分为上述几类，这几类可以用一个枚举类型表示代表。
解析json的目的是什么？是为了在程序中使用。json中存放的是数据，程序需要对这些数据
进行处理，利用这些数据进行相应操作。那么怎么读取json中的数据呢？这里就用到了解析
器，需要把json格式的文件中的内容解析成对应语言所能理解的数据。所以这个库称为json
解析库。
**************************************************************************
*/
typedef struct {
    union {
        struct { char *s; size_t len;}s; /*s->存储json解析出来的字符串，len->存储字符串长度*/
        double n;                        /*n->存储数字*/
    }u;
    lept_type type;
}lept_value;

/*************************************************************************
解析的时候，如果json数据的格式错误或者其他，将json解析成C中数据时会遇到各种各样的
问题，解析的时候对于每个遇到的问题都有一个状态码。如果成功解析，就返回LEPT_PARSE_
OK说明解析成功。
**************************************************************************
*/
enum {
    LEPT_PARSE_OK = 0,
    LEPT_PARSE_EXPECT_VALUE,
    LEPT_PARSE_INVALID_VALUE,
    LEPT_PARSE_ROOT_NOT_SINGULAR,
    LEPT_PARSE_NUMBER_TOO_BIG,
    LEPT_PARSE_MISS_QUOTATION_MARK,
    LEPT_PARSE_INVALID_STRING_ESCAPE,
    LEPT_PARSE_INVALID_STRING_CHAR
};

/*对输入的json串解析前，先给将要存储在C语言中的数据初始化*/
#define lept_init(v) do{ (v)->type = LEPT_NULL; } while(0)

int lept_parse(lept_value *v, const char *json);

void lept_free(lept_value *v);

lept_type lept_get_type(const lept_value *v);

#define lept_set_null(v) lept_free(v)

int lept_get_boolean(const lept_value *v);
void lept_set_boolean(lept_value *v, int b);

double lept_get_number(const lept_value *v);
void lept_set_number(lept_value *v, double n);

const char *lept_get_string(const lept_value *v);
size_t lept_get_string_length(const lept_value *v);
void lept_set_string(lept_value *v, const char *s, size_t len);

#endif
