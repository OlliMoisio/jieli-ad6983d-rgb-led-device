#ifndef __BASIC_DEPEND_H__
#define __BASIC_DEPEND_H__

#include <stdint.h>
#include "generic/typedef.h"
#include "adapter/include/common/sys_timer.h"
#include "misc/util_loops.h"
#include "misc/util_listify.h"

/*******************************************************************/
/*
 *-------------------   Typedef
 */

typedef unsigned char       bit1, uint8_t, u8_t;
typedef char                s8_t;
typedef unsigned short      uint16_t, u16_t;
typedef unsigned int        tu8, tu16, tbool, tu32, uint32_t, u32_t;
typedef signed int          s32_t;
typedef unsigned long long  u64_t;
typedef signed long long    s64_t;

#define __ASSERT                ASSERT
#define __ASSERT_NO_MSG(test)   ASSERT(test)
#define BUILD_ASSERT(...)

/**
 * @brief Bit mask with bits 0 through <tt>n-1</tt> (inclusive) set,
 * or 0 if @p n is 0.
 */
#define BIT_MASK(n) (BIT(n) - 1UL)

/**
 * @brief Set or clear a bit depending on a boolean value
 *
 * The argument @p var is a variable whose value is written to as a
 * side effect.
 *
 * @param var Variable to be altered
 * @param bit Bit number
 * @param set if 0, clears @p bit in @p var; any other value sets @p bit
 */
#define WRITE_BIT(var, bit, set) \
	((var) = (set) ? ((var) | BIT(bit)) : ((var) & ~BIT(bit)))

/**
 * @brief Divide and round up.
 *
 * Example:
 * @code{.c}
 * DIV_ROUND_UP(1, 2); // 1
 * DIV_ROUND_UP(3, 2); // 2
 * @endcode
 *
 * @param n Numerator.
 * @param d Denominator.
 *
 * @return The result of @p n / @p d, rounded up.
 */
#define DIV_ROUND_UP(n, d) (((n) + (d) - 1) / (d))

/*******************************************************************/
/*
 *-------------------   k_work_delayable
 */
// typedef int             sys_timer;

/** @brief The signature for a work item handler function.
 *
 * The function will be invoked by the thread animating a work queue.
 *
 * @param work the work item that provided the handler.
 */
typedef void (*k_work_handler_t)(struct k_work *work);

struct k_work {
    sys_timer   		systimer;
    k_work_handler_t	*callback;
};

struct k_work_delayable {
    struct k_work work;
    u32  end_time;
};

#define Z_WORK_INITIALIZER(work_handler) { \
	.callback = (work_handler), \
}

#define Z_WORK_DELAYABLE_INITIALIZER(work_handler) { \
	.work = { \
		.callback = (work_handler), \
	}, \
}

#define K_WORK_DELAYABLE_DEFINE(work, work_handler) \
	struct k_work_delayable work \
	  = Z_WORK_DELAYABLE_INITIALIZER(work_handler)

/**
 * @brief Initialize a statically-defined work item.
 *
 * This macro can be used to initialize a statically-defined workqueue work
 * item, prior to its first use. For example,
 *
 * @code static K_WORK_DEFINE(<work>, <work_handler>); @endcode
 *
 * @param work Symbol name for work item object
 * @param work_handler Function to invoke each time work item is processed.
 */
#define K_WORK_DEFINE(work, work_handler) \
	struct k_work work = Z_WORK_INITIALIZER(work_handler)

/**
 * @brief Generates a sequence of code with configurable separator.
 *
 * Example:
 *
 *     #define FOO(i, _) MY_PWM ## i
 *     { LISTIFY(PWM_COUNT, FOO, (,)) }
 *
 * The above two lines expand to:
 *
 *    { MY_PWM0 , MY_PWM1 }
 *
 * @param LEN The length of the sequence. Must be an integer literal less
 *            than 4095.
 * @param F A macro function that accepts at least two arguments:
 *          <tt>F(i, ...)</tt>. @p F is called repeatedly in the expansion.
 *          Its first argument @p i is the index in the sequence, and
 *          the variable list of arguments passed to LISTIFY are passed
 *          through to @p F.
 *
 * @param sep Separator (e.g. comma or semicolon). Must be in parentheses;
 *            this is required to enable providing a comma as separator.
 *
 * @note Calling LISTIFY with undefined arguments has undefined
 * behavior.
 */
