/*
 * Copyright (C) 2009 Tomek Grabiec <tgrabiec@gmail.com>
 * Copyright (C) 2009 Eduard - Gabriel Munteanu <eduard.munteanu@linux360.ro>
 *
 * This file is released under the GPL version 2 with the following
 * clarification and special exception:
 *
 *     Linking this library statically or dynamically with other modules is
 *     making a combined work based on this library. Thus, the terms and
 *     conditions of the GNU General Public License cover the whole
 *     combination.
 *
 *     As a special exception, the copyright holders of this library give you
 *     permission to link this library with independent modules to produce an
 *     executable, regardless of the license terms of these independent
 *     modules, and to copy and distribute the resulting executable under terms
 *     of your choice, provided that you also meet, for each linked independent
 *     module, the terms and conditions of the license of that module. An
 *     independent module is a module which is not derived from or based on
 *     this library. If you modify this library, you may extend this exception
 *     to your version of the library, but you are not obligated to do so. If
 *     you do not wish to do so, delete this exception statement from your
 *     version.
 *
 * Please refer to the file LICENSE for details.
 */

#include <stdlib.h>

#include "arch/registers.h"

#include "jit/args.h"

#include "vm/call.h"
#include "vm/method.h"

#ifdef CONFIG_X86_32

/**
 * Calls @method which address is obtained from a memory
 * pointed by @target. Function returns call result which
 * is supposed to be saved to %eax.
 */
static unsigned long native_call_gp(struct vm_method *method,
				    const void *target,
				    unsigned long *args)
{
	unsigned long result;

	__asm__ volatile (
		"movl %%ebx, %%ecx \n"
		"shl $2, %%ebx \n"
		"subl %%ebx, %%esp \n"
		"movl %%esp, %%edi \n"
		"cld \n"
		"rep movsd \n"
		"movl %%ebx, %%esi \n"
		"call *%3 \n"
		"addl %%esi, %%esp \n"
		: "=a" (result)
		: "b" (method->args_count), "S"(args), "m"(target)
		: "%ecx", "%edi", "cc"
	);

	return result;
}

static unsigned long vm_native_call_gp(struct vm_method *method,
				       const void *target,
				       unsigned long *args)
{
	unsigned long result;

	__asm__ volatile (
		"movl %%ebx, %%ecx \n"
		"shl $2, %%ebx \n"
		"subl %%ebx, %%esp \n"
		"movl %%esp, %%edi \n"
		"cld \n"
		"rep movsd \n"
		"movl %%ebx, %%esi \n"

		"pushl %%esp \n"
		"pushl %3 \n"
		"call vm_enter_vm_native \n"
		"addl $8, %%esp \n"
		"test %%eax, %%eax \n"
		"jnz 1f \n"

		"call * -8(%%esp)\n"
		"movl %%eax, %0 \n"

		"call vm_leave_vm_native \n"

		"1: addl %%esi, %%esp \n"
		: "=r" (result)
		: "b" (method->args_count), "S"(args), "r"(target)
		: "%ecx", "%edi", "cc"
	);

	return result;
}

/**
 * This calls a function with call arguments copied from @args
 * array. The array contains @args_count elements of machine word
 * size. The @target must be a variable holding a function
 * pointer. Call result will be stored in @result.
 */
void native_call(struct vm_method *method,
		 const void *target,
		 unsigned long *args,
		 union jvalue *result)
{
	switch (method->return_type.vm_type) {
	case J_VOID:
		native_call_gp(method, target, args);
		break;
	case J_REFERENCE:
		result->l = (jobject) native_call_gp(method, target, args);
		break;
	case J_INT:
		result->i = (jint) native_call_gp(method, target, args);
		break;
	case J_CHAR:
		result->c = (jchar) native_call_gp(method, target, args);
		break;
	case J_BYTE:
		result->b = (jbyte) native_call_gp(method, target, args);
		break;
	case J_SHORT:
		result->s = (jshort) native_call_gp(method, target, args);
		break;
	case J_BOOLEAN:
		result->z = (jboolean) native_call_gp(method, target, args);
		break;
	case J_LONG:
	case J_DOUBLE:
	case J_FLOAT:
		NOT_IMPLEMENTED;
		break;
	case J_RETURN_ADDRESS:
	case VM_TYPE_MAX:
		die("unexpected type");
	}
}

/**
 * This calls a VM native function with call arguments copied from
 * @args array. The array contains @args_count elements of machine
 * word size. The @target must be a pointer to a VM function. Call
 * result will be stored in @result.
 */
void vm_native_call(struct vm_method *method,
		    const void *target,
		    unsigned long *args,
		    union jvalue *result)
{
	switch (method->return_type.vm_type) {
	case J_VOID:
		vm_native_call_gp(method, target, args);
		break;
	case J_REFERENCE:
		result->l = (jobject) vm_native_call_gp(method, target, args);
		break;
	case J_INT:
		result->i = (jint) vm_native_call_gp(method, target, args);
		break;
	case J_CHAR:
		result->c = (jchar) vm_native_call_gp(method, target, args);
		break;
	case J_BYTE:
		result->b = (jbyte) vm_native_call_gp(method, target, args);
		break;
	case J_SHORT:
		result->s = (jshort) vm_native_call_gp(method, target, args);
		break;
	case J_BOOLEAN:
		result->z = (jboolean) vm_native_call_gp(method, target, args);
		break;
	case J_LONG:
	case J_DOUBLE:
	case J_FLOAT:
		NOT_IMPLEMENTED;
		break;
	case J_RETURN_ADDRESS:
	case VM_TYPE_MAX:
		die("unexpected type");
	}
}

