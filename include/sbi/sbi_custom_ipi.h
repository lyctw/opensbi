#ifndef __SBI_CUSTOM_IPI_H__
#define __SBI_CUSTOM_IPI_H__

#include <sbi/sbi_types.h>
#include <sbi/sbi_hartmask.h>

/* clang-format on */

struct sbi_custom_info {
	unsigned long start;
	unsigned long size;
	unsigned long dummy1;
	unsigned long dummy2;
	void (*local_fn)(struct sbi_custom_info *cinfo);
	struct sbi_hartmask smask;
};

void sbi_custom_ipi_fn1(struct sbi_custom_info *cinfo);

#define SBI_CUSTOM_INFO_INIT(__p, __start, __size, __dummy1, __dummy2, __lfn, __src) \
do { \
	(__p)->start = (__start); \
	(__p)->size = (__size); \
	(__p)->dummy1 = (__dummy1); \
	(__p)->dummy2 = (__dummy2); \
	(__p)->local_fn = (__lfn); \
	SBI_HARTMASK_INIT_EXCEPT(&(__p)->smask, (__src)); \
} while (0)

#endif /* __SBI_CUSTOM_IPI_H__ */
