/*
 * (c) 2008-2009 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *               Frank Mehnert <fm3@os.inf.tu-dresden.de>,
 *               Lukas Grützmacher <lg2@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <dlfcn.h>
#include <l4/sys/kip.h>
#include <l4/sys/debugger.h>
#include <l4/re/env.h>
#include <l4/sys/linkage.h>

void swap(long * a, long * b);
void print_values(long * values, size_t length, size_t start, size_t stop, long current);
void _qsort(long * values, size_t start, size_t stop);
void qsort(long * values, size_t length);
void random_values(long * values, size_t length);
bool is_sorted(long * values, size_t length);
long fib2(long n);
long fib1(long n);

#define FIB_INPUT		(1l << 30)
#define FIB_REPS		(1l <<  3)
#define VALUES_LENGTH	(1l << 14)
#define QSORT_REPS		(1l <<  5)
long VALUES[VALUES_LENGTH];

void swap(long * a, long * b) {
	long temp = *a;
	*a = *b;
	*b = temp;
}

#if DEBUG
void print_values(long * values, size_t length, size_t start, size_t stop, long current) {
	for (size_t i = 0; i < length; i++) {
		if (start <= i && i < stop) {
			printf("%3ld ", values[i]);
		} else {
			printf("    ");
		}
	}
	printf("curr = %3ld\n", current);
}
#else
void print_values(long *, size_t, size_t, size_t, long) {}
#endif

void _qsort(long * values, size_t start, size_t stop) {
	if (stop - start <= 1)
		return;
	if (stop - start == 2) {
		if (values[start] <= values[start+1])
			return;
		swap(&values[start], &values[start + 1]);
		return;
	}

	long pivot = values[start];
	long current = values[start + 1];
	print_values(values, VALUES_LENGTH, start, stop, current);

	// where to put to-be-sorted elements
	size_t front = start;		// when they're <= pivot, they go to a backwards-moving front end
	size_t back  = stop  - 1;	// when they're >  pivot, they go to a  forwards-moving back end

	/** memory layout of values from start to stop at this point in the program
	 * start
	 * |     front reads from here (front + 2)
	 * |     |        back
	 * |     |        |
	 * v  a  l  u  e  s
	 * |  |           |
	 * |  |           stop
	 * |  original was moved into current first
	 * |  thus writable
	 * |
	 * original stored in pivot
	 * thus, writable as front
	 */
	while (back - front > 1) {
		print_values(values, VALUES_LENGTH, start, stop, current);
		if (current > pivot) {
			swap(&current, &values[back]);
			back--;
		} else {
			values[front] = current;
			current = values[front + 2];
			front++;
		}
	}

	/** memory layout of values from start to stop at this point in the program
	 *
	 *       back points here
	 *       thus writable
	 *          |
	 * v  a  l  u  e  s
	 * |     |
	 * |     |
	 * |  front points here
	 * |  thus writable
	 * |
	 * original stored in pivot
	 * must be placed at front or front + 1
	 */

	if (current > pivot) {
		values[front + 1] = current;
	} else {
		values[front] = current;
		// make sure front points to the value that's already sorted
		front++;
	}
	values[front] = pivot;

	print_values(values, VALUES_LENGTH, start, stop, current);
	_qsort(values, start, front);
	_qsort(values, front + 1, stop);
	print_values(values, VALUES_LENGTH, start, stop, current);
}

void qsort(long * values, size_t length) {
	_qsort(values, 0, length);
}

void random_values(long* values, size_t length) {
	static long current = 0x283745034;
	for (size_t i = 0; i < length; i++) {
		current = current >> 8;
		current = current * current;
		current = current >> 8;
		values[i] = current & 0xff;
	}
}
bool is_sorted(long* values, size_t length) {
	for (size_t i = 1; i < length; i++) {
		if (values[i + 1] < values[i])
			return false;
	}
	return true;
}

