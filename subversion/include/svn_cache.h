/**
 * @copyright
 * ====================================================================
 * Copyright (c) 2008 CollabNet.  All rights reserved.
 *
 * This software is licensed as described in the file COPYING, which
 * you should have received as part of this distribution.  The terms
 * are also available at http://subversion.tigris.org/license-1.html.
 * If newer versions of this license are posted there, you may use a
 * newer version instead, at your option.
 *
 * This software consists of voluntary contributions made by many
 * individuals.  For exact contribution history, see the revision
 * history and logs, available at http://subversion.tigris.org/.
 * ====================================================================
 * @endcopyright
 *
 * @file svn_cache.h
 * @brief In-memory cache implementation.
 */


#ifndef SVN_CACHE_H
#define SVN_CACHE_H

#include <apr_pools.h>
#include <apr_hash.h>

#include "svn_types.h"
#include "svn_error.h"
#include "svn_iter.h"


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */



/**
 * @defgroup svn_cache_support In-memory caching
 * @{
 */

/**
 * A function type for copying an object @a in into a different pool @pool
 *  and returning the result in @a *out.
 *
 * @since New in 1.6.
*/
typedef svn_error_t *(svn_cache_dup_func_t)(void **out,
                                            void *in,
                                            apr_pool_t *pool);

/**
 * A function type for deserializing an object @a *out from the string
 * @a data of length @a data_len in the pool @pool.
 *
 * @since New in 1.6.
*/
typedef svn_error_t *(svn_cache_deserialize_func_t)(void **out,
                                                    const char *data,
                                                    apr_size_t data_len,
                                                    apr_pool_t *pool);

/**
 * A function type for serializing an object @a in into bytes.  The
 * function should allocate the serialized value in @a pool, set @a
 * *data to the serialized value, and set *data_len to its length.
 *
 * @since New in 1.6.
*/
typedef svn_error_t *(svn_cache_serialize_func_t)(char **data,
                                                  apr_size_t *data_len,
                                                  void *in,
                                                  apr_pool_t *pool);

/**
 * Opaque type for an in-memory cache.
 *
 * @since New in 1.6.
 */
typedef struct svn_cache_t svn_cache_t;

/**
 * Creates a new cache in @a *cache_p.  This cache will use @a pool
 * for all of its storage needs.  The elements in the cache will be
 * indexed by keys of length @a klen, which may be APR_HASH_KEY_STRING
 * if they are strings.  Cached values will be copied in and out of
 * the cache using @a dup_func.
 *
 * The cache stores up to @a pages * @a items_per_page items at a
 * time.  The exact cache invalidation strategy is not defined here,
 * but in general, a lower value for @a items_per_page means more
 * memory overhead for the same number of items, but a higher value
 * for @a items_per_page means more items are cleared at once.  Both
 * @a pages and @a items_per_page must be positive (though they both
 * may certainly be 1).
 *
 * If @a thread_safe is true, and APR is compiled with threads, all
 * accesses to the cache will be protected with a mutex.
 *
 * Note that NULL is a legitimate value for cache entries (and @a dup_func
 * will not be called on it).
 *
 * It is not safe for @a dup_func to interact with the cache itself.
 *
 * @since New in 1.6.
 */
svn_error_t *
svn_cache_create_inprocess(svn_cache_t **cache_p,
                           svn_cache_dup_func_t *dup_func,
                           apr_ssize_t klen,
                           apr_int64_t pages,
                           apr_int64_t items_per_page,
                           svn_boolean_t thread_safe,
                           apr_pool_t *pool);
/**
 * Creates a new cache in @a *cache_p, communicating to a memcached
 * process.  The elements in the cache will be indexed by keys of
 * length @a klen, which may be APR_HASH_KEY_STRING if they are
 * strings.  Values will be serialized for memcached using @a
 * serialize_func and deserialized using @a deserialize_func.  Because
 * the same memcached server may cache many different kinds of values,
 * @a prefix should be specified to differentiate this cache from
 * other caches.  @a *cache_p will be allocated in @a pool.
 *
 * These caches are always thread safe.
 *
 * These caches do not support svn_cache_iter.
 *
 * @since New in 1.6.
 */
svn_error_t *
svn_cache_create_memcache(svn_cache_t **cache_p,
                          svn_cache_serialize_func_t *serialize_func,
                          svn_cache_deserialize_func_t *deserialize_func,
                          apr_ssize_t klen,
                          const char *prefix,
                          apr_pool_t *pool);


/**
 * Fetches a value indexed by @a key from @a cache into @a *value,
 * setting @a *found to TRUE iff it is in the cache and FALSE if it is
 * not found.  The value is copied into @a pool using the copy
 * function provided to the cache's constructor.
 *
 * @since New in 1.6.
 */
svn_error_t *
svn_cache_get(void **value,
              svn_boolean_t *found,
              svn_cache_t *cache,
              const void *key,
              apr_pool_t *pool);

/**
 * Stores the value @value under the key @a key in @a cache.  @a pool
 * is used only for temporary allocations.  The cache makes copies of
 * @a key and @a value if necessary (that is, @a key and @a value may
 * have shorter lifetimes than the cache).
 *
 * If there is already a value for @a key, this will replace it.  Bear
 * in mind that in some circumstances this may leak memory (that is,
 * the cache's copy of the previous value may not be immediately
 * cleared); it is only guaranteed to not leak for caches created with
 * @a items_per_page equal to 1.
 *
 * @since New in 1.6.
 */
svn_error_t *
svn_cache_set(svn_cache_t *cache,
              const void *key,
              void *value,
              apr_pool_t *pool);

/**
 * Iterates over the elements currently in @a cache, calling @a func
 * for each one until there are no more elements or @a func returns an
 * error.  Uses @a pool for temporary allocations.
 *
 * If @a completed is not NULL, then on return - if @a func returns no
 * errors - @a *completed will be set to @c TRUE.
 *
 * If @a func returns an error other than @c SVN_ERR_ITER_BREAK, that
 * error is returned.  When @a func returns @c SVN_ERR_ITER_BREAK,
 * iteration is interrupted, but no error is returned and @a *completed is
 * set to @c FALSE.
 *
 * It is not legal to perform any other cache operations on @a cache
 * inside @a func.
 *
 * svn_cache_iter is not supported by all cache implementations; see
 * the svn_cache_create_* function for details.
 *
 * @since New in 1.6.
 */
svn_error_t *
svn_cache_iter(svn_boolean_t *completed,
               svn_cache_t *cache,
               svn_iter_apr_hash_cb_t func,
               void *baton,
               apr_pool_t *pool);
/** @} */


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* SVN_CACHE_H */
