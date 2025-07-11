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
#include <l4/re/env.h>
#include <l4/re/env>
#include <l4/util/util.h>
#include <l4/util/rdtsc.h>
#include <l4/sys/thread>
#include <l4/sys/scheduler>
#include <pthread-l4.h>
#include <thread>
#include <vector>
#include <stdexcept>

#include <l4/backtracer/btb_control.h>

void swap(long * a, long * b);
void print_values(long * values, size_t length, size_t start, size_t stop, long current);
void so_qsort      (long * values, size_t start, size_t stop);
void left_qsort    (long * values, size_t start, size_t stop);
void right_qsort   (long * values, size_t start, size_t stop);
void my_qsort      (long * values, size_t start, size_t stop);
void parallel_qsort(long * values, size_t start, size_t stop, size_t depth, size_t cpu);
void do_sort(void);
void qsort(long * values, size_t length);
void random_values(long * values, size_t length);
bool is_sorted(long * values, size_t length);
long fib2(long n);
long fib1(long n);
void dl_stuff(void);
static void thread_migrate(l4_umword_t cpu);

#define FIB_INPUT		(1l << 32)
#define VALUES_LENGTH	(1l << 12)
long VALUES[VALUES_LENGTH];

void swap(long * a, long * b) {
	long temp = *a;
	*a = *b;
	*b = temp;
}

#define DEBUG 1
#define dbg_verbose 0

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

void so_qsort(long * values, size_t start, size_t stop) {
	if (start + 1 >= stop)
		return;
	if (0 && stop - start > VALUES_LENGTH / 64)
		printf("single_sort_step [%lx .. %lx]\n", start, stop);
	// https://codereview.stackexchange.com/questions/283932/in-place-recursive-quick-sort-in-c

	long a = values[start];
	long b = values[stop];
	size_t middle = (start + stop) / 2;
	long c = values[middle];
	long pivot = a, current = b;
#if 1
	if ((b <= a && a <= c) || (c <= a && a <= b)) {
		// a is the middle of the three
		pivot = a; current = b;
	} else if ((a <= c && c <= b) || (b <= c && c <= a)) {
		pivot = b; current = a;
	} else if ((a <= c && c <= b) || (b <= c && c <= a)) {
		// c is the middle of the three, swap a into the middle of the array
		pivot = c; current = b; values[middle] = a;
	}
#endif

	// where to put to-be-sorted elements
	size_t front = start;
	size_t back  = stop;
	size_t pivot_counter = 1;

	while (front + 1 < back) {
		if (current > pivot) {
			values[back] = current;
			current = values[--back];
		} else if (current < pivot) {
			values[front] = current;
			current = values[++front];
		} else {
			pivot_counter ++;
		}
	}

	if (current >= pivot) {
		values[front] = pivot;
		values[back] = current;
	} else {
		values[front] = current;
		values[back] = pivot;
	}
	for (size_t i = 0; i < pivot_counter; i++) {
		values[++front] = pivot;
	}
	front--;
	so_qsort(values, start, front);
	so_qsort(values, back, stop);
}

void left_qsort(long * values, size_t start, size_t stop) {
	if (start + 1 >= stop)
		return;
	if (0 && stop - start > VALUES_LENGTH / 64)
		printf("single_sort_step [%lx .. %lx]\n", start, stop);
	// https://codereview.stackexchange.com/questions/283932/in-place-recursive-quick-sort-in-c

	long a = values[start];
	long b = values[stop];
	size_t middle = (start + stop) / 2;
	long c = values[middle];
	long pivot = a, current = b;
#if 1
	if ((b <= a && a <= c) || (c <= a && a <= b)) {
		// a is the middle of the three
		pivot = a; current = b;
	} else if ((a <= c && c <= b) || (b <= c && c <= a)) {
		pivot = b; current = a;
	} else if ((a <= c && c <= b) || (b <= c && c <= a)) {
		// c is the middle of the three, swap a into the middle of the array
		pivot = c; current = b; values[middle] = a;
	}
#endif

	// where to put to-be-sorted elements
	size_t front = start;
	size_t back  = stop;
	size_t pivot_counter = 1;

	while (front + 1 < back) {
		if (current > pivot) {
			values[back] = current;
			current = values[--back];
		} else if (current < pivot) {
			values[front] = current;
			current = values[++front];
		} else {
			pivot_counter ++;
		}
	}

	if (current >= pivot) {
		values[front] = pivot;
		values[back] = current;
	} else {
		values[front] = current;
		values[back] = pivot;
	}
	for (size_t i = 0; i < pivot_counter; i++) {
		values[++front] = pivot;
	}
	front--;
	if (dbg_verbose) printf("[%5ld..%5ld]: end of left_qsort\n", start, stop);
	left_qsort(values, start, front);
	right_qsort(values, back, stop);
}

