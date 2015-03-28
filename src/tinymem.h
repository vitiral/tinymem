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
 * \return          tm_index value. Use tm_void or other typecasts to convert
 *                  to useable pointer
 */
tm_index tmalloc(tm_size size);

/*---------------------------------------------------------------------------*/
/**
 * \brief           free memory index from tinymem
 * \param index     tm_index value to free
 * \return          void
 */
void tmfree(tm_index index);

/*---------------------------------------------------------------------------*/
/**
 * \brief           obtain void pointer from tm_index
 * \param index     valid tm_index
 * \return          (void *) with the original data
 */
inline void *tm_void(tm_index index);

/*---------------------------------------------------------------------------*/
/**
 * \brief           data type conversion macros
 * \param index     valid tm_index
 * \return          pointer of type specified
 */
#define tm_char_p(index)        ((char *)tm_void(index))
#define tm_int8_p(index)        ((int8_t *)tm_void(index))
#define tm_uint8_p(index)       ((uint8_t *)tm_void(index))
#define tm_int16_p(index)       ((int16_t *)tm_void(index))
#define tm_uint16_p(index)      ((uint16_t *)tm_void(index))
#define tm_int32_p(index)       ((int32_t *)tm_void(index))
#define tm_uint32_p(index)      ((uint32_t *)tm_void(index))
#endif

/** @} */
