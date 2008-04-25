#include <gpdef.h>
#include <gpstdlib.h>
#include <initval_port.h>

#ifdef USE_GP_MEM
#include <gpmem.h>
#endif

unsigned int HEAPSTART;
unsigned int HEAPEND;

extern void GpKeyPollingTimeSet(int loop_cnt);

/* No need for a prototype for main in a header file as it's called by crt0.s,
 * but will put one here to avoid compiler warnings. */
void main(int argc, char **argv);

void main(int argc, char **argv) {
	GM_HEAP_DEF gm_heap_def;
	(void)argc;  /* unused */
	(void)argv;  /* unused */
	
	_gp_sdk_init();
	
	//keyboard polling count setting
	GpKeyPollingTimeSet(KEYPOLLING_NUM);
	
#ifdef USE_GP_MEM
	gm_heap_def.heapstart = (void *)(HEAPSTART);
	gm_heap_def.heapend = (void *)(HEAPEND & ~3); //<== BOOTROM SPECTIFIC
	gm_heap_init(&gm_heap_def);
	
	gp_mem_func.malloc = gm_malloc;
	gp_mem_func.zimalloc = gm_zi_malloc;
	gp_mem_func.calloc = gm_calloc;
	gp_mem_func.free = gm_free;
	gp_mem_func.availablemem = gm_availablesize;
	gp_mem_func.malloc_ex = gm_malloc_ex;
	gp_mem_func.free_ex = gm_free_ex;
	gp_mem_func.make_mem_partition = gm_make_mem_part;
	
	gp_str_func.memset = gm_memset;
	gp_str_func.memcpy = gm_memcpy;
	gp_str_func.strcpy = gm_strcpy;
	gp_str_func.strncpy = gm_strncpy;
	gp_str_func.strcat = gm_strcat;
	gp_str_func.strncat = gm_strncat;
#if 1
	gp_str_func.gpstrlen = gm_lstrlen;
#else
	gp_str_func.lstrlen = gm_lstrlen;
#endif
	gp_str_func.sprintf = gm_sprintf;
	gp_str_func.uppercase = gm_uppercase;
	gp_str_func.lowercase = gm_lowercase;
	gp_str_func.compare = gm_compare;
	gp_str_func.trim_right = gm_trim_right;
#endif /*USE_GP_MEM*/
	
	//Font initialize
	//InitializeFont();
	
	GpKernelInitialize();
	GpKernelStart();
	GpAppExit();
	while(1);
}

int GpPredefinedStackGet(H_THREAD th) {
	switch (th) {
		case H_THREAD_GPMAIN:
			return GPMAIN_STACK_SIZE;
		case H_THREAD_NET:
			return NET_STACK_SIZE;
		case H_THREAD_TMR0:
		case H_THREAD_TMR1:
		case H_THREAD_TMR2:
		case H_THREAD_TMR3:
			return USER_STACK_SIZE;
		default:
			return 0;
	}
}
