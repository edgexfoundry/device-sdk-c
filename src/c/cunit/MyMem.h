/*
 *  CUnit - A Unit testing framework library for C.
 *  Copyright (C) 2001       Anil Kumar
 *  Copyright (C) 2004-2006  Anil Kumar, Jerry St.Clair
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
 *  Contains Memory Related Defines to use internal routines to detect Memory Leak
 *  in Debug Versions
 *
 *  18/Jun/2002   Memory Debug Functions. (AK)
 *
 *  17-Jul-2004   New interface for global function names. (JDS)
 *
 *  05-Sep-2004   Added internal test interface. (JDS)
 */

#ifndef CUNIT_MYMEM_H_SEEN
#define CUNIT_MYMEM_H_SEEN

#ifdef __cplusplus
extern "C" {
#endif

#define CU_CALLOC(x, y)         calloc((x), (y))
#define CU_MALLOC(x)            malloc((x))
#define CU_FREE(x)              free((x))
#define CU_REALLOC(x, y)        realloc((x), (y))
#define CU_CREATE_MEMORY_REPORT(x)
#define CU_DUMP_MEMORY_USAGE(x)

#ifdef CUNIT_BUILD_TESTS
/** Disable memory allocation for testing purposes. */
void test_cunit_deactivate_malloc(void);
/** Enable memory allocation for testing purposes. */
void test_cunit_activate_malloc(void);
/** Retrieve number of memory events for a given pointer */
unsigned int test_cunit_get_n_memevents(void* pLocation);
/** Retrieve number of allocations for a given pointer */
unsigned int test_cunit_get_n_allocations(void* pLocation);
/** Retrieve number of deallocations for a given pointer */
unsigned int test_cunit_get_n_deallocations(void* pLocation);

void test_cunit_MyMem(void);
#endif  /* CUNIT_BUILD_TESTS */

#ifdef __cplusplus
}
#endif
#endif  /*  CUNIT_MYMEM_H_SEEN  */
/** @} */