#define UTIL_PRIMITIVE_CAT(a, ...) a##__VA_ARGS__
#define UTIL_CAT(a, ...) UTIL_PRIMITIVE_CAT(a, __VA_ARGS__)
#define LISTIFY(LEN, F, sep, ...) UTIL_CAT(Z_UTIL_LISTIFY_, LEN)(F, sep, __VA_ARGS__)

/**
 * @brief Validate if two entities have a compatible type
 *
 * @param a the first entity to be compared
 * @param b the second entity to be compared
 * @return 1 if the two elements are compatible, 0 if they are not
 */
#define SAME_TYPE(a, b) __builtin_types_compatible_p(__typeof__(a), __typeof__(b))

// /**
//  * @brief Validate CONTAINER_OF parameters, only applies to C mode.
//  */
// #ifndef __cplusplus
// #define CONTAINER_OF_VALIDATE(ptr, type, field)               \
// 	BUILD_ASSERT(SAME_TYPE(*(ptr), ((type *)0)->field) || \
// 		     SAME_TYPE(*(ptr), void),                 \
// 		     "pointer type mismatch in CONTAINER_OF");
// #else
// #define CONTAINER_OF_VALIDATE(ptr, type, field)
// #endif

// /**
//  * @brief Get a pointer to a structure containing the element
//  *
//  * Example:
//  *
//  *	struct foo {
//  *		int bar;
//  *	};
//  *
//  *	struct foo my_foo;
//  *	int *ptr = &my_foo.bar;
//  *
//  *	struct foo *container = CONTAINER_OF(ptr, struct foo, bar);
//  *
//  * Above, @p container points at @p my_foo.
//  *
//  * @param ptr pointer to a structure element
//  * @param type name of the type that @p ptr is an element of
//  * @param field the name of the field within the struct @p ptr points to
//  * @return a pointer to the structure that contains @p ptr
//  */
// #define CONTAINER_OF(ptr, type, field)                               \
// 	({                                                           \
// 		CONTAINER_OF_VALIDATE(ptr, type, field)              \
// 		((type *)(((char *)(ptr)) - offsetof(type, field))); \
// 	})

/** @brief Cast @p x, a pointer, to an unsigned integer. */
#define POINTER_TO_UINT(x) ((uintptr_t) (x))

/**
 * @brief Check if a pointer @p ptr lies within @p array.
 *
 * In C but not C++, this causes a compile error if @p array is not an array
 * (e.g. if @p ptr and @p array are mixed up).
 *
 * @param array an array
 * @param ptr a pointer
 * @return 1 if @p ptr is part of @p array, 0 otherwise
 */
#define PART_OF_ARRAY(array, ptr)                                                                  \
	((ptr) && POINTER_TO_UINT(array) <= POINTER_TO_UINT(ptr) &&                                \
	 POINTER_TO_UINT(ptr) < POINTER_TO_UINT(&(array)[ARRAY_SIZE(array)]))

/**
 * @brief Whether @p ptr is an element of @p array
 *
 * This macro can be seen as a slightly stricter version of @ref PART_OF_ARRAY
 * in that it also ensures that @p ptr is aligned to an array-element boundary
 * of @p array.
 *
 * In C, passing a pointer as @p array causes a compile error.
 *
 * @param array the array in question
 * @param ptr the pointer to check
 *
 * @return 1 if @p ptr is part of @p array, 0 otherwise
 */
#define IS_ARRAY_ELEMENT(array, ptr)                                                               \
	((ptr) && POINTER_TO_UINT(array) <= POINTER_TO_UINT(ptr) &&                          \
	 POINTER_TO_UINT(ptr) < POINTER_TO_UINT(&(array)[ARRAY_SIZE(array)]) &&                    \
	 (POINTER_TO_UINT(ptr) - POINTER_TO_UINT(array)) % sizeof((array)[0]) == 0)

/**
 * @brief Index of @p ptr within @p array
 *
 * With `CONFIG_ASSERT=y`, this macro will trigger a runtime assertion
 * when @p ptr does not fall into the range of @p array or when @p ptr
 * is not aligned to an array-element boundary of @p array.
 *
 * In C, passing a pointer as @p array causes a compile error.
 *
 * @param array the array in question
 * @param ptr pointer to an element of @p array
 *
 * @return the array index of @p ptr within @p array, on success
 */
#define ARRAY_INDEX(array, ptr)                                                                    \
	({                                                                                         \
		__ASSERT_NO_MSG(IS_ARRAY_ELEMENT(array, ptr));                                     \
		(__typeof__((array)[0]) *)(ptr) - (array);                                         \
	})


#endif /* __BASIC_DEPEND_H__ */