long fib2(long n) {
	long a = 0;
	long b = 1;

	for (long i = 0; i < n; i++) {
		long d = a + b;
		a = b;
		b = d;
	}
	return a;
}

long fib1(long n) {
	return fib2(n);
}

void dl_stuff() {
	char * error;

	dlerror(); // clear error
	printf("started\n");
	void * main_program_handle = dlopen(NULL, RTLD_LAZY);
	if (error = dlerror()) { printf("dl error (main): %s\n", error); } else {
		printf("main program handle: %p\n", main_program_handle);
		void * dl_printf = dlsym(main_program_handle, "printf");
		if (error = dlerror()) { printf("dl error (printf): %s\n", error); } else {
			printf("printf is at %p, dl finds it at %p\n", &printf, dl_printf);
		}
	}

	dlerror();
	const char * lib_name = "rom/libdl.so";
	const char * function_name = "dlerror";
	void * handle = dlopen(lib_name, RTLD_LAZY);
	if (error = dlerror()) { printf("dl error (%s): %s\n", lib_name, error); } else {
		printf("handle: %p\n", handle);
		void * dl_function = dlsym(handle, function_name);
		if (error = dlerror()) { printf("dl error (%s): %s\n", function_name, error); } else {
			printf("%s is at ?, dl finds it at %p\n", function_name, dl_function);
		}
	}

	/*
	printf("using pseudo-handle RTLD_NEXT\n");
	void * dl_function = dlsym(RTLD_NEXT, function_name);
	if (error = dlerror()) { printf("dl error (%s): %s\n", function_name, error); } else {
		printf("%s is at ?, dl finds it at %p\n", function_name, dl_function);
	}
	*/
}

unsigned long print_utcb(
	const char * prefix,
	l4_utcb_t * utcb,
	l4_msgtag_t tag
) {
	unsigned long words = l4_msgtag_words(tag);
	printf(
		"%s l4_msgtag (%ld, %d, %d, %d) \n",
		prefix,
		l4_msgtag_label(tag),
		l4_msgtag_words(tag),
		l4_msgtag_items(tag),
		l4_msgtag_flags(tag)
	);

  printf("%s [", prefix);
	for (unsigned i = 0; i < words; i++) {
		printf("%d: %ld%s", i, l4_utcb_mr_u(utcb)->mr[i], (i < words - 1) ? ", " : "");
	}
  printf("]\n");

	return words;
}

l4_msgtag_t
l4_debugger_get_backtrace_buffer_section(
	l4_cap_idx_t cap,
	unsigned long * buffer,
	unsigned long buffer_capacity_in_bytes,
	unsigned long * returned_words,
	unsigned long * remaining_words
) L4_NOTHROW {
	l4_utcb_t * utcb = l4_utcb();
	l4_utcb_mr_u(utcb)->mr[0] = L4_DEBUGGER_GET_BTB_SECTION;
	l4_utcb_mr_u(utcb)->mr[1] = buffer;
	l4_utcb_mr_u(utcb)->mr[2] = buffer_capacity_in_bytes;
	l4_msgtag_t tag = l4_msgtag(0, 3, 0, 0);

	print_utcb("=>>", utcb, tag);

	l4_msgtag_t syscall_result = l4_invoke_debugger(cap, tag, utcb);

	print_utcb("<<=", utcb, syscall_result);

	if (l4_msgtag_has_error(syscall_result)) {
		*returned_words = 0;
		*remaining_words = 0;
		return syscall_result;
	}

	*returned_words  = l4_utcb_mr_u(utcb)->mr[0];
	*remaining_words = l4_utcb_mr_u(utcb)->mr[1];

	return syscall_result;
}

void print_backtrace_buffer_section (unsigned long * buffer, unsigned long words) {
	const unsigned word_width = 4;
	for (unsigned w = 0; w < words; w++) {
		if (w % word_width == 0)
			printf("--> %2d: ", w);

		printf("%16lx ", buffer[w]);
		if (w % word_width == word_width - 1)
			printf("\n");
	}

	printf("\n");
}

