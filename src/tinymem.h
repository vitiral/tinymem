/**
 * \defgroup tinymem
 *
 * tinymem memory manager
 *
 * See the README.md at https://github.com/cloudformdesign/tinymem
 * for more information
 *
 * Released under the MIT License
 * @{
 */


/**
 * \file
 *      tinymem main file. Import this file to use tinymem
 *  \author
 *      Garrett Berg, Garrett@cloudformdesign.com
 *
 */
#ifndef __tinymem_h
#define __tinymem_h
#include "tm_types.h"

/*---------------------------------------------------------------------------*/
/**
 * \brief           initialize tinymem library
 *                  Call this function in your initial setup (before your main
 *                  loop)
 * \return          void
 */
void tm_init();

/*---------------------------------------------------------------------------*/
/**
 * \brief allocate memory from the tinymem pool(s).
 * \param size      size of memory allocation
 * \return          tm_index_t value. Use tm_void or other typecasts to convert
 *                  to useable pointer
 */
inline tm_index_t tm_alloc(tm_size_t size);

/*---------------------------------------------------------------------------*/
/**
 * \brief           changes the size of memory of index in the tinymem pool(s)
 *                  See standard documentation on realloc for more info
 * \param index     tm_index_t to realloc
 * \param size      new requested size of index
 * \return          index with new size of memory.
 *                  If this index has changed, the previous index is been freed
 *                  If return value == 0, then no change has been done
 */
inline tm_index_t        tm_realloc(tm_index_t index, tm_size_t size);

/*---------------------------------------------------------------------------*/
/**
 * \brief           free memory index from tinymem
 * \param index     tm_index_t value to free
 * \return          void
 */
inline void tm_free(tm_index_t index);

/*---------------------------------------------------------------------------*/
/**
 * \brief           get sizeof the data referenced by index
 * \param index     tm_index_t value
 * \return          size of referenced data in bytes
 */
inline tm_size_t tm_sizeof(tm_index_t index);

/*---------------------------------------------------------------------------*/
/**
 * \brief           return whether index points to valid data
 * \param index     tm_index_t value
 * \return          1 if it does, 0 if it does not
 */
inline bool tm_valid(tm_index_t index);

/*---------------------------------------------------------------------------*/
/**
 * \brief           return status of poolid
 * \param poolid    poolid is the first 4 bits of any tm_index_t, name is status
 *                  bit name
 * \return          status bits coresponding to name
 */
//inline uint8_t tm_status(tm_index_t poolid, uint8_t name);

/*---------------------------------------------------------------------------*/
/**
 * \brief           obtain void pointer from tm_index_t
 * \param index     valid tm_index_t
 * \return          (void *) with the original data
 */
inline void *tm_void(tm_index_t index);

/*---------------------------------------------------------------------------*/
/**
 * \brief           data type conversion macros
 * \param index     valid tm_index_t
 * \return          pointer of type specified
 */
#define tm_char_p(index)        ((char *)tm_void(index))
#define tm_int8_p(index)        ((int8_t *)tm_void(index))
#define tm_uint8_p(index)       ((uint8_t *)tm_void(index))
#define tm_int16_p(index)       ((int16_t *)tm_void(index))
#define tm_uint16_p(index)      ((uint16_t *)tm_void(index))
#define tm_int32_p(index)       ((int32_t *)tm_void(index))
#define tm_uint32_p(index)      ((uint32_t *)tm_void(index))


void tm_print_stats();

#endif

/** @} */
