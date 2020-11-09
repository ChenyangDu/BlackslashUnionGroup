#ifndef __THREAD_FIXED_POINT_H
#define __THREAD_FIXED_POINT_H

/* 浮点数数据类型 */
typedef int fixed_t;
/* 浮点数小数部分位数 */
#define SHIFT_VALUE 16
/* 整数转为浮点数 */
#define INT_TO_FIXED(X) ((fixed_t)(X << SHIFT_VALUE))
/* 浮点数转为整数向0取整 */
#define FIXED_TO_INT_ZERO(X) (X >> SHIFT_VALUE)
/* 浮点数转为整数 */
#define FIXED_TO_INT_ROUND(X) (X >= 0 ? ((X + (1 << (SHIFT_VALUE - 1))) >> SHIFT_VALUE) : ((X - (1 << (SHIFT_VALUE - 1))) >> SHIFT_VALUE))
/* 浮点数加法 */
#define F_ADD(X,Y) (X + Y)
/* 浮点数减法 */
#define F_SUB(X,Y) (X - Y)
/* 浮点数乘法 */
#define F_MUL(X,Y) ((fixed_t)(((int64_t) X) * Y >> SHIFT_VALUE))
/* 浮点数除法 */
#define F_DIV(X,Y) ((fixed_t)((((int64_t) X) << SHIFT_VALUE) / Y))

#endif /* thread/fixed_point.h */