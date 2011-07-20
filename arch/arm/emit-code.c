#include "jit/emit-code.h"
#include "jit/basic-block.h"
#include "jit/lir-printer.h"
#include "jit/exception.h"

#include "vm/backtrace.h"
#include "vm/call.h"
#include "vm/itable.h"
#include "vm/stack-trace.h"
#include "vm/static.h"

#include "lib/buffer.h"

#include "arch/itable.h"
#include "arch/instruction.h"
#include "arch/constant-pool.h"
#include "arch/encode.h"

void __attribute__((regparm(1)))
itable_resolver_stub_error(struct vm_method *method, struct vm_object *obj)
{
	assert(!"not implemented");
}

int lir_print(struct insn *insn, struct string *s)
{
	assert(!"not implemented");
}

void native_call(struct vm_method *method, void *target, unsigned long *args, union jvalue *result)
{
	assert(!"not implemented");
}

void *emit_itable_resolver_stub(struct vm_class *vmc, struct itable_entry **sorted_table, unsigned int nr_entries)
{
	assert(!"not implemented");
}

bool called_from_jit_trampoline(struct native_stack_frame *frame)
{
	assert(!"not implemented");
}

bool show_exe_function(void *addr, struct string *str)
{
	assert(!"not implemented");
}

int fixup_static_at(unsigned long addr)
{
	assert(!"not implemented");
}

void emit_trampoline(struct compilation_unit *cu,
		     void *call_target,
		     struct jit_trampoline *trampoline)
{
	struct buffer *buf = trampoline->objcode;

	jit_text_lock();
	buf->buf = jit_text_ptr();

	jit_text_reserve(buffer_offset(buf));
	jit_text_unlock();
}

void emit_jni_trampoline(struct buffer *b, struct vm_method *vm, void *v)
{
	assert(!"not implemented");
}

void fixup_direct_calls(struct jit_trampoline *trampoline, unsigned long target)
{
	assert(!"not implemented");
}

void emit_unlock(struct buffer *buffer, struct vm_object *vo)
{
	assert(!"not implemented");
}

void emit_unlock_this(struct buffer *buffer)
{
	assert(!"not implemented");
}
static inline void write_imm24(struct buffer *buf, struct insn *insn,
				unsigned long imm, unsigned long offset)
{
	unsigned char *buffer;
	union {
		int val;
		unsigned char b[4];
	} imm_buf;

	buffer = buf->buf;
	imm_buf.val = imm;

	buffer[offset] = imm_buf.b[0];
	buffer[offset + 1] = imm_buf.b[1];
	buffer[offset + 2] = imm_buf.b[2];
}

/*
 * This function calculates the pc relative address of target
 * and emits the backpatched branch insns.
 */
void backpatch_branch_target(struct buffer *buf, struct insn *insn,
				    unsigned long target_offset)
{
	unsigned long backpatch_offset;
	long relative_addr;

	relative_addr = branch_rel_addr(insn, target_offset);
	relative_addr = relative_addr >> 2;
	backpatch_offset = insn->mach_offset;

	write_imm24(buf, insn, relative_addr, backpatch_offset);
}

void emit_nop(struct buffer *buf)
{
	assert(!"not implemented");
}

void emit_prolog(struct buffer *buf, unsigned long l)
{
	assert(!"not implemented");
}

void emit_lock(struct buffer *buf, struct vm_object *vo)
{
	assert(!"not implemented");
}

void emit_lock_this(struct buffer *buf)
{
	assert(!"not implemented");
}

void emit_epilog(struct buffer *buf)
{
	assert(!"not implemented");
}

void emit_unwind(struct buffer *buf)
{
	assert(!"not implemented");
}

void emit_trace_invoke(struct buffer *buf, struct compilation_unit *cu)
{
	assert(!"not implemented");
}

long branch_rel_addr(struct insn *insn, unsigned long target_offset)
{
	long addr;
	addr = target_offset - insn->mach_offset - PC_RELATIVE_OFFSET;

	return addr;
}

/*
 * This function emits the branch insns.
 * we first check if there is any resolution block is attached
 * to the CFG Edge. If there is a resolution block we backpatch
 * that insn. Else we check if the target basic_block is emitted
 * or not. If its emitted then we just have to find the PC
 * relative address of the target basic block but if its not
 * emitted then we have to backpatch that branch insn.
 */
long emit_branch(struct insn *insn, struct basic_block *bb)
{
	struct basic_block *target_bb;
	long addr = 0;
	int idx;

	target_bb = insn->operand.branch_target;

	if (!bb)
		idx = -1;
	else
		idx = bb_lookup_successor_index(bb, target_bb);

	if (idx >= 0 && branch_needs_resolution_block(bb, idx)) {
		list_add(&insn->branch_list_node,
			 &bb->resolution_blocks[idx].backpatch_insns);
	} else if (target_bb->is_emitted) {
		addr = branch_rel_addr(insn, target_bb->mach_offset);
	} else
		list_add(&insn->branch_list_node, &target_bb->backpatch_insns);

	return addr;

}

void emit_insn(struct buffer *buf, struct basic_block *bb, struct insn *insn)
{
	insn->mach_offset = buffer_offset(buf);

	insn_encode(insn, buf, bb);
}