bool export_backtrace_buffer_section (l4_cap_idx_t cap) {
	const unsigned kumem_page_order = 3;
	const unsigned kumem_capacity_in_bytes = (1 << (kumem_page_order + 10));
	static l4_addr_t kumem = 0;
	if (!kumem) {
		// TODO: this allocation is never freed. this makes sense,
		// because it basically has life-of-the-process lifetime.
		if (l4re_util_kumem_alloc(&kumem, kumem_page_order, L4_BASE_TASK_CAP, l4re_env()->rm)) {
			printf("!!! could not allocate %d kiB kumem!!!\n", 1 << kumem_page_order);
		}
		printf("successfully allocated %d kiB kumem at %p\n", 1 << kumem_page_order, (void *) kumem);
	}

	unsigned long * buffer = (unsigned long *) kumem;
	unsigned long returned_words;
	unsigned long remaining_words;
	l4_msgtag_t syscall_result = l4_debugger_get_backtrace_buffer_section(
		cap,
		buffer,
		kumem_capacity_in_bytes,
		&returned_words,
		&remaining_words
	);

	print_backtrace_buffer_section(buffer, returned_words);

	return (remaining_words > 0);
}

int main(void) {
	l4_cap_idx_t dbg_cap = L4_BASE_DEBUGGER_CAP;
	bool is_valid = l4_is_valid_cap(dbg_cap) > 0;
	printf(">>> dbg_cap %ld is %svalid <<<\n", dbg_cap, is_valid ? "" : "not ");

	//long result = l4re_debug_obj_debug(dbg_cap, 17);
	//printf(">>> result from call 17: %ld <<<\n", result);


	for (int i = 0;; i++) {

    while (export_backtrace_buffer_section(dbg_cap)) {}
		#if 0
		// some testing around with user space mapping,
		// Insert_ok does not yet yield a readable user side experience :/
		if (i > 1) {
			// l4_kernel_info_t * kip = l4_kip();
			const long unsigned * const Tbuf = 0xffffffff00200000;
			const long unsigned * const BTB  = 0xffffffff00100000;
			const long unsigned * const UBTB = 0x0000000000006000;
			const long unsigned * btb = BTB;
			const int read_length = 20;
			printf("reading %d words, starting from %p\n", read_length, btb);
			for (int b = 0; b < read_length; b++) {
				printf("btb @%p: %16lx\n", btb, *(btb++));
			}
		}
		#endif

		bool sorted;
		for (int i = 0; i < QSORT_REPS; i++) {
			random_values		(&(VALUES[0]), VALUES_LENGTH);
			qsort				(&(VALUES[0]), VALUES_LENGTH);
			sorted = is_sorted	(&(VALUES[0]), VALUES_LENGTH);
		}
 		if (sorted) {
			printf(
				">>>>>>>>>>>>>>>>>>>>>>>>>>>>><<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n"
				">>>>>>>>>>>> successfully sorted a few values <<<<<<<<<<<<<<\n"
				">>>>>>>>>>>>>>>>>>>>>>>>>>>>><<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n"
			);
		} else {
			printf(
				">>>>>>>>>>>>>>>>>>>>>>>>>>>>><<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n"
				">>>>>>>>> shamefully failed sorting a few values <<<<<<<<<<<\n"
				">>>>>>>>>>>>>>>>>>>>>>>>>>>>><<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n"
			);
		}

		long f;
		for (int i = 0; i < FIB_REPS; i++) {
			f = fib1(FIB_INPUT);
		}
		printf(
			">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>><<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n"
			">>>>>>>>>>>> fib(%11ld) = %20ld! <<<<<<<<<<<<<<\n"
			">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>><<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n",
			FIB_INPUT, f
		);
	}
}