void right_qsort(long * values, size_t start, size_t stop) {
	if (start + 1 >= stop)
		return;
	if (0 && stop - start > VALUES_LENGTH / 64)
		printf("single_sort_step [%lx .. %lx]\n", start, stop);
	// https://codereview.stackexchange.com/questions/283932/in-place-recursive-quick-sort-in-c

	long a = values[start];
	long b = values[stop];
	size_t middle = (start + stop) / 2;
	long c = values[middle];
	long pivot = a, current = b;
#if 1
	if ((b <= a && a <= c) || (c <= a && a <= b)) {
		// a is the middle of the three
		pivot = a; current = b;
	} else if ((a <= c && c <= b) || (b <= c && c <= a)) {
		// c is the middle of the three, swap a into the middle of the array
		pivot = c; current = b; values[middle] = a;
	} else if ((a <= c && c <= b) || (b <= c && c <= a)) {
		pivot = b; current = a;
	}
#endif

	// where to put to-be-sorted elements
	size_t front = start;
	size_t back  = stop;
	size_t pivot_counter = 1;

	while (front + 1 < back) {
		if (current > pivot) {
			values[back] = current;
			current = values[--back];
		} else if (current < pivot) {
			values[front] = current;
			current = values[++front];
		} else {
			pivot_counter ++;
		}
	}

	if (current > pivot) {
		values[front] = pivot;
		values[back] = current;
	} else {
		values[front] = current;
		values[back] = pivot;
	}
	for (size_t i = 0; i < pivot_counter; i++) {
		values[++front] = pivot;
	}
	front--;
	if (dbg_verbose) printf("[%5ld..%5ld]: end of right_qsort\n", start, stop);
	left_qsort(values, start, front);
	right_qsort(values, back, stop);
}

void parallel_qsort(long * values, size_t start, size_t stop, size_t depth, size_t cpu) {
	if (start + 1 >= stop)
		return;

	if (dbg_verbose) printf("[%5ld..%5ld]: thread_migrate(%ld)\n", start, stop, cpu);
	thread_migrate(cpu);
	if (0 && stop - start > VALUES_LENGTH / 64)
		printf("single_sort_step [%lx .. %lx]\n", start, stop);
	// https://codereview.stackexchange.com/questions/283932/in-place-recursive-quick-sort-in-c

	long a = values[start];
	long b = values[stop];
	size_t middle = (start + stop) / 2;
	long c = values[middle];
	long pivot = a, current = b;
#if 1
	if ((b <= a && a <= c) || (c <= a && a <= b)) {
		// a is the middle of the three
		pivot = a; current = b;
	} else if ((a <= c && c <= b) || (b <= c && c <= a)) {
		// c is the middle of the three, swap a into the middle of the array
		pivot = c; current = b; values[middle] = a;
	} else if ((a <= c && c <= b) || (b <= c && c <= a)) {
		pivot = b; current = a;
	}
#endif

	// where to put to-be-sorted elements
	size_t front = start;
	size_t back  = stop;
	size_t pivot_counter = 1;

	while (front + 1 < back) {
		if (current > pivot) {
			values[back] = current;
			current = values[--back];
		} else if (current < pivot) {
			values[front] = current;
			current = values[++front];
		} else {
			pivot_counter ++;
		}
	}

	if (current > pivot) {
		values[front] = pivot;
		values[back] = current;
	} else {
		values[front] = current;
		values[back] = pivot;
	}
	for (size_t i = 0; i < pivot_counter; i++) {
		values[++front] = pivot;
	}
	front--;

	if (dbg_verbose) printf("[%5ld..%5ld]: end of parallel\n", start, stop);
	if (depth) {
		left_qsort(values, start, front);
		right_qsort(values, back, stop);
	} else {
		if (dbg_verbose) printf(
			"[%5ld..%5ld]: splitting into two threads: [%5ld..%5ld] and [%5ld..%5ld]\n",
			start, stop, start, front, back, stop
		);
		std::thread left  { parallel_qsort, values, start, front, depth - 1, cpu };
		// TODO: better spreading of cpus
		std::thread right { parallel_qsort, values,  back, stop,  depth - 1, (cpu + 1) % 4 };

		left.join();
		right.join();
	}
}

void my_qsort(long * values, size_t start, size_t stop) {
	if (stop - start <= 1)
		return;
	if (stop - start == 2) {
		if (values[start] <= values[start+1])
			return;
		swap(&values[start], &values[start + 1]);
		return;
	}

	/*
	size_t middle = start + (stop - start) / 2;
	swap(values + start, values + middle);
	*/

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
	my_qsort(values, start, front);
	my_qsort(values, front + 1, stop);
	print_values(values, VALUES_LENGTH, start, stop, current);
}

void qsort(long * values, size_t length) {
	parallel_qsort(values, 0, length - 1, 2, 0);
	// left_qsort(values, 0, length - 1);
	// so_qsort(values, 0, length - 1);
	// my_qsort(values, 0, length);
}

void __attribute__ ((noinline)) random_values(long* values, size_t length) {
	static long current = 0x2837450344758476;
	static const long added = 0x2450293842573;
	for (size_t i = 0; i < length; i++) {
		current = current >> 16;
		current = current * current;
		current = current >> 16;
		current = current + added;
		values[i] = current;
	}
}
bool __attribute__ ((noinline)) is_sorted(long* values, size_t length) {
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
	long f = fib2(n);
	printf("fib(%ld) == %ld\n", n, f);
	return f;
}