#else /* CONFIG_X86_32 */

/**
 * Calls @method which address is obtained from a memory
 * pointed by @target. Function returns call result which
 * is supposed to be saved to %rax.
 */
static unsigned long native_call_gp(struct vm_method *method,
				    const void *target,
				    unsigned long *args)
{
	int i, sp = 0, r = 0;
	unsigned long *stack, regs[6];
	unsigned long result;

	stack = malloc(sizeof(unsigned long) * method->args_count);
	if (!stack)
		abort();

	for (i = 0; i < method->args_count; i++)
		if (method->args_map[i].reg == MACH_REG_UNASSIGNED)
			stack[sp++] = args[i];
		else
			regs[r++] = args[i];

	while (r < 6)
		regs[r++] = 0;

	__asm__ volatile (
		/* Copy stack arguments onto the stack. */
		"movq %%rbx, %%rcx \n"
		"shl $3, %%rbx \n"
		"subq %%rbx, %%rsp \n"
		"movq %%rsp, %%rdi \n"
		"cld \n"
		"rep movsq \n"

		/* Assign registers to register arguments. */
		"movq 0x00(%%rax), %%rdi \n"
		"movq 0x08(%%rax), %%rsi \n"
		"movq 0x10(%%rax), %%rdx \n"
		"movq 0x18(%%rax), %%rcx \n"
		"movq 0x20(%%rax), %%r8 \n"
		"movq 0x28(%%rax), %%r9 \n"

		"call *%3 \n"
		"addq %%rbx, %%rsp \n"
		: "=a" (result)
		: "b" (get_stack_args_count(method)), "S" (stack),
		  "m" (target), "a" (regs)
		: "%rcx", "%rdi", "%r8", "%r9", "cc"
	);

	free(stack);

	return 0;
}

static unsigned long vm_native_call_gp(struct vm_method *method,
				       const void *target,
				       unsigned long *args)
{
	abort();

	return 0;
}

/**
 * This calls a function with call arguments copied from @args
 * array. The array contains @args_count elements of machine word
 * size. The @target must be a variable holding a function
 * pointer. Call result will be stored in @result.
 */
void native_call(struct vm_method *method,
		 const void *target,
		 unsigned long *args,
		 union jvalue *result)
{
	switch (method->return_type.vm_type) {
	case J_VOID:
		native_call_gp(method, target, args);
		break;
	case J_REFERENCE:
		result->l = (jobject) native_call_gp(method, target, args);
		break;
	case J_INT:
		result->i = (jint) native_call_gp(method, target, args);
		break;
	case J_CHAR:
		result->c = (jchar) native_call_gp(method, target, args);
		break;
	case J_BYTE:
		result->b = (jbyte) native_call_gp(method, target, args);
		break;
	case J_SHORT:
		result->s = (jshort) native_call_gp(method, target, args);
		break;
	case J_BOOLEAN:
		result->z = (jboolean) native_call_gp(method, target, args);
		break;
	case J_LONG:
		result->j = (jlong) native_call_gp(method, target, args);
		break;
	case J_DOUBLE:
	case J_FLOAT:
		NOT_IMPLEMENTED;
		break;
	case J_RETURN_ADDRESS:
	case VM_TYPE_MAX:
		die("unexpected type");
	}
}

/**
 * This calls a VM native function with call arguments copied from
 * @args array. The array contains @args_count elements of machine
 * word size. The @target must be a pointer to a VM function. Call
 * result will be stored in @result.
 */
void vm_native_call(struct vm_method *method,
		    const void *target,
		    unsigned long *args,
		    union jvalue *result)
{
	switch (method->return_type.vm_type) {
	case J_VOID:
		vm_native_call_gp(method, target, args);
		break;
	case J_REFERENCE:
		result->l = (jobject) vm_native_call_gp(method, target, args);
		break;
	case J_INT:
		result->i = (jint) vm_native_call_gp(method, target, args);
		break;
	case J_CHAR:
		result->c = (jchar) vm_native_call_gp(method, target, args);
		break;
	case J_BYTE:
		result->b = (jbyte) vm_native_call_gp(method, target, args);
		break;
	case J_SHORT:
		result->s = (jshort) vm_native_call_gp(method, target, args);
		break;
	case J_BOOLEAN:
		result->z = (jboolean) vm_native_call_gp(method, target, args);
		break;
	case J_LONG:
		result->j = (jlong) vm_native_call_gp(method, target, args);
		break;
	case J_DOUBLE:
	case J_FLOAT:
		NOT_IMPLEMENTED;
		break;
	case J_RETURN_ADDRESS:
	case VM_TYPE_MAX:
		die("unexpected type");
	}
}

#warning NOT IMPLEMENTED

#endif /* CONFIG_X86_32 */

