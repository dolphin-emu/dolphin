/**
 * @file
 * Random number generator (RNG) module documentation file.
 */

/**
 * @addtogroup rng_module Random number generator (RNG) module
 * 
 * The Random number generator (RNG) module provides random number
 * generation, see \c ctr_dbrg_random() or \c havege_random().
 *
 * The former uses the block-cipher counter-mode based deterministic random
 * bit generator (CTR_DBRG) as specified in NIST SP800-90. It needs an external
 * source of entropy. For these purposes \c entropy_func() can be used. This is
 * an implementation based on a simple entropy accumulator design.
 *
 * The latter random number generator uses the HAVEGE (HArdware Volatile
 * Entropy Gathering and Expansion) software heuristic which is claimed 
 * to be an unpredictable or empirically strong* random number generation.
 *
 * \* Meaning that there seems to be no practical algorithm that can guess
 * the next bit with a probability larger than 1/2 in an output sequence.
 *
 * This module can be used to generate random numbers.
 */