void dl_stuff(void) {
	char * error;

	dlerror(); // clear error
	printf("started\n");
	void * main_program_handle = dlopen(NULL, RTLD_LAZY);
	if ((error = dlerror())) { printf("dl error (main): %s\n", error); } else {
		printf("main program handle: %p\n", main_program_handle);
		void * dl_printf = dlsym(main_program_handle, "printf");
		if ((error = dlerror())) { printf("dl error (printf): %s\n", error); } else {
			printf("printf is at %p, dl finds it at %p\n", &printf, dl_printf);
		}
	}

	dlerror();
	const char * lib_name = "rom/libdl.so";
	const char * function_name = "dlerror";
	void * handle = dlopen(lib_name, RTLD_LAZY);
	if ((error = dlerror())) { printf("dl error (%s): %s\n", lib_name, error); } else {
		printf("handle: %p\n", handle);
		void * dl_function = dlsym(handle, function_name);
		if ((error = dlerror())) { printf("dl error (%s): %s\n", function_name, error); } else {
			printf("%s is at ?, dl finds it at %p\n", function_name, dl_function);
		}
	}

	/*
	printf("using pseudo-handle RTLD_NEXT\n");
	void * dl_function = dlsym(RTLD_NEXT, function_name);
	if ((error = dlerror())) { printf("dl error (%s): %s\n", function_name, error); } else {
		printf("%s is at ?, dl finds it at %p\n", function_name, dl_function);
	}
	*/
}

void do_sort(void) {
	random_values           (&(VALUES[0]), VALUES_LENGTH);
	qsort                   (&(VALUES[0]), VALUES_LENGTH);
	bool sorted = is_sorted	(&(VALUES[0]), VALUES_LENGTH);
	if (!sorted && false) printf(
		">>>>>>>>>>>>>>>>>>>>>>>>>>>>><<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n"
		">>>>>>>>> shamefully failed sorting a few values <<<<<<<<<<<\n"
		">>>>>>>>>>>>>>>>>>>>>>>>>>>>><<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n"
	);
}

// this function is structured to work with <l4/backtracer/measure.h>
static int workload (l4_uint64_t cpu_id, l4_uint64_t steps, l4_uint64_t * started) {
	thread_migrate(cpu_id);
	*started = true;
	for (l4_uint64_t step = 0; step < steps; step++) {
		do_sort();
		// fib1(FIB_INPUT);
	}
	return 0;
}

int main (int argc, const char ** argv) {
	l4_uint64_t cpu_count = 1;
	if (argc > 1) {
		if (argc > 2) {
			printf("too many args, just takes one integer!\n");
			return 1;
		}
		const char * cpu_count_arg = argv[1];
		char * end;
		l4_uint64_t value = strtol(cpu_count_arg, &end, 10);
		if (*end == '\0') {
			cpu_count = value;
		} else {
			printf("uninterpretable cpu id: '%s'\n", argv[1]);
			return 1;
		}
	}
	l4_uint64_t steps = 1000;
	l4_uint64_t trace_interval_us = 1000;
	l4_debugger_backtracing_set_timestep(dbg_cap, trace_interval_us);

	// start threads
	std::vector<std::thread> threads;
	std::vector<l4_uint64_t> started (cpu_count, 0); // used as if bool!
	for (l4_uint64_t cpu_id = 0; cpu_id < cpu_count; cpu_id++) {
		threads.emplace_back(workload, (l4_uint64_t) cpu_id, (l4_uint64_t) steps, &started[cpu_id]);
	}

	// wait until threads have migrated
	for (l4_uint64_t cpu_id = 0; cpu_id < cpu_count; cpu_id++) {
		while (!started[cpu_id]) {
			usleep(1000);
		}
	}

	// start backtracer
	l4_debugger_backtracing_start(dbg_cap);

	bool stopped = false;
	// join threads
	for (l4_uint64_t cpu_id = 0; cpu_id < cpu_count; cpu_id++) {
		threads[cpu_id].join();
		if (!stopped) {
			// stop writing trace entries (kernel-side)
			l4_debugger_backtracing_stop(dbg_cap);
			// write the histogram of how long differently deep stacks took
			l4_debugger_backtracing_write_stats(dbg_cap);
			// tell the exporter (the userspace program backtracer) to export
			l4_debugger_backtracing_set_ready_for_export(dbg_cap, true);

			stopped = true;
		}
	}
}

// Migrate and pin a thread to a specific CPU
static void thread_migrate(l4_umword_t cpu) {
	// Cannot use pthread_setaffinity_np here. It ignores CPUs with id >= 64.
	// 2 is the default pthread priority in L4.
	auto sp = l4_sched_param(2);
	sp.affinity = l4_sched_cpu_set(cpu, 0);
	if (l4_error(
		L4Re::Env::env()->scheduler()->run_thread(
			Pthread::L4::cap(pthread_self()), sp
		)
	)) {
		throw std::runtime_error{"failed to migrate thread"};
	}
}

